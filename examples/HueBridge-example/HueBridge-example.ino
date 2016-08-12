/*------------------------------------------------------------------------------
  Example Sketch for Phillips Hue

  This lib requires you to have the neopixelbus lib in your arduino libs folder,
  for the colour management header files <RgbColor.h> & <HslColor.h>.
  https://github.com/Makuna/NeoPixelBus/tree/UartDriven
***  Needs to be UARTDRIVEN branch, or Animator Branch ***
  ------------------------------------------------------------------------------*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>

#include <ESP8266SSDP.h>
#include <ArduinoJson.h>

#include "HueBridge.h"
#include <Adafruit_NeoPixel.h>
// #include <FastLED.h>

//#include <NeoPixelBus.h>
//#include <NeoPixelBus.h> // NeoPixelAnimator branch


#include "/secrets.h" // Delete this line and populate the following
// const char* ssid = "*****";
// const char* pass = "*****";
const char* host = "Hue-Bridge";

const uint16_t aport = 8266;

WiFiServer TelnetServer(aport);
WiFiClient Telnet;
WiFiUDP OTA;

ESP8266WebServer HTTP(80);

//HueBridge* Hue = NULL; // used for dynamic memory allocation...

#define pixelCount 12 // Strip has 12 NeoPixels
#define pixelPin 14 // Strip is attached to pin 14

#define HUElights 12 // One light per NeoPixel
#define HUEgroups 1

void HandleHue(uint8_t Lightno, struct RgbColor rgb, HueLight* lightdata);

HueBridge Hue(&HTTP, HUElights, HUEgroups, &HandleHue);
//NeoPixelBus strip = NeoPixelBus(pixelCount, pixelPin);
//NeoPixelAnimator animator(&strip); // NeoPixel animation management object
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(pixelCount, pixelPin, NEO_GRB + NEO_KHZ800);

void HandleHue(uint8_t Lightno, struct RgbColor rgb, HueLight* lightdata) {

  // Hue callback....

  //  Serial.printf( "\n | Light = %u, Name = %s (%u,%u,%u) | ", Lightno, lightdata->Name, lightdata->Hue, lightdata->Sat, lightdata->Bri);
  //temporarily holds data from vals
  char x[10];
  char y[10];
  //4 is mininum width, 3 is precision; float value is copied onto buff
  dtostrf(lightdata->xy.x, 5, 4, x);
  dtostrf(lightdata->xy.y, 5, 4, y);


  Serial.printf( " | light = %3u, %15s , RGB(%3u,%3u,%3u), HSB(%5u,%3u,%3u), XY(%s,%s),mode=%s | \n",
                 Lightno, lightdata->Name,  rgb.R, rgb.G, rgb.B, lightdata->hsb.H, lightdata->hsb.S, lightdata->hsb.B, x, y, lightdata->Colormode);
  //        Serial.printf("changing light");
  pixels.setPixelColor(Lightno, pixels.Color(rgb.R, rgb.G, rgb.B)); // Moderately bright green color.

  pixels.show(); // This sends the updated pixel color to the hardware.
}



void setup() {
//  strip.Begin();
//  strip.Show();
  pixels.begin();

  Serial.begin(115200);

  delay(500);

  Serial.println("");
  Serial.println("Arduino HueBridge Test");

  Serial.printf("Sketch size: %u\n", ESP.getSketchSize());
  Serial.printf("Free size: %u\n", ESP.getFreeSketchSpace());

  uint8_t i = 0;

  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    i++;
    Serial.print(".");
    if (i == 20) {
      Serial.print("Failed");
      ESP.reset();
      break;
    } ;
  }


  if (WiFi.waitForConnectResult() == WL_CONNECTED) {


    MDNS.begin(host);
    MDNS.addService("arduino", "tcp", aport);
    OTA.begin(aport);
    TelnetServer.begin();
    TelnetServer.setNoDelay(true);
    delay(10);
    Serial.print("\nIP address: ");
    String IP = String(WiFi.localIP());
    //Serial.println(IP);
    Serial.println(WiFi.localIP());

    Hue.Begin();

    HTTP.begin();

  } else {
    Serial.println("CONFIG FAILED... rebooting");
    ESP.reset();
  }

  Serial.print("Free Heap: ");
  Serial.println(ESP.getFreeHeap());

}


void loop() {

  HTTP.handleClient();
  yield();

  OTA_handle();

  static unsigned long update_strip_time = 0;  //  keeps track of pixel refresh rate... limits updates to 33 Hz

  if (millis() - update_strip_time > 30)
  {
    //TODO update for new library
//    if ( animator.IsAnimating() ) animator.UpdateAnimations();
//    strip.Show();
//    update_strip_time = millis();
  }


}



void OTA_handle() {

  if (OTA.parsePacket()) {
    IPAddress remote = OTA.remoteIP();
    int cmd  = OTA.parseInt();
    int port = OTA.parseInt();
    int size   = OTA.parseInt();

    Serial.print("Update Start: ip:");
    Serial.print(remote);
    Serial.printf(", port:%d, size:%d\n", port, size);
    uint32_t startTime = millis();

    WiFiUDP::stopAll();

    if (!Update.begin(size)) {
      Serial.println("Update Begin Error");
      return;
    }

    WiFiClient client;

    bool connected = false;

    delay(2000);

    //do {
    connected = client.connect(remote, port);
    //} while (!connected || millis() - startTime < 20000);

    if (connected) {

      uint32_t written;
      while (!Update.isFinished()) {
        written = Update.write(client);
        if (written > 0) client.print(written, DEC);
      }
      Serial.setDebugOutput(false);

      if (Update.end()) {
        client.println("OK");
        Serial.printf("Update Success: %u\nRebooting...\n", millis() - startTime);
        ESP.restart();
      } else {
        Update.printError(client);
        Update.printError(Serial);
      }


    } else {
      Serial.printf("Connect Failed: %u\n", millis() - startTime);
      ESP.restart();
    }
  }
  //IDE Monitor (connected to Serial)
  if (TelnetServer.hasClient()) {
    if (!Telnet || !Telnet.connected()) {
      if (Telnet) Telnet.stop();
      Telnet = TelnetServer.available();
    } else {
      WiFiClient toKill = TelnetServer.available();
      toKill.stop();
    }
  }
  if (Telnet && Telnet.connected() && Telnet.available()) {
    while (Telnet.available())
      Serial.write(Telnet.read());
  }
  if (Serial.available()) {
    size_t len = Serial.available();
    uint8_t * sbuf = (uint8_t *)malloc(len);
    Serial.readBytes(sbuf, len);
    if (Telnet && Telnet.connected()) {
      Telnet.write((uint8_t *)sbuf, len);
      yield();
    }
    free(sbuf);
  }
  //delay(1);
}
