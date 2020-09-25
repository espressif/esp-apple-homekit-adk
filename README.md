# ESP Apple HomeKit ADK

[HomeKit](https://developer.apple.com/homekit/) is a framework developed by Apple for communicating with and controlling connected accessories in a user’s home using iOS devices. This project is a port of Apple's Open Source [HomeKit ADK](https://github.com/apple/HomeKitADK) for ESP32 and ESP32S2. This can be used by any developer to prototype non-commercial smart home accessories.

You can also check out the **ESP HomeKit SDK** [here](https://github.com/espressif/esp-homekit-sdk) which is Espressif's implementation of the HomeKit Specifications.

> Note: If you want to use HomeKit for commercial purposes, please check [here](https://www.espressif.com/en/products/sdks/esp-homekit-sdk).

## Get Started

### Set up Host environment

Set up the host environment and ESP IDF (**master** branch) as per the steps given [here](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html). The commit that was used for testing is: [012b111](https://github.com/espressif/esp-idf/commit/012b111d6d46812a768838c87417f5e3754bc90e)

### Set up MbedTLS

ESP-IDF currently uses MbedTLS 2.16.x, whereas HomeKit ADK requires 2.18. A branch mbedtls-2.16.6-adk is being maintained [here](https://github.com/espressif/mbedtls/tree/mbedtls-2.16.6-adk) which has the required patches from 2.18, on top of 2.16.6. To switch to this, follow these steps:

```text
$ cd $IDF_PATH/components/mbedtls/mbedtls
$ git pull
$ git checkout -b mbedtls-2.16.6-adk origin/mbedtls-2.16.6-adk
```

### Get esp-apple-homekit-adk

Please clone this repository using the below command:

```text
git clone --recursive https://github.com/espressif/esp-apple-homekit-adk.git
```

> Note the --recursive option. This is required to pull in the HomeKit ADK repository into esp-apple-homekit-adk. In case you have already cloned the repository without this option, execute this to the pull in the submodule:
> `git submodule update --init --recursive`


### Compile and Flash

You can use esp-apple-homekit-adk with any ESP32 or ESP32-S2 board. We have provided a Lightbulb example for reference. Compile and flash as below:

```text
$ cd /path/to/esp-apple-homekit-adk/examples/Lightbulb
$ export ESPPORT=/dev/tty.SLAB_USBtoUART #Set your board's serial port here
$ idf.py set-target <esp32/esp32s2>
$ idf.py menuconfig # Set Example Configuration -> WiFi SSID/Password
$ idf.py flash
$ esptool.py -p $ESPPORT write_flash 0x340000 accessory_setup.bin
$ idf.py monitor
```

As the device boots up and connects to your Home Wi-Fi network (configured using the menuconfig), you will see some logs like these:

```text
I (2788) wifi station: connected to ap SSID:myssid password:mypassword
1970-01-01'T'00:00:02'Z'	Default	[com.apple.mfi.HomeKit.Core:AccessoryServer] Version information:
libhap: Unknown
  - Version: Internal (May 12 2020 02:42:49) - compatibility version 7
Using platform: ESP32
  - Version: v4.2-dev-1485-g1ad1791df7-dirty (May 12 2020 02:42:38) - compatibility version 7
  - Available features:
    - Key-Value store
    - Accessory setup manager
    - Service discovery
1970-01-01'T'00:00:02'Z'	Default	[com.apple.mfi.HomeKit.Platform:Clock] Using 'gettimeofday'.
```

### Add acccessory in the Home app

Open the Home app on your iPhone/iPad and follow these steps

- Tap on "Add Accessory".
- Choose the "I Don't Have a Code or Cannot Scan" option.
- Tap on "Acme Light Bulb" in the list of Nearby Accessories.
- Select the "Add Anyway" option for the "Uncertified Accessory" prompt.
- Enter 11122333 as the Setup code.
- You should eventually see the "Acme Light Bulb added" message.
- Give a custom name, assign to a room, create scenes as required and you are done.


### Change the Setup Code

The setup code of 11122333 was encoded in the accessory_setup.bin image flashed at 0x340000. This was generated using Apple's tools and ESP IDF's nvs partition generator. Steps as follows:

```text
$ cd /path/to/esp-apple-homekit-adk/homekit_adk
$ make tools
$ cd Tools/
$ ./provision_raspi.sh --category 5 --setup-code 111-22-333 --setup-id ES32 Lightbulb
```

This will create the setup info and setup id files as below

```text
~# tree Lightbulb
Lightbulb
├── 40.10
└── 40.11
```
> Note: Compiling the ADK Tools and using provision_raspi.sh is out of scope of this project. If you face any issues with that, please report directly to Apple on [GitHub](https://github.com/apple/HomeKitADK/issues).


Now, generate an nvs image as below:

```text
$ cd Lightbulb
$ cp ../../../tools/accessory_setup/accessory_setup.csv .
$ python $IDF_PATH/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate accessory_setup.csv accessory_setup.bin 0x6000
```

Flash this using the same command given above.

```text
$ esptool.py -p $ESPPORT write_flash 0x340000 accessory_setup.bin
```

If you want to change the category, setup code or the setup id, change the values while running provision_raspi.sh. The script also generates a QR code that you can directly use for adding the accessory using the Home app.


### Resetting HomeKit Pairing

The accessory pairing information is stored in the NVS (Non Volatile Storage) partition. Once paired, the accessory cannot be paired again, without clearing this pairing information first. It can be done as below:

```text
$ esptool.py -p $ESPPORT erase_region 0x10000 0x6000
```

## Resources
  * Working with HomeKit : [https://developer.apple.com/homekit/](https://developer.apple.com/homekit/)
  * How to use the Home app : [https://support.apple.com/en-us/HT204893](https://support.apple.com/en-us/HT204893)
  * ESP SoC Resources      : [https://www.espressif.com/en/products/socs](https://www.espressif.com/en/products/socs)
  * ESP HomeKit SDK (for commercial applications) : [https://www.espressif.com/en/products/sdks/esp-homekit-sdk](https://www.espressif.com/en/products/sdks/esp-homekit-sdk)
