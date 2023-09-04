/*

***************************************************************************************************

WebSocket and mDNS code sourced from examples in Arduino IDE and then modified for purpose.

You will have to create arduino_secrets.h to store the Wifi Credentials (if using Wifi) with these two definitions:

#define SECRET_SSID "WifiSSID";
#define SECRET_PASS "WifiPassword";

***************************************************************************************************


*********** ToDo ***********
- Add Support for the Ethernet Wing
- Add Support for Relay Wing control
- Need to investigate ArduinoJSON to see if there is a way to list all included keys before trying to read value
  + Right now the code will support websocket data that doesn't follow the JSON format as long as it isn't a return character
  + A return character from the websocket connection will cause the code to crash and the chip will reboot
  + Ideally we would first search the deserialized JSON key's, and then based on what we find read the values and act on them so we aren't trying to read non-existant key:values

********** Bugs ***********
- If you send an empty string (maybe a CR or LF) to the websocket, the program will crash as soon as it tries to do a Serial.print() with that data. 


*********** Expected JSON from the Enterprise Automation System ***********

    {
      "control": {
        "redLED": "on",
        "greenLED": "off",
        "blueLED": "off",
        "relay0": "off",
        "display0": "Test Display Message"
      }
    }

*/


#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "arduino_secrets.h"

#define NUMPIXELS 1
#define PIN 40
#define SDA 8
#define SCL 9
#define redLEDpin 18
#define greenLEDpin 14
#define blueLEDpin 12
#define LDO2enable 39
#define button 17
#define relay 11
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define OLED_RESET -1     // Can set an oled reset pin if desired

Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 100000);

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

// Constants
const char* ssid = SECRET_SSID; // Defined in arduino_secrets.h
const char* password = SECRET_PASS; // Defined in arduino_secrets.h

// Globals
WebSocketsServer webSocket = WebSocketsServer(80);
int messageCounter = 0;

const char* LEDon = "on";
const char* LEDoff = "off";

// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected!\n", num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Connection from ", num);
        Serial.println(ip.toString());
      }
      break;

    // Echo text message back to client
    case WStype_TEXT:{

    Serial.printf("[%u] Text: %s\n", num, payload);

  
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, (char *)payload);

   if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
   } else {

      uint32_t color = strip.getPixelColor(0);

      //Serial.println(greenLED);
      //Serial.println(redLED);

      // We need to pull the current LED color state and then add or remove color
      // strip.getPixelColor returns 32 bits of data, only which the low 24 bits are used
      // Here is the believed formatting:
      // 0000 0000 RRRR RRRR GGGG GGGG BBBB BBBB
      // Each R, G or B value can be up to 255
      // We can use bit masking and shifting to play with this value!
      // Red = 16 bit offset
      // Green = 8 bit offset
      // Blue = 0 bit offset
      //
      // To turn on red, we shift 0xFF to the left by 16 bits and OR it with register
      // To turn off red, we need to AND 0xFF00FF with the register
      //
      // So to turn on green, we need to shift 0xFF to the left by 8 bits and OR it with register
      // To turn off green, we need to AND 0x00FFFF with the register
      //
      // To turn on blue, we just OR the register with 0xFF
      // To turn blue off, AND the register with 0xFFFF00
      //
      // To set values other than full on / off, we first need to turn off the color and then OR the new value
      display.clearDisplay();
      display.setCursor(0,0);             // Start at top-left corner
      display.setTextSize(1);             // Draw 2X-scale text
      display.setTextColor(SH110X_WHITE);

      const char* redLED = doc["control"]["redLED"];
      if (redLED){
        if (strcmp(redLED, LEDon) == 0){   
          color = color | (0xFF<<16);
          strip.setPixelColor(0, color);
          Serial.println("Red LED On!");
          display.println("LED:");
          display.println("Red On");
        } 
        if (strcmp(redLED, LEDoff) == 0){
          color = color & 0x00FFFF;
          strip.setPixelColor(0, color);
          Serial.println("Red LED Off!");
          display.println("LED:");
          display.println("Red Off");
        } 
      }
 
      const char* greenLED = doc["control"]["greenLED"];
      if (greenLED){
        if (strcmp(greenLED, LEDon) == 0){
          color = color | (0xFF<<8);
          strip.setPixelColor(0, color);
          Serial.println("Green LED On!");
          display.println("LED:");
          display.println("Green On");
        }
        if (strcmp(greenLED, LEDoff) == 0){
          color = color & 0xFF00FF;
          strip.setPixelColor(0, color);
          Serial.println("Green LED Off!");
          display.println("LED:");
          display.println("Green Off");
        }
      }
      
      const char* blueLED = doc["control"]["blueLED"];
      if (blueLED){
        if (strcmp(blueLED, LEDon) == 0){
          color = color | (0xFF);
          strip.setPixelColor(0, color);
          Serial.println("Blue LED On!");
          display.println("LED:");
          display.println("Blue On");
        }
        if (strcmp(blueLED, LEDoff) == 0){
          color = color & 0xFFFF00;
          strip.setPixelColor(0, color);
          Serial.println("Blue LED Off!");
          display.println("LED:");
          display.println("Blue Off");
        }
      }

      const char* relay0 = doc["control"]["relay0"];
      if (relay0){
        if (strcmp(relay0, LEDon) == 0){
          digitalWrite(relay, HIGH);
          digitalWrite(greenLEDpin, HIGH);
          digitalWrite(redLEDpin, LOW);
          Serial.println("Relay On!");
          display.println("Relay:");
          display.println("On");
        }
        if (strcmp(relay0, LEDoff) == 0){
          digitalWrite(relay, LOW);
          digitalWrite(greenLEDpin, LOW);
          digitalWrite(redLEDpin, HIGH);
          Serial.println("Relay Off!");
          display.println("Relay:");
          display.println("Off");
        }
      }

      display.display();
      strip.show(); 
   }
      //webSocket.sendTXT(num, payload);  
      webSocket.broadcastTXT(payload);
    }
      break;


    // For everything else: do nothing
    case WStype_BIN:
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
    default:
      break;
  }
}

void setup() {

  // Start Serial port
  Serial.begin(115200);

  Wire.begin(SDA, SCL);
  
  display.begin(0x3D, true); // Address 0x3D default
  display.display();
  display.clearDisplay();
  // initialize digital pin LED_BUILTIN as an output.
  //pinMode(4, INPUT);
  //pinMode(LED_BUILTIN, OUTPUT);
  pinMode(redLEDpin, OUTPUT);
  pinMode(greenLEDpin, OUTPUT);
  pinMode(blueLEDpin, OUTPUT);
  pinMode(LDO2enable, OUTPUT);
  pinMode(button, INPUT_PULLUP);
  pinMode(relay, OUTPUT);

  digitalWrite(redLEDpin, HIGH);
  digitalWrite(greenLEDpin, HIGH);
  digitalWrite(blueLEDpin, HIGH);
  delay(100);
  digitalWrite(LDO2enable, HIGH);
  delay(100);

  // Connect to access point
  Serial.println("Connecting");
  WiFi.begin(ssid, password);
  
  while ( WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
  }

  // Print our IP address
  Serial.println("Connected!");
  Serial.print("My IP address: ");
  Serial.println(WiFi.localIP());

  // Start WebSocket server and assign callback
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp32.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    if (!MDNS.begin("esp32")) {
        Serial.println("Error setting up MDNS responder!");
        while(1) {
            delay(1000);
        }
    }
    Serial.println("mDNS responder started");

        // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);

strip.begin();
strip.clear();

}

void loop() {
  // If button is pressed, we want to send a websocket message with JSON
  // webSocket.broadcastTXT(payload);
  // {"event":{"button0":"pressed"}}

  //Serial.println(analogRead(4));
  //if (analogRead(4) < 1000) { //Until we get a button wired in, we're using the on-board ambient light sensor as an input for this testing.
  //  messageCounter++;
     
    if (!digitalRead(button)) {
      delay(250);
      //webSocket.broadcastTXT("Hey! Who turned out the lights?!");
      webSocket.broadcastTXT("{\"event\":{\"button0\":\"pressed\"}}");
      //messageCounter = 0;
    }
  // Look for and handle WebSocket data
  webSocket.loop();
  
}