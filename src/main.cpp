/*
Juraj Repcik

Before compiling, create config.h (copy and edit config_template.h). 
*/

#include "OTA.h"
#include "config.h"
#include "FastLED.h"
#include "GxEPD2_BW.h"
#include <HTTPClient.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include "driver/touch_pad.h"
//ESPNOW
#include <WiFi.h>
#include <esp_now.h>


// array for addressable LEDs control
CRGB leds[4];
// Instantiate the GxEPD2_BW class for our display type

//TODO - choose correct display according to MB version.
#if defined(MakerBadgeVersionA) || defined(MakerBadgeVersionB)
#error "Define correct display type. TODO for version A and B. Did you get MakerBadge at Maker Faire Brno 2022 or later? try MakerBadgeVersionC (in config.h)"
#endif
#if defined(MakerBadgeVersionC) || defined(MakerBadgeVersionD)
GxEPD2_BW<GxEPD2_213_B74, GxEPD2_213_B74::HEIGHT> display(GxEPD2_213_B74(IO_disp_CS, IO_disp_DC, IO_disp_RST, IO_disp_BUSY));  // GDEM0213B74 122x250, SSD1680
#endif
float analogReadBatt();
void enter_sleep(uint16_t TimedWakeUpSec);
uint8_t readTouchPins(void);
uint8_t MakerBadgeSetupWiFi(void);
void MakerBadgeSetupOTA(void);
void DisplayBadge(void);
void CallbackTouch3(void){} //empty function.
void FWloadMode(void);
String httpGETRequest(const char* serverName);
struct DispData httpParseReply(String payload);
void DisplayMenu(void);
void DisplayHomeAssistant(void);
void low_battery_shutdown(void);
void Playground(void); 
//espnow
void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength);
void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen);
void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status);
void broadcast(const String &message);
char espnow_rcv_buffer[ESP_NOW_MAX_DATA_LEN + 1];
int msgLen = 0; //0 is no message, others, message pending
char peerMac[18];
#define MAGICPREFIX "*/(MB"

//Structure for getting data from Home Assistant
struct DispData{
  bool valid = false;
  String RawState = "Empty";
  String LastChangedStr = "Empty";
  //float TamperatureOutside = 0;
  //uint16_t co2 = 0;
};

//States of menu
enum mbStates{Menu, HomeAssistant, Badge, FWupdate, PlaceholderOne};


uint8_t TouchPins = 0x00;
uint8_t TouchPinsLast = 0x00;
uint16_t BattBar = 0;

//Variable for remembering current mode (mbStates) while in deep sleep. 
//The memory space for this var is in ULP (ultra-low-power coprocessor) that is powered even in deep sleep.
RTC_DATA_ATTR mbStates CurrentMode = Menu; //to store in ULP, kept during deep sleep

void setup() {
  delay(3000); //uncomment to see first serial logs on USB serial, delay while windows recognizes the USB and conencts to serial
  Serial.printf("[%d] Start\n",millis());

#ifdef MakerBadgeVersionD
  //Battery voltage reading 
  //can be read right after High->Low transition of IO_BAT_meas_disable
  //Here, pin should not go LOW, so intentionally digitalWrite called as first.
  //First write output register (PORTx) then activate output direction (DDRx). Pin will go from highZ(sleep) to HIGH without LOW pulse.
  digitalWrite(IO_BAT_meas_disable,HIGH); 
  pinMode(IO_BAT_meas_disable,OUTPUT); 
#endif

  //Serial
  Serial.begin(115200);
  //ADC
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  //LEDs
  pinMode(IO_led_disable,OUTPUT);
  digitalWrite(IO_led_disable,HIGH); //disabled for now, functions will enable it when needed
  FastLED.addLeds<WS2812B, IO_led, GRB>(leds, 4);

  //Display
#ifdef MakerBadgeVersionD
  pinMode(IO_EPD_power_disable,OUTPUT);
  digitalWrite(IO_EPD_power_disable,LOW); //enable power to EPD
#endif
  display.init(0); //enter 115200 instead 0 to see debug in console
  display.setRotation(3);
  display.setTextColor(GxEPD_BLACK);



  touch_pad_init(); //deinit is needed when going to sleep. Without deinit - extra 100uA. done in enter sleep function
  //touch wakeup, uncomment if you want to wake up on touch pin.
  //touchAttachInterrupt(IO_touch3,CallbackTouch3,TOUCH_TRESHOLD); //Middle touch input is wake up interrupt.
  //esp_sleep_enable_touchpad_wakeup(); //disabled intentionally - same effect as reset button.

  //Wake up and resume to last mode. If woken up from reset(not timer) - show menu.
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
          FWloadMode(); //forever powered on, shuts down on low battery
          break;
        case PlaceholderOne:
          Playground(); //forever powered on, shuts down on low battery
          break;
      }
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      //falldown
      //define custom action for touch wake up case, and add break;
    default:
      //normal power-up after reset
      DisplayMenu(); //menu to select a mode. Blocking.
      enter_sleep(1); //sleeps for 1s and gets back to switch timer wakeup cause.
  }
} //end setup

void loop() {
  //intentionally empty
}

void DisplayMenu(void){
  digitalWrite(IO_led_disable,LOW);
  leds[0] = CRGB(5,10,0);
  leds[1] = CRGB(5,10,0);
  FastLED.show();
  display.setFont(&FreeMonoBold9pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(true);
  BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 12);
    display.print("   Maker Badge Menu");
    display.setCursor(0, 39);
    display.printf("   1. Home Assistant\n   2. Badge\n   3. FW update\n   4. Playground");
    display.fillRect(0,20,250,2,GxEPD_BLACK);
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  uint8_t flipLED = 1;
  uint32_t lastMillis = 0;
  uint16_t timeout = 100; //*0.6s = 600s = 10min
  while(1){
    delay(150);
    switch(readTouchPins()){
    //switch(1){
      case 0b00001: //key 1 (left)
        CurrentMode = HomeAssistant;
        return;
      case 0b00010: //2
        CurrentMode = Badge;
        return;
      case 0b00100: //3
        CurrentMode = FWupdate;
        return;
      case 0b01000: //4
        CurrentMode = PlaceholderOne;
        return;
      //case 0b10000: //5
        //return;
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
  DispData ActualDispData;
  BattBar = ((analogReadBatt()*10-34)*31); //measure battery before connecting to WiFi - if empty, will shutdown.

  if(MakerBadgeSetupWiFi()){
    enter_sleep(HA_UPDATE_PERIOD_SEC); //on fail
  }
  //No OTA is set up to save energy, FWupdate mode is for OTA
  display.setFont(&FreeMonoBold9pt7b);
  
  //Serial.printf("Batt: %.3f, Bar:%d\n",analogReadBatt(),BattBar);
  //Serial.println(httpGETRequest("http://192.168.1.14:8123/api/")); //test of HA - should get API RUNNING
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
  digitalWrite(IO_led_disable,LOW);
  leds[0] = CRGB(00,10,0);
  FastLED.show();
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);
  BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.print(BadgeName);
    //display.drawLine(226,7,226-6,7+6,GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(70, 70);
    display.print(BadgeLine2);
    display.setCursor(45, 100);
    display.print(BadgeLine3);
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  digitalWrite(IO_led_disable,HIGH);
  display.powerOff();
  enter_sleep(0);
}

void FWloadMode(void){
  BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
  digitalWrite(IO_led_disable,LOW);
  leds[0] = CRGB(0,0,50); //Blue, connecting
  FastLED.show();
  MakerBadgeSetupOTA();
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.print(" FW update");
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(29, 60);
    display.print(WiFi.localIP().toString());
    display.setCursor(0, 80);
    display.printf("MakerBadge-%02x%02x%02x.local",mac[3], mac[4], mac[5]);
    display.setCursor(50, 106);
    display.printf("Batt %.2f V",analogReadBatt());
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());
  uint8_t ledrotate = 0;
  while(1){
    if (WiFi.isConnected())
      leds[ledrotate++] = CRGB(20,20,0);
    else  
      leds[ledrotate++] = CRGB(20,0,0);
    FastLED.show();
    delay(600);
    FastLED.clear(true);
    if (ledrotate == 4) ledrotate = 0;
    analogReadBatt(); //for low voltage shutdown
  }
}

void Playground(void){

  Serial.println("Playground-enter");
  WiFi.mode(WIFI_STA);
  // Output my MAC address - useful for later
  Serial.print("My MAC Address is: ");
  Serial.println(WiFi.macAddress());
  String MyMAC;
  MyMAC = WiFi.macAddress();
  // shut down wifi
  WiFi.disconnect();

  if (esp_now_init() == ESP_OK)
  {
    Serial.println("ESPNow Init Success");
    esp_now_register_recv_cb(receiveCallback);
    esp_now_register_send_cb(sentCallback);
  }
  else
  {
    Serial.println("ESPNow Init Failed");
  }
  char sendbuff[251];
  snprintf(sendbuff,250,"%s%s",MAGICPREFIX,"Hello World 2 lorem ipsum");
  broadcast(sendbuff);

  BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
  digitalWrite(IO_led_disable,LOW);
  leds[0] = CRGB(50,0,50); //Violet
  FastLED.show();
  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(true);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(0, 20);
    display.print(BadgeName+MyMAC.substring(8));
    display.setFont(NULL);
    display.setCursor(16, 64);
    display.print("123456789|1x|0123456789|2x|0123456789");
    display.setCursor(16, 64+8);
    display.print("ABCDEFGHI|  |JKLMNOPQRS|  |TUVWXYZ_!<");
    display.setCursor(0, 64+8+10);
    display.print("Write msg with touch binary,BOOT->send!");
  } while (display.nextPage());
  uint8_t ledrotate = 0;
  uint8_t test_rectangle_x=0;
  uint8_t test_rectangle_y=0;
  FastLED.clear(true);
  while(1){
    leds[ledrotate++] = CRGB(0,0,0);
    leds[ledrotate] = CRGB(10,0,10);
    FastLED.show();
    delay(300);
    if (ledrotate == 4) ledrotate = 0;
    analogReadBatt(); //for low voltage shutdown

    if(test_rectangle_x-8>DISP_X-8) //intentional underflow
      test_rectangle_x = -10;
    display.setPartialWindow(test_rectangle_x, test_rectangle_y, 16, 8);
    test_rectangle_x+= 8;
    do {
      display.fillScreen(GxEPD_WHITE);
      display.fillRect(test_rectangle_x,test_rectangle_y,8,8,GxEPD_BLACK);
    } while (display.nextPage());
    if(msgLen>0 & 
      espnow_rcv_buffer[0]==MAGICPREFIX[0] & 
      espnow_rcv_buffer[1]==MAGICPREFIX[1] & 
      espnow_rcv_buffer[2]==MAGICPREFIX[2] & 
      espnow_rcv_buffer[3]==MAGICPREFIX[3] & 
      espnow_rcv_buffer[4]==MAGICPREFIX[4])
    {
      display.setPartialWindow(0, 25, DISP_X, 34); //todo, not till the end
      test_rectangle_x+= 8;
      do {
        //display.setFont(NULL);//5x7
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(0, 39);
        display.print(&peerMac[9]);//only last 3 bytes
        display.print(">");
        display.print(&espnow_rcv_buffer[5]); //without magic prefix, write last received message
      } while (display.nextPage());
      msgLen = 0;
      for(uint8_t lalarm=0;lalarm<8;lalarm++){
        fill_solid(leds,4,CRGB(255,0,00));
        FastLED.show();
        delay(200);
        FastLED.clear(true);
        delay(200);
      }
    }
  }
}

void formatMacAddress(const uint8_t *macAddr, char *buffer, int maxLength)
{
  snprintf(buffer, maxLength, "%02x:%02x:%02x:%02x:%02x:%02x", macAddr[0], macAddr[1], macAddr[2], macAddr[3], macAddr[4], macAddr[5]);
}

void receiveCallback(const uint8_t *macAddr, const uint8_t *data, int dataLen)
{
  // format the mac address
  //char peerMac[18]; //chenged to global
  formatMacAddress(macAddr, peerMac, 18);
  // only allow a maximum of 250 characters in the message + a null terminating byte
  msgLen = min(ESP_NOW_MAX_DATA_LEN, dataLen);
  strncpy(espnow_rcv_buffer, (const char *)data, msgLen);
  // make sure we are null terminated
  espnow_rcv_buffer[msgLen] = 0;
  // debug log the message to the serial port
  Serial.printf("Received message from: %s - %s\n", peerMac, espnow_rcv_buffer);
}

// callback when data is sent
void sentCallback(const uint8_t *macAddr, esp_now_send_status_t status)
{
  char macStr[18];
  formatMacAddress(macAddr, macStr, 18);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void broadcast(const String &message)
{
  // this will broadcast a message to everyone in range
  uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, broadcastAddress, 6);
  if (!esp_now_is_peer_exist(broadcastAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  esp_err_t result = esp_now_send(broadcastAddress, (const uint8_t *)message.c_str(), message.length());
  // and this will send a message to a specific device
  /*uint8_t peerAddress[] = {0x3C, 0x71, 0xBF, 0x47, 0xA5, 0xC0};
  esp_now_peer_info_t peerInfo = {};
  memcpy(&peerInfo.peer_addr, peerAddress, 6);
  if (!esp_now_is_peer_exist(peerAddress))
  {
    esp_now_add_peer(&peerInfo);
  }
  esp_err_t result = esp_now_send(peerAddress, (const uint8_t *)message.c_str(), message.length());*/
  if (result == ESP_OK)
  {
    Serial.println("Broadcast message success");
  }
  else if (result == ESP_ERR_ESPNOW_NOT_INIT)
  {
    Serial.println("ESPNOW not Init.");
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


//---------------------------------
float analogReadBatt(){
#ifdef MakerBadgeVersionD
  digitalWrite(IO_BAT_meas_disable,LOW);
  delayMicroseconds(150);
#endif
  uint16_t batt_adc = analogRead(AIN_batt);
#ifdef MakerBadgeVersionD
  digitalWrite(IO_BAT_meas_disable,HIGH);
#endif
  float battv = (BATT_V_CAL_SCALE*2.0*(2.50*batt_adc/4096));
  //Serial.printf("Battv: %fV, Bat w/ calibration %fV, raw ADC %d\n",battv/BATT_V_CAL_SCALE,battv,batt_adc);
  if(battv<3.45){ //3.3V is sustem power, 150mV is LDO dropoff (estimates only)
    Serial.printf("Bat %fV, shutting down...\n",battv,batt_adc);
    //ESP_LOGE("MakerBadge","Batt %f V",battv); //log to HW UART
    low_battery_shutdown();
  }
  return battv; //volts float
}

//shutdown the system to protect battery
void low_battery_shutdown(void){
  digitalWrite(IO_led_disable,HIGH);
  //display badge, but without battery measurement, show discharged.
  display.setFont(&FreeMonoBold18pt7b);
    display.setFullWindow();
    display.firstPage();
    display.setTextWrap(false);
    do {
      display.fillScreen(GxEPD_WHITE);
      display.setCursor(0, 30);
      display.print(BadgeName);
      display.setFont(&FreeMonoBold9pt7b);
      display.setCursor(70, 70);
      display.print(BadgeLine2);
      display.setCursor(45, 100);
      display.print(BadgeLine3);
      display.setCursor(55, DISP_Y-5);
      display.print("Discharged!");
    } while (display.nextPage());
    display.powerOff();
  enter_sleep(0);
}

//when 0, enters sleep without timed wakeup - sleeps forever.
void enter_sleep(uint16_t TimedWakeUpSec){
  touch_pad_deinit();
  digitalWrite(IO_led_disable,HIGH);
#ifdef MakerBadgeVersionD
  digitalWrite(IO_BAT_meas_disable,HIGH);
  digitalWrite(IO_EPD_power_disable,HIGH);
#endif
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true,false);
  while(WiFi.isConnected()){} //wait on disconnect
  if (TimedWakeUpSec != 0){
    esp_sleep_enable_timer_wakeup(TimedWakeUpSec*1000000);
  }
  esp_deep_sleep_start();
}

/**
 * @brief Reads touch pins
 * 
 * @return uint8_t 0bxx54321 where 54321 are touch inputs. 1 is the one on the left.
 */
uint8_t readTouchPins() {
  uint8_t TouchResultMask = 0x00;
  for (int i = 0; i < 5; i++) {
    // (5 - i) because pins order is reversed
    if (touchRead(5 - i) > TOUCH_TRESHOLD ) {
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
    digitalWrite(IO_led_disable,LOW);
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
  http.addHeader("Authorization",HAtoken); //authorization for home assistant

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
  //find and parse: "state":"lalalalalalalla1234",
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

