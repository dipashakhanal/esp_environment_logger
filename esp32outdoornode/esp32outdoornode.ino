#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <math.h>

const char* WIFI_SSID = "frontiers_2.4";

const char* WIFI_PASSWORD = "PUT_FRONTIERS_PASSWORD_HERE";


const char* MQTT_SERVER = "broker.hivemq.com";

const uint16_t MQTT_PORT = 1883;

const char* TOPIC_OUTDOOR_TEMP =
  "envmon_2026_A9F3/outdoor/temperature";

const char* TOPIC_OUTDOOR_HUM =
  "envmon_2026_A9F3/outdoor/humidity";

const char* TOPIC_OUTDOOR_STATUS =
  "envmon_2026_A9F3/outdoor/status";



const uint8_t I2C_SDA_PIN = 32;

const uint8_t I2C_SCL_PIN = 33;



const uint8_t SHT31_ADDRESS_PRIMARY = 0x44;

const uint8_t SHT31_ADDRESS_SECONDARY = 0x45;



const unsigned long SENSOR_INTERVAL_MS =
  15UL * 60UL * 1000UL;


const unsigned long WIFI_RETRY_INTERVAL_MS =
  10UL * 1000UL;


const unsigned long MQTT_RETRY_INTERVAL_MS =
  5UL * 1000UL;


const unsigned long SHT31_RETRY_INTERVAL_MS =
  10UL * 1000UL;



WiFiClient wifiClient;

PubSubClient mqttClient(wifiClient);

Adafruit_SHT31 outdoorSHT31 =
  Adafruit_SHT31();




bool sht31Ready = false;

uint8_t activeSHT31Address = 0;

unsigned long lastSensorTime = 0;

unsigned long lastWiFiAttempt = 0;

unsigned long lastMQTTAttempt = 0;

unsigned long lastSHT31Attempt = 0;



void scanI2CBus() {

  Serial.println();

  Serial.println(
    "-------------------------------"
  );

  Serial.println(
    "I2C SCAN"
  );

  Serial.println(
    "-------------------------------"
  );

  int devicesFound = 0;


  for (
    uint8_t address = 1;
    address < 127;
    address++
  ) {

    Wire.beginTransmission(
      address
    );

    uint8_t error =
      Wire.endTransmission();


    if (error == 0) {

      Serial.print(
        "I2C device found at 0x"
      );


      if (address < 16) {

        Serial.print("0");
      }


      Serial.println(
        address,
        HEX
      );


      devicesFound++;
    }
  }


  if (devicesFound == 0) {

    Serial.println(
      "NO I2C DEVICES FOUND."
    );

    Serial.println(
      "Check:"
    );

    Serial.println(
      "VCC -> 3.3V"
    );

    Serial.println(
      "GND -> GND"
    );

    Serial.println(
      "SDA -> GPIO32"
    );

    Serial.println(
      "SCL -> GPIO33"
    );

  }

  else {

    Serial.print(
      "Total I2C devices found: "
    );

    Serial.println(
      devicesFound
    );
  }


  Serial.println(
    "-----------------------------"
  );
}


bool initializeSHT31() {

  lastSHT31Attempt =
    millis();


  Serial.println();

  Serial.println(
    "Checking outdoor SHT31..."
  );


  
  if (
    outdoorSHT31.begin(
      SHT31_ADDRESS_PRIMARY
    )
  ) {

    sht31Ready = true;

    activeSHT31Address =
      SHT31_ADDRESS_PRIMARY;


    Serial.println(
      "SHT31 detected at 0x44."
    );


    return true;
  }


  // --------------------------------
  // TRY ADDRESS 0x45
  // --------------------------------

  Serial.println(
    "No SHT31 at 0x44."
  );

  Serial.println(
    "Trying 0x45..."
  );


  if (
    outdoorSHT31.begin(
      SHT31_ADDRESS_SECONDARY
    )
  ) {

    sht31Ready = true;

    activeSHT31Address =
      SHT31_ADDRESS_SECONDARY;


    Serial.println(
      "SHT31 detected at 0x45."
    );


    return true;
  }



  sht31Ready = false;

  activeSHT31Address = 0;


  Serial.println(
    "OUTDOOR SHT31 NOT DETECTED."
  );


  Serial.println(
    "Running I2C scanner..."
  );


  scanI2CBus();


  return false;
}



bool readOutdoorSensor(
  float& temperature,
  float& humidity
) {

  if (!sht31Ready) {

    return false;
  }


  temperature =
    outdoorSHT31.readTemperature();


  humidity =
    outdoorSHT31.readHumidity();


  
  if (
    !isfinite(temperature) ||
    !isfinite(humidity) ||

    temperature < -40.0f ||
    temperature > 125.0f ||

    humidity < 0.0f ||
    humidity > 100.0f
  ) {

    Serial.println(
      "Invalid SHT31 reading."
    );


    Serial.println(
      "SHT31 will be reinitialized."
    );


    sht31Ready = false;

    activeSHT31Address = 0;


    return false;
  }


  return true;
}




void startWiFi() {

  Serial.println();

  Serial.print(
    "Connecting to Wi-Fi: "
  );

  Serial.println(
    WIFI_SSID
  );


  WiFi.mode(
    WIFI_STA
  );

  WiFi.setSleep(false);


  WiFi.setAutoReconnect(
    true
  );


  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );


  lastWiFiAttempt =
    millis();
}



void maintainWiFi() {

  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {

    return;
  }


  unsigned long now =
    millis();


  if (
    lastWiFiAttempt == 0 ||

    now - lastWiFiAttempt >=
    WIFI_RETRY_INTERVAL_MS
  ) {

    Serial.println(
      "Wi-Fi disconnected."
    );


    Serial.println(
      "Retrying..."
    );


    WiFi.disconnect();


    startWiFi();
  }
}



void publishOnlineStatus() {

  if (!mqttClient.connected()) {

    return;
  }


  bool success =
    mqttClient.publish(

      TOPIC_OUTDOOR_STATUS,

      "ONLINE",

      true
    );


  Serial.print(
    "Outdoor ONLINE status: "
  );


  Serial.println(
    success
      ? "OK"
      : "FAILED"
  );
}


void clearOldRetainedValues() {




  mqttClient.publish(

    TOPIC_OUTDOOR_TEMP,

    "",

    true
  );


  mqttClient.publish(

    TOPIC_OUTDOOR_HUM,

    "",

    true
  );
}



void maintainMQTT() {

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    return;
  }


  if (
    mqttClient.connected()
  ) {

    mqttClient.loop();

    return;
  }


  unsigned long now =
    millis();


  if (
    lastMQTTAttempt != 0 &&

    now - lastMQTTAttempt <
    MQTT_RETRY_INTERVAL_MS
  ) {

    return;
  }


  lastMQTTAttempt =
    now;


  String clientID =
    "ESP32OutdoorNode-";


  clientID +=
    String(
      (uint32_t)
      ESP.getEfuseMac(),

      HEX
    );


  Serial.print(
    "Connecting to MQTT..."
  );




  bool connected =
    mqttClient.connect(

      clientID.c_str(),

      TOPIC_OUTDOOR_STATUS,

      0,

      true,

      "OFFLINE"
    );


  if (connected) {

    Serial.println(
      "CONNECTED"
    );


    publishOnlineStatus();
  }

  else {

    Serial.print(
      "FAILED. MQTT state = "
    );


    Serial.println(
      mqttClient.state()
    );
  }
}



void publishOutdoorReading() {

  if (!mqttClient.connected()) {

    return;
  }


  float temperature = NAN;

  float humidity = NAN;


  if (
    !readOutdoorSensor(
      temperature,
      humidity
    )
  ) {

    Serial.println(
      "Sensor unavailable."
    );


    Serial.println(
      "Outdoor reading NOT published."
    );


    return;
  }




  char temperatureText[16];

  char humidityText[16];


  dtostrf(

    temperature,

    0,

    2,

    temperatureText
  );


  dtostrf(

    humidity,

    0,

    2,

    humidityText
  );




  bool tempOK =
    mqttClient.publish(

      TOPIC_OUTDOOR_TEMP,

      temperatureText,

      true
    );


  bool humidityOK =
    mqttClient.publish(

      TOPIC_OUTDOOR_HUM,

      humidityText,

      true
    );


  
  publishOnlineStatus();


  Serial.println();

  Serial.println(
    "--------------------------------"
  );


  Serial.print(
    "Outdoor Temperature : "
  );

  Serial.print(
    temperature,
    2
  );

  Serial.println(
    " C"
  );


  Serial.print(
    "Outdoor Humidity    : "
  );

  Serial.print(
    humidity,
    2
  );

  Serial.println(
    " %"
  );


  Serial.print(
    "SHT31 Address       : 0x"
  );


  if (
    activeSHT31Address < 16
  ) {

    Serial.print("0");
  }


  Serial.println(
    activeSHT31Address,
    HEX
  );


  Serial.print(
    "Temperature MQTT    : "
  );

  Serial.println(
    tempOK
      ? "OK"
      : "FAILED"
  );


  Serial.print(
    "Humidity MQTT       : "
  );

  Serial.println(
    humidityOK
      ? "OK"
      : "FAILED"
  );
}



void setup() {

  Serial.begin(
    115200
  );


  delay(
    1500
  );


  Serial.println();

  Serial.println(
    "-------------------------------"
  );

  Serial.println(
    " ESP32 OUTDOOR ENVIRONMENT NODE"
  );

  Serial.println(
    " SDA = GPIO32"
  );

  Serial.println(
    " SCL = GPIO33"
  );

  Serial.println(
    "-------------------------------"
  );


  

  Wire.begin(

    I2C_SDA_PIN,

    I2C_SCL_PIN
  );


  delay(
    500
  );

  scanI2CBus();
  
  initializeSHT31();

  startWiFi();



  mqttClient.setServer(

    MQTT_SERVER,

    MQTT_PORT
  );


  mqttClient.setKeepAlive(
    15
  );


  mqttClient.setSocketTimeout(
    15
  );


  mqttClient.setBufferSize(
    256
  );

  lastSensorTime =
    millis()
    - SENSOR_INTERVAL_MS;
}



void loop() {



  maintainWiFi();
  maintainMQTT();


  unsigned long now =
    millis();

  if (
    !sht31Ready &&

    (
      lastSHT31Attempt == 0 ||

      now - lastSHT31Attempt >=
      SHT31_RETRY_INTERVAL_MS
    )
  ) {

    initializeSHT31();
  }


  if (
    sht31Ready &&

    mqttClient.connected() &&

    now - lastSensorTime >=
    SENSOR_INTERVAL_MS
  ) {

    lastSensorTime =
      now;


    publishOutdoorReading();
  }


  if (
    mqttClient.connected()
  ) {

    mqttClient.loop();
  }


  delay(
    20
  );
}