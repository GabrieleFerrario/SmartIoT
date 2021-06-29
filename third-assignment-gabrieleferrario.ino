#include "model.h" // Machine Learning Model (SVM)

// InfluxDB library
#include <InfluxDbClient.h>

// MySQL libraries
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>

//Json
#include <ArduinoJson.h>

// include WiFi library
#include <ESP8266WiFi.h>

// MQTT
#include <MQTT.h>

#include "secrets.h"

#include <DHT.h>

// display
#include <LiquidCrystal_I2C.h>   // display library
#include <Wire.h>                // I2C library

#include <HttpClient.h>

// include Web server library
#include <ESP8266WebServer.h>

WiFiClient client;

ESP8266WebServer server(80);   // HTTP server on port 80

// MySQL server cfg
char mysql_user[] = MYSQL_USER;       // MySQL user login username
char mysql_password[] = MYSQL_PASS;   // MySQL user login password
IPAddress server_addr(MYSQL_IP);      // IP of the MySQL *server* here
MySQL_Connection conn((Client *)&client);
char query[128];
char INSERT_DATA[] = "INSERT INTO `gferrario`.`slave` (`name`, `mac`, `status`) VALUES ('%s', '%s', '%s')";

String openWeatherMapApiKey = KEY;


unsigned long lastTime_weather = 0;
unsigned long lastTime2_weather = 0;
unsigned long lastTime_home = 0;
unsigned long lastTime2_home = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 10 seconds (10000)
unsigned long timerDelay = 10000;


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
Point pointDeviceHome("Home_Monitoring");

// MQTT data
MQTTClient mqttClient = MQTTClient(1024);
//MQTTClient mqttClient;                     // handles the MQTT communication protocol
WiFiClient networkClient;                  // handles the network connection to the MQTT broker

String topic_home = "";   // topic to control home monitoring
String topic_weather = "";   // topic to control weather monitoring

String mac;

String city;

// control flags
bool flag_weather = false;
bool flag_home = false;

long previousLCDMillis = 0;    // for LCD screen update
long lcdInterval = 5000;       // for LCD change
unsigned long currentLCDMillis = 0;


int static init_db_1 = 0;
// alert
int lower_bound_light = 0;
int upper_bound_light = 1024;
int lower_bound_temperature = -20;
int upper_bound_temperature = 50;
int lower_bound_rssi = -80;

// for the http request to openweather
String serverPath;

char name_nodemcu[2][20];
char mac_nodemcu[2][20];

int sleep_time=-1;
int execution_time=-1;
unsigned long execution_timer = 0;
void setup() {
  Serial.begin(115200);
  
  // set TILT pin as input with pull-up
  pinMode(TILT, INPUT_PULLUP);
  
  Serial.println(F("\n\nCheck LCD connection..."));
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
  //dht
  dht.begin();
  WiFi.mode(WIFI_STA);
  
  // set LED pin as outputs
  pinMode(LED, OUTPUT);
  // turn led off
  digitalWrite(LED, HIGH);
  //leds
  pinMode(LED_EXTERNAL, OUTPUT);
  pinMode(LED_EXTERNAL2, OUTPUT);
  //wifi
  WiFi.mode(WIFI_STA);

  
  // setup MQTT
  mqttClient.begin(MQTT_BROKERIP, 1883, networkClient);   // setup communication with MQTT broker
  mqttClient.onMessage(mqttMessageReceived);              // callback on message received from MQTT broker
  
  mac = WiFi.macAddress();
  
  connectToWiFi();
 
  connectToMQTTBroker();   // connect to MQTT broker (if not already connected)

  Serial.println(F("\n\nSetup completed.\n\n"));
  digitalWrite(BUILTIN_LED, LOW);
  lcd.setBacklight(255);    // set backlight to maximum
  lcd.home();               // move cursor to 0,0
  showWelcome();
  delay(100);

  // blink backlight
  lcd.setBacklight(0);   // backlight off
  delay(40);
  lcd.setBacklight(255);

  digitalWrite(LED_EXTERNAL, HIGH);
  digitalWrite(LED_EXTERNAL2, HIGH);
  
  //connectToWiFi();
  //mqttClient.loop();       // MQTT client loop
  Serial.println(sleep_time);
  Serial.println(execution_time);
  while(sleep_time < 0 && execution_time <0){
    connectToWiFi();
    mqttClient.loop();
    //yield();
  }
  bool check = 0;
  while(execution_timer < (execution_time/1000) | (check == 0 && flag_home))
  {
    check = execution_home();
    execution_weather();
   
    execution_timer = millis();
    //yield();
  }
  if(flag_home){
    const char * node = "NodeMCUHome";
    String node_mac = (mac + "_home");
    const char * modality = "Sleep";
    WriteMultiToMYSQL(node, node_mac.c_str(), modality);
  }
  if(flag_weather){
    const char * node = "NodeMCUWeather";
    String node_mac = (mac + "_weather");
    const char * modality = "Sleep";
    WriteMultiToMYSQL(node, node_mac.c_str(), modality);
  }
  Serial.print("deep sleep for ");
  Serial.print(sleep_time);
  Serial.println(" seconds");
  ESP.deepSleep(sleep_time); 
}

void loop() {
}

void connectToWiFi() {
  if (WiFi.status() != WL_CONNECTED) {   // not connected
    Serial.print(F("Attempting to connect to SSID: "));
    Serial.println(SECRET_SSID);

    while (WiFi.status() != WL_CONNECTED) {
#ifdef IP
      WiFi.config(ip, dns, gateway, subnet);
#endif
      WiFi.mode(WIFI_STA);
      WiFi.begin(SECRET_SSID, SECRET_PASS);   // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(F("."));
      delay(5000);
    }
    Serial.println(F("\nConnected"));
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

// write data to influxdb
int WriteHome(int rssi,  float h, float hic, String tilt, int lightSensorValue, int check_temp, int check_tilt, int check_light, int check_wifi, int led_external_status, 
  int led_external2_status, int lower_bound_light_db, int upper_bound_light_db, int lower_bound_temperature_db, int upper_bound_temperature_db, int lower_bound_rssi_db) {
  
  pointDeviceHome.clearFields();
  check_influxdb();

  if (init_db_1 == 0) {   // set tags
      pointDeviceHome.addTag("Home", "Home_monitoring");
      init_db_1 = 1;
      }
  
  // Store measured value into point
  pointDeviceHome.clearFields();
  // Report RSSI of currently connected network
  pointDeviceHome.addField("rssi", rssi);
  pointDeviceHome.addField("humidity", h);
  pointDeviceHome.addField("temperature", hic);
  pointDeviceHome.addField("tilt", tilt);
  pointDeviceHome.addField("light", lightSensorValue);
  pointDeviceHome.addField("check_temperature", check_temp);
  pointDeviceHome.addField("check_tilt", check_tilt);
  pointDeviceHome.addField("check_light", check_light);
  pointDeviceHome.addField("check_wifi", check_wifi);
  pointDeviceHome.addField("led_external_status", led_external_status);
  pointDeviceHome.addField("led_external2_status", led_external2_status);
  pointDeviceHome.addField("lower_bound_light", lower_bound_light_db);
  pointDeviceHome.addField("upper_bound_light", upper_bound_light_db);
  pointDeviceHome.addField("lower_bound_temperature", lower_bound_temperature_db);
  pointDeviceHome.addField("upper_bound_temperature", upper_bound_temperature_db);
  pointDeviceHome.addField("lower_bound_rssi", lower_bound_rssi_db);

  Serial.print(F("Writing Home: "));
  Serial.println(pointDeviceHome.toLineProtocol());
  if (!client_idb.writePoint(pointDeviceHome)) {
    Serial.print(F("InfluxDB write failed: "));
    Serial.println(client_idb.getLastErrorMessage());
  }
}

void connectToMQTTBroker() {
  if (!mqttClient.connected()) {   // not connected
    lastWill(); // before of the connect
    Serial.print(F("\nConnecting to MQTT broker..."));
    while (!mqttClient.connect(MQTT_CLIENTID, MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.print(F("."));
      delay(1000);
    }
    Serial.println(F("\nConnected!"));

    // connected to broker, subscribe topics
    mqttClient.subscribe("gferrario/setup");
    Serial.println(F("\nSubscribed to gferrario/setup topic!"));
     mqttClient.subscribe("gferrario/home");
    Serial.println(F("\nSubscribed to gferrario/home topic!"));
    mqttClient.subscribe("gferrario/serveSlave");
    Serial.println(F("\nSubscribed to gferrario/serveSlave topic!"));
    mqttClient.subscribe("gferrario/serveSlaveHome");
    Serial.println(F("\nSubscribed to gferrario/serveSlaveHome topic!"));
    mqttClient.subscribe("gferrario/serveSlaveWeather");
    Serial.println(F("\nSubscribed to gferrario/serveSlaveWeather topic!"));
    mqttClient.subscribe("gferrario/homeAlert/#");
    Serial.println(F("\nSubscribed to gferrario/homeAlert/# topic!"));
    //mqttClient.subscribe("gferrario/weather");
    //Serial.println(F("\nSubscribed to gferrario/weather topic!"));
    //mqttClient.subscribe("gferrario/deactiveSlaveWeather");
    //Serial.println(F("\nSubscribed to gferrario/deactiveSlaveWeather topic!"));
    //mqttClient.subscribe("gferrario/deactiveSlaveHome");
    //Serial.println(F("\nSubscribed to gferrario/deactiveSlaveHome topic!"));
    mqttClient.subscribe("gferrario/slave/#");
    Serial.println(F("\nSubscribed to gferrario/slave/# topic!"));
    //mqttClient.subscribe("gferrario/slaveWeather");
    //Serial.println(F("\nSubscribed to gferrario/slaveWeather topic!"));
  }
}
// http request for openweather api
String httpGETRequest(const char* serverName) {
  HTTPClient http;
    
  // Your IP address with path or Domain name with URL path 
  http.begin(serverName);
  
  // Send HTTP POST request
  int httpResponseCode = http.GET();
  
  String payload = "{}"; 
  
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}

void mqttMessageReceived(String &topic, String &payload) {
  // this function handles a message from the MQTT broker
  Serial.println("Incoming MQTT message: " + topic + " - " + payload);
  DynamicJsonDocument doc(1024);
  deserializeJson(doc, payload);

  if(topic == "gferrario/setup"){
    sleep_time = doc["sleep_time"];
    execution_time = doc["execution_time"];
    execution_time = execution_time + 2000000;
    Serial.println("Setup done!");
    DynamicJsonDocument ack(512);
    ack["goal"] = "setup";
    ack["status"] = "done";
    String buffer;
    serializeJson(ack, buffer);
    mqttClient.publish("gferrario/ack", buffer);
    
  } else if (strstr(topic.c_str(), "gferrario/slave/") != NULL && doc["status"] == "activated"){ 
     // Slave Weather
     // verify the identity of the web app request
     if(doc["slave"] == "NodeMCUWeather"){
        DynamicJsonDocument doc_slave(512);
        
        doc_slave["mac"] = mac + "_weather" ;
        doc_slave["topic"] = "gferrario/serveSlaveWeather" ; // topic in which the slave remains waiting for answer from the master
        doc_slave["slave"] = doc["slave"];
        doc_slave["status"] = "activated";

        doc_slave["city"] = doc["city"];
        //doc_slave["countryCode"] = doc["countryCode"];
        
        String buffer;
        serializeJson(doc_slave, buffer);
        
        mqttClient.publish("gferrario/serveSlave", buffer);
        
     }
     if(doc["slave"] == "NodeMCUHome"){
        DynamicJsonDocument doc_slave(512);
        
        doc_slave["mac"] = mac + "_home" ;
        doc_slave["topic"] = "gferrario/serveSlaveHome" ; // topic in which the slave remains waiting for answer from the master
        doc_slave["slave"] = doc["slave"];
        doc_slave["status"] = "activated";
        
        String buffer;
        serializeJson(doc_slave, buffer);
        
        mqttClient.publish("gferrario/serveSlave", buffer);
      }
  }
  /*if (topic == "gferrario/slaveHome" && doc["status"] == "activated"){ 
      // Slave Home
      // verify the identity of the web app request
      if(doc["slave"] == "NodeMCUHome"){
        DynamicJsonDocument doc_slave(512);
        
        doc_slave["mac"] = mac + "_home" ;
        doc_slave["topic"] = "gferrario/serveSlaveHome" ; // topic in which the slave remains waiting for answer from the master
        doc_slave["slave"] = doc["slave"];
        doc_slave["status"] = "activated";
        
        String buffer;
        serializeJson(doc_slave, buffer);
        
        mqttClient.publish("gferrario/serveSlave", buffer);
      }
      
  }*/
  if (strstr(topic.c_str(), "gferrario/homeAlert/") != NULL){
    if (doc["sensor"] == "temperature"){
      lower_bound_temperature = doc["min"];
      upper_bound_temperature = doc["max"];
      
    }else if(doc["sensor"] == "light"){
      lower_bound_light = doc["min"];
      upper_bound_light = doc["max"];
      
    }else if(doc["sensor"] == "wifi"){
      lower_bound_rssi = doc["min"];
    }
    if(doc["sensor"] == "temperature" | doc["sensor"] == "light" | doc["sensor"] == "wifi")
    {
      Serial.println("ACK messafe for new sensor bounds");
      DynamicJsonDocument ack(512);
      /*ack["goal"] = strcat(strcat(doc["sensor"]," min: "),doc["min"]);
      if(doc["sensor"] == "temperature" | doc["sensor"] == "light")
        ack["goal"] = strcat(strcat(ack["goal"], " max: "),doc["max"]);*/
      ack["goal"] = doc["sensor"];
      ack["status"] = "done"; 
      String buffer;
      serializeJson(ack, buffer);
      mqttClient.publish("gferrario/ack", buffer);
    }
  }
  if(topic == "gferrario/alarms" and doc["slave"] == "NodeMCUHome"){
        // il master imposta/aggiorna gli alerts
        lower_bound_light = doc["lower_bound_light"];
        upper_bound_light = doc["upper_bound_light"];
        lower_bound_temperature = doc["lower_bound_temperature"];
        upper_bound_temperature = doc["upper_bound_temperature"];
        lower_bound_rssi = doc["lower_bound_rssi"];
        DynamicJsonDocument ack(512);
        ack["goal"] = "alarms";
        ack["status"] = "done";
        String buffer;
        serializeJson(ack, buffer);
        mqttClient.publish("gferrario/ack", buffer);
      }
  if (topic == "gferrario/serveSlave"){ 
    // Master is listening to new slaves who want to join or detach from the network
    String res = checkIdentity(doc["slave"], doc["mac"]);
    DynamicJsonDocument doc_service(512);
    if(res != ""){
      if (res == "weather"){ 
        // verify the identity of the slaves

        if(doc["status"] == "activated"){
          
          doc_service["topic"] = "gferrario/weather"; // provides the topic in which the slave must publish
          doc_service["slave"] = doc["slave"];
          doc_service["mac"] = doc["mac"];
  
          // the master composes the endpoint for the get and provides it to the slave
          city = (const char*)doc["city"];
          //String countryCode = doc["countryCode"]; 
          //doc_service["serverPath"] = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&APPID=" + openWeatherMapApiKey;
          doc_service["serverPath"] = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "&APPID=" + openWeatherMapApiKey;
          doc_service["city"] = doc["city"];
          String buffer;
          serializeJson(doc_service, buffer);
          mqttClient.publish((const char*)doc["topic"], buffer); // publish on the topic where the slave is listening to news from the master
        
        }
        
      } else if (res == "home"){
        if(doc["status"] == "activated"){

          if(!flag_home) // to avoid sending the setup data back to the slave when a user wants to update alerts
          {
            doc_service["topic"] = "gferrario/home";
            doc_service["slave"] = doc["slave"];
            doc_service["mac"] = doc["mac"];
      
            String buffer;
            serializeJson(doc_service, buffer);
            mqttClient.publish((const char*)doc["topic"], buffer); // publish on the topic where the slave is listening to news from the master node
          }
          
        }
      }
      // save the information of the slaves that has connected/ disconnected in Mysql
      if((doc["status"] == "deactivated" & res == "home") || (doc["status"] == "deactivated" & res == "weather"))// || (!flag_home & !flag_weather))
      {
        
        if (doc["status"] == "deactivated" & res == "home" & flag_home)
        {
          DynamicJsonDocument ack(512);
          ack["goal"] = "deactivated home monitoring";
          ack["status"] = "done";
          String buffer;
          serializeJson(ack, buffer);
          mqttClient.publish("gferrario/ack", buffer);
          WriteMultiToMYSQL((const char*)doc["slave"], (const char*)doc["mac"], (const char*)doc["status"]);
          flag_home= false;
        }
        else if (doc["status"] == "deactivated" & res == "weather" & flag_weather)
        {
          DynamicJsonDocument ack(512);
          ack["goal"] = "deactivated weather monitoring";
          ack["status"] = "done";
          String buffer;
          serializeJson(ack, buffer);
          mqttClient.publish("gferrario/ack", buffer);
          WriteMultiToMYSQL((const char*)doc["slave"], (const char*)doc["mac"], (const char*)doc["status"]);
          flag_weather= false;
        }
      }
    }
    else{
      Serial.println("Intruder!!!");
    }
  }
  if (topic == "gferrario/serveSlaveHome") { // Slave Home
  // the slave receives a reply from the master and begins to publish the measurements on the topic indicated by him
      Serial.println("Slave home IN FASE DI AVVIO");
      if(checkIdentity(doc["slave"], doc["mac"]) == "home"){
        Serial.println("Slave home attivato");
        topic_home = (const char*)doc["topic"]; // imposta il topic
        flag_home = true; // flag to indicate that the slave must start monitoring
        Serial.println("Activated Slave Home");
        DynamicJsonDocument ack(512);
        ack["goal"] = "activated home monitoring";
        ack["status"] = "done";
        String buffer;
        serializeJson(ack, buffer);
        mqttClient.publish("gferrario/ack", buffer);
      }
  }
  if (topic == "gferrario/serveSlaveWeather") { // Slave Weather
  // the slave receives a reply from the master and starts to publish the measurements on the topic indicated by him

      if(checkIdentity(doc["slave"], doc["mac"]) == "weather"){

        serverPath = (const char*)doc["serverPath"]; // set the endpoint for the api (which he got from the master)
        
        Serial.println(serverPath);
        if(!flag_weather)
        {
          Serial.println("Activated Slave Weather");
          flag_weather = true; // flag to indicate that the slave must start monitoring
          topic_weather = (const char*)doc["topic"]; // set topic
          DynamicJsonDocument ack(512);
          ack["goal"] = "activated weather monitoring";
          ack["status"] = "done";
          String buffer;
          serializeJson(ack, buffer);
          mqttClient.publish("gferrario/ack", buffer);
        }
      }
  }
  if (topic == "gferrario/home") { // Master
    // the master receives the measurements and saves them on Influxdb
    
    WriteHome((int)doc["rssi"], doc["humidity"], doc["temperature"], doc["tilt"], doc["light"], doc["check_temp"], doc["check_tilt"], doc["check_light"], doc["check_wifi"], doc["led_external_status"], doc["led_external2_status"], doc["lower_bound_light"], doc["upper_bound_light"], doc["lower_bound_temperature"], doc["upper_bound_temperature"], doc["lower_bound_rssi"]);

  }
  if(strstr(topic.c_str(), "gferrario/slave/") != NULL && doc["status"] == "deactivated"){ 
    // disable the slave and update the master on the new status
     
     if(doc["slave"] == "NodeMCUWeather"){
        
        DynamicJsonDocument doc_slave(512);
        doc_slave["mac"] = mac + "_weather" ;
        doc_slave["slave"] = doc["slave"];
        doc_slave["status"] = "deactivated";
        Serial.println("Deactivated Slave Weather");
        String buffer;
        serializeJson(doc_slave, buffer);
        mqttClient.publish("gferrario/serveSlave", buffer);
     }
  }

  if( strstr(topic.c_str(), "gferrario/slave/") != NULL && doc["status"] == "deactivated"){ 
    // disable the slave and update the master on the new status
     
     if(doc["slave"] == "NodeMCUHome"){
        DynamicJsonDocument doc_slave(512);
        doc_slave["mac"] = mac + "_home" ;
        doc_slave["slave"] = doc["slave"];
        doc_slave["status"] = "deactivated";
        
        Serial.println("Deactivated Slave Home");
        
        String buffer;
        serializeJson(doc_slave, buffer);
        mqttClient.publish("gferrario/serveSlave", buffer);
     }
  }
}
// write slave information on mysql
int WriteMultiToMYSQL(const char* field1, const char* field2, const char* field3) {
  int error;
  if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
    Serial.println(F("MySQL connection established."));

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);

    sprintf(query, INSERT_DATA, field1, field2, field3);
    Serial.println(query);
    // execute the query
    cur_mem->execute(query);
    
    delete cur_mem;
    error = 1;
    Serial.println(F("Data recorded on MySQL"));

    conn.close();
  } else {
    Serial.println(F("MySQL connection failed."));
    error = -1;
  }

  return error;
}

void showHome(int h, float hic, long rssi, int lightSensorValue){
  // home monitoring screen for display
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
}    
void showWelcome(){
  // welcome screen for display
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Hi :)");
}
void lastWill(){
  // notify other clients about an ungracefully disconnection
  DynamicJsonDocument doc(512);
  doc["device"] = "ESP8266";  
  doc["mac"] = mac;
  doc["status"] = "disconnected";

  String buffer;
  serializeJson(doc, buffer);
  mqttClient.setWill("gferrario/lastwill", buffer.c_str(), false, 1);

}

String checkIdentity(String name_nodemcu, String mac_nodemcu) {
  //Verify that the slave is eligible
  //I have done this check to avoid network instrusions
  String job = "";
  if (conn.connect(server_addr, 3306, mysql_user, mysql_password)) {
    Serial.println(F("MySQL connection established."));

    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    
    String select_query= "select distinct job from `gferrario`.`slave_catalogue` where name=\""+ name_nodemcu +"\" and mac=\""+ mac_nodemcu +"\"";
    Serial.println(select_query);
    
    // execute the query
    cur_mem->execute(select_query.c_str());
    column_names *cols = cur_mem->get_columns();
    row_values *row = NULL;
    
    row = cur_mem->get_next_row();
    if (row != NULL) {
      job= row->values[0];
    }
    delete cur_mem;
    conn.close();
  } else {
    Serial.println(F("MySQL connection failed."));
  }
  return job;
}
void execution_weather()
{
  
  const char* weather;
  float temperature;
  float humidity;
  float pressure;
  float wind;
  float lat;
  float lon;
  connectToWiFi();
  mqttClient.loop();       // MQTT client loop
  if (((millis() - lastTime_weather) > timerDelay && flag_weather) | (lastTime_weather == 0 && flag_weather)) {
    Eloquent::ML::Port::SVM classifier; // SVM classifier
    String jsonBuffer;
    // Send an HTTP GET request to openweather map
    jsonBuffer = httpGETRequest(serverPath.c_str());
    Serial.println(jsonBuffer);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, jsonBuffer);
    const int capacity = JSON_OBJECT_SIZE(6);
    DynamicJsonDocument json(1024);
    weather = doc["weather"][0]["description"]; 
    temperature = (double)doc["main"]["temp"] - 273,15; // convert in celsius
    humidity = doc["main"]["humidity"]; 
    pressure = doc["main"]["pressure"];
    wind = doc["wind"]["speed"];
    lat = doc["coord"]["lat"];
    lon = doc["coord"]["lon"];
    json["city"] = city;
    float sample[] = {temperature, humidity, wind, pressure};
    String weather_predicted = classifier.predictLabel(sample);
    
    Serial.print("correct weather: ");
    Serial.println(weather);
    Serial.print("predicted weather: ");
    Serial.println(weather_predicted);
    
    json["weather_condition"] = weather_predicted;
    json["temperature"] = temperature;
    json["humidity"] = humidity;
    json["pressure"] = pressure;
    json["wind"] = wind;
    json["lat"] = lat;
    json["long"] = lon;
    char buffer[512];
    size_t n = serializeJson(json, buffer);
    mqttClient.publish(topic_weather.c_str(), buffer, n);
    lastTime_weather = millis();
  }

}

bool execution_home(){
 
  // sensors
  static bool led_external_status = HIGH;
  static bool led_external2_status = HIGH;
  static int check_temp = 0;
  int check_tilt = 0;
  int check_light = 0;
  int check_wifi = 0;
  float h;
  long rssi;
  const char* tilt;
  float hic;
  unsigned long lightSensorValue;
  
    
  if (((millis() - lastTime_home) > timerDelay && flag_home) | (lastTime_home == 0 && flag_home)) {
    h = dht.readHumidity();      // humidity percentage, range 20-80% (±5% accuracy)
    float t = dht.readTemperature();   // temperature Celsius, range 0-50°C (±2°C accuracy)
    rssi = WiFi.RSSI();
    byte val = digitalRead(TILT);   // read the SW-520D state

    // check alerts
    if (val == HIGH) {

      tilt = "TILTED";
      check_tilt = 1;
      
    } else {

      tilt = "NOT TILTED";
      check_tilt = 0;
    }

    if (isnan(h) || isnan(t)) {   // readings failed, skip
      Serial.println(F("Failed to read from DHT sensor!"));
      return false;
    }

    // compute heat index in Celsius (isFahreheit = false)
    hic = dht.computeHeatIndex(t, h, false);
    lightSensorValue = analogRead(PHOTORESISTOR);


    if (lightSensorValue < lower_bound_light) {
      check_light = 1;  
    } else if(lightSensorValue > upper_bound_light){
      check_light = 2;
    } else{
      check_light = 0;  
    }

    if (hic < lower_bound_temperature){
      check_temp = 1;
    }
    else if (hic > upper_bound_temperature){
      check_temp = 2;
    } else{
      check_temp = 0;
    }

    if(rssi < lower_bound_rssi){
      check_wifi = 1;
    } else{
      check_wifi = 0;
    }

    // change the status of the leds according to the alerts
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

    // json composition
    DynamicJsonDocument doc(512);
    
    doc["ssid"] = ssid;
    doc["rssi"] = rssi;
    doc["humidity"] = h;
    doc["temperature"] = hic;
    doc["tilt"] = tilt;
    
    doc["light"] = lightSensorValue;
    doc["check_temp"] = check_temp;
    doc["check_tilt"] = check_tilt;
    doc["check_light"] = check_light;
    doc["check_wifi"] = check_wifi;
    doc["led_external_status"] = led_external_status;
    doc["led_external2_status"] = led_external2_status;
    
    doc["lower_bound_light"] = lower_bound_light;
    doc["upper_bound_light"] = upper_bound_light;
    doc["lower_bound_temperature"] = lower_bound_temperature;
    doc["upper_bound_temperature"] = upper_bound_temperature;
    doc["lower_bound_rssi"] = lower_bound_rssi;
    
    String buffer;
    serializeJson(doc, buffer);
    mqttClient.publish(topic_home.c_str(), buffer);
    
    lastTime_home = millis();
    showHome(int(h), hic, rssi, lightSensorValue);
  }
  return led_external_status;
}
