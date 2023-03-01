# Maker Badge FW
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

# Hardware
Hardware for a makerbadge made by [@dronecz](https://github.com/dronecz/maker_badge).

## Where to by HW? 
[Makermarket](http://makermarket.cz/)

## Compatibility
Project is compatible, or can be easily adjusted for any eInk that is supported by GxEPD2 library and has ESP32-S2 (or other ESP32 IC). 
