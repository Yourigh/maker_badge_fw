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
//#include <Fonts/FreeSans_18pt.h>
#include <Fonts/FreeSansBold_12pt.h>
#include <Fonts/RobotoCondensed-Regular_18pt.h>
#include "gfxlatin2.h" //for Czech characters support.
#include "driver/touch_pad.h"
//ESPNOW
#include <WiFi.h>
#include <esp_now.h>
//Ozone
#include "DFRobot_OzoneSensor.h"

#define COLLECT_NUMBER   20              // collect number, the collection range is 1-100
#define Ozone_IICAddress OZONE_ADDRESS_3
/*   iic slave Address, The default is ADDRESS_3
       ADDRESS_0               0x70      // iic device address
       ADDRESS_1               0x71
       ADDRESS_2               0x72
       ADDRESS_3               0x73
*/
DFRobot_OzoneSensor Ozone;

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
String getLine(const String& data, int lineIndex);
void DisplayMenu(void);
void DisplayHomeAssistant(void);
void low_battery_shutdown(void);
void MakerCall(void); 
void SensorMode(void);
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
enum mbStates{Menu, HomeAssistant, Badge, FWupdate, EspNow_sms, Sensors};


uint8_t TouchPins = 0x00;
uint8_t TouchPinsLast = 0x00;
uint16_t BattBar = 0;

//Variable for remembering current mode (mbStates) while in deep sleep. 
//The memory space for this var is in ULP (ultra-low-power coprocessor) that is powered even in deep sleep.
RTC_DATA_ATTR mbStates CurrentMode = Menu; //to store in ULP, kept during deep sleep

void setup() {
  //delay(3000); //uncomment to see first serial logs on USB serial, delay while windows recognizes the USB and conencts to serial
  //Serial.printf("[%d] Start\n",millis());

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

  //BOOT button
  pinMode(0,INPUT_PULLUP);

  touch_pad_init(); //deinit is needed when going to sleep. Without deinit - extra 100uA. done in enter sleep function
  //touch wakeup, uncomment if you want to wake up on touch pin.
  //touchAttachInterrupt(IO_touch3,CallbackTouch3,TOUCH_TRESHOLD); //Middle touch input is wake up interrupt.
  //esp_sleep_enable_touchpad_wakeup(); //disabled intentionally - same effect as reset button.

  //Wake up and resume to last mode. If woken up from reset(not timer) - show menu.
  //Serial.printf("CurrentMode is:%d",CurrentMode);
  
#if DEBUG_HA
  while(1)
    DisplayHomeAssistant(); 
#endif

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
        case EspNow_sms:
          MakerCall(); //forever powered on, shuts down on low battery
          break;
        case Sensors:
          SensorMode(); //forever powered on, shuts down on low battery
          break;
      }
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      //falldown
      //define custom action for touch wake up case, and add break;
    default:
      //normal power-up after reset
#ifdef MakerCall_only
      MakerCall(); //never leaves this function
#else
      DisplayMenu(); //menu to select a mode. Leaves function upon selecting the mode or shutdown on timeout.
#endif
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
    display.printf("   1. Home Assistant\n   2. Badge\n   3. FW update\n   4. MakerCall\n   5. Sensors");
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
        CurrentMode = EspNow_sms;
        return;
      case 0b10000: //5
        CurrentMode = Sensors;
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
  DispData ActualDispData;
  BattBar = ((analogReadBatt()*10-34)*31); //measure battery before connecting to WiFi - if empty, will shutdown.

  if(MakerBadgeSetupWiFi()){
    enter_sleep(HA_UPDATE_PERIOD_SEC); //on fail
  }
  //No OTA is set up to save energy, FWupdate mode is for OTA
  display.setFont(&RobotoCondensed_Regular18pt8b);  
  
  //Serial.printf("Batt: %.3f, Bar:%d\n",analogReadBatt(),BattBar);
  //Serial.println(httpGETRequest("http://192.168.1.14:8123/api/")); //test of HA - should get API RUNNING
  ActualDispData = httpParseReply(httpGETRequest(HAreqURL));
  ActualDispData.RawState = utf8tocp(ActualDispData.RawState); //for czech support
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
    display.setCursor(0, 23);
    display.print(getLine(ActualDispData.RawState,0));
    display.setCursor(0, 23+30);
    display.print(getLine(ActualDispData.RawState,1));
    display.setFont(&FreeSansBold12pt8b);
    display.setCursor(0, 23+30+28);
    display.print(getLine(ActualDispData.RawState,2));
#if SHOW_LAST_UPDATE
    display.setFont(NULL); // default 5x7 system font?
    display.setCursor(23, DISP_Y-10);
    display.print(ActualDispData.LastChangedStr);
#endif
  } while (display.nextPage());

uint16_t HA_SLEEP_SEC = 10;
uint8_t hour = ActualDispData.LastChangedStr.substring(11, 13).toInt();

// Check if the current hour is within the night time range
if (HA_UPDATE_NIGHT_UTC_HR_START < HA_UPDATE_NIGHT_UTC_HR_STOP) {
    // For ranges that don't cross midnight (like 1AM to 5AM)
    if (hour >= HA_UPDATE_NIGHT_UTC_HR_START && hour < HA_UPDATE_NIGHT_UTC_HR_STOP) {
        HA_SLEEP_SEC= HA_UPDATE_PERIOD_SEC_NIGHT;
    } else {
        HA_SLEEP_SEC= HA_UPDATE_PERIOD_SEC_DAY;
    }
} else {
    // For ranges that cross midnight (like 22PM to 8AM)
    if (hour >= HA_UPDATE_NIGHT_UTC_HR_START || hour < HA_UPDATE_NIGHT_UTC_HR_STOP) {
        HA_SLEEP_SEC= HA_UPDATE_PERIOD_SEC_NIGHT;
    } else {
        HA_SLEEP_SEC= HA_UPDATE_PERIOD_SEC_DAY;
    }
}
//Serial.printf("Going to sleep for %d s\n",HA_SLEEP_SEC);

#if DEBUG_HA
  delay(3000);
#else
  enter_sleep(HA_SLEEP_SEC);
#endif
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
  char text[24];
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 30);
    strcpy( text, BadgeName );
    display.print(text);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(0, 70);
    strcpy( text, BadgeLine2 );
    display.print(text);
    display.setCursor(0, 100);
    strcpy( text, BadgeLine3 );
    display.print(text);
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

void MakerCall(void){
  //y first, then x coordinate
  const char keyboard[4][10] = { 
        {'1','2','3','4','5','6','7','8','9','0'}, 
        {'Q','W','E','R','T','Y','U','I','O','P'}, 
        {'A','S','D','F','G','H','J','K','L','<'},
        {'.','Z','X','C','V',' ','B','N','M','?'}
        };
  uint8_t keyboard_xy[2] = {0,2}; //x,y char A
  uint8_t keyboard_xy_old[2] = {0,0};//x,y 
  #define kb_x keyboard_xy[0]
  #define kb_y keyboard_xy[1]
  #define kbo_x keyboard_xy_old[0]
  #define kbo_y keyboard_xy_old[1]

  Serial.println("MakerCall-enter");
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
  char writebuff[251] = {0};
  uint8_t writebuff_len = 0;
  bool enter_key_flag = false;

  BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
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
    display.setCursor(0, 13);
    display.print(BadgeName+MyMAC.substring(8));
    display.fillRect(0,20,DISP_X,3,GxEPD_BLACK);
    display.setFont(NULL);
    display.setCursor(3, 64);
    display.print("1 2 3 4 5 6 7 8 9 0");
    display.setCursor(3, 64+11);
    display.print("Q W E R T Y U I O P");
    display.setCursor(3, 64+11*2);
    display.print("A S D F G H J K L <");
    display.setCursor(3, 64+11*3);
    display.print(". Z X C V _ B N M ?");
    display.setCursor(130, 64);
    display.print("     MAKERCALL");
    display.setCursor(130, 64+11);
    display.print("DN | UP | OK | < | >");
    display.setCursor(130, 64+11*2);
    display.print(" BOOT btn sends to");
    display.setCursor(130, 64+11*3);
    display.print(" all MakerBadges!");
  } while (display.nextPage());
  //uint8_t ledrotate = 0;

  if (!setCpuFrequencyMhz(80)) //to save power - decreses consumption from 82mA to 76mA
    Serial.println("Error - Not valid frequency!");
  uint32_t last_battbar_update = 0;

  while(1){
    //leds[ledrotate++] = CRGB(0,0,0);
    //if (ledrotate == 4) ledrotate = 0;
    //leds[ledrotate] = CRGB(10,0,10);
    //FastLED.show();
    
    if(millis()> last_battbar_update + 60000){ //60s
      BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
      //will shutdown on low voltage
      display.setPartialWindow(0, 120, DISP_X, 8);//y and h multiples of 8
      do {
        display.fillScreen(GxEPD_WHITE);
        display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
      } while (display.nextPage());

      last_battbar_update = millis();
    } 

    //if new data received
    if(msgLen>0 && 
      espnow_rcv_buffer[0]==MAGICPREFIX[0] && 
      espnow_rcv_buffer[1]==MAGICPREFIX[1] && 
      espnow_rcv_buffer[2]==MAGICPREFIX[2] && 
      espnow_rcv_buffer[3]==MAGICPREFIX[3] && 
      espnow_rcv_buffer[4]==MAGICPREFIX[4])
    {
      Serial.println("Received data - update screen");
      display.setPartialWindow(0, 27, DISP_X, 30);
      display.setTextWrap(true);
      do {
        display.fillScreen(GxEPD_WHITE);
      } while (display.nextPage());
      do {
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(0, 39);
        display.print(&peerMac[9]);//only last 3 bytes
        display.print(">");
        display.print(&espnow_rcv_buffer[5]); //without magic prefix, write last received message
      } while (display.nextPage());
      msgLen = 0;
      digitalWrite(IO_led_disable,LOW);
      for(uint8_t lalarm=0;lalarm<4;lalarm++){
        
        fill_solid(leds,4,CRGB(255,0,00));
        FastLED.show();
        delay(200);
        FastLED.clear(true);
        delay(200);
      }
      display.setFont(NULL); //prepare font for next time.
      display.setTextWrap(false);
      digitalWrite(IO_led_disable,HIGH);
    }
    //keyboard and writing message
    switch(readTouchPins()){
      case 0b00001: //key 1 (left) (down function)
        kb_y= kb_y>2 ? 0 : kb_y+1;
        break;
      case 0b00010: //2 (up function)
        kb_y= kb_y==0 ? 3 : kb_y-1;
        break;
      case 0b00100: //3  (select function - enter key into write buffer)
        writebuff_len = strlen(writebuff);
        if (keyboard[kb_y][kb_x] == '<'){ //backspace
          writebuff[writebuff_len-1] = 0;
        } else {
          writebuff[writebuff_len++] = keyboard[kb_y][kb_x];
          writebuff[writebuff_len] = 0;
        }
        digitalWrite(IO_led_disable,LOW);
        enter_key_flag = true;
        fill_solid(leds,4,CRGB(0,64,0));
        FastLED.show();
        delay(150);
        FastLED.clear(true);
        break;
        digitalWrite(IO_led_disable,HIGH);
      case 0b01000: //4  (left function)
        kb_x= kb_x==0 ? 9 : kb_x-1;
        break;
      case 0b10000: //5  (right function)
        kb_x= kb_x>8 ? 0 : kb_x+1;
        break;
      default:
        delay(150);        
        break;
    }
    if(kb_x!=kbo_x || kb_y!=kbo_y || enter_key_flag){
    Serial.printf("Keyboard x:%d y:%d char:%c | old  x:%d y:%d char:%c\n",
      kb_x,kb_y,keyboard[kb_y][kb_x],
      kbo_x,kbo_y,keyboard[kbo_y][kbo_x]);
    }
    
    if(kb_x!=kbo_x){ //x change -> remove old hoirzontal
      display.setPartialWindow(12*kbo_x, 64, 2, 40);//y and h multiples of 8
      do {
        display.fillScreen(GxEPD_WHITE); //remove any horizontal line selector
      } while (display.nextPage());
    }

    if(kb_y!=kbo_y || kb_x!=kbo_x){ //y or x change
      display.setPartialWindow(12*kb_x, 64, 2, 40);//y and h multiples of 8
      do {
        display.fillScreen(GxEPD_WHITE); //remove any horizontal line selector
        display.fillRect(12*kb_x,64+10*kb_y,2, 10,GxEPD_BLACK);
        } while (display.nextPage());
    }
    if(enter_key_flag){
      Serial.printf("key select update, buffer %s\n",writebuff);
      
      display.setPartialWindow(0, 104, DISP_X, 16);//y and h multiples of 8
      do {
        display.fillScreen(GxEPD_WHITE); //remove any horizontal line selector
        display.setCursor(0, 104+8);
        display.print(writebuff);
      } while (display.nextPage());
    }

    if(digitalRead(0)==0 && strlen(writebuff)){ //pressed send and not empty buffer
      //send message
      digitalWrite(IO_led_disable,LOW);
      snprintf(sendbuff,250,"%s%s",MAGICPREFIX,writebuff);
      broadcast(sendbuff);
      
      fill_solid(leds,4,CRGB(32,32,0)); //yellow, indicate sending
        FastLED.show();
        delay(150);
        FastLED.clear(true);
      display.setPartialWindow(0, 104, DISP_X, 16);//y and h multiples of 8
      do {
        display.fillScreen(GxEPD_WHITE); //remove any horizontal line selector
        display.setCursor(0, 104+8);
        display.print("Sent> ");
        display.print(writebuff);
      } while (display.nextPage());
      writebuff[0]=0; //clear
      delay(100); //crappy debounce
      digitalWrite(IO_led_disable,HIGH);
    }

    //old marking line removed, update old with recent.
    kbo_x = kb_x;
    kbo_y = kb_y;
    enter_key_flag = false;


  }
}

void SensorMode(void){
  unsigned long startTime = millis();
  uint8_t timeout = 0;
  int16_t OzValue = -1;

  Wire.begin(8,9,100000); //SDA, SCL, 100kHz

  while(!Ozone.begin(Ozone_IICAddress)) {
    Serial.println("I2c device number error !");
    delay(10);
    if (millis() - startTime > 3000) {
      Serial.println("Timeout occurred");
      timeout=1;
      break;
    }
  }  
  Ozone.setModes(MEASURE_MODE_PASSIVE);

  display.setFont(&FreeMonoBold18pt7b);
  display.setFullWindow();
  display.firstPage();
  display.setTextWrap(false);
  BattBar = ((analogReadBatt()-3.45)*333.3); //3.45V to 4.2V range convert to 0-250px.
  char text[24];
  if(timeout){
    strcpy(text, "ERR");
  } else {
    if (OzValue == -1)
      strcpy(text, "INIT");
    else
      sprintf(text, "%d", OzValue);
  }
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.print(text);
    //display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(0, 100);
    strcpy( text, "Ozone PPB" );
    display.print(text);
    //display.setCursor(0, 100);
    //strcpy( text, "Runtime[s]: " );
    //display.print(text);
    //display.setCursor(120, 100);
    //sprintf(text, "%d", millis()/1000);
    //display.print(text);
    display.fillRect(0,DISP_Y-2,BattBar,2,GxEPD_BLACK);
  } while (display.nextPage());

  if (timeout) {
    digitalWrite(IO_led_disable,HIGH);
    display.powerOff();
    enter_sleep(0);
  }

  while(1){
    OzValue = Ozone.readOzoneData(COLLECT_NUMBER);
    Serial.println("Screen partial update");
      display.setPartialWindow(0, 0, DISP_X, 30);
      do {
        display.fillScreen(GxEPD_WHITE);
      } while (display.nextPage());
      do {
        display.setFont(&FreeMonoBold18pt7b);
        display.setCursor(0, 30);
        sprintf(text, "%d", OzValue);
        display.print(text);
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(175, 15);
        sprintf(text, "%d", millis()/1000);
        display.print(text);
      } while (display.nextPage());
    delay(2000);
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
      display.setCursor(0, 100);
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

  //last update used for distingushing night and day
  SearchIndex = payload.indexOf("\"last_changed\":\"");
  if (SearchIndex != -1){
    SearchIndexEnd = payload.indexOf("\"",SearchIndex+20);
    ActualDispData.LastChangedStr = payload.substring(SearchIndex+16,SearchIndexEnd);
  }

  return ActualDispData;
}

/**
 * @brief Retrieves a specified line from a string.
 * 
 * This function searches for newline characters (\n) to identify lines within a string.
 * It returns the entire line at the zero-based index specified by lineIndex. If the line
 * index exceeds the number of lines in the string, an empty string is returned. If lineIndex
 * is 0, the first line is returned. Lines are counted based on the occurrence of newline
 * characters, with the first line being from the start of the string to the first newline.
 *
 * @param data The string containing lines separated by newline characters.
 * @param lineIndex The zero-based index of the line to retrieve. (0 for the first line)
 * @return String The line at the specified index or an empty string if the index is out of range.
 */
String getLine(const String& data, int lineIndex) {
    int line = 0; // Start at the first line
    int start = 0; // Start index of the line
    int end; // End index of the line (position of newline character)

    for (int i = 0; i <= lineIndex; ++i) {
        end = data.indexOf("\\n", start); // Find the newline character from start index
        if (end == -1) {
            if (i == lineIndex) { // If this is the last line, return the rest of the string
                return data.substring(start);
            } else { // If the requested line index is higher than the total number of lines, return empty
                return "";
            }
        } else {
            if (i == lineIndex) { // If current line is the requested one, return the line
                return data.substring(start, end);
            }
            // Not the requested line, move to the next line
            start = end + 2;
        }
    }
    return ""; // If for some reason the line index is out of range, return empty
}