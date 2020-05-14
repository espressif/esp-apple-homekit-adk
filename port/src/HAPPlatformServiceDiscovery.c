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

#include <unistd.h>

#include <string.h>
#include <mdns.h>
#include "HAPPlatform+Init.h"
#include "HAPPlatformServiceDiscovery+Init.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "ServiceDiscovery" };

static HAPPlatformServiceDiscoveryRef hapService;

void HAPPlatformServiceDiscoveryRegister(
        HAPPlatformServiceDiscoveryRef serviceDiscovery,
        const char* name,
        const char* protocol,
        HAPNetworkPort port,
        HAPPlatformServiceDiscoveryTXTRecord* txtRecords,
        size_t numTXTRecords) {
    HAPPrecondition(serviceDiscovery);
    HAPPrecondition(name);
    HAPPrecondition(protocol);
    HAPPrecondition(txtRecords);

    char tmp_proto[64] = {0};
    strncpy(tmp_proto, protocol, sizeof(tmp_proto));
    char *serv_type = strtok(tmp_proto, ".");
    char *proto = strtok(NULL, ".");
    strcpy(serviceDiscovery->serv_type, serv_type);
    strcpy(serviceDiscovery->proto, proto);
    
    HAPLogDebug(&logObject, "name: \"%s\"", name);
    HAPLogDebug(&logObject, "protocol: \"%s\"", protocol);
    HAPLogDebug(&logObject, "port: %u", port);
    
    mdns_txt_item_t *txtRecords_copy = malloc(sizeof(mdns_txt_item_t) * numTXTRecords);
    if (txtRecords_copy) {
        for (size_t i = 0; i < numTXTRecords; i++) {
            HAPPrecondition(!txtRecords[i].value.numBytes || txtRecords[i].value.bytes);
            HAPPrecondition(txtRecords[i].value.numBytes <= UINT8_MAX);
            if (txtRecords[i].value.bytes) {
                HAPLogBufferDebug(&logObject, txtRecords[i].value.bytes, txtRecords[i].value.numBytes,
                        "txtRecord[%lu]: \"%s\"", (unsigned long) i, txtRecords[i].key);
            } else {
                HAPLogDebug(&logObject, "txtRecord[%lu]: \"%s\"", (unsigned long) i, txtRecords[i].key);
            }
            txtRecords_copy[i].key = (char *)txtRecords[i].key;
            txtRecords_copy[i].value = (char *)txtRecords[i].value.bytes;
        }
        esp_err_t err = mdns_service_add(name, serv_type, proto, port, txtRecords_copy, numTXTRecords);
         HAPLogDebug(&logObject, "Error: \"%d\"", err);
        free(txtRecords_copy);
    }
    hapService = serviceDiscovery;
}

void HAPPlatformServiceDiscoveryUpdateTXTRecords(
        HAPPlatformServiceDiscoveryRef serviceDiscovery,
        HAPPlatformServiceDiscoveryTXTRecord* txtRecords,
        size_t numTXTRecords) {
    HAPPrecondition(serviceDiscovery);
    HAPPrecondition(txtRecords);

    mdns_txt_item_t *txtRecords_copy = malloc(sizeof(mdns_txt_item_t) * numTXTRecords);
    if (txtRecords_copy) {
        for (size_t i = 0; i < numTXTRecords; i++) {
            HAPPrecondition(!txtRecords[i].value.numBytes || txtRecords[i].value.bytes);
            HAPPrecondition(txtRecords[i].value.numBytes <= UINT8_MAX);
            if (txtRecords[i].value.bytes) {
                HAPLogBufferDebug(&logObject, txtRecords[i].value.bytes, txtRecords[i].value.numBytes,
                        "txtRecord[%lu]: \"%s\"", (unsigned long) i, txtRecords[i].key);
            } else {
                HAPLogDebug(&logObject, "txtRecord[%lu]: \"%s\"", (unsigned long) i, txtRecords[i].key);
            }
            txtRecords_copy[i].key = (char *) txtRecords[i].key;
            txtRecords_copy[i].value = (char *) txtRecords[i].value.bytes;
        }
        mdns_service_txt_set(serviceDiscovery->serv_type, serviceDiscovery->proto, txtRecords_copy, numTXTRecords);
        free(txtRecords_copy);
    }
}

void HAPPlatformServiceDiscoveryStop(HAPPlatformServiceDiscoveryRef serviceDiscovery) {
    HAPPrecondition(serviceDiscovery);
    mdns_service_remove(serviceDiscovery->serv_type, serviceDiscovery->proto);
}

void HAPPlatformServiceDiscoveryCreate(
    HAPPlatformServiceDiscoveryRef serviceDiscovery,
    const HAPPlatformServiceDiscoveryOptions *options)
{
    HAPPrecondition(serviceDiscovery);

    mdns_init();

    if (options->hostName) {
        mdns_hostname_set(options->hostName);
    } else {
        mdns_hostname_set("MyHost");
    }
}
