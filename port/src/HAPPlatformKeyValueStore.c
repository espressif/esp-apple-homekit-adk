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

#include "HAPPlatformKeyValueStore+Init.h"
#include <string.h>
#include <nvs.h>
#include <nvs_flash.h>
static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "KeyValueStore" };

void HAPPlatformKeyValueStoreCreate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        const HAPPlatformKeyValueStoreOptions* options) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(options);
    HAPPrecondition(options->part_name);
    HAPPrecondition(options->namespace_prefix);

    // Initialize NVS

    if (options->read_only) {
        nvs_flash_init_partition(options->part_name);
    } else {
        esp_err_t err;
        err = nvs_flash_init_partition(options->part_name);
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            // NVS partition was truncated and needs to be erased
            // Retry nvs_flash_init
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init_partition(options->part_name);
        }
        ESP_ERROR_CHECK(err);
    }

    keyValueStore->part_name = strdup(options->part_name);
    keyValueStore->namespace_prefix = strdup(options->namespace_prefix);
    keyValueStore->read_only = options->read_only;

    HAPLog(&logObject, "keyValueStore %s Initialised", keyValueStore->part_name);
}

/**
 * Gets the handle for accessing the key value store.
 *
 * @param       keyValueStore   Key-value store.
 * @param       domain          Domain.
 * @param[out]  store_handle    Pointer to an allocated NVS storage handle
 */
HAP_RESULT_USE_CHECK
static esp_err_t HAPPlatformKeyValueStoreGetHandle(
    HAPPlatformKeyValueStoreRef keyValueStore,
    HAPPlatformKeyValueStoreDomain domain,
    nvs_handle *store_handle)
{
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->namespace_prefix);
    char name_space[15];
    snprintf(name_space, sizeof(name_space), "%s.%02X", keyValueStore->namespace_prefix, domain);
    return nvs_open_from_partition(keyValueStore->part_name, name_space, NVS_READWRITE, store_handle);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreGet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        void* _Nullable const bytes,
        size_t maxBytes,
        size_t* _Nullable numBytes,
        bool* found) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(!maxBytes || bytes);
    HAPPrecondition((bytes == NULL) == (numBytes == NULL));
    HAPPrecondition(found);

    nvs_handle store_handle;

    esp_err_t err;

    err = HAPPlatformKeyValueStoreGetHandle(keyValueStore, domain, &store_handle);
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) opening NVS!", err);
        return kHAPError_Unknown;
    }

    char keyname[15]; /* 15 is max key length for NVS */
    snprintf(keyname, sizeof(keyname), "%02X", key);

    size_t num_bytes = maxBytes;

    *found = false;
    err = nvs_get_blob(store_handle, keyname, bytes, &num_bytes);
    nvs_close(store_handle);
    /* Checking error code after nvs_close(), because the close has to be called in any case */
    if (err != ESP_OK) {
        HAPLog(&logObject, "Error (%d). Key %02X not found in KeyStore", err, key);
        return kHAPError_None;
    }
    *found = true;
    if(numBytes != NULL){
        *numBytes = num_bytes;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreSet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        const void* bytes,
        size_t numBytes) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(bytes);

    HAPLogBufferDebug(&logObject, bytes, numBytes, "Write %02X.%02X", domain, key);

    nvs_handle store_handle;
    esp_err_t err;
    err = HAPPlatformKeyValueStoreGetHandle(keyValueStore, domain, &store_handle);
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) opening NVS!", err);
        return kHAPError_Unknown;
    }

    char keyname[15]; /* 15 is max key length for NVS */
    snprintf(keyname, sizeof(keyname), "%02X", key);
    err = nvs_set_blob(store_handle, keyname, (const void *) bytes, (size_t) numBytes);
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) setting NVS blob!", err);
        nvs_close(store_handle);
        return kHAPError_Unknown;
    }

    err = nvs_commit(store_handle);
    nvs_close(store_handle);
    /* Checking error code after nvs_close(), because the close has to be called in any case */
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) committing to NVS!", err);
        return kHAPError_Unknown;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreRemove(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key) {
    HAPPrecondition(keyValueStore);

    nvs_handle store_handle;
    esp_err_t err;
    err = HAPPlatformKeyValueStoreGetHandle(keyValueStore, domain, &store_handle);
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) opening NVS!", err);
        return kHAPError_Unknown;
    }

    char keyname[15]; /* 15 is max key length for NVS */
    snprintf(keyname, sizeof(keyname), "%02X", key);
    err = nvs_erase_key(store_handle, keyname);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        HAPLogError(&logObject, "Error (%d) erasing NVS key!", err);
        nvs_close(store_handle);
        return kHAPError_Unknown;
    }

    err = nvs_commit(store_handle);
    nvs_close(store_handle);
    /* Checking error code after nvs_close(), because the close has to be called in any case */
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) committing to NVS!", err);
        return kHAPError_Unknown;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreEnumerate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreEnumerateCallback callback,
        void* _Nullable context) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(callback);

    bool shouldContinue = true;
    char name_space[15];
    snprintf(name_space, sizeof(name_space), "%s.%02X", keyValueStore->namespace_prefix, domain);

    nvs_iterator_t it = nvs_entry_find(keyValueStore->part_name, name_space, NVS_TYPE_BLOB);
    while (it != NULL && shouldContinue) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        it = nvs_entry_next(it);
        HAPError hap_err = callback(context, keyValueStore, domain, (HAPPlatformKeyValueStoreKey)atoi(info.key), &shouldContinue);
            if (hap_err != kHAPError_None) {
                return kHAPError_Unknown;
            }
    };
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStorePurgeDomain(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain) {
    HAPPrecondition(keyValueStore);

    (void) domain;

    nvs_handle store_handle;
    esp_err_t err;
    err = HAPPlatformKeyValueStoreGetHandle(keyValueStore, domain, &store_handle);
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) opening NVS!", err);
        return kHAPError_Unknown;
    }

    err = nvs_erase_all(store_handle);
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) erasing NVS namespace!", err);
        nvs_close(store_handle);
        return kHAPError_Unknown;
    }

    err = nvs_commit(store_handle);
    nvs_close(store_handle);
    /* Checking error code after nvs_close(), because the close has to be called in any case */
    if (err != ESP_OK) {
        HAPLogError(&logObject, "Error (%d) committing to NVS!", err);
        return kHAPError_Unknown;
    }
    return kHAPError_None;
}
