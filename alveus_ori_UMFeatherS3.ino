/*

***************************************************************************************************

WebSocket and mDNS code sourced from examples in Arduino IDE and then modified for purpose.

You will have to create arduino_secrets.h to store the Wifi Credentials (if using Wifi) with these two definitions:

#define SECRET_SSID "WifiSSID";
#define SECRET_PASS "WifiPassword";

***************************************************************************************************


*********** ToDo ***********
- Add Support for the Ethernet Wing (This is a lot more difficult than expected and probably won't happen)
- Add support for the music wing so a JSON command received through websocket can trigger a file playback
  - This may be more difficult as we don't want it blocking while playing the file and the current library doesn't allow interrupt based playback on ESP32
  - It might be worth having a separate Feather (like the RP2040 Feather) to handle the music playback, with basic digital IO interface back to the FeatherS3
- Add support for a magnetic door contact sensor


********** Bugs ***********
- If you send an empty string (maybe a CR or LF) to the websocket, the program will crash as soon as it tries to do a Serial.print() with that data. 


*********** Expected JSON from the Enterprise Automation System ***********

    {
      "control": {
        "redOnboardLED": "on",
        "greenOnboardLED": "off",
        "blueOnboardLED": "off",
        "redButtonLED": "off",
        "greenButtonLED": "off",
        "blueButtonLED": "off",
        "relay0": "off",
        "display0": "Test Display Message"
      }
    }

- XXXOnboardLED is the NeoPixel that is on the FeatherS3 itself
- XXXButtonLED is the LED ring of the external push button
- relay0 is the relay wing
- display0 is the OLED display (128x128) connected via I2C STEMMA QT connector

*/


#include <WiFi.h> // Base wifi
#include <WebSocketsServer.h> // For all things websocket
#include <ESPmDNS.h> // For mDNS service to make finding the FeatherM3 on the network easier
#include <WiFiClient.h> // To connect to the Wifi network
#include <ArduinoJson.h> // For handling all things JSON
#include <Adafruit_NeoPixel.h> // For the on-board NeoPixel
#include <SPI.h> // For Ethernet Wing (not currently used)
#include <Wire.h> // For the display
#include <Adafruit_GFX.h> // For the display
#include <Adafruit_SH110X.h> //For the Display
#include "arduino_secrets.h"

/* For the On-Board NeoPixel and Library*/
#define NUMPIXELS 1
#define PIN 40

/* For the STEMMA QT connector the display uses */
#define SDA 8
#define SCL 9

/* IO Pins connected to the various external devices */
#define redLEDpin 18
#define greenLEDpin 14
#define blueLEDpin 12
#define LDO2enable 39
#define button 17
#define relay 11

/* OLED Screen */
#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 128 // OLED display height, in pixels
#define OLED_RESET -1     // Can set an oled reset pin if desired

// Constants
const char* ssid = SECRET_SSID; // Defined in arduino_secrets.h
const char* password = SECRET_PASS; // Defined in arduino_secrets.h
const char* LEDon = "on";
const char* LEDoff = "off";

// Globals
WebSocketsServer webSocket = WebSocketsServer(80);

Adafruit_SH1107 display = Adafruit_SH1107(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET, 1000000, 100000);

Adafruit_NeoPixel strip(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

/**********************/
/* Start of functions */
/**********************/

// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

  // Figure out the type of WebSocket event
  switch(type) {

    // Client has disconnected
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Websocket Disconnected!\n", num);
      break;

    // New client has connected
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Websocket Connection from ", num);
        Serial.println(ip.toString());
      }
      break;

    // Echo text message back to client
    case WStype_TEXT:{

    Serial.printf("\n\n[%u] Text from Websocket: %s\n", num, payload);

  
    StaticJsonDocument<200> doc; // Set aside a block of memory for the object
    DeserializationError error = deserializeJson(doc, (char *)payload); // Check and see if we got valid JSON from the websocket data

    /* If we get back invalid JSON, we want to skip trying to parse it, otherwise let's see if it has something we want */
   if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
   } else {

      uint32_t color = strip.getPixelColor(0); // We need to get the NeoPixel color so we don't overwrite an existing color if we only get 1 or 2 color keys back in the JSON file

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
      
      /* Before we do any further parsing, let's clear the display and put the cursor in the upper left corner in case we need to show something on the screen*/
      display.clearDisplay();
      display.setCursor(0,0);             // Start at top-left corner
      display.setTextSize(2);             // Draw 2X-scale text
      display.setTextColor(SH110X_WHITE);

      
      /* Up to this point we still don't know what's in the JSON, only that the formatting is valid. */
      /* We will attempt to read for our known Key values, and then check to see if we get something good back or just NULL */
      /* If we get a NULL, then we skip this key, otherwise we process it. */
      
      const char* redOnboardLED = doc["control"]["redOnboardLED"]; // Read a defined key value
      if (redOnboardLED){ // Test to see if it's NULL or has a value
        if (strcmp(redOnboardLED, LEDon) == 0){ // Check to see if the value is "on"
          color = color | (0xFF<<16); // We found "on" so let's set the color via bit masking
          strip.setPixelColor(0, color); // Write the new color to the NeoPixel
          Serial.println("Red Onboard LED On!");
        } 
        if (strcmp(redOnboardLED, LEDoff) == 0){ // Check to see if the value is "off"
          color = color & 0x00FFFF; // We found "off" so we need to remove the bits via bit masking
          strip.setPixelColor(0, color); // Write the new color to the NeoPixel
          Serial.println("Red Onboard LED Off!");
        } 
      }
 
      const char* greenOnboardLED = doc["control"]["greenOnboardLED"];
      if (greenOnboardLED){
        if (strcmp(greenOnboardLED, LEDon) == 0){
          color = color | (0xFF<<8);
          strip.setPixelColor(0, color);
          Serial.println("Green Onboard LED On!");
        }
        if (strcmp(greenOnboardLED, LEDoff) == 0){
          color = color & 0xFF00FF;
          strip.setPixelColor(0, color);
          Serial.println("Green Onboard LED Off!");
        }
      }
      
      const char* blueOnboardLED = doc["control"]["blueOnboardLED"];
      if (blueOnboardLED){
        if (strcmp(blueOnboardLED, LEDon) == 0){
          color = color | (0xFF);
          strip.setPixelColor(0, color);
          Serial.println("Blue Onboard LED On!");
        }
        if (strcmp(blueOnboardLED, LEDoff) == 0){
          color = color & 0xFFFF00;
          strip.setPixelColor(0, color);
          Serial.println("Blue Onboard LED Off!");
        }
      }

      /* For the push button LED ring, we do the same thing, but we have to manipulate the IO pins to turn colors on and off this time. */
      const char* redButtonLED = doc["control"]["redButtonLED"];
      if (redButtonLED){
        if (strcmp(redButtonLED, LEDon) == 0){   
          digitalWrite(redLEDpin, LOW); // The colors are Active Low
          Serial.println("Red Button LED On!");
        } 
        if (strcmp(redButtonLED, LEDoff) == 0){
          digitalWrite(redLEDpin, HIGH);
          Serial.println("Red Button LED Off!");
        } 
      }

      const char* greenButtonLED = doc["control"]["greenButtonLED"];
      if (greenButtonLED){
        if (strcmp(greenButtonLED, LEDon) == 0){   
          digitalWrite(greenLEDpin, LOW);
          Serial.println("Green Button LED On!");
        } 
        if (strcmp(greenButtonLED, LEDoff) == 0){
          digitalWrite(greenLEDpin, HIGH);
          Serial.println("Green Button LED Off!");
        } 
      }

      const char* blueButtonLED = doc["control"]["blueButtonLED"];
      if (blueButtonLED){
        if (strcmp(blueButtonLED, LEDon) == 0){   
          digitalWrite(blueLEDpin, LOW);
          Serial.println("Blue Button LED On!");
        } 
        if (strcmp(blueButtonLED, LEDoff) == 0){
          digitalWrite(blueLEDpin, HIGH);
          Serial.println("Blue Button LED Off!");
        } 
      }

      /* Now to check and see if there was a relay command. */
      const char* relay0 = doc["control"]["relay0"];
      if (relay0){
        if (strcmp(relay0, LEDon) == 0){ // LEDon contains the "on" text only, so it's only being used for comparison here.
          digitalWrite(relay, HIGH);
          digitalWrite(greenLEDpin, HIGH);
          digitalWrite(redLEDpin, LOW);
          Serial.println("Relay On!");
        }
        if (strcmp(relay0, LEDoff) == 0){
          digitalWrite(relay, LOW);
          digitalWrite(greenLEDpin, LOW);
          digitalWrite(redLEDpin, HIGH);
          Serial.println("Relay Off!");
        }
      }

      /* Time to see if there is a key value pair for our display */
      const char* display0 = doc["control"]["display0"];
      if (display0){
        display.println(display0); // Put the found information in the buffer, we have to call display.display() to actually show it on screen.
      }


      display.display(); // Write the buffer to the display
      strip.show(); // Push the new LED color to the NeoPixel
    }
      //webSocket.sendTXT(num, payload);  
      webSocket.broadcastTXT(payload); // We echo back the JSON we received
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

     
  if (!digitalRead(button)) {
    
    StaticJsonDocument<200> senddoc;
    delay(250);
    //webSocket.broadcastTXT("Hey! Who turned out the lights?!");
    senddoc["event"]["button0"] = "pressed";
    serializeJson(senddoc, Serial);
    String json_string;
    serializeJson(senddoc, json_string);
    webSocket.broadcastTXT(json_string);
    //webSocket.broadcastTXT("{\"event\":{\"button0\":\"pressed\"}}");
  }
  
  if ( WiFi.status() != WL_CONNECTED){
    Serial.println("Lost Wifi");
    display.println("Lost Wifi");
    display.display();
    WiFi.begin(ssid, password);
    while ( WiFi.status() != WL_CONNECTED ) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Wifi Recon");
    display.println("Wifi Recon");
    display.display();
  }
  // Look for and handle WebSocket data
  webSocket.loop();
  
}