#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include "arduino_secrets.h"

// Modified 21AUG2023

// Constants
const char* ssid = SECRET_SSID; // Defined in arduino_secrets.h
const char* password = SECRET_PASS; // Defined in arduino_secrets.h

// Globals
WebSocketsServer webSocket = WebSocketsServer(80);
int messageCounter = 0;

// Called when receiving any WebSocket message
void onWebSocketEvent(uint8_t num,
                      WStype_t type,
                      uint8_t * payload,
                      size_t length) {

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
    case WStype_TEXT:
      Serial.printf("[%u] Text: %s\n", num, payload);
      //webSocket.sendTXT(num, payload);
      webSocket.broadcastTXT(payload);
      break;

    // send message to client
    // webSocket.sendTXT(num, "message here");

    // send data to all connected clients
    // webSocket.broadcastTXT("message here");

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

}

void loop() {
  // If button is pressed, we want to send a websock message with JSON
  // webSocket.broadcastTXT(payload);
  // {"event":{"button0":"pressed"}}

  //Serial.println(analogRead(4));
  if (analogRead(4) < 1000) {
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