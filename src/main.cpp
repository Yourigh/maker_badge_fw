#include "OTA.h"
#include "credentials.h"
#include "FastLED.h"
#include "MakerBadgePins.h"
#include "GxEPD2_BW.h"
#include <HTTPClient.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
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
struct DispData httpParseReply(String payload);
void DisplayMenu(void);
void DisplayHAPrepareLayout(void);
void DisplayHomeAssistant(void);

struct DispData{
  bool valid = false;
  String RawState = "Empty";
  //float TamperatureOutside = 0;
  //uint16_t co2 = 0;
};
enum mbStates{Menu, HomeAssistant, Badge, FWupdate};

struct DispData ActualDispData;
bool ScreenUpdate = true;
uint8_t TouchPins = 0x00;
uint8_t TouchPinsLast = 0x00;
uint16_t BattBar = 0;
RTC_DATA_ATTR mbStates CurrentMode = Menu; //to store in ULP, kept during deep sleep

void setup() {
  //Serial
  Serial.begin(115200);
  //ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  //Display
  display.init(0); //enter 115200 to see debug in console
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);
  //LEDs
  pinMode(IO_led_enable_n,OUTPUT);
  digitalWrite(IO_led_enable_n,HIGH);
  FastLED.addLeds<WS2812B, IO_led, GRB>(leds, 4);
  //touch wakeup
  touchAttachInterrupt(IO_touch3,CallbackTouch3,TOUCH_TRESHOLD); //Middle touch input is wake up interrupt.
  esp_sleep_enable_touchpad_wakeup();

  //Serial.printf("CurrentMode is:%d",CurrentMode);
  switch (esp_sleep_get_wakeup_cause()){
    case ESP_SLEEP_WAKEUP_TIMER:
      //restore last used mode.
      switch (CurrentMode){
        case HomeAssistant:
          DisplayHomeAssistant(); //sleeps and periodically updates
          break;
        case Badge:
          DisplayBadge(); //sleep forever
          break;
        case FWupdate:
          FWloadMode(); //forever blocking
          break;
      }
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      //falldown
    default:
      //normal power-up after reset
      DisplayMenu(); //menu to select a mode. Blocking.
      enter_sleep(1); //sleeps for 1s and gets back to switch timer wakeup cause.
  }
} //end setup

void loop() {
  //empty
}

float analogReadBatt(){
  return (2.0*(2.50*analogRead(AIN_batt)/4096)); //volts float
}

void enter_sleep(uint16_t TimedWakeUpSec){
  if (TimedWakeUpSec != 0){
    if (ESP_OK != esp_sleep_enable_timer_wakeup(TimedWakeUpSec*1000000)){
      //out of range
      digitalWrite(IO_led_enable_n,LOW);
      leds[0]=CRGB(255,0,0);
      FastLED.show();
      delay(500);
    }
  }
  digitalWrite(IO_led_enable_n,HIGH);
  //display.powerOff();
  esp_deep_sleep_start();
  //MakerBadge B
  //A: 569uA - no wakeup enabled, deep sleep, with display
  //B: 119uA - no wakeup enabled, deep sleep, without display (disconnected)
  //C: 579uA - as A+touch wakup
  //D: 577uA - as A but enabled touch and timed wakup
  //measured on pin header - 3V3 
  //additional 200uA (batt sense) + 60uA (LDO) estimated
}

/**
 * @brief Reads touch pins
 * 
 * @return uint8_t 0bxx54321 where 54321 are touch inputs.
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
  //Serial.printf("out:%x\n",TouchResultMask);
  return TouchResultMask;
}

uint8_t MakerBadgeSetupWiFi(void){
  if(setupWiFi("MakerBadge", mySSID, myPASSWORD)){
    digitalWrite(IO_led_enable_n,LOW);
    Serial.println("Connection failed");
    leds[0] = CRGB(100,0,0);
    FastLED.show();
    delay(500);
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
  digitalWrite(IO_led_enable_n,LOW);
  leds[0] = CRGB(00,10,0);
  FastLED.show();
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);

  BattBar = ((analogReadBatt()*10-32)*25);

  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.print("Juraj Repcik");
    //display.drawLine(226,7,226-6,7+6,GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(70, 70);
    display.print(" _maker");
    display.setCursor(45, 100);
    display.print(" keep making...");
    display.fillRect(0,DISP_Y-8,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  digitalWrite(IO_led_enable_n,HIGH);
  display.powerOff();
  enter_sleep(0);
}

void FWloadMode(void){
  digitalWrite(IO_led_enable_n,LOW);
  leds[0] = CRGB(0,0,50); //Blue, connecting
  FastLED.show();
  MakerBadgeSetupOTA();
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);
  BattBar = ((analogReadBatt()*10-32)*25);
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.print(" FW update");
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(50, 90);
    display.printf("Batt %.2f V",analogReadBatt());
    display.fillRect(0,DISP_Y-8,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  uint8_t ledrotate = 0;
  while(1){
    leds[ledrotate++] = CRGB(20,20,0);
    FastLED.show();
    delay(600);
    FastLED.clear(true);
    if (ledrotate == 4) ledrotate = 0;
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
    //Serial.print("HTTP Response code: ");
    //Serial.println(httpResponseCode);
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
  //Serial.print("HTML GET REPLY:");
  //Serial.println(payload);
  int StateIndex = payload.indexOf("\"state\":\"");
  if (StateIndex == -1){
    ActualDispData.valid = false;
    ActualDispData.RawState = "not valid";
    return ActualDispData;
  }
  int StateIndexEnd = payload.indexOf("\"",StateIndex+10); //+10 for skipping "state":" string
  DispData ActualDispData;
  //Serial.printf("indexes %d - %d\n",StateIndex,StateIndexEnd);
  String StateStr = payload.substring(StateIndex+9,StateIndexEnd);
  //Serial.print("ISOLATED STATE:");
  //Serial.println(StateStr);
  ActualDispData.valid = true;
  ActualDispData.RawState = StateStr;
  return ActualDispData;
}

void DisplayMenu(void){
  digitalWrite(IO_led_enable_n,LOW);
  leds[0] = CRGB(5,10,0);
  leds[1] = CRGB(5,10,0);
  FastLED.show();
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(true);
  BattBar = ((analogReadBatt()*10-32)*25);
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 12);
    display.print("   Maker Badge Menu");
    display.setCursor(0, 39);
    display.printf("   1. Home Assistant\n\n   2. Badge\n\n   3. FW update");
    display.fillRect(0,20,250,2,GxEPD_BLACK);
    display.fillRect(0,DISP_Y-8,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  uint8_t flipLED = 1;
  uint32_t lastMillis = 0;
  while(1){
    delay(150);
    switch(readTouchPins()){
      case 0b00001: //1
        CurrentMode = HomeAssistant;
        return;
      case 0b00010: //2
        CurrentMode = Badge;
        return;
      case 0b00100: //3
        CurrentMode = FWupdate;
        return;
      default:
        CurrentMode = Menu;
        break;
    }
    if ((lastMillis+600) < millis()){
      leds[  flipLED & 0x01   ] = CRGB(5,10,0);
      leds[!(flipLED++ & 0x01)] = CRGB(0,0,0);
      FastLED.show();
      lastMillis = millis();
    }
  }
}

//prepare non-changing graphics on the screen. DisplayHomeAssistant will use only partial update.
void DisplayHAPrepareLayout(void){
  //TODO
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 12);
    display.print("   Home Assistant");
    display.fillRect(0,18,250,1,GxEPD_BLACK);
  } while (display.nextPage());
}

void DisplayHomeAssistant(void){
  //LEDs disabled
  //digitalWrite(IO_led_enable_n,LOW);

  if(MakerBadgeSetupWiFi()){
    enter_sleep(HA_UPDATE_PERIOD_SEC);
  }
  //NO OTA is set up, FWupdate mode is for OTA
  display.setFont(&FreeMonoBold9pt7b);
  
  BattBar = ((analogReadBatt()*10-32)*25);
  //Serial.printf("Batt: %.3f, Bar:%d\n",analogReadBatt(),BattBar);
  //Serial.println(httpGETRequest("http://192.168.1.14:8123/api/")); //test - should get API RUNNING
  ActualDispData = httpParseReply(httpGETRequest(HAreqURL));
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 12);
    display.print("    Home Assistant");
    display.setFont(&FreeMonoBold12pt7b);
    display.fillRect(0,18,250,1,GxEPD_BLACK);
    display.fillRect(0,DISP_Y-8,BattBar,2,GxEPD_BLACK);
    display.setCursor(0, 39);
    display.print(ActualDispData.RawState);
  } while (display.nextPage());

  enter_sleep(HA_UPDATE_PERIOD_SEC);
}