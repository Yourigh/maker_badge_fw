# Maker Badge Arduino FW
This firmware gives Maker Badge these functions:
* Badge
* OTA FW update
* Home mode - periodically updated dashboard with data from home assitant server. 
  * uses [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/) http get.
  * deep sleep is used in the time between updates

![20230115_225630](https://user-images.githubusercontent.com/25552139/212756508-df7927dd-351f-4965-90e9-c199fa787e72.jpg)

## Develpoment tools
* Framework: Arduino
* IDE: Platformio

## How to convert Makerbadge from CPY to Arduino?

This guide is made for programming makerbadge in Platformio IDE in VScode.

0. Start VScode with platformio and clone this repo.
1. Set or verify the USB upload is enabled in platformio.ini:
    ```
    [platformio]
    default_envs = esp32-s2-USB
    ```
2. Rename `config_template.h` to `config.h` (enter your name if you want)
3. Hit **Upload**
4. Arduino code should run. 

Pro tip: For better serial debugging add 3s delay on the start of the `setup()` by uncommenting `delay(3000);`, so COM port has time to connect before you log anything. Don't forget to comment it in final code. 

# Hardware
Hardware for a makerbadge made by [@dronecz](https://github.com/dronecz/maker_badge).

## Where to by HW? 
[Makermarket](http://makermarket.cz/)

## Compatibility
Project is compatible, or can be easily adjusted for any eInk that is supported by GxEPD2 library and has ESP32-S2 (or other ESP32 IC). 

