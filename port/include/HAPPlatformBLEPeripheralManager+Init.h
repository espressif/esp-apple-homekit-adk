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

#ifndef HAP_PLATFORM_BLE_PERIPHERAL_MANAGER_INIT_H
#define HAP_PLATFORM_BLE_PERIPHERAL_MANAGER_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAPPlatform.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

typedef struct {
    HAPPlatformBLEPeripheralManagerUUID type;
    bool isPrimary;
    HAPPlatformBLEPeripheralManagerAttributeHandle handle;
} HAPPlatformBLEPeripheralManagerService;

typedef struct {
    HAPPlatformBLEPeripheralManagerUUID type;
    HAPPlatformBLEPeripheralManagerCharacteristicProperties properties;
    HAPPlatformBLEPeripheralManagerAttributeHandle handle;
    HAPPlatformBLEPeripheralManagerAttributeHandle valueHandle;
    HAPPlatformBLEPeripheralManagerAttributeHandle cccDescriptorHandle;
} HAPPlatformBLEPeripheralManagerCharacteristic;

typedef struct {
    HAPPlatformBLEPeripheralManagerUUID type;
    HAPPlatformBLEPeripheralManagerDescriptorProperties properties;
    HAPPlatformBLEPeripheralManagerAttributeHandle handle;
} HAPPlatformBLEPeripheralManagerDescriptor;

HAP_ENUM_BEGIN(uint8_t, HAPPlatformBLEPeripheralManagerAttributeType) {
    kHAPPlatformBLEPeripheralManagerAttributeType_None,
    kHAPPlatformBLEPeripheralManagerAttributeType_Service,
    kHAPPlatformBLEPeripheralManagerAttributeType_Characteristic,
    kHAPPlatformBLEPeripheralManagerAttributeType_Descriptor
} HAP_ENUM_END(uint8_t, HAPPlatformBLEPeripheralManagerAttributeType);

typedef struct {
    HAPPlatformBLEPeripheralManagerAttributeType type;
    union {
        HAPPlatformBLEPeripheralManagerDescriptor descriptor;
        HAPPlatformBLEPeripheralManagerCharacteristic characteristic;
        HAPPlatformBLEPeripheralManagerService service;
    } _;
} HAPPlatformBLEPeripheralManagerAttribute;

/**
 * BLE peripheral manager initialization options.
 */
typedef struct {
    HAPPlatformBLEPeripheralManagerAttribute* attributes;
    size_t numAttributes;
} HAPPlatformBLEPeripheralManagerOptions;

/**
 * BLE peripheral manager.
 */
struct HAPPlatformBLEPeripheralManager {
    // Opaque type. Do not access the instance fields directly.
    /**@cond */
    HAPPlatformBLEPeripheralManagerAttribute* attributes;
    size_t numAttributes;

    HAPPlatformBLEPeripheralManagerDelegate delegate;
    HAPPlatformBLEPeripheralManagerDeviceAddress deviceAddress;
    char deviceName[64 + 1];

    uint8_t advertisingBytes[31];
    uint8_t numAdvertisingBytes;
    uint8_t scanResponseBytes[31];
    uint8_t numScanResponseBytes;
    HAPBLEAdvertisingInterval advertisingInterval;

    bool isDeviceAddressSet : 1;
    bool didPublishAttributes : 1;
    bool isConnected : 1;
    /**@endcond */
};

/**
 * Initializes the BLE peripheral manager.
 *
 * @param[out] blePeripheralManager Pointer to an allocated but uninitialized HAPPlatformBLEPeripheralManager structure.
 * @param      options              Initialization options.
 */
void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager,
        const HAPPlatformBLEPeripheralManagerOptions* options);

#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
