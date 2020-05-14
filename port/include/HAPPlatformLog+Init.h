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

#ifndef HAP_PLATFORM_LOG_INIT_H
#define HAP_PLATFORM_LOG_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Logs a POSIX error, for example fetched from errno.
 *
 * - The error number will be mapped to a locale-dependent error message string.
 *
 * @param      type                 A log type constant, indicating the level of logging to perform.
 * @param      message              The log message.
 * @param      errorNumber          POSIX error number.
 * @param      function             Function name in which the error occurred.
 * @param      file                 File name in which the error occurred.
 * @param      line                 Line number on which the error occurred.
 */
void HAPPlatformLogPOSIXError(
        HAPLogType type,
        const char* message,
        int errorNumber,
        const char* function,
        const char* file,
        int line);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
