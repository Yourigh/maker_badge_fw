#include "OTA.h"
#include "credentials.h"
#include "FastLED.h"
#include "MakerBadgePins.h"
#include "GxEPD2_BW.h"
#include <HTTPClient.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

CRGB leds[4];
// Instantiate the GxEPD2_BW class for our display type
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(IO_disp_CS, IO_disp_DC, IO_disp_RST, IO_disp_BUSY));  // GDEM0213B74 128x250, SSD1680
float analogReadBatt();
void enter_sleep(uint16_t TimedWakeUpSec);
uint8_t readTouchPins(void);
uint8_t MakerBadgeSetupWiFi(void);
void MakerBadgeSetupOTA(void);
void DisplayBadge(void);
void CallbackTouch3(void){}
void FWloadMode(void);
String httpGETRequest(const char* serverName);
struct DispData{
  bool valid = false;
  String RawState = "Empty";
  float TamperatureOutside = 0;
  uint16_t co2 = 0;
};
struct DispData httpParseReply(String payload);
struct DispData ActualDispData;

bool ScreenUpdate = true;
uint8_t TouchPins = 0x00;
uint8_t TouchPinsLast = 0x00;
uint16_t BattBar = 0;
String HA_state;

void setup() {
  //ADC
  //analogSetPinAttenuation(AIN_batt,ADC_11db);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  //Display
  display.init(0); //enter 115200 to see debug in console
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
  //LEDs
  pinMode(IO_led_enable_n,OUTPUT);
  digitalWrite(IO_led_enable_n,LOW);
  FastLED.addLeds<WS2812B, IO_led, GRB>(leds, 4);
  //touch wakeup
  touchAttachInterrupt(IO_touch3,CallbackTouch3,TOUCH_TRESHOLD); //Middle touch input is wake up interrupt.
  esp_sleep_enable_touchpad_wakeup();

  if (readTouchPins()==0b10001) //if 1 & 5 is touched on boot - go to badge and sleep
    DisplayBadge(); //sleep afterwards

  if (readTouchPins()==0b01110) //go to infinite loop if 2 & 4 touched on boot - intended for FW load
    FWloadMode(); //forever blocking


  //continue on home mode
  leds[0] = CRGB(0,0,1);
  FastLED.show();

  if(MakerBadgeSetupWiFi()){
    delay(500); //fail, blink red
    enter_sleep(20);
  }

  leds[0] = CRGB(10,0,0);
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Booting");
  
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();

  Serial.println("All done");
  leds[0] = CRGB(3,3,0);// CRGB::Green;
  FastLED.show();
}

void loop() {
  // Your code here
  TouchPins = readTouchPins();
  if (TouchPins != TouchPinsLast){
    TouchPinsLast = TouchPins;
    ScreenUpdate = true;
  }

  BattBar = ((analogReadBatt()*10-32)*25);

  Serial.printf("Batt: %.3f, Bar:%d\n",analogReadBatt(),BattBar);

  //Serial.println(httpGETRequest("http://192.168.1.14:8123/api/")); //test - should get API RUNNING
  ActualDispData = httpParseReply(httpGETRequest(HAreqURL));
  

  if(ScreenUpdate){
    do {
      display.fillScreen(GxEPD_WHITE);
      display.fillRect(0,DISP_Y-8,BattBar,2,GxEPD_BLACK);
      display.setCursor(10, 20);
      display.print(ActualDispData.RawState);
      display.setCursor(50, 70);
      display.printf("touch 0x%x",readTouchPins());
      display.setCursor(60, 90);
      display.printf("Batt %.2f V",analogReadBatt());
    } while (display.nextPage());
    ScreenUpdate = false;
    Serial.printf("Screen Updated\n");
    //display.setPartialWindow(DISP_X/2, 70-9, 7*5, 28); 
    //on second+ refresh, bounds are where text is, only text will be updated
  }
  delay(500);
}

float analogReadBatt(){
  return (2.0*(2.50*analogRead(AIN_batt)/4096)); //volts float
}

void enter_sleep(uint16_t TimedWakeUpSec){
  if (TimedWakeUpSec != 0){
    esp_sleep_enable_timer_wakeup(TimedWakeUpSec*1000000);
  }
  digitalWrite(IO_led_enable_n,HIGH);
  display.powerOff();
  esp_deep_sleep_start();
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

uint8_t MakerBadgeSetupWiFi(void){
  if(setupWiFi("MakerBadge", mySSID, myPASSWORD)){
    Serial.println("Connection failed");
    leds[0] = CRGB(255,0,0);
    FastLED.show();
    return 1;
  } else {
    return 0;
  }
}

void MakerBadgeSetupOTA(void){
  if(0==MakerBadgeSetupWiFi()){
    setupOTA();
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

  BattBar = ((analogReadBatt()*10-32)*25);

  do {
    display.fillScreen(GxEPD_WHITE);
    /*
    //RULLER
    for(int rul=1;rul<25;rul++)
      display.drawLine(rul*10,0,rul*10,2,GxEPD_BLACK);
    for(int rul=1;rul<12;rul++)
      display.drawLine(250,rul*10,250-2,rul*10,GxEPD_BLACK);
    //RULLER END
    */
    display.setCursor(0, 30);
    display.print("Juraj Repcik");
    //display.drawLine(226,7,226-6,7+6,GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(70, 70);
    display.print("_maker");
    display.setCursor(45, 100);
    display.print("keep making...");
    display.fillRect(0,DISP_Y-8,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  digitalWrite(IO_led_enable_n,HIGH);
  display.powerOff();
  enter_sleep(0); //TODO change to big value or zero
}

void FWloadMode(void){
  leds[0] = CRGB(0,0,50); //Blue, connecting
  FastLED.show();
  MakerBadgeSetupOTA();
  while(1){
    delay(600);
    leds[0] = CRGB(20,20,0);
    FastLED.show();
    delay(600);
    FastLED.clear(true);
  }
}

String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;
    
  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);
  
  // If you need Node-RED/server authentication, insert user and password below
  //http.setAuthorization("REPLACE_WITH_SERVER_USERNAME", "REPLACE_WITH_SERVER_PASSWORD");
  http.addHeader("Authorization",HAtoken);

  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

struct DispData httpParseReply(String payload){
  //TODO find and parse: "state":"lalalalalalalla1234",
  Serial.print("HTML GET REPLY:");
  Serial.println(payload);
  int StateIndex = payload.indexOf("\"state\":\"");
  int StateIndexEnd = payload.indexOf("\"",StateIndex);
  DispData ActualDispData;
  if ((StateIndex == -1) | (StateIndexEnd == -1)){
    ActualDispData.valid = false;
    return ActualDispData;
  }
  String StateStr = payload.substring(StateIndex,StateIndexEnd);

  Serial.print("ISOLATED STATE:");
  Serial.println(StateStr);

  ActualDispData.valid = true;
  ActualDispData.RawState = StateStr;
  return ActualDispData;
}
