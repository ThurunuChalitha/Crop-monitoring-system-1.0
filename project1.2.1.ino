#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <DHT.h>
#include <Adafruit_MQTT_Client.h>
#include <Adafruit_MQTT.h>
#include <Arduino.h>
#include <SinricPro.h>
#include <SinricProTemperaturesensor.h>

#define DHTTYPE DHT11 // type of the temperature sensor
const int DHTPin = 13; //--> The pin used for the DHT11 sensor 
DHT dht(DHTPin, DHTTYPE); //--> Initialize DHT sensor, DHT dht(Pin_used, Type_of_DHT_Sensor);


//SinricPro Credentials
#define APP_KEY           "244a2569-9d86-4229-b53e-dde906465397"      // Should look like "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "b113d12d-6d18-47fc-a3e4-55e12fe74bae-3ba8ade7-5f09-4962-a2a8-8b9cff5efadc"   // Should look like "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define TEMP_SENSOR_ID    "622cedd5d0fd258c52f6015b"    // Should look like "5dc1564130xxxxxxxxxxxxxx"
#define EVENT_WAIT_TIME   60000               // send event every 60 seconds

#define ON_Board_LED 2  //--> Defining an On Board LED, used for indicators when the process of connecting to a wifi router

const char* ssid = "TP-LINK_C25ED8"; //--> Your wifi name or SSID.
const char* password = "xyz1234567"; //--> Your wifi password.
const int sensor_pin = A0;  /* Connect Soil moisture analog sensor pin to A0 of NodeMCU */
bool deviceIsOn; 
float lastTemperature;                        // last known temperature (for compare)
float lastHumidity;                           // last known humidity (for compare)
unsigned long lastEvent = (-EVENT_WAIT_TIME); // last time event has been sent 

//Relays for switching appliances
#define Relay1            12


//----------------------------------------Host & httpsPort
const char* host = "script.google.com";
const int httpsPort = 443;
//----------------------------------------

bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Temperaturesensor turned %s (via SinricPro) \r\n", state?"on":"off");
  deviceIsOn = state; // turn on / off temperature sensor
  return true; // request handled properly
}

#define AIO_SERVER      "broker.hivemq.com" //IP address 
#define AIO_SERVERPORT  1883                   // port
#define AIO_USERNAME    ""
#define AIO_KEY         ""

WiFiClient client1;
WiFiClientSecure client; //--> Create a WiFiClientSecure object.

Adafruit_MQTT_Client mqtt(&client1, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish Humidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidThurunu");
Adafruit_MQTT_Publish Temperature = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/tempThurunu");
Adafruit_MQTT_Publish SoilM = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/soilThurunu");


// Setup a feed called 'onoff' for subscribing .
Adafruit_MQTT_Subscribe Light1 = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/LED_Thuru");

String GAS_ID = "AKfycbytUt8ckFiuHPiYNU2ifAVsD8sPFrtoNlhcKCFHPLPTfxGPEqqAIZYaV9aIIUIY8Leh_Q"; //--> spreadsheet script ID

void MQTT_connect();
void setupSinricPro();

void setup() {
  setupSinricPro();
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(500);
  
  pinMode(Relay1, OUTPUT);

  dht.begin();  //--> Start reading DHT11 sensors
  delay(500);
  
  WiFi.begin(ssid, password); //--> Connect to your WiFi router
  Serial.println("");
    
  pinMode(ON_Board_LED,OUTPUT); //--> On Board LED port Direction output
  digitalWrite(ON_Board_LED, HIGH); //--> Turn off Led On Board

    // Setting up onoff feed.
  mqtt.subscribe(&Light1);

  //----------------------------------------Wait for connection
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    //----------------------------------------Make the On Board Flashing LED on the process of connecting to the wifi router.
    digitalWrite(ON_Board_LED, LOW);
    delay(250);
    digitalWrite(ON_Board_LED, HIGH);
    delay(250);
    //----------------------------------------
  }
  //----------------------------------------
  digitalWrite(ON_Board_LED, HIGH); //--> Turn off the On Board LED when it is connected to the wifi router.
  Serial.println("");
  Serial.print("Successfully connected to : ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  //----------------------------------------

  client.setInsecure();
  
}
//uint32_t x = 0;

void loop() {

  MQTT_connect();
  SinricPro.handle();
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(20000))) {
    if (subscription == &Light1) {
      Serial.print(F("Got: "));
      Serial.println((char *)Light1.lastread);
      int Light1_State = atoi((char *)Light1.lastread);
      digitalWrite(Relay1, Light1_State);
      
    }

  }
  
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  float m = ( 100.00 - ( (analogRead(sensor_pin)/1023.00) * 100.00 ) );   // reading soil moisture
  unsigned long actualMillis = millis();
  if (actualMillis - lastEvent < EVENT_WAIT_TIME) return; //only check every EVENT_WAIT_TIME milliseconds
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  if (! Humidity.publish(h)) {
    Serial.println(F("CONNECTION Failed humidity"));
  } else {
    Serial.println(F("Humidity CONNECTION OK!"));
  }
  if (! Temperature.publish(t)) {
    Serial.println(F("CONNECTION Failed temperature"));
  } else {
    Serial.println(F("Temperature CONNECTION OK!"));
  }
   if (! SoilM.publish(m)) {
    Serial.println(F("CONNECTION Failed soil moisture"));
  } else {
    Serial.println(F(" Soil Moisture CONNECTION OK!"));
  }
  
  String Temp = "Temperature : " + String(t) + " °C";
  String Humi = "Humidity : " + String(h) + " %";
  String SoilM = "Soil Moisture : " + String(m) + " %";
  Serial.println(Temp);
  Serial.println(Humi);
  Serial.println(SoilM);
  
  sendData(t, h, m); //--> Calls the sendData Subroutine

// handeling sensor for SinricPro

  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];  // get temperaturesensor device
  bool success = mySensor.sendTemperatureEvent(t, h); // send event
  if (success) {  // if event was sent successfuly, print temperature and humidity to serial
    Serial.printf("Temperature: %2.1f Celsius\tHumidity: %2.1f%%\r\n", t, h);
  } else {  // if sending event failed, print error message
    Serial.printf("Something went wrong...could not send Event to server!\r\n");
  }

  if (t == lastTemperature || h == lastHumidity) return; // if no values changed do nothing...
  lastTemperature = t;  // save actual temperature for next compare
  lastHumidity = h;        // save actual humidity for next compare
  lastEvent = actualMillis;       // save actual time for next compare
  
}

// Subroutine for sending data to Google Sheets
void sendData(float tem, float hum, float moist) {
  Serial.println("==========");
  Serial.print("connecting to ");
  Serial.println(host);
  
  //----------------------------------------Connect to Google host
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed to Scripts");
    return;
  }
  //----------------------------------------

  //----------------------------------------Processing data and sending data
  String string_temperature =  String(tem);
  // String string_temperature =  String(tem, DEC); 
  String string_humidity =  String(hum); 
  String string_soilMoist =  String(moist, DEC);
  String url = "/macros/s/" + GAS_ID + "/exec?temperature=" + string_temperature + "&humidity=" + string_humidity + "&soilmoist=" + string_soilMoist;
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
         "Host: " + host + "\r\n" +
         "User-Agent: BuildFailureDetectorESP8266\r\n" +
         "Connection: close\r\n\r\n");

  Serial.println("request sent");
  //----------------------------------------

  //----------------------------------------Checking whether the data was sent successfully or not
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.print("reply was : ");
  Serial.println(line);
  Serial.println("closing connection");
  Serial.println("==========");
  Serial.println();
  //----------------------------------------
} 


//reconnecting function
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { 
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset 
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}

//..........................................
//SinricPro
//.................................................
void setupSinricPro() {
  // add device to SinricPro
  SinricProTemperaturesensor &mySensor = SinricPro[TEMP_SENSOR_ID];
  mySensor.onPowerState(onPowerState);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  //SinricPro.restoreDeviceStates(true); // Uncomment to restore the last known state from the server.
  SinricPro.begin(APP_KEY, APP_SECRET);  
}


//bool onPowerState(const String &deviceId, bool &state) {
//  Serial.printf("Temperaturesensor turned %s (via SinricPro) \r\n", state?"on":"off");
//  deviceIsOn = state; // turn on / off temperature sensor
//  return true; // request handled properly
//}
//==============================================================================
