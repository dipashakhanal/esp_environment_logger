

#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <hd44780.h>
#include <hd44780ioClass/hd44780_I2Cexp.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <math.h>
#include "secrets.h"



const char* GOOGLE_WEB_APP_URL =
  "https://script.google.com/macros/s/AKfycbzNr_ny0x-OjkAdsRiuIKQm0tH4PzFPpi0F2zzujOQa5xM2Mu2gCbj-CNZG8tQZqJac/exec";


const bool LCD_CONNECTED = true;



const char* MQTT_SERVER = "broker.hivemq.com";
const uint16_t MQTT_PORT = 1883;

const char* TOPIC_OUTDOOR_TEMP   = "envmon_2026_A9F3/outdoor/temperature";
const char* TOPIC_OUTDOOR_HUM    = "envmon_2026_A9F3/outdoor/humidity";
const char* TOPIC_OUTDOOR_STATUS = "envmon_2026_A9F3/outdoor/status";



const uint8_t I2C_SDA_PIN = 21;
const uint8_t I2C_SCL_PIN = 22;
const uint8_t SHT31_ADDRESS = 0x44;

const uint8_t SD_CS_PIN   = 5;
const uint8_t SD_SCK_PIN  = 18;
const uint8_t SD_MISO_PIN = 19;
const uint8_t SD_MOSI_PIN = 23;



const long GMT_OFFSET_SECONDS = (5L * 3600L) + (45L * 60L);
const int DAYLIGHT_OFFSET_SECONDS = 0;



const unsigned long DISPLAY_INTERVAL_MS      = 4UL * 1000UL;
const unsigned long RECORD_INTERVAL_MS       = 15UL * 60UL * 1000UL;
const unsigned long WIFI_RETRY_INTERVAL_MS   = 10UL * 1000UL;
const unsigned long MQTT_RETRY_INTERVAL_MS   = 5UL * 1000UL;
const unsigned long SD_RETRY_INTERVAL_MS     = 30UL * 1000UL;
const unsigned long SHT31_RETRY_INTERVAL_MS  = 10UL * 1000UL;
const unsigned long PENDING_SYNC_INTERVAL_MS = 15UL * 60UL * 1000UL;


const unsigned long OUTDOOR_TIMEOUT_MS = 30UL * 1000UL;


const uint8_t MAX_PENDING_UPLOADS_PER_SYNC = 3;



const char* DATA_FILE = "/data.csv";
const char* PENDING_FILE = "/pending.csv";
const char* PENDING_TEMP_FILE = "/pending_tmp.csv";
const char* PENDING_BACKUP_FILE = "/pending_bak.csv";

const char* CSV_HEADER =
  "record_id,timestamp,indoor_temp_c,indoor_humidity_pct,"
  "outdoor_temp_c,outdoor_humidity_pct,outdoor_status";



Adafruit_SHT31 indoorSHT31 = Adafruit_SHT31();
hd44780_I2Cexp lcd;
WiFiClient mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);



float indoorTemperature = NAN;
float indoorHumidity = NAN;
float outdoorTemperature = NAN;
float outdoorHumidity = NAN;

bool sht31Ready = false;
bool sdReady = false;
bool lcdReady = false;
bool ntpStarted = false;
bool outdoorOnline = false;

unsigned long outdoorTempReceivedAt = 0;
unsigned long outdoorHumReceivedAt = 0;
unsigned long lastDisplayTime = 0;
unsigned long lastRecordTime = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastMQTTAttempt = 0;
unsigned long lastSDAttempt = 0;
unsigned long lastSHT31Attempt = 0;
unsigned long lastPendingSyncTime = 0;

byte degreeSymbol[8] = {
  B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000
};



void clearOutdoorData() {
  outdoorTemperature = NAN;
  outdoorHumidity = NAN;
  outdoorTempReceivedAt = 0;
  outdoorHumReceivedAt = 0;
}

bool indoorDataValid() {
  return isfinite(indoorTemperature) &&
         isfinite(indoorHumidity) &&
         indoorHumidity >= 0.0f &&
         indoorHumidity <= 100.0f;
}

bool outdoorDataValid() {
  
  return
    isfinite(outdoorTemperature) &&
    isfinite(outdoorHumidity) &&
    outdoorHumidity >= 0.0f &&
    outdoorHumidity <= 100.0f;
}

void maintainOutdoorFreshness() {
  
}


void printLCDLine(uint8_t row, String text) {
  if (!LCD_CONNECTED || !lcdReady) return;

  if (text.length() > 20) text = text.substring(0, 20);
  while (text.length() < 20) text += " ";

  lcd.setCursor(0, row);
  lcd.print(text);
}

void initializeLCD() {
  if (!LCD_CONNECTED) {
    lcdReady = false;
    Serial.println("LCD disabled in software.");
    return;
  }

  Serial.println("Initializing 20x4 LCD...");

  int status = lcd.begin(20, 4);
  if (status != 0) {
    lcdReady = false;
    Serial.print("LCD initialization failed. Error: ");
    Serial.println(status);
    return;
  }

  lcdReady = true;
  lcd.backlight();
  lcd.createChar(0, degreeSymbol);
  lcd.clear();

  printLCDLine(0, "Environment Monitor");
  printLCDLine(1, "Starting...");

  Serial.println("20x4 LCD initialized.");
}

void updateLCD() {
  if (!LCD_CONNECTED || !lcdReady) return;

  String line0 = "IN T:";
  String line1 = "IN H:";

  if (indoorDataValid()) {
    line0 += String(indoorTemperature, 1);
    line0 += (char)0;
    line0 += "C";

    line1 += String(indoorHumidity, 1);
    line1 += "%";
  } else {
    line0 += "SENSOR ERROR";
    line1 += "--";
  }

  String line2;
  String line3;

  if (outdoorDataValid()) {
    line2 = "OUT T:";
    line2 += String(outdoorTemperature, 1);
    line2 += (char)0;
    line2 += "C";

    line3 = "OUT H:";
    line3 += String(outdoorHumidity, 1);
    line3 += "%";
  } else {
    line2 = "OUTDOOR: OFF";
    line3 = "NO FRESH DATA";
  }

  printLCDLine(0, line0);
  printLCDLine(1, line1);
  printLCDLine(2, line2);
  printLCDLine(3, line3);
}



void startNTP() {
  if (WiFi.status() != WL_CONNECTED || ntpStarted) return;

  configTime(
    GMT_OFFSET_SECONDS,
    DAYLIGHT_OFFSET_SECONDS,
    "pool.ntp.org",
    "time.google.com",
    "time.nist.gov"
  );

  ntpStarted = true;
  Serial.println("NTP configured for Nepal time.");
}

bool clockValid() {
  return time(nullptr) > 1700000000;
}

String getTimestamp() {
  struct tm timeInfo;

  if (getLocalTime(&timeInfo, 1000)) {
    char buffer[25];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
    return String(buffer);
  }

  return "UNSYNCED_" + String(millis());
}



void startWiFiConnection() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lastWiFiAttempt = millis();
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  unsigned long now = millis();

  if (lastWiFiAttempt == 0 ||
      now - lastWiFiAttempt >= WIFI_RETRY_INTERVAL_MS) {
    Serial.println("Wi-Fi unavailable. Retrying...");
    WiFi.disconnect();
    startWiFiConnection();
  }
}



void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[64];
  unsigned int copyLength = length;

  if (copyLength >= sizeof(message)) copyLength = sizeof(message) - 1;

  memcpy(message, payload, copyLength);
  message[copyLength] = '\0';

  String valueText(message);
  valueText.trim();


  if (strcmp(topic, TOPIC_OUTDOOR_STATUS) == 0) {
    if (valueText.equalsIgnoreCase("ONLINE")) {
      if (!outdoorOnline) Serial.println("Outdoor node status: ONLINE");
      outdoorOnline = true;
    } else {
      Serial.print("Outdoor node status: ");
      Serial.println(valueText.length() ? valueText : "OFFLINE");
      outdoorOnline = false;
      Serial.println("Keeping last outdoor reading on LCD.");
    }

    updateLCD();
    return;
  }

 
  if (valueText.length() == 0) return;

  char* endPointer = nullptr;
  float value = strtof(valueText.c_str(), &endPointer);

  if (endPointer == valueText.c_str() || !isfinite(value)) {
    Serial.print("Invalid MQTT value on ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(valueText);
    return;
  }

  if (strcmp(topic, TOPIC_OUTDOOR_TEMP) == 0) {
    if (value < -60.0f || value > 100.0f) {
      Serial.println("Rejected implausible outdoor temperature.");
      return;
    }

    outdoorTemperature = value;
    outdoorTempReceivedAt = millis();

    Serial.print("Outdoor temperature: ");
    Serial.print(value, 2);
    Serial.println(" C");
  }

  else if (strcmp(topic, TOPIC_OUTDOOR_HUM) == 0) {
    if (value < 0.0f || value > 100.0f) {
      Serial.println("Rejected implausible outdoor humidity.");
      return;
    }

    outdoorHumidity = value;
    outdoorHumReceivedAt = millis();

    Serial.print("Outdoor humidity: ");
    Serial.print(value, 2);
    Serial.println(" %");
  }
}


void maintainMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (mqttClient.connected()) {
    mqttClient.loop();
    return;
  }

  unsigned long now = millis();

  if (lastMQTTAttempt != 0 &&
      now - lastMQTTAttempt < MQTT_RETRY_INTERVAL_MS) return;

  lastMQTTAttempt = now;

  String clientID = "ESP32IndoorHub-";
  clientID += String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("Connecting to MQTT...");

  if (mqttClient.connect(clientID.c_str())) {
    Serial.println("connected.");

    bool t = mqttClient.subscribe(TOPIC_OUTDOOR_TEMP);
    bool h = mqttClient.subscribe(TOPIC_OUTDOOR_HUM);
    bool s = mqttClient.subscribe(TOPIC_OUTDOOR_STATUS);

    Serial.print("Temperature subscription: ");
    Serial.println(t ? "OK" : "FAILED");

    Serial.print("Humidity subscription: ");
    Serial.println(h ? "OK" : "FAILED");

    Serial.print("Status subscription: ");
    Serial.println(s ? "OK" : "FAILED");

    mqttClient.loop();
  } else {
    Serial.print("MQTT failed. State: ");
    Serial.println(mqttClient.state());
  }
}



bool initializeSHT31() {
  lastSHT31Attempt = millis();

  Serial.println("Checking indoor SHT31...");
  sht31Ready = indoorSHT31.begin(SHT31_ADDRESS);

  if (sht31Ready) {
    Serial.println("SHT31 detected at 0x44.");
    return true;
  }

  indoorTemperature = NAN;
  indoorHumidity = NAN;

  Serial.println("SHT31 not detected. Will retry automatically.");
  return false;
}

bool readIndoorSensor() {
  if (!sht31Ready) return false;

  float temperature = indoorSHT31.readTemperature();
  float humidity = indoorSHT31.readHumidity();

  if (!isfinite(temperature) ||
      !isfinite(humidity) ||
      temperature < -40.0f ||
      temperature > 125.0f ||
      humidity < 0.0f ||
      humidity > 100.0f) {

    indoorTemperature = NAN;
    indoorHumidity = NAN;
    sht31Ready = false;

    Serial.println("Invalid indoor SHT31 reading. Reinitializing...");
    return false;
  }

  indoorTemperature = temperature;
  indoorHumidity = humidity;
  return true;
}

void printCombinedReading() {
  Serial.println("--------------------------------");

  if (indoorDataValid()) {
    Serial.print("Indoor  : ");
    Serial.print(indoorTemperature, 2);
    Serial.print(" C, ");
    Serial.print(indoorHumidity, 2);
    Serial.println(" %");
  } else {
    Serial.println("Indoor  : SENSOR ERROR");
  }

  if (outdoorDataValid()) {
    Serial.print("Outdoor : ");
    Serial.print(outdoorTemperature, 2);
    Serial.print(" C, ");
    Serial.print(outdoorHumidity, 2);
    Serial.println(" %");
  } else {
    Serial.println("Outdoor : OFF / NO FRESH DATA");
  }
}



void recoverPendingFiles() {
  if (!SD.exists(PENDING_FILE) && SD.exists(PENDING_BACKUP_FILE)) {
    SD.rename(PENDING_BACKUP_FILE, PENDING_FILE);
  }

  if (SD.exists(PENDING_FILE) && SD.exists(PENDING_BACKUP_FILE)) {
    SD.remove(PENDING_BACKUP_FILE);
  }

  if (SD.exists(PENDING_TEMP_FILE)) {
    SD.remove(PENDING_TEMP_FILE);
  }
}

bool initializeSDCard() {
  lastSDAttempt = millis();

  Serial.println("Initializing microSD card...");

  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
  delay(50);

  if (!SD.begin(SD_CS_PIN, SPI, 4000000)) {
    Serial.println("SD initialization failed. Will retry automatically.");
    return false;
  }

  if (SD.cardType() == CARD_NONE) {
    Serial.println("No microSD card detected.");
    SD.end();
    return false;
  }

  recoverPendingFiles();

  if (!SD.exists(DATA_FILE)) {
    File file = SD.open(DATA_FILE, FILE_WRITE);
    if (!file) {
      Serial.println("Could not create /data.csv.");
      SD.end();
      return false;
    }

    file.println(CSV_HEADER);
    file.flush();
    file.close();
    Serial.println("Created /data.csv.");
  }

  if (!SD.exists(PENDING_FILE)) {
    File file = SD.open(PENDING_FILE, FILE_WRITE);
    if (file) file.close();
  }

  Serial.print("SD card ready. Size: ");
  Serial.print(SD.cardSize() / (1024ULL * 1024ULL));
  Serial.println(" MB");

  return true;
}

bool appendLineToFile(const char* path, const String& line) {
  if (!sdReady) return false;

  File file = SD.open(path, FILE_APPEND);

  if (!file) {
    Serial.print("Could not open ");
    Serial.println(path);
    sdReady = false;
    SD.end();
    return false;
  }

  size_t written = file.println(line);
  file.flush();
  file.close();

  return written > 0;
}



String makeCSVLine(
  const String& recordID,
  const String& timestamp,
  float indoorTemp,
  float indoorHum,
  bool outdoorValid,
  float outdoorTemp,
  float outdoorHum
) {
  String line;
  line.reserve(200);

  line += recordID;
  line += ",";
  line += timestamp;
  line += ",";
  line += String(indoorTemp, 2);
  line += ",";
  line += String(indoorHum, 2);
  line += ",";

  if (outdoorValid) line += String(outdoorTemp, 2);

  line += ",";

  if (outdoorValid) line += String(outdoorHum, 2);

  line += ",";
  line += outdoorValid ? "ON" : "OFF";

  return line;
}



bool googleResponseSuccess(const String& response) {
  return response.indexOf("\"success\":true") >= 0 ||
         response.indexOf("\"success\": true") >= 0;
}

bool uploadRecordToGoogle(
  const String& recordID,
  const String& timestamp,
  float indoorTemp,
  float indoorHum,
  bool outdoorValid,
  float outdoorTemp,
  float outdoorHum
) {
  if (WiFi.status() != WL_CONNECTED) return false;

  String json;
  json.reserve(360);

  json += "{";
  json += "\"record_id\":\"" + recordID + "\",";
  json += "\"timestamp\":\"" + timestamp + "\",";
  json += "\"indoor_temperature\":" + String(indoorTemp, 2) + ",";
  json += "\"indoor_humidity\":" + String(indoorHum, 2) + ",";

  json += "\"outdoor_temperature\":";
  json += outdoorValid ? String(outdoorTemp, 2) : "null";
  json += ",";

  json += "\"outdoor_humidity\":";
  json += outdoorValid ? String(outdoorHum, 2) : "null";
  json += ",";

  json += "\"outdoor_status\":\"";
  json += outdoorValid ? "ON" : "OFF";
  json += "\"}";

  Serial.print("Uploading record to Google: ");
  Serial.println(recordID);
  Serial.print("Google JSON: ");
  Serial.println(json);

  const uint8_t MAX_ATTEMPTS = 3;

  for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure postClient;
    postClient.setInsecure();

    HTTPClient postHttp;
    const char* responseHeaders[] = { "Location" };
    postHttp.collectHeaders(responseHeaders, 1);

    if (!postHttp.begin(postClient, GOOGLE_WEB_APP_URL)) {
      Serial.println("Google POST begin failed.");
      delay(1000UL * attempt);
      continue;
    }

    postHttp.setConnectTimeout(15000);
    postHttp.setTimeout(25000);
    postHttp.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    postHttp.setReuse(false);
    postHttp.addHeader("Content-Type", "application/json");
    postHttp.addHeader("Accept", "application/json");
    postHttp.addHeader("Connection", "close");

    int postCode = postHttp.POST(json);
    String postBody = postCode > 0 ? postHttp.getString() : "";
    String redirectURL = postHttp.header("Location");

    Serial.print("Google POST code: ");
    Serial.println(postCode);

    if (postCode >= 200 && postCode < 300) {
      Serial.print("Google response: ");
      Serial.println(postBody);

      bool ok = googleResponseSuccess(postBody);
      postHttp.end();

      if (ok) {
        Serial.println("Google Sheets upload successful.");
        return true;
      }
    }

    else if ((postCode == 301 || postCode == 302 || postCode == 303 ||
              postCode == 307 || postCode == 308) &&
             redirectURL.length() > 0) {

      postHttp.end();

      WiFiClientSecure getClient;
      getClient.setInsecure();

      HTTPClient getHttp;

      if (getHttp.begin(getClient, redirectURL)) {
        getHttp.setConnectTimeout(15000);
        getHttp.setTimeout(25000);
        getHttp.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        getHttp.setReuse(false);
        getHttp.addHeader("Accept", "application/json");
        getHttp.addHeader("Connection", "close");

        int getCode = getHttp.GET();
        String response = getCode > 0 ? getHttp.getString() : "";

        Serial.print("Google redirect GET code: ");
        Serial.println(getCode);
        Serial.print("Google response: ");
        Serial.println(response);

        bool ok = getCode >= 200 && getCode < 300 &&
                  googleResponseSuccess(response);

        getHttp.end();

        if (ok) {
          Serial.println("Google Sheets upload successful.");
          return true;
        }
      }
    }

    else {
      if (postCode <= 0) {
        Serial.print("Google HTTP error: ");
        Serial.println(postHttp.errorToString(postCode));
      } else {
        Serial.print("Google POST response: ");
        Serial.println(postBody);
      }

      postHttp.end();
    }

    if (attempt < MAX_ATTEMPTS) {
      Serial.println("Google upload not confirmed. Retrying...");
      delay(1500UL * attempt);
    }
  }

  Serial.println("Google upload failed; record remains on SD pending queue.");
  return false;
}



bool parsePendingLine(
  const String& line,
  String& recordID,
  String& timestamp,
  float& indoorTemp,
  float& indoorHum,
  bool& outdoorValid,
  float& outdoorTemp,
  float& outdoorHum
) {
  int p1 = line.indexOf(',');
  int p2 = line.indexOf(',', p1 + 1);
  int p3 = line.indexOf(',', p2 + 1);
  int p4 = line.indexOf(',', p3 + 1);
  int p5 = line.indexOf(',', p4 + 1);
  int p6 = line.indexOf(',', p5 + 1);

  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p6 < 0) {
    return false;
  }

  recordID = line.substring(0, p1);
  timestamp = line.substring(p1 + 1, p2);
  indoorTemp = line.substring(p2 + 1, p3).toFloat();
  indoorHum = line.substring(p3 + 1, p4).toFloat();

  String outdoorTempText = line.substring(p4 + 1, p5);
  String outdoorHumText = line.substring(p5 + 1, p6);
  String statusText = line.substring(p6 + 1);
  statusText.trim();

  outdoorValid = statusText.equalsIgnoreCase("ON") &&
                 outdoorTempText.length() > 0 &&
                 outdoorHumText.length() > 0;

  if (outdoorValid) {
    outdoorTemp = outdoorTempText.toFloat();
    outdoorHum = outdoorHumText.toFloat();
  } else {
    outdoorTemp = NAN;
    outdoorHum = NAN;
  }

  return true;
}

bool replacePendingQueueSafely() {
  if (SD.exists(PENDING_BACKUP_FILE)) SD.remove(PENDING_BACKUP_FILE);

  if (SD.exists(PENDING_FILE)) {
    if (!SD.rename(PENDING_FILE, PENDING_BACKUP_FILE)) {
      Serial.println("Could not create pending queue backup.");
      return false;
    }
  }

  if (!SD.rename(PENDING_TEMP_FILE, PENDING_FILE)) {
    Serial.println("Could not activate new pending queue. Restoring backup.");

    if (SD.exists(PENDING_BACKUP_FILE)) {
      SD.rename(PENDING_BACKUP_FILE, PENDING_FILE);
    }

    return false;
  }

  if (SD.exists(PENDING_BACKUP_FILE)) SD.remove(PENDING_BACKUP_FILE);
  return true;
}

void syncPendingRecords() {
  if (!sdReady || WiFi.status() != WL_CONNECTED) return;
  if (!SD.exists(PENDING_FILE)) return;

  File pending = SD.open(PENDING_FILE, FILE_READ);
  if (!pending) return;

  if (pending.size() == 0) {
    pending.close();
    return;
  }

  if (SD.exists(PENDING_TEMP_FILE)) SD.remove(PENDING_TEMP_FILE);

  File temporary = SD.open(PENDING_TEMP_FILE, FILE_WRITE);
  if (!temporary) {
    pending.close();
    Serial.println("Could not create pending temporary file.");
    return;
  }

  uint8_t uploadedThisPass = 0;
  bool stopUploading = false;

  while (pending.available()) {
    String line = pending.readStringUntil('\n');
    line.trim();

    if (line.length() == 0) continue;

    if (stopUploading || uploadedThisPass >= MAX_PENDING_UPLOADS_PER_SYNC) {
      temporary.println(line);
      continue;
    }

    String recordID;
    String timestamp;
    float indoorTemp;
    float indoorHum;
    bool outdoorValid;
    float outdoorTemp;
    float outdoorHum;

    if (!parsePendingLine(
          line,
          recordID,
          timestamp,
          indoorTemp,
          indoorHum,
          outdoorValid,
          outdoorTemp,
          outdoorHum)) {
      Serial.println("Malformed pending row preserved.");
      temporary.println(line);
      continue;
    }

    mqttClient.loop();

    bool uploaded = uploadRecordToGoogle(
      recordID,
      timestamp,
      indoorTemp,
      indoorHum,
      outdoorValid,
      outdoorTemp,
      outdoorHum
    );

    if (uploaded) {
      uploadedThisPass++;
      Serial.print("Pending record synced: ");
      Serial.println(recordID);
    } else {
      temporary.println(line);
      stopUploading = true;
      Serial.println("Pending sync paused. Record remains on SD.");
    }
  }

  pending.close();
  temporary.flush();
  temporary.close();

  if (!replacePendingQueueSafely()) {
    Serial.println("Pending queue replacement failed.");
  }
}



void createScheduledRecord() {
  if (!sht31Ready || !readIndoorSensor()) {
    Serial.println("Record skipped: indoor sensor unavailable.");
    return;
  }

  if (!clockValid()) {
    Serial.println("Record delayed: waiting for valid NTP time.");
    return;
  }

  if (!sdReady) {
    Serial.println("Record skipped: SD card unavailable (prevents SD/Google mismatch).");
    return;
  }

  bool outdoorValid = outdoorDataValid();

  String timestamp = getTimestamp();

  String recordID = String((uint32_t)ESP.getEfuseMac(), HEX);
  recordID += "-";
  recordID += String((uint32_t)time(nullptr));
  recordID += "-";
  recordID += String(millis());

  String csvLine = makeCSVLine(
    recordID,
    timestamp,
    indoorTemperature,
    indoorHumidity,
    outdoorValid,
    outdoorTemperature,
    outdoorHumidity
  );

  Serial.println();
  Serial.println("----------------------------");
  Serial.println("FIFTEEN-MINUTE RECORD");
  Serial.print("Timestamp: ");
  Serial.println(timestamp);
  printCombinedReading();

  bool permanentSaved = appendLineToFile(DATA_FILE, csvLine);
  bool pendingSaved = appendLineToFile(PENDING_FILE, csvLine);

  Serial.print("SD data.csv save: ");
  Serial.println(permanentSaved ? "OK" : "FAILED");

  Serial.print("SD pending queue: ");
  Serial.println(pendingSaved ? "OK" : "FAILED");

  // Upload the newly queued record(s) if possible.
  if (permanentSaved && pendingSaved && WiFi.status() == WL_CONNECTED) {
    syncPendingRecords();
  }

  updateLCD();
}



void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("--------------------------------");
  Serial.println(" ESP32 INDOOR ENVIRONMENT HUB");
  Serial.println(" ROBUST FINAL VERSION");
  Serial.println("--------------------------------");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(100);

  initializeLCD();
  initializeSHT31();

  if (sht31Ready && readIndoorSensor()) {
    printCombinedReading();
  }

  sdReady = initializeSDCard();

  startWiFiConnection();

  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);
  mqttClient.setKeepAlive(15);
  mqttClient.setSocketTimeout(15);

  unsigned long now = millis();
  lastDisplayTime = now;
  lastRecordTime = now;
  lastPendingSyncTime = now;
}



void loop() {
  maintainWiFi();

  if (WiFi.status() == WL_CONNECTED) startNTP();

  maintainMQTT();
  maintainOutdoorFreshness();

  unsigned long now = millis();

  
  if (!sht31Ready &&
      (lastSHT31Attempt == 0 ||
       now - lastSHT31Attempt >= SHT31_RETRY_INTERVAL_MS)) {

    if (initializeSHT31() && readIndoorSensor()) {
      Serial.println("Indoor SHT31 recovered successfully.");
    }
  }


  if (!sdReady &&
      (lastSDAttempt == 0 ||
       now - lastSDAttempt >= SD_RETRY_INTERVAL_MS)) {
    sdReady = initializeSDCard();
  }

  
  if (now - lastDisplayTime >= DISPLAY_INTERVAL_MS) {
    lastDisplayTime = now;

    if (sht31Ready) readIndoorSensor();

    printCombinedReading();
    updateLCD();
  }

  
  if (sdReady &&
      WiFi.status() == WL_CONNECTED &&
      now - lastPendingSyncTime >= PENDING_SYNC_INTERVAL_MS) {

    lastPendingSyncTime = now;
    syncPendingRecords();
  }

  
  if (now - lastRecordTime >= RECORD_INTERVAL_MS) {
    lastRecordTime = now;
    createScheduledRecord();
  }

  delay(20);
}