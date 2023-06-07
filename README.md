# Maker Badge Arduino FW
This firmware gives Maker Badge these functions:
* Badge
* OTA FW update
* Home mode - periodically updated dashboard with data from home assitant server. 
  * uses [Home Assistant REST API](https://developers.home-assistant.io/docs/api/rest/) http get.
  * deep sleep is used in the time between updates
* MakerCall
  * Communication broadcast tool that can send message to nearby MakerBadges that have MakerCall turned on.
  * Due to active RF recieving, power consumption in this mode is 76mA, so it is better if you could use bigger capacity battery (for example from old phone, like I did)
  * See how it works: 
  * 
    [![YoutubeVideo](https://img.youtube.com/vi/rwIuPDdZZNg/0.jpg)](https://youtu.be/rwIuPDdZZNg)

![image](https://github.com/Yourigh/maker_badge_fw/assets/25552139/ec7d42d9-fdac-4ad0-848a-a3c991af247c)
<img src="https://user-images.githubusercontent.com/25552139/212756508-df7927dd-351f-4965-90e9-c199fa787e72.jpg" alt="MB_HA" title="MakerBadge Home assistant" width="340"/>

## Develpoment tools
* Framework: Arduino
* IDE: Platformio

# How to flash
To use all the features, the config file must be edited and program compiled and flashed using PlatformIO or Arduino IDE. (guide B)

If you want to use only MakerCall communicator, follow the shorter guide (guide A).

## A. Only MakerCall flash guide
Direct flash of the firmware without an option to change code.

1. Download flash tool from [releases/assets](https://github.com/Yourigh/maker_badge_fw/releases)
2. Unpack all
3. Connect MakerBadge to PC and put to download mode (hold BOOT and click RESET)
4. Run **update_badge_verC.bat** or **update_badge_verD.bat** depending on your badge version.
  *Firmware should be flashed. If the device is not found check COM port number and edit it in update script*
5. Press Reset button to run new firmware.

## B. Full firmware flash guide
This guide is made for programming makerbadge in Platformio IDE in VScode. MakerBadge come originally flashed with CircutPython. 

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

## Do you want to go back to CPY?
This firmware use Arduino and compiled code. The MakerBadge is originally shipped with CircutPython. To go back to CPY, follow this [guide](https://learn.adafruit.com/adafruit-metro-esp32-s2/circuitpython)

The original python code is saved in [this repository](https://github.com/makerfaireczech/maker_badge/tree/main).

# Hardware
Hardware for a makerbadge made by [@dronecz](https://github.com/dronecz/maker_badge).

## Known bugs
* up to revision C (C included), badge sometimes "forgets" where in menu it was when waking up from deep sleep. This is caused by voltage drop on ESP32-S2 power pin during high load in wake-up. Hot fix is possible in HW (how-to TBD).

## Where to by HW? 
[Makermarket](http://makermarket.cz/)

## Compatibility
Project is compatible, or can be easily adjusted for any eInk that is supported by GxEPD2 library and has ESP32-S2 (or other ESP32 IC). 

## Sources
* ESPNOW part inspired by
https://github.com/atomic14/ESPNowSimpleSample
