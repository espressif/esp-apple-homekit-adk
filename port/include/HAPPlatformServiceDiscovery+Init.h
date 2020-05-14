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

#ifndef HAP_PLATFORM_SERVICE_DISCOVERY_INIT_H
#define HAP_PLATFORM_SERVICE_DISCOVERY_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"
#include "HAPPlatformFileHandle.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**@file
 * Bonjour service discovery using ESP IDF mdns component
 *
 * **Example**

   @code{.c}

   // Allocate service discovery object.
   static HAPPlatformServiceDiscovery serviceDiscovery;

   // Initialize service discovery object. Depends on a TCP stream manager instance.
   HAPPlatformServiceDiscoveryCreate(&platform.serviceDiscovery,
       &(const HAPPlatformServiceDiscoveryOptions) {
           // Register services on all available network interfaces.
           .hostname = NULL
       });

   @endcode
 */

/**
 * Service discovery initialization options.
 */
typedef struct {
    /**
     * Hostname. A value of NULL means use default hostname.
     */
    char *_Nullable hostName;
} HAPPlatformServiceDiscoveryOptions;

/**
 * Service discovery.
 */
struct HAPPlatformServiceDiscovery {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    char serv_type[32];
    char proto[32];
    /**@endcond */
};

/**
 * Initializes service discovery.
 *
 * @param      serviceDiscovery     Pointer to an allocated but uninitialized HAPPlatformServiceDiscovery structure.
 * @param      options              Initialization options.
 */
void HAPPlatformServiceDiscoveryCreate(
        HAPPlatformServiceDiscoveryRef serviceDiscovery,
        const HAPPlatformServiceDiscoveryOptions* options);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
