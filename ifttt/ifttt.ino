#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <FS.h>   // Include the SPIFFS library
#include <ESP8266Webhook.h>
#include <WebSocketsServer.h>
#include <Hash.h>

ESP8266WiFiMulti wifiMulti;     // Create an instance of the ESP8266WiFiMulti class, called 'wifiMulti'
ESP8266WebServer server(80);    // Create a webserver object that listens for HTTP request on port 80

String getContentType(String filename); // convert the file extension to the MIME type
bool handleFileRead(String path);       // send the right file to the client (if it exists)

const int buttonPin=D4;
int buttonPushCounter = 0;   // counter for the number of button presses
int buttonState = 1;         // current state of the button
int lastButtonState = 1;     // previous state of the button

#define api_key "*******************************"      //Your Webhook key
#define ifttt_event "***********" //Your IFTTT event name

Webhook webhook(api_key, ifttt_event);        //Create an object for IFTTT webhook

WebSocketsServer webSocket = WebSocketsServer(81);  //websocket connection

#define USE_SERIAL Serial1

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {

    switch(type) {
        case WStype_DISCONNECTED:
            USE_SERIAL.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket.remoteIP(num);
                USE_SERIAL.printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        
            }
            break;
        case WStype_TEXT:
            USE_SERIAL.printf("[%u] get Text: %s\n", num, payload);
                         
            break;
        case WStype_BIN:
            USE_SERIAL.printf("[%u] get binary length: %u\n", num, length);
            hexdump(payload, length);

            // send message to client
            // webSocket.sendBIN(num, payload, length);
            break;
    }

}

void setup() {
  Serial.begin(115200);         // Start the Serial communication to send messages to the computer
  delay(10);
  Serial.println('\n');
  pinMode(buttonPin, INPUT_PULLUP);

  wifiMulti.addAP("************", "*****");   // add Wi-Fi networks you want to connect to
  


  Serial.println("Connecting ...");
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect
    delay(250);
    Serial.print('.');
  }
  Serial.println('\n');
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());              // Tell us what network we're connected to
  Serial.print("IP address:\t");
  Serial.println(WiFi.localIP());           // Send the IP address of the ESP8266 to the computer

  if (MDNS.begin("esp8266")) {              // Start the mDNS responder/webserver using esp8266.local instead of ip
    Serial.println("mDNS responder started");
  } else {
    Serial.println("Error setting up MDNS responder!");
  }

  SPIFFS.begin();                           // Start the SPI Flash Files System
  
  server.onNotFound([]() {                              // If the client requests any URI
    if (!handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();                           // Actually start the server
  Serial.println("HTTP server started");
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}

void loop(void) {
      webSocket.loop();
      server.handleClient();
      buttonState = digitalRead(buttonPin);
        // compare the buttonState to its previous state
      if (buttonState != lastButtonState) {
        if(digitalRead(buttonPin)==LOW){  //press the button
           buttonPushCounter++;
           // send data to all connected clients
          webSocket.broadcastTXT(String(buttonPushCounter).c_str());
          
           Serial.print("number of button pushes: ");
            Serial.println(buttonPushCounter);
         } 
          // Delay a little bit to avoid bouncing
        delay(50);
      }
        lastButtonState = buttonState;
        if (buttonPushCounter ==5) {                  
          
          int request = webhook.trigger(String(buttonPushCounter));                 //Trigger with 1 value
          if(request == 200){ //If http_code is 200, then print "Triggered successfully"
            Serial.println("ifttt webhook Triggered successfully!");
          }
          else{
            Serial.println("ifttt webhook Failed");
          }
          buttonPushCounter=0;
      }
   
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  return "text/plain";
}

bool handleFileRead(String path) { // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    buttonPushCounter=0;
    webSocket.broadcastTXT(String(buttonPushCounter).c_str());
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false;                                         // If the file doesn't exist, return false
}
