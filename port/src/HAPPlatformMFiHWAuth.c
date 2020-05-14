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

#include "HAP+Internal.h"
#include "HAPPlatformMFiHWAuth+Init.h"
#include <esp_err.h>
#include "driver/i2c.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "MFiHWAuth" };

#define AUTH_WR_ADDR_LOW_RST    0x20
#define AUTH_WR_ADDR_HIGH_RST   0x22

#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          CONFIG_HAP_IC2_SPEED
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TX_BUF_DISABLE   0
#define ACK_CHECK_EN                1
#define ACK_VAL                     0
#define NACK_VAL                    1

/* To use GPIOs 17/18 for the Hardware I2C, replace the numbers 26/27 below
 */
#define I2C_MASTER_SDA_GPIO         CONFIG_HAP_I2C_SDA_GPIO
#define I2C_MASTER_SCL_GPIO         CONFIG_HAP_I2C_SCL_GPIO

#define I2C_MASTER_MAX_READ         CONFIG_HAP_I2C_MAX_READ_COUNT
#define I2C_MASTER_RETRY_TIMES      500
#define I2C_MASTER_TICKS_TIMES      2 * I2C_MASTER_RETRY_TIMES
#define I2C_MASTER_MAX_RETRY        10
#define I2C_MASTER_INTERNAL_TIMES   8 * I2C_MASTER_RETRY_TIMES
/**
 * @brief Initialize I2C information
 */
static int esp_mfi_i2c_init(void)
{
    int ret = 0;
    i2c_config_t conf;
    int i2c_master_port = I2C_MASTER_NUM;

    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_GPIO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_GPIO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;

    ret = i2c_param_config(i2c_master_port, &conf);
    if (ret != ESP_OK) {
        HAPLogError(&logObject, "I2C parameter initial fail %d", ret);
        return ESP_FAIL;
    }

    ret = i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    if (ret != ESP_OK) {
        HAPLogError(&logObject, "I2C driver initial fail %d", ret);
        return ESP_FAIL;
    }

    return 0;
}
/**
 * @brief write data buffer to slave
 */
static int esp_mfi_i2c_write(uint8_t slvaddr, const uint8_t *buff, uint32_t len)
{
    uint16_t i;

    if (!buff)
        return ESP_FAIL;

    HAPLog(&logObject, "Writing to HW I2C");

    int ret = 0;
    i2c_cmd_handle_t cmd = NULL;
    i = 0;

    do {
        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);

        // Send write address of the CP
        i2c_master_write_byte(cmd, slvaddr, ACK_CHECK_EN);
        // Send data out.
        i2c_master_write(cmd, (uint8_t *)buff, len, ACK_CHECK_EN);

        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TICKS_TIMES / portTICK_RATE_MS);
        i ++;
        i2c_cmd_link_delete(cmd);
        ets_delay_us(I2C_MASTER_RETRY_TIMES);
    } while(ret != ESP_OK && i < I2C_MASTER_MAX_RETRY);

    if (ret != ESP_OK) {
        HAPLogError(&logObject, "Write data to slave fail %d.", ret);
    }
    return ret;
}

/**
 * @brief read data form slave
 */
static int esp_mfi_i2c_read(uint8_t slvaddr, uint8_t regaddr, uint8_t *buff, uint32_t len)
{
    uint16_t i, j = 0;

    if (!buff)
        return ESP_FAIL;

    HAPLog(&logObject, "Reading from HW I2C");

    int ret = 0;
    i2c_cmd_handle_t cmd = NULL;
    i = 0;

    do {
        for (j = 0; j < I2C_MASTER_MAX_READ; j++) {
            cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);

            // Send write address of the CP
            i2c_master_write_byte(cmd, slvaddr, ACK_CHECK_EN);
            // Send register address to slave.
            i2c_master_write_byte(cmd, regaddr, ACK_CHECK_EN);

            i2c_master_stop(cmd);
            ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TICKS_TIMES / portTICK_RATE_MS);
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                break;
            } else {
                ets_delay_us(I2C_MASTER_INTERNAL_TIMES);
            }
        }

        ets_delay_us(I2C_MASTER_INTERNAL_TIMES);

        cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);

        i2c_master_write_byte(cmd, slvaddr + 1, ACK_CHECK_EN);
        if (len == 1)
            i2c_master_read_byte(cmd, buff, NACK_VAL);
        else {
            i2c_master_read(cmd, buff, len - 1, ACK_VAL);
            i2c_master_read_byte(cmd, buff + len - 1, NACK_VAL);
        }

        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, I2C_MASTER_TICKS_TIMES / portTICK_RATE_MS);
        i ++;
        i2c_cmd_link_delete(cmd);
    } while (ret != ESP_OK && i < I2C_MASTER_MAX_RETRY);

    if (ret != ESP_OK) {
        HAPLogError(&logObject, "Read data from slave fail %d.", ret);
    }

    return ret;
}
void HAPPlatformMFiHWAuthCreate(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    int ret = 0;

    ret = esp_mfi_i2c_init();
    if (ret != ESP_OK)
        HAPFatalError();

    mfiHWAuth->slaveAddr = AUTH_WR_ADDR_LOW_RST;
}

void HAPPlatformMFiHWAuthRelease(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    mfiHWAuth->slaveAddr = 0;
}

HAP_RESULT_USE_CHECK
bool HAPPlatformMFiHWAuthIsPoweredOn(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    return mfiHWAuth->poweredOn;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiHWAuthPowerOn(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    mfiHWAuth->poweredOn = true;
    return kHAPError_None;
}

void HAPPlatformMFiHWAuthPowerOff(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    mfiHWAuth->poweredOn = false;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiHWAuthWrite(HAPPlatformMFiHWAuthRef mfiHWAuth, const void* bytes, size_t numBytes) {
    HAPPrecondition(mfiHWAuth);
    HAPPrecondition(bytes);
    HAPPrecondition(numBytes >= 1 && numBytes <= 128);

    if (!HAPPlatformMFiHWAuthIsPoweredOn(mfiHWAuth)) {
        return kHAPError_InvalidState;
    }

    if (esp_mfi_i2c_write(mfiHWAuth->slaveAddr, bytes, numBytes) != ESP_OK) {
        return kHAPError_Unknown;
    }
    
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiHWAuthRead(
        HAPPlatformMFiHWAuthRef mfiHWAuth,
        uint8_t registerAddress,
        void* bytes,
        size_t numBytes) {
    HAPPrecondition(mfiHWAuth);
    HAPPrecondition(bytes);
    HAPPrecondition(numBytes >= 1 && numBytes <= 128);

    if (!HAPPlatformMFiHWAuthIsPoweredOn(mfiHWAuth)) {
        return kHAPError_InvalidState;
    }

    if (esp_mfi_i2c_read(mfiHWAuth->slaveAddr, registerAddress, bytes, numBytes) != ESP_OK) {
        return kHAPError_Unknown;
    }

    return kHAPError_None;
}
