#define IO_led_enable_n 21
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
#define DISP_Y 128
#define TOUCH_TRESHOLD 20000 //ca. 15000 when empty, 30k for touch

//settings
#define HA_UPDATE_PERIOD_SEC 600