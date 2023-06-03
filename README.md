# Maker Badge Arduino FW
This firmware gives Maker Badge these functions:
* Badge
* OTA FW update
* Home mode - periodically updated dashboard with data from home assitant server. 
  * uses [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/) http get.
  * deep sleep is used in the time between updates
* MakerCall
  * Communication broadcast tool that can send message to nearby MakerBadges that have MakerCall turned on
  * See how it works: [Youtube video](TBD)

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
    #default_envs = esp32-s2-OTA
    ```
2. Connect USB-C to PC and put Makerbadge to download mode (hold BOOT and click RESET)
3. Rename `config_template.h` to `config.h` (enter your name if you want)
4. Verify in `config.h` that you have the correct Maker Badge version selected.
5. Hit **Upload** (for the first upload, MB need to be in download mode)
6. Arduino code should run. 

Pro tip: 
* For better serial debugging add 3s delay on the start of the `setup()` by uncommenting `delay(3000);`, so COM port has time to connect before you log anything. Don't forget to comment it in final code. 

### Do you want to use Arduino IDE? No problem.

1. Make a new folder (`makerbadge`) in your pc.
2. Copy src/main.cpp and rename to `makebadge.ino` and place to foler
3. Copy `config_template.h`, place to folder and rename to `config.h` and verify in `config.h` that you have the correct Maker Badge version selected.
4. Copy `OTA.h` to the folder
5. Install missing libraries (FastLED by Daniel Garcia and GxEPD2)
6. Select Adafruit Feather ESP32-S2 board
7. Put Badge into download mode
8. Upload.

### Upload debug

If experiencing issues, you can try to do a full-erase of ESP32 memory.
* Open command line in VScode and run `pio run --target erase`. Result should say `Chip erase completed successfully in...`. Error on the end is ok.

# Hardware
Hardware for a makerbadge made by [@dronecz](https://github.com/dronecz/maker_badge).

## Known bugs
* up to revision C (C included), badge sometimes "forgets" where in menu it was when waking up from deep sleep. This is caused by voltage drop on ESP32-S2 power pin during high load in wake-up. Hot fix is possible in HW (how-to TBD).

## Where to by HW? 
[Makermarket](http://makermarket.cz/)

## Compatibility
Project is compatible, or can be easily adjusted for any eInk that is supported by GxEPD2 library and has ESP32-S2 (or other ESP32 IC). 