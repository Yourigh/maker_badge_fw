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

  leds[0] = CRGB(0,0,50); //Blue, connecting
  FastLED.show();

  if(setupOTA("MakerBadge", mySSID, myPASSWORD)){
    Serial.println("Connection failed");
    leds[0] = CRGB(255,0,0);
    FastLED.show();
    delay(2000);
  } else {
    leds[0] = CRGB(0,10,0);// CRGB::Green;
    FastLED.show();
  }
  

  // Your setup code
  delay(5000);
  
  Serial.println("OTA Initialized");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Hostname: ");
  Serial.println(ArduinoOTA.getHostname());
}

void loop() {
  // Your code here
}