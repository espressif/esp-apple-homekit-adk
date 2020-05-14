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

#ifndef HAP_PLATFORM_ACCESSORY_SETUP_INIT_H
#define HAP_PLATFORM_ACCESSORY_SETUP_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**@file
 * Accessory setup manager.
 *
 * - The linked key-value store needs to be provisioned with accessory setup information
 *   before this implementation may be used. Please refer to the Provision tool.
 *
 * **Example**

   @code{.c}

   // Get dependencies.
   HAPPlatformKeyValueStoreRef keyValueStore;

   // Allocate accessory setup manager.
   static HAPPlatformAccessorySetup accessorySetup;

   // Initialize accessory setup manager.
   HAPPlatformAccessorySetupCreate(&accessorySetup,
       &(const HAPPlatformAccessorySetupOptions) {
           .keyValueStore = keyValueStore
       });

   @endcode
 */

/**
 * Accessory setup manager initialization options.
 */
typedef struct {
    /**
     * Key-value store.
     */
    HAPPlatformKeyValueStoreRef keyValueStore;
} HAPPlatformAccessorySetupOptions;

/**
 * Accessory setup manager.
 */
struct HAPPlatformAccessorySetup {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    HAPPlatformKeyValueStoreRef keyValueStore;
    /**@endcond */
};

/**
 * Initializes the accessory setup manager.
 *
 * @param[out] accessorySetup       Pointer to an allocated but uninitialized HAPPlatformAccessorySetup structure.
 * @param      options              Initialization options.
 */
void HAPPlatformAccessorySetupCreate(
        HAPPlatformAccessorySetupRef accessorySetup,
        const HAPPlatformAccessorySetupOptions* options);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
