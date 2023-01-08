#include "OTA.h"
#include "credentials.h"
#include "FastLED.h"
#include "MakerBadgePins.h"
#include "GxEPD2_BW.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

CRGB leds[4];
// Instantiate the GxEPD2_BW class for our display type
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(IO_disp_CS, IO_disp_DC, IO_disp_RST, IO_disp_BUSY));  // GDEM0213B74 128x250, SSD1680
void enter_sleep(void);
uint8_t readTouchPins(void);
void MakerBadgeSetupOTA(void);
void DisplayBadge(void);
void CallbackTouch3(void){}

bool ScreenUpdate = true;
uint8_t TouchPins = 0x00;

void setup() {
  //ADC
  analogSetPinAttenuation(AIN_batt,ADC_11db);
  //Display
  display.init(0); //enter 115200 to see debug in console
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
  //LEDs
  pinMode(IO_led_enable_n,OUTPUT);
  digitalWrite(IO_led_enable_n,LOW);
  FastLED.addLeds<WS2812B, IO_led, GRB>(leds, 4);
  //touch
  touchAttachInterrupt(IO_touch3,CallbackTouch3,TOUCH_TRESHOLD);
  //Sleep
  esp_sleep_enable_touchpad_wakeup();

  if (readTouchPins()==0b10001)
    DisplayBadge();

  leds[0] = CRGB(10,0,0);
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Booting");

  leds[0] = CRGB(0,0,50); //Blue, connecting
  FastLED.show();
  MakerBadgeSetupOTA();

  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    //display.drawBitmap(0, 0, bitmap, display.epd2.WIDTH, display.epd2.HEIGHT, GxEPD_BLACK);
    display.drawCircle(0,0,50,GxEPD_BLACK);
    display.setCursor(55, 10);
    display.print("Juraj Repcik");

  } while (display.nextPage());
  display.setPartialWindow(0, 20, DISP_X, DISP_Y-10);

  Serial.println("All done");
  leds[0] = CRGB(1,1,0);// CRGB::Green;
  FastLED.show();
}

void loop() {
  // Your code here
  TouchPins = readTouchPins();
  if (TouchPins){
    ScreenUpdate = true;
  }

  //Serial.printf("ADC mV: %d\n",analogReadMilliVolts(AIN_batt));
  analogReadResolution(12);
  Serial.printf("ADC raw: %d, mV: %d, mycalc V %f:\n",analogRead(AIN_batt),analogReadMilliVolts(AIN_batt),analogRead(AIN_batt)/823.8);  

  if(ScreenUpdate){
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setCursor(50, 70);
      display.printf("touch 0x%x",readTouchPins());
      display.setCursor(60, 90);
      display.printf("Batt %.2f V",analogRead(AIN_batt)/823.8);
    } while (display.nextPage());
    ScreenUpdate = false;
    display.setPartialWindow(DISP_X/2, 70-9, 7*5, 25);
  }
  delay(600);
}

void enter_sleep(void){
  digitalWrite(IO_led_enable_n,HIGH);
  display.powerOff();
}

/**
 * @brief Reads touch pins
 * 
 * @return uint8_t 0bxxxabcd where abcd are touch inputs.
 */
uint8_t readTouchPins() {
  uint16_t touchread;
  uint8_t TouchResultMask = 0x00;
  for (int i = 0; i < 5; i++) {
    // (5 - i) because pins order is reversed
    touchread = touchRead(5 - i);
    if (touchread > TOUCH_TRESHOLD ) {
      //Serial.printf("Captouch #%0d reading: %d\n", i, touchread);
      TouchResultMask |= 1<<i;
    }
    //Serial.printf("CT#%0d: %d//", i, touchread);
  }
  //Serial.printf("\n");
  return TouchResultMask;
}

void MakerBadgeSetupOTA(void){
  if(setupOTA("MakerBadge", mySSID, myPASSWORD)){
    Serial.println("Connection failed");
    leds[0] = CRGB(255,0,0);
    FastLED.show();
    delay(2000);
  } else {
    leds[0] = CRGB(0,10,0);// CRGB::Green;
    FastLED.show();
  }
  
  Serial.println("OTA Initialized");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(ArduinoOTA.getHostname());
}

void DisplayBadge(void){
  leds[0] = CRGB(00,100,0);
  FastLED.show();
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);
  do {
    display.fillScreen(GxEPD_WHITE);
    //RULLER
    for(int rul=1;rul<25;rul++)
      display.drawLine(rul*10,0,rul*10,2,GxEPD_BLACK);
    for(int rul=1;rul<12;rul++)
      display.drawLine(250,rul*10,250-2,rul*10,GxEPD_BLACK);
    //RULLER END
    display.setCursor(0, 30);
    display.print("Juraj Repcik");
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(70, 70);
    display.print("_maker");
    display.setCursor(45, 100);
    display.print("keep making...");
  } while (display.nextPage());
  digitalWrite(IO_led_enable_n,HIGH);
  display.powerOff();
  //esp_deep_sleep_start();
  while(1){}
}