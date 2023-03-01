/*
Juraj Repcik and edit by Matyas Potocky aka cqeta1564

Before compiling, create config.h (copy and edit config_template.h). 
*/

#include <pgmspace.h>
#include "Bitmaps.h"
#include <esp_now.h>
#include <WiFi.h>
#include "OTA.h"
#include "config.h"
#include "FastLED.h"
#include "GxEPD2_BW.h"
#include <HTTPClient.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#ifdef MakerBadgeVersion1
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
#endif

CRGB leds[4];
// Instantiate the GxEPD2_BW class for our display type
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(IO_disp_CS, IO_disp_DC, IO_disp_RST, IO_disp_BUSY));  // GDEM0213B74 122x250, SSD1680
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
void DisplayHomeAssistant(void);
void ESP_Now(void);

struct DispData{
  bool valid = false;
  String RawState = "Empty";
  String LastChangedStr = "Empty";
  //float TamperatureOutside = 0;
  //uint16_t co2 = 0;
};
enum mbStates{Menu, HomeAssistant, Badge, FWupdate, ESP_Now_menu};

uint8_t TouchPins = 0x00;
uint8_t TouchPinsLast = 0x00;
uint16_t BattBar = 0;
RTC_DATA_ATTR mbStates CurrentMode = Menu; //to store in ULP, kept during deep sleep

void setup() {
  #ifdef MakerBadgeVersion1
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector - hot fix for voltage drop on reboot or wifi connection.
  #endif
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
  //esp_sleep_enable_touchpad_wakeup(); //disabled intentionally - same effect as reset button.

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
        case ESP_Now_menu:
          ESP_Now(); //idk blocking
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
    display.drawBitmap(0, 0, smile, 19, 19, GxEPD_BLACK); //ATTENTION FOR 4 AND 5 VARIABLE THERE MUST BE THE SIZE OF THE IMAGE
    display.setCursor(0, 39);
    display.printf("   1. Home Assistant\n   2. Badge\n   3. FW update\n   4. ESP-Now");
    display.fillRect(0,20,250,2,GxEPD_BLACK);
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  uint8_t flipLED = 1;
  uint32_t lastMillis = 0;
  uint16_t timeout = 1000; //*0.6s = 600s = 10min
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
      case 0b01000: //4
        CurrentMode = ESP_Now_menu;
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
      if (0 == timeout--)
        enter_sleep(0);
    }
  }
}

void DisplayHomeAssistant(void){
  //LEDs disabled
  //digitalWrite(IO_led_enable_n,LOW);
  DispData ActualDispData;

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
    display.fillRect(0,18,250,1,GxEPD_BLACK); //line below Home Assistant heading
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
    display.setCursor(0, 39);
    display.print(ActualDispData.RawState);
#if SHOW_LAST_UPDATE
    display.setFont(NULL); // default 5x7 system font?
    display.setCursor(23, DISP_Y-10);
    display.print(ActualDispData.LastChangedStr);
#endif
  } while (display.nextPage());

  enter_sleep(HA_UPDATE_PERIOD_SEC);
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
    display.print(BadgeName);
    //display.drawLine(226,7,226-6,7+6,GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(70, 70);
    display.print(" _maker");
    display.setCursor(45, 100);
    display.print(" keep making...");
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
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
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
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

//---------------------------------
//ESP-Now starts here
//---------------------------------

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
// Formats MAC Address
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}
 
 
void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
// Called when data is received
{
  // Only allow a maximum of 250 characters in the message + a null terminating byte
  char buffer[ESP_NOW_MAX_DATA_LEN + 1];
  int msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  strncpy(buffer, (const char *)data, msgLen);
 
  // Make sure we are null terminated
  buffer[msgLen] = 0;
 
  // Format the MAC address
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
 
  // Send Debug log message to the serial port
  Serial.printf("Received message from: %s - %s\n", macStr, buffer);
 
  // Check switch status
  if (strcmp("red", buffer) == 0) // If "str1" == "str2" => 0
  {
    leds[0] = CRGB(100,0,0); // LED Red TEMPORARY SOLUTION
    leds[1] = CRGB(100,0,0); // LED Red
    leds[2] = CRGB(100,0,0); // LED Red
    leds[3] = CRGB(100,0,0); // LED Red
    FastLED.show();
  }
  else if (strcmp("green", buffer) == 0) // If "str1" == "str2" => 0
  {
    leds[0] = CRGB(0,100,0); //LED Green TEMPORARY SOLUTION
    leds[1] = CRGB(0,100,0); //LED Green
    leds[2] = CRGB(0,100,0); //LED Green
    leds[3] = CRGB(0,100,0); //LED Green
    FastLED.show();
  }
  else if (strcmp("blue", buffer) == 0) // If "str1" == "str2" => 0
  {
    leds[0] = CRGB(0,0,100); //LED Blue TEMPORARY SOLUTION
    leds[1] = CRGB(0,0,100); //LED Blue
    leds[2] = CRGB(0,0,100); //LED Blue
    leds[3] = CRGB(0,0,100); //LED Blue
    FastLED.show();
  }
  else if (strcmp("off", buffer) == 0) // If "str1" == "str2" => 0
  {
    leds[0] = CRGB(0,0,0); //LED Off TEMPORARY SOLUTION
    leds[1] = CRGB(0,0,0); //LED Off
    leds[2] = CRGB(0,0,0); //LED Off
    leds[3] = CRGB(0,0,0); //LED Off
    FastLED.show();
  }
}

void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
// Called when data is sent
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}
 
void broadcast(const String &message)
// Emulates a broadcast
{
  // Broadcast a message to every device in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  // Send message
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)message.c_str(), message.length());
 
  // Print results to serial monitor
  if (result == ESP_OK)
  {
    Serial.println("Broadcast message success");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_INIT)
  {
    Serial.println("ESP-NOW not Init.");
  }
  else if (result == ESP_ERR_ESPNOW_ARG)
  {
    Serial.println("Invalid Argument");
  }
  else if (result == ESP_ERR_ESPNOW_INTERNAL)
  {
    Serial.println("Internal Error");
  }
  else if (result == ESP_ERR_ESPNOW_NO_MEM)
  {
    Serial.println("ESP_ERR_ESPNOW_NO_MEM");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_FOUND)
  {
    Serial.println("Peer not found.");
  }
  else
  {
    Serial.println("Unknown error");
  }
}

void ESP_Now(void){
  digitalWrite(IO_led_enable_n,LOW);
  leds[0] = CRGB(0,0,50); //Blue, preparing
  FastLED.show();

  // Display
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);
  BattBar = ((analogReadBatt()*10-32)*25);
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(25, 0, esp_now, 200, 50, GxEPD_BLACK); //ATTENTION FOR 4 AND 5 VARIABLE THERE MUST BE THE SIZE OF THE IMAGE
    display.drawBitmap(50, 55, espressif, 150, 26, GxEPD_BLACK); //ATTENTION FOR 4 AND 5 VARIABLE THERE MUST BE THE SIZE OF THE IMAGE
    display.setFont(&FreeMono9pt7b);
    display.setCursor(2, 110);
    display.printf("Batt %.2f V",analogReadBatt());
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());

  // Set ESP32 in STA mode to begin with
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Broadcast Demo");
 
  // Print MAC address
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
 
  // Disconnect from WiFi
  WiFi.disconnect();
 
  // Initialize ESP-NOW
  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESP-NOW Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
    leds[0] = CRGB(0,50,0); //Green, succeed
    FastLED.show();
    delay(500);
    leds[0] = CRGB(0,0,0); //Off, finished setup
    FastLED.show();
  }
  else
  {
    Serial.println("ESP-NOW Init Failed");
    leds[0] = CRGB(50,0,0); //Red, failed
    FastLED.show();
    delay(3000);
    ESP.restart();
  }

  // Loop
  while(1){
    delay(150);
    // Transmit
    switch(readTouchPins()){
      case 0b00001: //1. Button => Red
//        Serial.println("1. BTN");
        broadcast("red");
        leds[0] = CRGB(100,0,0); // LED Red TEMPORARY SOLUTION
        leds[1] = CRGB(100,0,0); // LED Red
        leds[2] = CRGB(100,0,0); // LED Red
        leds[3] = CRGB(100,0,0); // LED Red
        FastLED.show();
        break;
      case 0b00010: //2. Button => Green
//        Serial.println("2. BTN");
        broadcast("green");
        leds[0] = CRGB(0,100,0); //LED Green TEMPORARY SOLUTION
        leds[1] = CRGB(0,100,0); //LED Green
        leds[2] = CRGB(0,100,0); //LED Green
        leds[3] = CRGB(0,100,0); //LED Green
        FastLED.show();
        break;
      case 0b00100: //3. Button => Blue
//        Serial.println("3. BTN");
        broadcast("blue");
        leds[0] = CRGB(0,0,100); //LED Blue TEMPORARY SOLUTION
        leds[1] = CRGB(0,0,100); //LED Blue
        leds[2] = CRGB(0,0,100); //LED Blue
        leds[3] = CRGB(0,0,100); //LED Blue
        FastLED.show();
        break;
      case 0b01000: //4. Button => Off
//        Serial.println("4. BTN");
        broadcast("off");
        leds[0] = CRGB(0,0,0); //LED Off TEMPORARY SOLUTION
        leds[1] = CRGB(0,0,0); //LED Off
        leds[2] = CRGB(0,0,0); //LED Off
        leds[3] = CRGB(0,0,0); //LED Off
        FastLED.show();
        break;
      default:
//        Serial.println(" Do nothing, only repeat switch 4 read BTN");
        break;  
      Serial.println(" BAF!"); // Never ever :) Pocuvaj, henty text nikedy neuvidis.
    }
    // Recive
//    Serial.println("repeat while and wait 4 push BTN or RECEIVE");
  }
//  Serial.println("vyskocil????");
}

//---------------------------------
//ESP-Now ends here
//---------------------------------

float analogReadBatt(){
  return (2.0*(2.50*analogRead(AIN_batt)/4096)); //volts float
}

//when 0, enters sleep without timed wakeup - sleeps forever.
void enter_sleep(uint16_t TimedWakeUpSec){
  if (TimedWakeUpSec != 0){
    esp_sleep_enable_timer_wakeup(TimedWakeUpSec*1000000);
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

  DispData ActualDispData;

  int SearchIndex = payload.indexOf("\"state\":\"");
  if (SearchIndex == -1){
    ActualDispData.valid = false;
    ActualDispData.RawState = "not valid";
    return ActualDispData;
  }
  int SearchIndexEnd = payload.indexOf("\"",SearchIndex+10); //+10 for skipping "state":" string
  
  //Serial.printf("indexes %d - %d\n",SearchIndex,SearchIndexEnd);
  ActualDispData.valid = true;
  ActualDispData.RawState = payload.substring(SearchIndex+9,SearchIndexEnd);
  ActualDispData.RawState.replace("\\n","\n"); //home assistant cannot send new line. sends \n in text instead. Replace by real new line here.
  //Serial.print("ISOLATED STATE:");
  //Serial.println(StateStr);

#if SHOW_LAST_UPDATE
  SearchIndex = payload.indexOf("\"last_changed\":\"");
  if (SearchIndex != -1){
    SearchIndexEnd = payload.indexOf("\"",SearchIndex+20);
    ActualDispData.LastChangedStr = payload.substring(SearchIndex+16,SearchIndexEnd);
  }
#endif
  return ActualDispData;
}

