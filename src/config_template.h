//SELECT MAKERBADGE VERSION (found on back side near USB-C connector). Only one can be uncommented
//#define MakerBadgeVersionA
//#define MakerBadgeVersionB
#define MakerBadgeVersionC
//#define MakerBadgeVersionD

//CREDENTIALS
#define mySSID "blabla"
#define myPASSWORD "blablabla"

//HOME ASSISTANT SETUP
/*
In HA/Profile create "Long-Lived Acess Token" and paste it to HAtoken here.
In configuration.yaml add sensor with custom state (example):
template:
  - sensor:
      - name: "MakerBadge-Data"
        state: >
          {{"Out: "+states("sensor.ble_temperature_bt_outside")+" 'C\nBox: "+states("sensor.ble_temperature_bt_room")+" 'C\nCO2: "+states("sensor.workroom_co2")+" ppm\nOpenCO2: "+states("sensor.sdc40_ppm")}}
*/
#define HAreqURL "http://<IP>:8123/api/states/sensor.makerbadge_data"
#define HAtoken "Bearer <add 183 characters log code generated by HA>"
#define HA_UPDATE_PERIOD_SEC 600 //periodic update of data, also wake-up period (600s lasts for 3 weeks on 1500mAh battery)
#define SHOW_LAST_UPDATE 0 //shows last update date and time in small font on the display, for debug

//BADGE BASIC SETUP - for quick setup, adjust alignmant in code if needed
#define BadgeName "Jmeno Primeni"
#define BadgeLine2 " _maker"
#define BadgeLine3 "Firma/Projekt"

//MakerBadge Pins
#define IO_led_disable 21
#define IO_led 18 //4x WS2812B, GRB order
#define NUMPIXELS 4
#define IO_btn_boot 0
#define IO_touch1 5
#define IO_touch2 4
#define IO_touch3 3
#define IO_touch4 2
#define IO_touch5 1
#define AIN_batt 6 //analog input, batteryVoltage = 2*analogRead(AIN_batt)
#define IO_disp_CS 41
#define IO_disp_DC 40
#define IO_disp_RST 39
#define IO_disp_BUSY 42
#define DISP_X 250
#define DISP_Y 122
#define TOUCH_TRESHOLD 20000 //ca. 15000 when empty, 30k for touch

#ifdef MakerBadgeVersionD
    #define IO_EPD_power_disable 16 //put to low to power E-paper display
    #define IO_BAT_meas_disable 14 //put to low to measure battery voltage. Battery has to be measured right after pulling low.
    #define BATT_V_CAL_SCALE 1.05
#else   
    #define BATT_V_CAL_SCALE 1.00
#endif