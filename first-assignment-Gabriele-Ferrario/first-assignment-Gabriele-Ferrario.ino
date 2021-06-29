// InfluxDB library
#include <InfluxDbClient.h>
// include WiFi library
#include <ESP8266WiFi.h>
#include "secrets.h"

#include <DHT.h>

// display
#include <LiquidCrystal_I2C.h>   // display library
#include <Wire.h>                // I2C library


// include Web server library
#include <ESP8266WebServer.h>

WiFiClient client;

ESP8266WebServer server(80);   // HTTP server on port 80

// led esterno
#define LED_EXTERNAL D6   // LED pin (D0 and D4 are already used by board LEDs)
#define LED_EXTERNAL2 D7 
#define LED BUILTIN_LED   // LED pin

// photoresistor
#define PHOTORESISTOR A0              // photoresistor pin
#define PHOTORESISTOR_THRESHOLD 128   // turn led on for light values lesser than this
// tilt
#define TILT D5           // SW-520D pin, eg. D1 is GPIO5 and has optional internal pull-up


#define DISPLAY_CHARS 16    // number of characters on a line
#define DISPLAY_LINES 2     // number of display lines
#define DISPLAY_ADDR 0x27   // display address on I2C bus

LiquidCrystal_I2C lcd(DISPLAY_ADDR, DISPLAY_CHARS, DISPLAY_LINES);   // display object

// WiFi cfg
char ssid[] = SECRET_SSID;   // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password
// DHT sensor
#define DHTPIN D3       // sensor I/O pin, eg. D3 (DO NOT USE D0 or D4! see above notes)
#define DHTTYPE DHT11   // sensor type DHT 11
DHT dht = DHT(DHTPIN, DHTTYPE);

// InfluxDB cfg
InfluxDBClient client_idb(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);
Point pointDevice("device_status");

void setup() {
  Serial.begin(115200);
  Serial.println(F("\n\nCheck LCD connection..."));
  // set TILT pin as input with pull-up
  pinMode(TILT, INPUT_PULLUP);  
  // display
  Wire.begin();
  Wire.beginTransmission(DISPLAY_ADDR);
  byte error = Wire.endTransmission();

  if (error == 0) {
    Serial.println(F("LCD found."));
    lcd.begin(DISPLAY_CHARS, 2);   // initialize the lcd

  } else {
    Serial.print(F("LCD not found. Error "));
    Serial.println(error);
    Serial.println(F("Check connections and configuration. Reset to try again!"));
    while (true)
      delay(100);
  }
  dht.begin();
  WiFi.mode(WIFI_STA);
  
  // set LED pin as outputs
  pinMode(LED, OUTPUT);
  // turn led off
  digitalWrite(LED, HIGH);

  pinMode(LED_EXTERNAL, OUTPUT);
  pinMode(LED_EXTERNAL2, OUTPUT);

  WiFi.mode(WIFI_STA);
  server.on("/", handle_root);
  server.on("/Reset", handle_ledoff);
  server.onNotFound(handle_NotFound);

  server.begin();
  Serial.println(F("HTTP server started"));
  
  Serial.println(F("\n\nSetup completed.\n\n"));
}

static int show = 0;
static bool led_external_status = HIGH;
static bool led_external2_status = HIGH;
static int check_temp = 0;
static int check_tilt = 0;
static int check_light = 0;
static int check_wifi = 0;
float h = 0;
long rssi = 0;
String tilt = "";
float hic = 0;
unsigned int lightSensorValue = 0;

void loop() {
  
  int static init_db = 0;
  String IP_local = "No connection";
  
  digitalWrite(BUILTIN_LED, LOW);
  
  if (show == 0) {
    lcd.setBacklight(255);    // set backlight to maximum
    lcd.home();               // move cursor to 0,0
    lcd.clear();              // clear text
    lcd.print("Hi! :)");   // show text
    delay(100);

    // blink backlight
    lcd.setBacklight(0);   // backlight off
    delay(40);
    lcd.setBacklight(255);

    IP_local = connectionToWiFi();
    Serial.println(IP_local);

    digitalWrite(LED_EXTERNAL, led_external_status);
    digitalWrite(LED_EXTERNAL2, led_external2_status);
  
  } else {

    // reading temperature or humidity takes about 250 milliseconds!
    h = dht.readHumidity();      // humidity percentage, range 20-80% (±5% accuracy)
    float t = dht.readTemperature();   // temperature Celsius, range 0-50°C (±2°C accuracy)
    rssi = connectionToWiFi();
    byte val = digitalRead(TILT);   // read the SW-520D state
    tilt = "";
    
    if (val == HIGH) {
     
      tilt = "TILTED";
      check_tilt = 1;
      
    } else {
     
      tilt = "NOT TILTED";
    }
    
    if (isnan(h) || isnan(t)) {   // readings failed, skip
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }
     
     
    // compute heat index in Celsius (isFahreheit = false)
    hic = dht.computeHeatIndex(t, h, false);
    lightSensorValue = analogRead(PHOTORESISTOR);
    
    
     if (lightSensorValue < 200) {
      check_light = 1;  
     } else if(lightSensorValue > 1000){
      check_light = 2;
     }  
     
     if (hic < 10){
      check_temp = 1;
     }
     else if (hic > 30){
      check_temp = 2;
     }
     if(rssi <-75){
      check_wifi = 1;
     }
     
    lcd.setCursor(0, 0);
    lcd.print("H: ");
    lcd.print(int(h));
    lcd.print("% T:");
    lcd.print(hic);
    lcd.print(" C");
    lcd.setCursor(0, 1);
    lcd.print("WiFi:");
    lcd.print(rssi);
    lcd.print(" L:");
    lcd.print(lightSensorValue);

    server.handleClient();   // listening for clients on port 80
 
    check_influxdb();
    
    if (init_db == 0) {   // set tags
        pointDevice.addTag("device", "ESP8266");
        pointDevice.addTag("Home", "Home_monitoring");
        init_db = 1;
      }
      
    WriteMultiToDB(ssid, (int)rssi, h, hic, tilt, lightSensorValue, check_temp, check_tilt, check_light, check_wifi, led_external_status, led_external2_status);   // write on MySQL table if connection works
    
    if (check_temp == 0 & check_tilt == 0 & check_light == 0 & check_wifi == 0){
      led_external2_status = LOW;
      digitalWrite(LED_EXTERNAL2, led_external2_status);
      led_external_status = HIGH;
      digitalWrite(LED_EXTERNAL, led_external_status);
    }
    else{
      led_external_status = LOW;
      digitalWrite(LED_EXTERNAL, led_external_status);
      led_external2_status = HIGH;
      digitalWrite(LED_EXTERNAL2, led_external2_status);
    }
  }

  show = 1;
  delay(1000);
}

long connectionToWiFi() {

  long rssi;
  // connect to WiFi (if not already connected)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("Connecting to SSID: "));
    Serial.println(SECRET_SSID);

    WiFi.begin(ssid, pass);
    while (WiFi.status() != WL_CONNECTED) {
      Serial.print(F("."));
      delay(500);
    }
    Serial.println(F("\nConnected!"));

    rssi = WiFi.RSSI();
    return rssi;
  }
  else {
    rssi = WiFi.RSSI();
    return rssi;
  }
}

void check_influxdb() {
  // Check server connection
  if (client_idb.validateConnection()) {
    Serial.print(F("Connected to InfluxDB: "));
    Serial.println(client_idb.getServerUrl());
  } else {
    Serial.print(F("InfluxDB connection failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

int WriteMultiToDB(char ssid[], int rssi,  float h, float hic, String tilt, int lightSensorValue, int check_temp, int check_tilt, int check_light, int check_wifi, int led_external_status, int led_external2_status) {

  // Store measured value into point
  pointDevice.clearFields();
  // Report RSSI of currently connected network
  pointDevice.addField("rssi", rssi);
  pointDevice.addField("humidity", h);
  pointDevice.addField("temperature", hic);
  pointDevice.addField("tilt", tilt);
  pointDevice.addField("light", lightSensorValue);
  pointDevice.addField("check_temperature", check_temp);
  pointDevice.addField("check_tilt", check_tilt);
  pointDevice.addField("check_light", check_light);
  pointDevice.addField("check_wifi", check_wifi);
  pointDevice.addField("led_external_status", led_external_status);
  pointDevice.addField("led_external2_status", led_external2_status);

  Serial.print(F("Writing: "));
  Serial.println(pointDevice.toLineProtocol());
  if (!client_idb.writePoint(pointDevice)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void handle_root() {
  server.send(200, F("text/html"), SendHTML());
}

void handle_ledoff() {
  reset_status();
  server.send(200, F("text/html"), SendHTML());
}

void handle_NotFound() {
  server.send(404, F("text/plain"), F("Not found"));
}

void reset_status() {
  Serial.println(F("RESET!")); 
  check_temp=0; 
  check_tilt = 0; 
  check_light = 0;
  check_wifi = 0;
}


String SendHTML() {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head>\n";
  ptr += "<link rel=\"stylesheet\" href=\"https://code.getmdl.io/1.3.0/material.blue-red.min.css\" />\n";
  ptr += "<title>Home Monitoring</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;} h3 {color: #444444;margin-bottom: 50px;}\n";
  ptr += ".button {display: block;width: 80px;background-color: #1abc9c;border: none;color: white;padding: 13px 30px;text-decoration: none;font-size: 25px;margin: 0px auto 35px;cursor: pointer;border-radius: 4px;}\n";
  ptr += ".button-on {background-color: #1abc9c;}\n";
  ptr += ".button-on:active {background-color: #16a085;}\n";
  ptr += ".button-off {background-color: #ff4133;}\n";
  ptr += ".button-off:active {background-color: #d00000;}\n";
  ptr += "p {font-size: 14px;color: #888;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<h1 align=\"center\">Home monitoring </h1>\n";
  ptr += "<table align=\"center\" class=\"mdl-data-table mdl-js-data-table mdl-shadow--4dp\" id=\"table\">\n";
  ptr += "<thead>\n";
  ptr += "<tr>\n";
  String temp_color="black";
  if (check_temp > 0){
    temp_color="red";
   }
  ptr += "<th><font color="+temp_color+">Temperature</th>\n";
  ptr += "<th><font color=\"black\">Humidity</th>\n";
  String light_color="black";
  if (check_light > 0){
    light_color="red";
   }
  ptr += "<th><font color="+light_color+">Light</th>\n";
  String wifi_color="black";
  if (check_wifi == 1){
    wifi_color="red";
   }
  ptr += "<th><font color="+wifi_color+">Wi-Fi</th>\n";
  String tilt_color="black";
  if (check_tilt == 1){
    tilt_color="red";
   }
  ptr += "<th><font color="+tilt_color+">Tilt</th>\n";
  ptr += "</tr>\n";
  ptr += "</thead>\n";
  ptr += "<tr>\n";
  ptr +=  "<td id=temperature>"+  String(hic) +" &#8451;</td>\n";
  ptr +=  "<td id=humidity>"+  String(h) +" %</td>\n";
  ptr +=  "<td id=light>"+  String(lightSensorValue) +"</td>\n";
  ptr +=  "<td id=wifi>"+  String(rssi) +"</td>\n";
  ptr +=  "<td id=tilt>"+  String(tilt) +"</td>\n";
  ptr += "</tr>\n";
  ptr += "</table>\n";
  ptr += "<div align=\"center\" style=\"margin-bottom: 25px; margin-top: 20px;\">\n";
  if (check_temp > 0 | check_tilt == 1 | check_light > 0 | check_wifi == 1) {
    ptr += "<image id=\"alarm\" src=\"https://icons.iconarchive.com/icons/gakuseisean/ivista-2/128/Alarm-Error-icon.png\" height=\"100\" width=\"100\"/>\n";
    ptr += "<a class=\"button button-off\" href=\"/Reset\" style=\"margin-top: 15px;\">Reset</a>\n";
  } else {
    ptr += "<image id=\"alarm\" src=\"https://icons.iconarchive.com/icons/gakuseisean/ivista-2/128/Alarm-Tick-icon.png\" height=\"100\" width=\"100\"/>\n";
  }
  ptr += "<a class=\"button button-on\" href=\"/\"  style=\"margin-top: 15px;\">Refresh</a>\n";
  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
