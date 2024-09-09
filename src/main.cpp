/*
Developed on
SparkFun Thing Plus ESP32 WROOM (USB-C): https://www.sparkfun.com/products/20168
SparkFun Qwiic Button: https://www.sparkfun.com/products/16842
SparkFun Qwiic OLED (1.3", 128x64): https://www.sparkfun.com/products/23453

I2C device found at address 0x11 greenButton
I2C device found at address 0x20 redButton
I2C device found at address 0x36 
I2C device found at address 0x3D

Note: If code-upload fails, reboot board while pressing BOOT button
*/

#include <Arduino.h>
#include <Wire.h>
#include <SparkFun_Qwiic_OLED.h>
#include <SparkFun_Qwiic_Button.h>
#include "CountDown.h"
#include "i2c-scanner.h"
#include <WiFi.h>
#include <HTTPClient.h>

#include <res/qw_fnt_5x7.h> // QW_FONT_5X7
#include <res/qw_fnt_8x16.h> // QW_FONT_8X16
#include <res/qw_fnt_7segment.h> // QW_FONT_7SEGMENT
#include <res/qw_fnt_largenum.h> // QW_FONT_LARGENUM
#include <res/qw_fnt_31x48.h> // QW_FONT_31X48
 
const char* ssid = "v38o";
const char* password = "00lesswiresOK";

Qwiic1in3OLED myOLED;
QwiicButton greenButton; 
QwiicButton redButton;

CountDown CD;
HTTPClient http;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Connected to AP successfully!");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("Disconnected from WiFi access point");
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.wifi_sta_disconnected.reason);
  Serial.println("Trying to Reconnect");
  WiFi.begin(ssid, password);
}


int duration = 10;
int clicks = 0;
int prev_clicks = 0;
int prev_remaining = 0;
int state = 1; // 0:restart, 1:register clicks, 200+:save game, 300+: qr halloj?
 
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(115200);
  Serial.println("Starting speedclick");

  // clear and setup wifi
  WiFi.disconnect(true);
  delay(1000);
  WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFI");
  Serial.println(ssid);

  Wire.begin(); //Join I2C bus

  // Initalize the OLED device and related graphics system
  if (myOLED.begin() == false)
  {
      Serial.println("Qwiic1in3OLED failed. Freezing...");
      while (true)
          ;
  }
  Serial.println("Qwiic1in3OLED available");
  // check https://docs.sparkfun.com/SparkFun_Qwiic_OLED_Arduino_Library/api_draw/#setfont
  // myOLED.setFont(QW_FONT_8X16);
  myOLED.flipVertical(true);
  myOLED.flipHorizontal(true);
  myOLED.text(4, 4, "Scan QR code begin", 1); // 0=black, 1=white
  myOLED.display();

  //check if the buttons will acknowledge over I2C
  //connect to Qwiic button at address 0x5B
  if (greenButton.begin(0x11) == false){
    Serial.println("greenButton did not acknowledge! Freezing.");
    while(1);
  }
  //connect to Qwiic button at default address, 0x6F
  if (redButton.begin(0x20) == false) {
    Serial.println("Button 2 did not acknowledge! Freezing.");
    while(1);
  }
  Serial.println("Both buttons connected.");

  state = 0;
}
 
void loop()
{
  // i2c_scan_loop();
  
  prev_clicks = clicks;
  int remaining = CD.remaining();
  // Serial.println("00 remaining:" + String(remaining));

  if( state == 0 ){
    Serial.println("Restarting (state=0)");
    state = 1;
    clicks = 0;
    remaining = duration;
    CD.start(0, 0, 0, duration);
    delay(10);
  }

  if( state == 1 && remaining == 0 ){
    Serial.println("Timeout! Score: "+ clicks);
    if( clicks > 0 ){
      state = 201;
    }else{
      Serial.println("Timeout! Score: "+ String(clicks) +" - not saving");
      myOLED.erase();
      myOLED.text(4, 4, "Click GREEN btn", 1);
      myOLED.display();
      delay(1000);
      state = 0;
    }
  }

  //check if button 2 is pressed, and tell us if it is!
  if (redButton.isPressed() == true){
    Serial.println("redButton pressed");
    while (redButton.isPressed() == true)
      delay(10);  //wait for user to stop pressing
    Serial.println("redButton released");

    Serial.println("new game, reset btn");
    delay(1000);
    state = 0; // restart
    // state = 201;
  }

  // save game
  if( state > 200 ){
    
    if( state == 201 ){
      Serial.println("Saving");
      myOLED.erase();
      myOLED.text(4, 4, "Saving...", 1);
      myOLED.display();

      String gamename = "speedclick";
      String player = "jorgen";
      String score = String(clicks);
      // String payload = "http://127.0.0.1:5555/highscores/add?game="+ gamename +"&player="+ player +"&score="+ score;
      String payload = "http://192.168.7.87:5555/highscores/add?game="+ gamename +"&player="+ player +"&score="+ score;
      Serial.println("payload:"+ payload);

      if(WiFi.status() == WL_CONNECTED){
        myOLED.erase();
        myOLED.text(4, 4, "Talking...", 1);
        myOLED.display();
        http.begin(payload);
        state = 202;
      }else{
        myOLED.erase();
        myOLED.text(4, 4, "No WIFI :-(", 1);
        myOLED.display();
      }
    }

    if( state == 202 ){
      int httpCode = http.GET();
      if (httpCode > 0) {
        String response = http.getString();
        Serial.println("httpCode:"+ httpCode);
        Serial.println("response:"+ response);
        myOLED.erase();
        myOLED.text(4, 4, "Saved", 1);
        myOLED.display();
        http.end();
        delay(1000);
        Serial.println("new game, post save");
        state = 0;
      } else {
        Serial.println("Error on HTTP request");
        myOLED.erase();
        myOLED.text(4, 4, "Net Error: "+ String(httpCode), 1);
        myOLED.display();
        http.end();
        delay(1000);
        Serial.println("new game, post error");
        state = 0;
      }
    }
  }
  
  

  // game loop
  if( state == 1 ){

    //check if button 1 is pressed, and tell us if it is!
    if (greenButton.isPressed() == true) {
      Serial.println("greenButton pressed");
      digitalWrite(LED_BUILTIN, HIGH);
      clicks++;
      while (greenButton.isPressed() == true)
        delay(10);  //wait for user to stop pressing
      Serial.println("greenButton released");
      digitalWrite(LED_BUILTIN, LOW);
    }

    if( remaining != prev_remaining || clicks != prev_clicks ){
      prev_remaining = remaining;
      // Update display
      myOLED.erase();

      // progress bar
      int w = 128 * (remaining / (duration + 0.0));
      // Serial.print("remaining");
      // Serial.print(remaining);
      // Serial.print(", fraction:");
      // Serial.print(remaining / 60.0);
      // Serial.print(", w:");
      // Serial.println(w);
      myOLED.rectangleFill(0, 64-32, w, 32, 1);

      // https://docs.sparkfun.com/SparkFun_Qwiic_OLED_Arduino_Library/api_draw/#getdrawmode
      myOLED.setDrawMode(grROPXOR); 

      myOLED.setFont(QW_FONT_7SEGMENT);
      // myOLED.setFont(QW_FONT_LARGENUM);
      // myOLED.setFont(QW_FONT_31X48);
      myOLED.text(4, 4, ""+String(clicks), 1);
      // myOLED.text(4, 4, "Clicks: "+ String(clicks), 1);

      myOLED.setFont(QW_FONT_5X7);
      myOLED.text(4, 64-16, "Remaining:"+ String(remaining), 1);

      myOLED.display();
    }
  }
  delay(20); // 2
}
