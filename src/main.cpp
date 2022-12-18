#define ESP32_RTOS  // Uncomment this line if you want to use the code with freertos only on the ESP32 // Has to be done before including "OTA.h"
#include "OTA.h"
#include "credentials.h"
#include "FastLED.h"
#include "MakerBadgePins.h"

CRGB leds[4];

void setup() {
  pinMode(IO_led_enable_n,OUTPUT);
  digitalWrite(IO_led_enable_n,LOW);
  FastLED.addLeds<WS2812B, IO_led, GRB>(leds, 4);
  delay(5);
  leds[0] = CRGB(10,0,0);
  FastLED.show();

  Serial.begin(115200);
  Serial.println("Booting");

  setupOTA("MakerBadge", mySSID, myPASSWORD);
  leds[0] = CRGB(0,50,0);// CRGB::Green;
  FastLED.show();

  // Your setup code
  delay(5000);
  
  Serial.println("OTA Initialized");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(ArduinoOTA.getHostname());
}

void loop() {
#if defined(ESP32_RTOS) && defined(ESP32)
  //empty
#else // If you do not use FreeRTOS, you have to regulary call the handle method.
  ArduinoOTA.handle();
#endif

  // Your code here
}