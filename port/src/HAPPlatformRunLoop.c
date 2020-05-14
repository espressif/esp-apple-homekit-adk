// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.
//
// Copyright 2020 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This implementation is based on `select` for maximum portability but may be extended to also support
// `poll`, `epoll` or `kqueue`.

#include "HAPPlatform.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/select.h>

#include "HAPPlatform+Init.h"
#include "HAPPlatformFileHandle.h"
#include "HAPPlatformLog+Init.h"
#include "HAPPlatformRunLoop+Init.h"

// Header files added by Espressif
#include <string.h>
#include <sys/types.h>
#include <lwip/sockets.h>
#include <sys/syslimits.h>

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "RunLoop" };

#define LOOPBACK_PORT   12321

/**
 * Internal file handle type, representing the registration of a platform-specific file descriptor.
 */
typedef struct HAPPlatformFileHandle HAPPlatformFileHandle;

/**
 * Internal file handle representation.
 */
struct HAPPlatformFileHandle {
    /**
     * Platform-specific file descriptor.
     */
    int fileDescriptor;

    /**
     * Set of file handle events on which the callback shall be invoked.
     */
    HAPPlatformFileHandleEvent interests;

    /**
     * Function to call when one or more events occur on the given file descriptor.
     */
    HAPPlatformFileHandleCallback callback;

    /**
     * The context parameter given to the HAPPlatformFileHandleRegister function.
     */
    void* _Nullable context;

    /**
     * Previous file handle in linked list.
     */
    HAPPlatformFileHandle* _Nullable prevFileHandle;

    /**
     * Next file handle in linked list.
     */
    HAPPlatformFileHandle* _Nullable nextFileHandle;

    /**
     * Flag indicating whether the platform-specific file descriptor is registered with an I/O multiplexer or not.
     */
    bool isAwaitingEvents;
};

/**
 * Internal timer type.
 */
typedef struct HAPPlatformTimer HAPPlatformTimer;

/**
 * Internal timer representation.
 */
struct HAPPlatformTimer {
    /**
     * Deadline at which the timer expires.
     */
    HAPTime deadline;

    /**
     * Callback that is invoked when the timer expires.
     */
    HAPPlatformTimerCallback callback;

    /**
     * The context parameter given to the HAPPlatformTimerRegister function.
     */
    void* _Nullable context;

    /**
     * Next timer in linked list.
     */
    HAPPlatformTimer* _Nullable nextTimer;
};

/**
 * Run loop state.
 */
HAP_ENUM_BEGIN(uint8_t, HAPPlatformRunLoopState) { /**
                                                    * Idle.
                                                    */
                                                   kHAPPlatformRunLoopState_Idle,

                                                   /**
                                                    * Running.
                                                    */
                                                   kHAPPlatformRunLoopState_Running,

                                                   /**
                                                    * Stopping.
                                                    */
                                                   kHAPPlatformRunLoopState_Stopping
} HAP_ENUM_END(uint8_t, HAPPlatformRunLoopState);

static struct {
    /**
     * Sentinel node of a circular doubly-linked list of file handles
     */
    HAPPlatformFileHandle fileHandleSentinel;

    /**
     * Pointer to sentinel node, representing a circular doubly-linked list of file handles
     */
    HAPPlatformFileHandle* _Nullable fileHandles;

    /**
     * File handle cursor, used to handle reentrant modifications of global file handle list during iteration.
     */
    HAPPlatformFileHandle* _Nullable fileHandleCursor;

    /**
     * Start of linked list of timers, ordered by deadline.
     */
    HAPPlatformTimer* _Nullable timers;
    
    /**
     * Loopback file descriptor to receive data.
     */
    volatile int loopbackFileDescriptor;

    /**
     * Self-pipe byte buffer.
     *
     * - Callbacks are serialized into the buffer as:
     *   - 8-byte aligned callback pointer.
     *   - Context size (up to UINT8_MAX).
     *   - Context (unaligned). When invoking the callback, the context is first moved to be 8-byte aligned.
     */
    HAP_ALIGNAS(8)
    char loopbackBytes[sizeof(HAPPlatformRunLoopCallback) + 1 + UINT8_MAX];

    /**
     * Number of bytes in self-pipe byte buffer.
     */
    size_t numLoopbackBytes;

    /**
     * File handle for self-pipe.
     */
    HAPPlatformFileHandleRef loopbackFileHandle;

    /**
     * Current run loop state.
     */
    HAPPlatformRunLoopState state;
} runLoop = { .fileHandleSentinel = { .fileDescriptor = -1,
                                      .interests = { .isReadyForReading = false,
                                                     .isReadyForWriting = false,
                                                     .hasErrorConditionPending = false },
                                      .callback = NULL,
                                      .context = NULL,
                                      .prevFileHandle = &runLoop.fileHandleSentinel,
                                      .nextFileHandle = &runLoop.fileHandleSentinel,
                                      .isAwaitingEvents = false },
              .fileHandles = &runLoop.fileHandleSentinel,
              .fileHandleCursor = &runLoop.fileHandleSentinel,

              .timers = NULL,

              .loopbackFileDescriptor = -1 };

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileHandleRegister(
        HAPPlatformFileHandleRef* fileHandle_,
        int fileDescriptor,
        HAPPlatformFileHandleEvent interests,
        HAPPlatformFileHandleCallback callback,
        void* _Nullable context) {
    HAPPrecondition(fileHandle_);

    // Prepare fileHandle.
    HAPPlatformFileHandle* fileHandle = calloc(1, sizeof(HAPPlatformFileHandle));
    if (!fileHandle) {
        HAPLog(&logObject, "Cannot allocate more file handles.");
        *fileHandle_ = 0;
        return kHAPError_OutOfResources;
    }
    fileHandle->fileDescriptor = fileDescriptor;
    fileHandle->interests = interests;
    fileHandle->callback = callback;
    fileHandle->context = context;
    fileHandle->prevFileHandle = runLoop.fileHandles->prevFileHandle;
    fileHandle->nextFileHandle = runLoop.fileHandles;
    fileHandle->isAwaitingEvents = false;
    runLoop.fileHandles->prevFileHandle->nextFileHandle = fileHandle;
    runLoop.fileHandles->prevFileHandle = fileHandle;

    *fileHandle_ = (HAPPlatformFileHandleRef) fileHandle;
    return kHAPError_None;
}

void HAPPlatformFileHandleUpdateInterests(
        HAPPlatformFileHandleRef fileHandle_,
        HAPPlatformFileHandleEvent interests,
        HAPPlatformFileHandleCallback callback,
        void* _Nullable context) {
    HAPPrecondition(fileHandle_);
    HAPPlatformFileHandle* fileHandle = (HAPPlatformFileHandle * _Nonnull) fileHandle_;

    fileHandle->interests = interests;
    fileHandle->callback = callback;
    fileHandle->context = context;
}

void HAPPlatformFileHandleDeregister(HAPPlatformFileHandleRef fileHandle_) {
    HAPPrecondition(fileHandle_);
    HAPPlatformFileHandle* fileHandle = (HAPPlatformFileHandle * _Nonnull) fileHandle_;

    HAPPrecondition(fileHandle->prevFileHandle);
    HAPPrecondition(fileHandle->nextFileHandle);

    if (fileHandle == runLoop.fileHandleCursor) {
        runLoop.fileHandleCursor = fileHandle->nextFileHandle;
    }

    fileHandle->prevFileHandle->nextFileHandle = fileHandle->nextFileHandle;
    fileHandle->nextFileHandle->prevFileHandle = fileHandle->prevFileHandle;

    fileHandle->fileDescriptor = -1;
    fileHandle->interests.isReadyForReading = false;
    fileHandle->interests.isReadyForWriting = false;
    fileHandle->interests.hasErrorConditionPending = false;
    fileHandle->callback = NULL;
    fileHandle->context = NULL;
    fileHandle->nextFileHandle = NULL;
    fileHandle->prevFileHandle = NULL;
    fileHandle->isAwaitingEvents = false;
    HAPPlatformFreeSafe(fileHandle);
}

static void ProcessSelectedFileHandles(
        fd_set* readFileDescriptors,
        fd_set* writeFileDescriptors,
        fd_set* errorFileDescriptors) {
    HAPPrecondition(readFileDescriptors);
    HAPPrecondition(writeFileDescriptors);
    HAPPrecondition(errorFileDescriptors);

    runLoop.fileHandleCursor = runLoop.fileHandles->nextFileHandle;
    while (runLoop.fileHandleCursor != runLoop.fileHandles) {
        HAPPlatformFileHandle* fileHandle = runLoop.fileHandleCursor;
        runLoop.fileHandleCursor = fileHandle->nextFileHandle;

        if (fileHandle->isAwaitingEvents) {
            HAPAssert(fileHandle->fileDescriptor != -1);
            fileHandle->isAwaitingEvents = false;
            if (fileHandle->callback) {
                HAPPlatformFileHandleEvent fileHandleEvents;
                fileHandleEvents.isReadyForReading = fileHandle->interests.isReadyForReading &&
                                                     FD_ISSET(fileHandle->fileDescriptor, readFileDescriptors);
                fileHandleEvents.isReadyForWriting = fileHandle->interests.isReadyForWriting &&
                                                     FD_ISSET(fileHandle->fileDescriptor, writeFileDescriptors);
                fileHandleEvents.hasErrorConditionPending = fileHandle->interests.hasErrorConditionPending &&
                                                            FD_ISSET(fileHandle->fileDescriptor, errorFileDescriptors);

                if (fileHandleEvents.isReadyForReading || fileHandleEvents.isReadyForWriting ||
                    fileHandleEvents.hasErrorConditionPending) {
                    fileHandle->callback((HAPPlatformFileHandleRef) fileHandle, fileHandleEvents, fileHandle->context);
                }
            }
        }
    }
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformTimerRegister(
        HAPPlatformTimerRef* timer_,
        HAPTime deadline,
        HAPPlatformTimerCallback callback,
        void* _Nullable context) {
    HAPPrecondition(timer_);
    HAPPlatformTimer* _Nullable* newTimer = (HAPPlatformTimer * _Nullable*) timer_;
    HAPPrecondition(callback);

    // Prepare timer.
    *newTimer = calloc(1, sizeof(HAPPlatformTimer));
    if (!*newTimer) {
        HAPLog(&logObject, "Cannot allocate more timers.");
        return kHAPError_OutOfResources;
    }
    (*newTimer)->deadline = deadline ? deadline : 1;
    (*newTimer)->callback = callback;
    (*newTimer)->context = context;

    // Insert timer.
    for (HAPPlatformTimer* _Nullable* nextTimer = &runLoop.timers;; nextTimer = &(*nextTimer)->nextTimer) {
        if (!*nextTimer) {
            (*newTimer)->nextTimer = NULL;
            *nextTimer = *newTimer;
            break;
        }
        if ((*nextTimer)->deadline > deadline) {
            // Search condition must be '>' and not '>=' to ensure that timers fire in ascending order of their
            // deadlines and that timers registered with the same deadline fire in order of registration.
            (*newTimer)->nextTimer = *nextTimer;
            *nextTimer = *newTimer;
            break;
        }
    }

    return kHAPError_None;
}

void HAPPlatformTimerDeregister(HAPPlatformTimerRef timer_) {
    HAPPrecondition(timer_);
    HAPPlatformTimer* timer = (HAPPlatformTimer*) timer_;

    // Find and remove timer.
    for (HAPPlatformTimer* _Nullable* nextTimer = &runLoop.timers; *nextTimer; nextTimer = &(*nextTimer)->nextTimer) {
        if (*nextTimer == timer) {
            *nextTimer = timer->nextTimer;
            HAPPlatformFreeSafe(timer);
            return;
        }
    }

    // Timer not found.
    HAPFatalError();
}

static void ProcessExpiredTimers(void) {
    // Get current time.
    HAPTime now = HAPPlatformClockGetCurrent();

    // Enumerate timers.
    while (runLoop.timers) {
        if (runLoop.timers->deadline > now) {
            break;
        }

        // Update head, so that reentrant add / removes do not interfere.
        HAPPlatformTimer* expiredTimer = runLoop.timers;
        runLoop.timers = runLoop.timers->nextTimer;

        // Invoke callback.
        expiredTimer->callback((HAPPlatformTimerRef) expiredTimer, expiredTimer->context);

        // Free memory.
        HAPPlatformFreeSafe(expiredTimer);
    }
}

void CloseLoopback(int fileDescriptor)
{
    if (fileDescriptor != -1) {
        HAPLogDebug(&logObject, "close(%d);", fileDescriptor);
        int e = close(fileDescriptor);
        if (e != 0) {
            int _errno = errno;
            HAPAssert(e == -1);
            HAPPlatformLogPOSIXError(kHAPLogType_Error, "Closing loopback failed (log, fileDescriptor0).",
                _errno, __func__, HAP_FILE, __LINE__);
        }
    }
}

static void HandleLoopbackFileHandleCallback(
    HAPPlatformFileHandleRef fileHandle,
    HAPPlatformFileHandleEvent fileHandleEvents,
    void *_Nullable context HAP_UNUSED)
{
    HAPAssert(fileHandle);
    HAPAssert(fileHandle == runLoop.loopbackFileHandle);
    HAPAssert(fileHandleEvents.isReadyForReading);

    HAPAssert(runLoop.numLoopbackBytes < sizeof runLoop.loopbackBytes);
    
    ssize_t n;
    do {
        n = recvfrom(runLoop.loopbackFileDescriptor,
                &runLoop.loopbackBytes[runLoop.numLoopbackBytes],
                sizeof runLoop.loopbackBytes - runLoop.numLoopbackBytes, 0, NULL, NULL);
    } while (n == -1 && errno == EINTR);
    if (n == -1 && errno == EAGAIN) {
        return;
    }

    if (n < 0) {
        int _errno = errno;
        HAPAssert(n == -1);
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "Loopback read failed.", _errno, __func__, HAP_FILE, __LINE__);
        HAPFatalError();
    }
    if (n == 0) {
        HAPLogError(&logObject, "Loopback socket read returned no data.");
        HAPFatalError();
    }

    HAPAssert((size_t) n <= sizeof runLoop.loopbackBytes - runLoop.numLoopbackBytes);
    runLoop.numLoopbackBytes += (size_t) n;
    for (;;) {
        if (runLoop.numLoopbackBytes < sizeof (HAPPlatformRunLoopCallback) + 1) {
            break;
        }
        size_t contextSize = (size_t) runLoop.loopbackBytes[sizeof (HAPPlatformRunLoopCallback)];
        if (runLoop.numLoopbackBytes < sizeof (HAPPlatformRunLoopCallback) + 1 + contextSize) {
            break;
        }

        HAPPlatformRunLoopCallback callback;
        HAPRawBufferCopyBytes(
            &callback,
            &runLoop.loopbackBytes[0],
            sizeof (HAPPlatformRunLoopCallback));
        HAPRawBufferCopyBytes(
            &runLoop.loopbackBytes[0],
            &runLoop.loopbackBytes[sizeof (HAPPlatformRunLoopCallback) + 1],
            runLoop.numLoopbackBytes - (sizeof (HAPPlatformRunLoopCallback) + 1));
        runLoop.numLoopbackBytes -= (sizeof (HAPPlatformRunLoopCallback) + 1);

        // Issue memory barrier to ensure visibility of data referenced by callback context.
        __sync_synchronize();

        callback(
            contextSize ? &runLoop.loopbackBytes[0] : NULL,
            contextSize);

        HAPRawBufferCopyBytes(
            &runLoop.loopbackBytes[0],
            &runLoop.loopbackBytes[contextSize],
            runLoop.numLoopbackBytes - contextSize);
        runLoop.numLoopbackBytes -= contextSize;
    }
}

void HAPPlatformRunLoopCreate(const HAPPlatformRunLoopOptions* options) {
    HAPPrecondition(options);
    HAPPrecondition(options->keyValueStore);
    HAPError err;

    HAPLogDebug(&logObject, "Storage configuration: runLoop = %lu", (unsigned long) sizeof runLoop);
    HAPLogDebug(&logObject, "Storage configuration: fileHandle = %lu", (unsigned long) sizeof(HAPPlatformFileHandle));
    HAPLogDebug(&logObject, "Storage configuration: timer = %lu", (unsigned long) sizeof(HAPPlatformTimer));

    // Open loop back

    HAPPrecondition(runLoop.loopbackFileDescriptor == -1);
    int fileDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fileDescriptor < 0) {
        int _errno = errno;
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "Loopback creation failed (log, call 'socket').",
            _errno, __func__, HAP_FILE, __LINE__);
        HAPFatalError();
    }
    int e = fcntl(fileDescriptor, F_SETFL, O_NONBLOCK);
    if (e == -1) {
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "System call 'fcntl' to set loopback recv file descriptor flags to 'non-blocking' failed.",
            errno, __func__, HAP_FILE, __LINE__);
        HAPFatalError();
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LOOPBACK_PORT);
    inet_aton("127.0.0.1", &addr.sin_addr);
    if (bind(fileDescriptor, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CloseLoopback(fileDescriptor);
        int _errno = errno;
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "Loopback socket bind failed (log, call 'bind').",
            _errno, __func__, HAP_FILE, __LINE__);
        HAPFatalError();
    }

    runLoop.loopbackFileDescriptor = fileDescriptor;

    err = HAPPlatformFileHandleRegister(&runLoop.loopbackFileHandle,
        runLoop.loopbackFileDescriptor,
        (HAPPlatformFileHandleEvent) {
            .isReadyForReading = true,
            .isReadyForWriting = false,
            .hasErrorConditionPending = false
        },
        HandleLoopbackFileHandleCallback, NULL);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Failed to register loopback file handle.");
        HAPFatalError();
    }
    HAPAssert(runLoop.loopbackFileHandle);

    runLoop.state = kHAPPlatformRunLoopState_Idle;
    
    // Issue memory barrier to ensure visibility of write to runLoop.selfPipeFileDescriptor1 on other threads.
    __sync_synchronize();
}

void HAPPlatformRunLoopRelease(void) {
    CloseLoopback(runLoop.loopbackFileDescriptor);

    runLoop.loopbackFileDescriptor = -1;

    if (runLoop.loopbackFileHandle) {
        HAPPlatformFileHandleDeregister(runLoop.loopbackFileHandle);
        runLoop.loopbackFileHandle = 0;
    }

    runLoop.state = kHAPPlatformRunLoopState_Idle;

    // Issue memory barrier to ensure visibility of write to runLoop.loopbackFileDescriptor1 on other threads.
    __sync_synchronize();
}

void HAPPlatformRunLoopRun(void) {
    HAPPrecondition(runLoop.state == kHAPPlatformRunLoopState_Idle);

    HAPLogInfo(&logObject, "Entering run loop.");
    runLoop.state = kHAPPlatformRunLoopState_Running;
    do {
        fd_set readFileDescriptors;
        fd_set writeFileDescriptors;
        fd_set errorFileDescriptors;

        FD_ZERO(&readFileDescriptors);
        FD_ZERO(&writeFileDescriptors);
        FD_ZERO(&errorFileDescriptors);

        int maxFileDescriptor = -1;

        HAPPlatformFileHandle* fileHandle = runLoop.fileHandles->nextFileHandle;
        while (fileHandle != runLoop.fileHandles) {
            fileHandle->isAwaitingEvents = false;
            if (fileHandle->fileDescriptor != -1) {
                if (fileHandle->interests.isReadyForReading) {
                    HAPAssert(fileHandle->fileDescriptor >= 0);
                    HAPAssert(fileHandle->fileDescriptor < FD_SETSIZE);
                    FD_SET(fileHandle->fileDescriptor, &readFileDescriptors);
                    if (fileHandle->fileDescriptor > maxFileDescriptor) {
                        maxFileDescriptor = fileHandle->fileDescriptor;
                    }
                    fileHandle->isAwaitingEvents = true;
                }
                if (fileHandle->interests.isReadyForWriting) {
                    HAPAssert(fileHandle->fileDescriptor >= 0);
                    HAPAssert(fileHandle->fileDescriptor < FD_SETSIZE);
                    FD_SET(fileHandle->fileDescriptor, &writeFileDescriptors);
                    if (fileHandle->fileDescriptor > maxFileDescriptor) {
                        maxFileDescriptor = fileHandle->fileDescriptor;
                    }
                    fileHandle->isAwaitingEvents = true;
                }
                if (fileHandle->interests.hasErrorConditionPending) {
                    HAPAssert(fileHandle->fileDescriptor >= 0);
                    HAPAssert(fileHandle->fileDescriptor < FD_SETSIZE);
                    FD_SET(fileHandle->fileDescriptor, &errorFileDescriptors);
                    if (fileHandle->fileDescriptor > maxFileDescriptor) {
                        maxFileDescriptor = fileHandle->fileDescriptor;
                    }
                    fileHandle->isAwaitingEvents = true;
                }
            }
            fileHandle = fileHandle->nextFileHandle;
        }

        struct timeval timeoutValue;
        struct timeval* timeout = NULL;

        HAPTime nextDeadline = runLoop.timers ? runLoop.timers->deadline : 0;
        if (nextDeadline) {
            HAPTime now = HAPPlatformClockGetCurrent();
            HAPTime delta;
            if (nextDeadline > now) {
                delta = nextDeadline - now;
            } else {
                delta = 0;
            }
            HAPAssert(!timeout);
            timeout = &timeoutValue;
            timeout->tv_sec = (time_t)(delta / 1000);
            timeout->tv_usec = (suseconds_t)((delta % 1000) * 1000);
        }

        HAPAssert(maxFileDescriptor >= -1);
        HAPAssert(maxFileDescriptor < FD_SETSIZE);

        int e = select(
                maxFileDescriptor + 1, &readFileDescriptors, &writeFileDescriptors, &errorFileDescriptors, timeout);
        if (e == -1 && errno == EINTR) {
            continue;
        }
        if (e < 0) {
            int _errno = errno;
            HAPAssert(e == -1);
            HAPPlatformLogPOSIXError(
                    kHAPLogType_Error, "System call 'select' failed.", _errno, __func__, HAP_FILE, __LINE__);
            HAPFatalError();
        }

        ProcessExpiredTimers();

        ProcessSelectedFileHandles(&readFileDescriptors, &writeFileDescriptors, &errorFileDescriptors);
    } while (runLoop.state == kHAPPlatformRunLoopState_Running);

    HAPLogInfo(&logObject, "Exiting run loop.");
    HAPAssert(runLoop.state == kHAPPlatformRunLoopState_Stopping);
    runLoop.state = kHAPPlatformRunLoopState_Idle;
}

void HAPPlatformRunLoopStop(void) {
    if (runLoop.state == kHAPPlatformRunLoopState_Running) {
        runLoop.state = kHAPPlatformRunLoopState_Stopping;
    }
}

HAPError HAPPlatformRunLoopScheduleCallback(
        HAPPlatformRunLoopCallback callback,
        void* _Nullable const context,
        size_t contextSize) {
    HAPPrecondition(callback);
    HAPPrecondition(!contextSize || context);

    if (contextSize > UINT8_MAX) {
        HAPLogError(&logObject, "Contexts larger than UINT8_MAX are not supported.");
        return kHAPError_OutOfResources;
    }
    if (contextSize + 1 + sizeof callback > PIPE_BUF) {
        HAPLogError(&logObject, "Context too large (PIPE_BUF).");
        return kHAPError_OutOfResources;
    }

    // Serialize event context.
    // Format: Callback pointer followed by 1 byte context size and context data.
    // Context is copied to offset 0 when invoking the callback to ensure proper alignment.
    uint8_t bytes[sizeof callback + 1 + UINT8_MAX];
    size_t numBytes = 0;
    HAPRawBufferCopyBytes(&bytes[numBytes], &callback, sizeof callback);
    numBytes += sizeof callback;
    bytes[numBytes] = (uint8_t) contextSize;
    numBytes++;
    if (context) {
        HAPRawBufferCopyBytes(&bytes[numBytes], context, contextSize);
        numBytes += contextSize;
    }
    HAPAssert(numBytes <= sizeof bytes);

    int fileDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fileDescriptor < 0) {
        int _errno = errno;
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "Loopback client socket failed (log, call 'bind').",
            _errno, __func__, HAP_FILE, __LINE__);
        HAPFatalError();
    }

    int e = fcntl(fileDescriptor, F_SETFL, O_NONBLOCK);
    if (e == -1) {
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "System call 'fcntl' to set loopback send file descriptor flags to 'non-blocking' failed.",
            errno, __func__, HAP_FILE, __LINE__);
        HAPFatalError();
    }
    
    struct sockaddr_in to_addr;
    to_addr.sin_family = AF_INET;
    to_addr.sin_port = htons(LOOPBACK_PORT);
    inet_aton("127.0.0.1", &to_addr.sin_addr);
    ssize_t n;
    do {
        n = sendto(fileDescriptor, bytes, numBytes, 0, (struct sockaddr *)&to_addr, sizeof(to_addr));
    } while (n == -1 && errno == EINTR);
    close(fileDescriptor);
    if (n == -1) {
        int _errno = errno;
        HAPPlatformLogPOSIXError(kHAPLogType_Error,
            "Loopback client socket failed to send data (log, call 'sendto').",
            _errno, __func__, HAP_FILE, __LINE__);
        return kHAPError_Unknown;
    }

    return kHAPError_None;
}
