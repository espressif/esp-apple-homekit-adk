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

#ifndef HAP_PLATFORM_MFI_HW_AUTH_INIT_H
#define HAP_PLATFORM_MFI_HW_AUTH_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Apple Authentication Coprocessor provider.
 */
struct HAPPlatformMFiHWAuth {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    uint8_t slaveAddr;
    bool poweredOn;
    /**@endcond */
};

/**
 * Initializes an Apple Authentication Coprocessor provider.
 *
 * @param[out] mfiHWAuth            Pointer to an allocated but uninitialized HAPPlatformMFiHWAuth structure.
 */
void HAPPlatformMFiHWAuthCreate(HAPPlatformMFiHWAuthRef mfiHWAuth);

/**
 * Deinitializes an Apple Authentication Coprocessor.
 *
 * @param      mfiHWAuth            Initialized Apple Authentication Coprocessor provider.
 */
void HAPPlatformMFiHWAuthRelease(HAPPlatformMFiHWAuthRef mfiHWAuth);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
