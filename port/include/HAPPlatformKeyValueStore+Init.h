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

#ifndef HAP_PLATFORM_KEY_VALUE_STORE_INIT_H
#define HAP_PLATFORM_KEY_VALUE_STORE_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**@file
 * NVS based key-value store implementation.
 *
 * **Example**

   @code{.c}

   // Allocate key-value store.
   static HAPPlatformKeyValueStore keyValueStore;

   // Initialize key-value store.
   HAPPlatformKeyValueStoreCreate(&keyValueStore,
       &(const HAPPlatformKeyValueStoreOptions) {
           .part_name = "nvs",
           .namespace_prefix = "hap"
       });

   @endcode
 */

/**
 * Key-value store item.
 *
 * - Each item stores the value of one key in RAM. Values are not stored persistently.
 */
typedef struct {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    bool active;
    HAPPlatformKeyValueStoreDomain domain;
    HAPPlatformKeyValueStoreKey key;
    size_t numBytes;
    uint8_t bytes[128];
    /**@endcond */
} HAPPlatformKeyValueStoreItem;

/**
 * Key-value store initialization options.
 */
typedef struct {
    /** Name of flash partition that will be used storing the Key-Value pairs.
     * Recommended names are "nvs" and "fctry" as they are the defaults.
     */
    const char *part_name;
    /** Prefix for the namespace under which the Key Value pairs will be stored. Recommended name is "hap"
     * or any other small name (upto 10 characters).
     */
    const char *namespace_prefix;

    /** Flag to indicate if erasing this partition is allowed */
    bool read_only;
} HAPPlatformKeyValueStoreOptions;

/**
 * Key-value store.
 */
struct HAPPlatformKeyValueStore {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    const char *part_name;
    const char *namespace_prefix;
    bool read_only;
    /**@endcond */
};

/**
 * Initializes the key-value store.
 *
 * @param[out] keyValueStore        Pointer to an allocated but uninitialized HAPPlatformKeyValueStore structure.
 * @param      options              Initialization options.
 */
void HAPPlatformKeyValueStoreCreate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        const HAPPlatformKeyValueStoreOptions* options);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
