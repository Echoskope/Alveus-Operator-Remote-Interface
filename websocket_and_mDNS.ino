/*

***************************************************************************************************

WebSocket and mDNS code sourced from examples in Arduino IDE and then modified for purpose.

You will have to create arduino_secrets.h to store the Wifi Credentials (if using Wifi) with these two definitions:

#define SECRET_SSID "WifiSSID";
#define SECRET_PASS "WifiPassword";

***************************************************************************************************


*********** ToDo ***********
- Update for FeatherS3
  + Have to change the LED over to using the IO ports instead of the onboard LED (although the FeatherS3 does have one, but it's a different flavor of controller)
  + FeatherS3 also does not use the DotStar LED controller
- Add Support for the Ethernet Wing
- Add Support for Relay Wing control
- Need to investigate ArduinoJSON to see if there is a way to list all included keys before trying to read value
  + Right now the code will support websocket data that doesn't follow the JSON format as long as it isn't a return character
  + A return character from the websocket connection will cause the code to crash and the chip will reboot
  + Ideally we would first search the deserialized JSON key's, and then based on what we find read the values and act on them so we aren't trying to read non-existant key:values


*********** Expected JSON from the Enterprise Automation System ***********

    {
      "control": {
        "greenLED": "on",
        "redLED": "off",
      }
    }

*/


#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include "arduino_secrets.h"

#define NUMPIXELS 1
#define DATAPIN 40
#define CLOCKPIN 45
Adafruit_DotStar strip(NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BRG);

// Constants
const char* ssid = SECRET_SSID; // Defined in arduino_secrets.h
const char* password = SECRET_PASS; // Defined in arduino_secrets.h

// Globals
WebSocketsServer webSocket = WebSocketsServer(80);
int messageCounter = 0;

const char* LEDon = "on";
const char* LEDoff = "off";
const char* jsonGreen = "greenLED";
const char* jsonRed = "redLED";
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
    
    if ((strstr((char *)payload, jsonGreen) == NULL) || (strstr((char *)payload, jsonRed) == NULL)) {
      Serial.println("greenLED or redLED not found!");
      break;
    }
    
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, (char *)payload);

   if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
   } else {
      
      Serial.println("Test Point -1");      

      const char* greenLED = doc["control"]["greenLED"];
      const char* redLED = doc["control"]["redLED"];

      //Serial.println(greenLED);
      //Serial.println(redLED);

      // We need to pull the current LED color state and then add or remove color
      // strip.getPixelColor returns 32 bits of data, only which the low 24 bits are used
      // Here is the believed formatting:
      // 0000 0000 GGGG GGGG RRRR RRRR BBBB BBBB
      // Note the order of colors G and R and swapped (I don't know why it works this way)
      // Each R, G or B value can be up to 255
      // We can use bit masking and shifting to play with this value!
      // Green = 16 bit offset
      // Red = 8 bit offset
      // Blue = 0 bit offset
      //
      // So to turn on green, we need to shift 0xFF to the left by 16 bits and OR it with register
      // To turn off green, we need to AND 0x00FFFF with the register
      //
      // To turn on red, we shift 0xFF to the left by 8 bits and OR it with register
      // To turn off red, we need to AND 0xFF00FF with the register
      //
      // To turn on blue, we just OR the register with 0xFF
      // To turn blue off, AND the register with 0xFFFF00
      //
      // To set values other than full on / off, we first need to turn off the color and then OR the new value

      uint32_t color = strip.getPixelColor(0);
      
      if (strcmp(greenLED, LEDon) == 0){
        color = color | (0xFF<<16);
        strip.setPixelColor(0, color);
        Serial.println("Green LED On!");
      }
      if (strcmp(redLED, LEDon) == 0){
        color = color | (0xFF<<8);
        strip.setPixelColor(0, color);
        Serial.println("Red LED On!");
      } 
      if (strcmp(greenLED, LEDoff) == 0){
        color = color & 0x00FFFF;
        strip.setPixelColor(0, color);
        Serial.println("Green LED Off!");
      }
      if (strcmp(redLED, LEDoff) == 0){
        color = color & 0xFF00FF;
        strip.setPixelColor(0, color);
        Serial.println("Red LED Off!");
      } 
      
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

  pinMode(4, INPUT);

  // Start Serial port
  Serial.begin(115200);
  delay(10000);
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
  strip.show();


}

void loop() {
  // If button is pressed, we want to send a websocket message with JSON
  // webSocket.broadcastTXT(payload);
  // {"event":{"button0":"pressed"}}

  //Serial.println(analogRead(4));
  if (analogRead(4) < 1000) { //Until we get a button wired in, we're using the on-board ambient light sensor as an input for this testing.
    messageCounter++;
     
    if (messageCounter > 5000) {
      //webSocket.broadcastTXT("Hey! Who turned out the lights?!");
      webSocket.broadcastTXT("{\"event\":{\"button0\":\"pressed\"}}");
      messageCounter = 0;
    }
  }
  // Look for and handle WebSocket data
  webSocket.loop();
  
}