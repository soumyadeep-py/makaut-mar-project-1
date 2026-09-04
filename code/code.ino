#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LittleFS.h>

#include <Adafruit_AHTX0.h>
#include <BH1750.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
//                      PIN DEFINITIONS
// ============================================================

#define SOIL_PIN        34

#define I2C_SDA        21
#define I2C_SCL        22

#define BUTTON_PIN     27

#define RGB_R_PIN      25
#define RGB_G_PIN      26
#define RGB_B_PIN      33

#define BUZZER_PIN     14


// ============================================================
//                         OLED
// ============================================================

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define OLED_ADDRESS    0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);

bool oledDetected = false;


// ============================================================
//                         SENSORS
// ============================================================

Adafruit_AHTX0 aht;
BH1750 bh1750;

bool ahtDetected  = false;
bool bhDetected   = false;


// ============================================================
//                          WEB SERVER
// ============================================================

WebServer server(80);

const char* AP_SSID     = "SmartPlant-ESP32";
const char* AP_PASSWORD = "plant1234";

bool wifiActive = false;


// ============================================================
//                     WIFI TIMEOUT
// ============================================================

const unsigned long WIFI_NO_CLIENT_TIMEOUT = 60000UL;

unsigned long wifiDisconnectTime = 0;


// ============================================================
//                     SENSOR STATES
// ============================================================

enum SensorID {
  SOIL_SENSOR,
  AHT_SENSOR,
  LIGHT_SENSOR
};

enum SensorState {
  SENSOR_RESTING,
  SENSOR_ACTIVE,
  SENSOR_NOT_FOUND,
  SENSOR_FAULT
};


// ============================================================
//                     SYSTEM MODES
// ============================================================

enum SystemMode {
  NORMAL_MODE,
  ALERT_MODE
};

SystemMode systemMode = NORMAL_MODE;


// ============================================================
//                  NORMAL SENSOR CYCLING
// ============================================================
//
// One sensor:
//
// ACTIVE  = 3 seconds
// RESTING = 5 seconds
//
// Sequence:
//
// Soil    → 3s ACTIVE → 5s REST
// AHT20   → 3s ACTIVE → 5s REST
// BH1750  → 3s ACTIVE → 5s REST
//
// Full cycle = 24 seconds
// ============================================================

const unsigned long SENSOR_ACTIVE_TIME = 3000UL;
const unsigned long SENSOR_REST_TIME   = 5000UL;

const unsigned long SENSOR_READ_INTERVAL = 1000UL;

SensorID currentSensor = SOIL_SENSOR;

unsigned long sensorCycleStart = 0;
unsigned long lastSensorRead   = 0;


// ============================================================
//                       SENSOR DATA
// ============================================================

float soilPercent = NAN;

float temperature = NAN;
float humidity    = NAN;

float lightLux = NAN;


// ============================================================
//                     SOIL CALIBRATION
// ============================================================
//
// CHANGE THESE AFTER TESTING YOUR SENSOR.
//
// Usually:
//
// Higher ADC → drier soil
// Lower ADC  → wetter soil
//
// Measure:
// 1. Dry soil
// 2. Wet soil
//
// Then replace these values.
// ============================================================

const int SOIL_DRY_RAW = 3000;
const int SOIL_WET_RAW = 1500;


// ============================================================
//                       THRESHOLDS
// ============================================================
// These are example values.
// Adjust according to your plant.
//
// HYSTERESIS:
//
// ALERT threshold and CLEAR threshold are different.
//
// Example:
// Soil < 30%  → ALERT
// Soil > 35%  → clear alert
// ============================================================

// ---------------- SOIL ----------------

const float SOIL_ALERT_LOW = 30.0;
const float SOIL_CLEAR_LOW = 35.0;


// ---------------- TEMPERATURE ----------------

const float TEMP_ALERT_HIGH = 32.0;
const float TEMP_CLEAR_HIGH = 30.0;


// ---------------- HUMIDITY ----------------

const float HUM_ALERT_LOW  = 35.0;
const float HUM_CLEAR_LOW  = 40.0;

const float HUM_ALERT_HIGH = 85.0;
const float HUM_CLEAR_HIGH = 80.0;


// ---------------- LIGHT ----------------

const float LIGHT_ALERT_LOW = 100.0;
const float LIGHT_CLEAR_LOW = 150.0;


// ============================================================
//                        OLED SLIDESHOW
// ============================================================

const unsigned long OLED_SLIDE_TIME = 2500UL;

unsigned long oledLastChange = 0;

int oledPage = 0;

const int OLED_PAGE_COUNT = 6;


// ============================================================
//                          BUZZER
// ============================================================

bool buzzerActive = false;

unsigned long buzzerStart = 0;

const unsigned long BUZZER_BEEP_TIME = 150UL;


// ============================================================
//                          BUTTON
// ============================================================

unsigned long lastButtonChange = 0;

const unsigned long DEBOUNCE_TIME = 50UL;


// ============================================================
//                     UTILITY FUNCTIONS
// ============================================================

String sensorStateToString(SensorState state) {

  switch (state) {

    case SENSOR_ACTIVE:
      return "ACTIVE";

    case SENSOR_RESTING:
      return "RESTING";

    case SENSOR_NOT_FOUND:
      return "NOT FOUND";

    case SENSOR_FAULT:
      return "FAULT";
  }

  return "UNKNOWN";
}


// ============================================================
//                         RGB LED
// ============================================================

void setRGB(
  bool red,
  bool green,
  bool blue
) {

  digitalWrite(
    RGB_R_PIN,
    red ? HIGH : LOW
  );

  digitalWrite(
    RGB_G_PIN,
    green ? HIGH : LOW
  );

  digitalWrite(
    RGB_B_PIN,
    blue ? HIGH : LOW
  );
}


void updateRGB() {

  // RED = alert
  if (systemMode == ALERT_MODE) {

    setRGB(true, false, false);

    return;
  }


  // BLUE = Wi-Fi ON
  if (wifiActive) {

    setRGB(false, false, true);

    return;
  }


  // GREEN = normal and Wi-Fi OFF
  setRGB(false, true, false);
}


// ============================================================
//                          BUZZER
// ============================================================

void startBeep() {

  tone(
    BUZZER_PIN,
    2000
  );

  buzzerActive = true;

  buzzerStart = millis();
}


void updateBuzzer() {

  if (!buzzerActive)
    return;


  if (
    millis() - buzzerStart >=
    BUZZER_BEEP_TIME
  ) {

    noTone(BUZZER_PIN);

    buzzerActive = false;
  }
}


// ============================================================
//                      STARTUP BEEP
// ============================================================

void startupBeep() {

  tone(
    BUZZER_PIN,
    1800
  );

  delay(120);

  noTone(
    BUZZER_PIN
  );

  delay(100);

  tone(
    BUZZER_PIN,
    2200
  );

  delay(120);

  noTone(
    BUZZER_PIN
  );
}


// ============================================================
//                       SOIL SENSOR
// ============================================================

SensorState soilState = SENSOR_RESTING;


// ------------------------------------------------------------
// Read soil sensor
// ------------------------------------------------------------

void readSoil() {

  int raw = analogRead(
    SOIL_PIN
  );


  Serial.print(
    "Soil RAW: "
  );

  Serial.println(raw);


  // ----------------------------------------------------------
  // Basic invalid-reading detection
  // ----------------------------------------------------------

  if (
    raw < 20 ||
    raw > 4080
  ) {

    soilState = SENSOR_FAULT;

    soilPercent = NAN;

    return;
  }


  // ----------------------------------------------------------
  // Valid reading
  // ----------------------------------------------------------

  soilState = SENSOR_ACTIVE;


  soilPercent = map(
    raw,
    SOIL_DRY_RAW,
    SOIL_WET_RAW,
    0,
    100
  );


  soilPercent = constrain(
    soilPercent,
    0,
    100
  );


  Serial.print(
    "Soil Moisture: "
  );

  Serial.print(
    soilPercent,
    1
  );

  Serial.println("%");
}


// ============================================================
//                        AHT20 SENSOR
// ============================================================

SensorState ahtState = SENSOR_NOT_FOUND;


// ------------------------------------------------------------
// Read AHT20
// ------------------------------------------------------------

void readAHT() {

  if (!ahtDetected) {

    ahtState = SENSOR_NOT_FOUND;

    return;
  }


  sensors_event_t humidityEvent;
  sensors_event_t temperatureEvent;


  aht.getEvent(
    &humidityEvent,
    &temperatureEvent
  );


  temperature =
    temperatureEvent.temperature;

  humidity =
    humidityEvent.relative_humidity;


  if (
    isnan(temperature) ||
    isnan(humidity)
  ) {

    ahtState = SENSOR_FAULT;

    return;
  }


  ahtState = SENSOR_ACTIVE;


  Serial.print(
    "Temperature: "
  );

  Serial.print(
    temperature,
    1
  );

  Serial.println(" C");


  Serial.print(
    "Humidity: "
  );

  Serial.print(
    humidity,
    1
  );

  Serial.println(" %");
}


// ============================================================
//                       BH1750 SENSOR
// ============================================================

SensorState lightState = SENSOR_NOT_FOUND;


// ------------------------------------------------------------
// Read BH1750
// ------------------------------------------------------------

void readBH1750() {

  if (!bhDetected) {

    lightState = SENSOR_NOT_FOUND;

    return;
  }


  float lux =
    bh1750.readLightLevel();


  if (lux < 0) {

    lightState = SENSOR_FAULT;

    lightLux = NAN;

    return;
  }


  lightLux = lux;

  lightState = SENSOR_ACTIVE;


  Serial.print(
    "Light: "
  );

  Serial.print(
    lightLux,
    1
  );

  Serial.println(" lux");
}


// ============================================================
//                   SENSOR CYCLE MANAGEMENT
// ============================================================

void startSensor(SensorID sensor) {

  currentSensor = sensor;

  sensorCycleStart = millis();

  lastSensorRead = 0;


  if (sensor == SOIL_SENSOR) {

    if (soilState != SENSOR_FAULT)
      soilState = SENSOR_ACTIVE;
  }


  else if (sensor == AHT_SENSOR) {

    if (ahtDetected)
      ahtState = SENSOR_ACTIVE;
    else
      ahtState = SENSOR_NOT_FOUND;
  }


  else if (sensor == LIGHT_SENSOR) {

    if (bhDetected)
      lightState = SENSOR_ACTIVE;
    else
      lightState = SENSOR_NOT_FOUND;
  }
}


// ------------------------------------------------------------
// Move current sensor into RESTING state
// ------------------------------------------------------------

void setCurrentSensorResting() {

  if (currentSensor == SOIL_SENSOR) {

    if (soilState == SENSOR_ACTIVE)
      soilState = SENSOR_RESTING;
  }


  else if (currentSensor == AHT_SENSOR) {

    if (ahtState == SENSOR_ACTIVE)
      ahtState = SENSOR_RESTING;
  }


  else if (currentSensor == LIGHT_SENSOR) {

    if (lightState == SENSOR_ACTIVE)
      lightState = SENSOR_RESTING;
  }
}


// ------------------------------------------------------------
// Move to next sensor
// ------------------------------------------------------------

void advanceSensor() {

  setCurrentSensorResting();


  if (currentSensor == SOIL_SENSOR) {

    startSensor(AHT_SENSOR);
  }


  else if (currentSensor == AHT_SENSOR) {

    startSensor(LIGHT_SENSOR);
  }


  else {

    startSensor(SOIL_SENSOR);
  }
}


// ============================================================
//                    NORMAL SENSOR MODE
// ============================================================

void normalSensorTask() {

  unsigned long elapsed =
    millis() - sensorCycleStart;


  // ==========================================================
  // ACTIVE PERIOD
  // ==========================================================

  if (elapsed < SENSOR_ACTIVE_TIME) {

    if (
      lastSensorRead == 0 ||
      millis() - lastSensorRead >=
      SENSOR_READ_INTERVAL
    ) {

      lastSensorRead = millis();


      if (currentSensor == SOIL_SENSOR) {

        readSoil();
      }


      else if (currentSensor == AHT_SENSOR) {

        readAHT();
      }


      else if (currentSensor == LIGHT_SENSOR) {

        readBH1750();
      }
    }


    return;
  }


  // ==========================================================
  // REST PERIOD
  // ==========================================================

  if (
    elapsed <
    SENSOR_ACTIVE_TIME +
    SENSOR_REST_TIME
  ) {

    // Keep sensor in resting state

    if (currentSensor == SOIL_SENSOR) {

      if (soilState == SENSOR_ACTIVE)
        soilState = SENSOR_RESTING;
    }


    else if (currentSensor == AHT_SENSOR) {

      if (ahtState == SENSOR_ACTIVE)
        ahtState = SENSOR_RESTING;
    }


    else if (currentSensor == LIGHT_SENSOR) {

      if (lightState == SENSOR_ACTIVE)
        lightState = SENSOR_RESTING;
    }


    return;
  }


  // ==========================================================
  // NEXT SENSOR
  // ==========================================================

  advanceSensor();
}


// ============================================================
//                        ALERT LOGIC
// ============================================================


// ------------------------------------------------------------
// Has any threshold been exceeded?
// ------------------------------------------------------------

bool alertConditionDetected() {

  // Soil too dry
  if (!isnan(soilPercent)) {

    if (
      soilPercent <
      SOIL_ALERT_LOW
    ) {

      return true;
    }
  }


  // Temperature too high
  if (!isnan(temperature)) {

    if (
      temperature >
      TEMP_ALERT_HIGH
    ) {

      return true;
    }
  }


  // Humidity too low/high
  if (!isnan(humidity)) {

    if (
      humidity < HUM_ALERT_LOW ||
      humidity > HUM_ALERT_HIGH
    ) {

      return true;
    }
  }


  // Light too low
  if (!isnan(lightLux)) {

    if (
      lightLux <
      LIGHT_ALERT_LOW
    ) {

      return true;
    }
  }


  return false;
}


// ------------------------------------------------------------
// HYSTERESIS CLEAR CONDITION
// ------------------------------------------------------------
//
// Alert isn't cleared immediately at the same threshold.
//
// Example:
//
// Soil <30%       → ALERT
// Soil >35%       → safe again
//
// This prevents rapid ON/OFF switching.
// ------------------------------------------------------------

bool alertConditionCleared() {

  // Soil
  if (!isnan(soilPercent)) {

    if (
      soilPercent <
      SOIL_CLEAR_LOW
    ) {

      return false;
    }
  }


  // Temperature
  if (!isnan(temperature)) {

    if (
      temperature >
      TEMP_CLEAR_HIGH
    ) {

      return false;
    }
  }


  // Humidity
  if (!isnan(humidity)) {

    if (
      humidity < HUM_CLEAR_LOW ||
      humidity > HUM_CLEAR_HIGH
    ) {

      return false;
    }
  }


  // Light
  if (!isnan(lightLux)) {

    if (
      lightLux <
      LIGHT_CLEAR_LOW
    ) {

      return false;
    }
  }


  return true;
}


// ============================================================
//                      ENTER ALERT MODE
// ============================================================

void enterAlertMode() {

  if (
    systemMode ==
    ALERT_MODE
  ) {

    return;
  }


  systemMode =
    ALERT_MODE;


  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    "       ALERT MODE ACTIVE"
  );

  Serial.println(
    "================================"
  );


  setRGB(
    true,
    false,
    false
  );


  startBeep();
}


// ============================================================
//                       EXIT ALERT MODE
// ============================================================

void exitAlertMode() {

  if (
    systemMode ==
    NORMAL_MODE
  ) {

    return;
  }


  systemMode =
    NORMAL_MODE;


  Serial.println();
  Serial.println(
    "Returning to NORMAL MODE"
  );


  updateRGB();
}


// ============================================================
//                   ALERT SENSOR POLLING
// ============================================================

unsigned long lastAlertRead = 0;

const unsigned long ALERT_READ_INTERVAL = 1000UL;


void alertSensorTask() {

  if (
    millis() - lastAlertRead <
    ALERT_READ_INTERVAL
  ) {

    return;
  }


  lastAlertRead = millis();


  // ==========================================================
  // In alert mode, all sensors are monitored
  // ==========================================================

  readSoil();

  readAHT();

  readBH1750();


  // ==========================================================
  // Keep available sensors marked ACTIVE
  // ==========================================================

  if (soilState == SENSOR_RESTING)
    soilState = SENSOR_ACTIVE;

  if (ahtDetected &&
      ahtState == SENSOR_RESTING)
    ahtState = SENSOR_ACTIVE;

  if (bhDetected &&
      lightState == SENSOR_RESTING)
    lightState = SENSOR_ACTIVE;


  // ==========================================================
  // Check conditions
  // ==========================================================

  if (
    alertConditionDetected()
  ) {

    enterAlertMode();
  }


  else if (
    alertConditionCleared()
  ) {

    exitAlertMode();
  }
}


// ============================================================
//                       WIFI START
// ============================================================

void startWiFiAP() {

  if (wifiActive)
    return;


  Serial.println();
  Serial.println(
    "Starting Wi-Fi Access Point..."
  );


  WiFi.mode(WIFI_AP);


  bool result =
    WiFi.softAP(
      AP_SSID,
      AP_PASSWORD
    );


  if (!result) {

    Serial.println(
      "ERROR: Failed to start Wi-Fi AP"
    );

    return;
  }


  delay(200);


  IPAddress ip =
    WiFi.softAPIP();


  Serial.println();
  Serial.println(
    "Wi-Fi AP started"
  );


  Serial.print(
    "SSID: "
  );

  Serial.println(
    AP_SSID
  );


  Serial.print(
    "Password: "
  );

  Serial.println(
    AP_PASSWORD
  );


  Serial.print(
    "IP address: "
  );

  Serial.println(
    ip
  );


  wifiActive = true;

  wifiDisconnectTime = 0;


  // Start web server
  server.begin();


  Serial.println(
    "Web server started"
  );


  updateRGB();
}


// ============================================================
//                       WIFI STOP
// ============================================================

void stopWiFiAP() {

  if (!wifiActive)
    return;


  Serial.println();
  Serial.println(
    "Stopping Wi-Fi..."
  );


  WiFi.softAPdisconnect(
    true
  );


  WiFi.mode(
    WIFI_OFF
  );


  wifiActive = false;

  wifiDisconnectTime = 0;


  Serial.println(
    "Wi-Fi OFF"
  );


  updateRGB();
}


// ============================================================
//                         BUTTON
// ============================================================
//
// GPIO 27:
//
// Not pressed → HIGH
// Pressed       → LOW
//
// Button connection:
//
// GPIO 27 ---- BUTTON ---- GND
// ============================================================

void checkButton() {

  static bool lastReading =
    HIGH;

  static bool stableState =
    HIGH;


  bool reading =
    digitalRead(
      BUTTON_PIN
    );


  // Detect change
  if (
    reading !=
    lastReading
  ) {

    lastButtonChange =
      millis();

    lastReading =
      reading;
  }


  // Debounce
  if (
    millis() -
    lastButtonChange >=
    DEBOUNCE_TIME
  ) {


    if (
      reading !=
      stableState
    ) {

      stableState =
        reading;


      // Button pressed
      if (
        stableState ==
        LOW
      ) {

        Serial.println(
          "BUTTON PRESSED"
        );


        if (!wifiActive) {

          startWiFiAP();
        }
      }
    }
  }
}


// ============================================================
//                     JSON HELPER
// ============================================================

String jsonFloat(
  float value,
  int decimals
) {

  if (isnan(value))
    return "null";


  return String(
    value,
    decimals
  );
}


// ============================================================
//                         WEB API
// ============================================================

void handleAPI() {

  String json = "{";


  // ----------------------------------------------------------
  // System
  // ----------------------------------------------------------

  json += "\"system\":\"";


  if (
    systemMode ==
    ALERT_MODE
  ) {

    json += "ALERT";

  } else {

    json += "NORMAL";
  }


  json += "\",";


  // ----------------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------------

  json += "\"wifi\":";

  json +=
    wifiActive
    ? "true"
    : "false";

  json += ",";


  // ----------------------------------------------------------
  // Soil
  // ----------------------------------------------------------

  json += "\"soil\":{";


  json += "\"value\":";

  json +=
    jsonFloat(
      soilPercent,
      1
    );

  json += ",";


  json += "\"state\":\"";

  json +=
    sensorStateToString(
      soilState
    );

  json += "\"},";


  // ----------------------------------------------------------
  // Temperature
  // ----------------------------------------------------------

  json += "\"temperature\":{";


  json += "\"value\":";

  json +=
    jsonFloat(
      temperature,
      1
    );

  json += ",";


  json += "\"state\":\"";

  json +=
    sensorStateToString(
      ahtState
    );

  json += "\"},";


  // ----------------------------------------------------------
  // Humidity
  // ----------------------------------------------------------

  json += "\"humidity\":{";


  json += "\"value\":";

  json +=
    jsonFloat(
      humidity,
      1
    );

  json += ",";


  json += "\"state\":\"";

  json +=
    sensorStateToString(
      ahtState
    );

  json += "\"},";


  // ----------------------------------------------------------
  // Light
  // ----------------------------------------------------------

  json += "\"light\":{";


  json += "\"value\":";

  json +=
    jsonFloat(
      lightLux,
      1
    );

  json += ",";


  json += "\"state\":\"";

  json +=
    sensorStateToString(
      lightState
    );

  json += "\"";


  json += "}";


  // ----------------------------------------------------------
  // End
  // ----------------------------------------------------------

  json += "}";


  server.send(
    200,
    "application/json",
    json
  );
}


// ============================================================
//                    SERVE INDEX.HTML
// ============================================================

void handleRoot() {

  File file =
    LittleFS.open(
      "/index.html",
      "r"
    );


  if (!file) {

    server.send(
      404,
      "text/plain",
      "index.html not found"
    );

    return;
  }


  server.streamFile(
    file,
    "text/html"
  );


  file.close();
}


// ============================================================
//                    SERVE STYLE.CSS
// ============================================================

void handleCSS() {

  File file =
    LittleFS.open(
      "/style.css",
      "r"
    );


  if (!file) {

    server.send(
      404,
      "text/plain",
      "style.css not found"
    );

    return;
  }


  server.streamFile(
    file,
    "text/css"
  );


  file.close();
}


// ============================================================
//                    SERVE SCRIPT.JS
// ============================================================

void handleJS() {

  File file =
    LittleFS.open(
      "/script.js",
      "r"
    );


  if (!file) {

    server.send(
      404,
      "text/plain",
      "script.js not found"
    );

    return;
  }


  server.streamFile(
    file,
    "application/javascript"
  );


  file.close();
}


// ============================================================
//                    SERVER NOT FOUND
// ============================================================

void handleNotFound() {

  server.send(
    404,
    "text/plain",
    "404 - Not Found"
  );
}


// ============================================================
//                    SETUP SERVER ROUTES
// ============================================================
//
// Routes are registered ONLY ONCE.
// Wi-Fi can be switched OFF/ON without
// registering the routes again.
// ============================================================

void setupServerRoutes() {

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/style.css",
    HTTP_GET,
    handleCSS
  );


  server.on(
    "/script.js",
    HTTP_GET,
    handleJS
  );


  server.on(
    "/api",
    HTTP_GET,
    handleAPI
  );


  server.onNotFound(
    handleNotFound
  );
}


// ============================================================
//                        WIFI TASK
// ============================================================
//
// Behavior:
//
// Boot
//  ↓
// Wi-Fi ON
//  ↓
// Device connected?
//  ↓
// YES → keep Wi-Fi ON
// NO  → start 1 minute timer
//  ↓
// 1 minute without device
//  ↓
// Wi-Fi OFF
//
// Button can wake Wi-Fi again.
// ============================================================

void wifiTask() {

  if (!wifiActive)
    return;


  server.handleClient();


  int connectedDevices =
    WiFi.softAPgetStationNum();


  // ----------------------------------------------------------
  // NO DEVICE CONNECTED
  // ----------------------------------------------------------

  if (
    connectedDevices == 0
  ) {


    if (
      wifiDisconnectTime == 0
    ) {

      wifiDisconnectTime =
        millis();


      Serial.println(
        "No device connected."
      );

      Serial.println(
        "Starting 1 minute shutdown timer..."
      );
    }


    if (
      millis() -
      wifiDisconnectTime >=
      WIFI_NO_CLIENT_TIMEOUT
    ) {

      Serial.println(
        "No device connected for 1 minute."
      );


      stopWiFiAP();

      return;
    }
  }


  // ----------------------------------------------------------
  // DEVICE CONNECTED
  // ----------------------------------------------------------

  else {

    // Cancel shutdown timer
    wifiDisconnectTime = 0;
  }
}


// ============================================================
//                        OLED HEADER
// ============================================================

void oledHeader(
  const char* title
) {

  display.clearDisplay();


  display.setTextColor(
    SSD1306_WHITE
  );


  display.setTextSize(1);


  display.setCursor(
    0,
    0
  );


  display.println(
    title
  );


  display.drawLine(
    0,
    10,
    127,
    10,
    SSD1306_WHITE
  );
}


// ============================================================
//                       OLED DISPLAY
// ============================================================

void showOLED() {

  if (!oledDetected)
    return;


  oledHeader(
    "SMART PLANT"
  );


  display.setCursor(
    0,
    17
  );


  // ----------------------------------------------------------
  // PAGE 0 - SYSTEM
  // ----------------------------------------------------------

  if (oledPage == 0) {

    display.println(
      "System:"
    );


    display.setTextSize(2);


    if (
      systemMode ==
      ALERT_MODE
    ) {

      display.println(
        "ALERT"
      );

    } else {

      display.println(
        "NORMAL"
      );
    }


    display.setTextSize(1);
  }


  // ----------------------------------------------------------
  // PAGE 1 - SOIL
  // ----------------------------------------------------------

  else if (oledPage == 1) {

    display.println(
      "Soil Moisture"
    );


    if (
      soilState ==
      SENSOR_NOT_FOUND
    ) {

      display.setTextSize(2);

      display.println(
        "N/A"
      );

      display.setTextSize(1);

      display.println(
        "NOT FOUND"
      );
    }


    else if (
      soilState ==
      SENSOR_FAULT
    ) {

      display.setTextSize(2);

      display.println(
        "ERROR"
      );

      display.setTextSize(1);

      display.println(
        "FAULT"
      );
    }


    else {

      display.setTextSize(2);

      display.print(
        soilPercent,
        1
      );

      display.println(
        "%"
      );

      display.setTextSize(1);

      display.println(
        sensorStateToString(
          soilState
        )
      );
    }
  }


  // ----------------------------------------------------------
  // PAGE 2 - TEMPERATURE
  // ----------------------------------------------------------

  else if (oledPage == 2) {

    display.println(
      "Temperature"
    );


    if (
      ahtState ==
      SENSOR_NOT_FOUND
    ) {

      display.setTextSize(2);

      display.println(
        "N/A"
      );

      display.setTextSize(1);

      display.println(
        "NOT FOUND"
      );
    }


    else if (
      ahtState ==
      SENSOR_FAULT
    ) {

      display.setTextSize(2);

      display.println(
        "ERROR"
      );

      display.setTextSize(1);

      display.println(
        "FAULT"
      );
    }


    else {

      display.setTextSize(2);

      display.print(
        temperature,
        1
      );

      display.println(
        " C"
      );

      display.setTextSize(1);

      display.println(
        sensorStateToString(
          ahtState
        )
      );
    }
  }


  // ----------------------------------------------------------
  // PAGE 3 - HUMIDITY
  // ----------------------------------------------------------

  else if (oledPage == 3) {

    display.println(
      "Humidity"
    );


    if (
      ahtState ==
      SENSOR_NOT_FOUND
    ) {

      display.setTextSize(2);

      display.println(
        "N/A"
      );

      display.setTextSize(1);

      display.println(
        "NOT FOUND"
      );
    }


    else if (
      ahtState ==
      SENSOR_FAULT
    ) {

      display.setTextSize(2);

      display.println(
        "ERROR"
      );

      display.setTextSize(1);

      display.println(
        "FAULT"
      );
    }


    else {

      display.setTextSize(2);

      display.print(
        humidity,
        1
      );

      display.println(
        "%"
      );

      display.setTextSize(1);

      display.println(
        sensorStateToString(
          ahtState
        )
      );
    }
  }


  // ----------------------------------------------------------
  // PAGE 4 - LIGHT
  // ----------------------------------------------------------

  else if (oledPage == 4) {

    display.println(
      "Light"
    );


    if (
      lightState ==
      SENSOR_NOT_FOUND
    ) {

      display.setTextSize(2);

      display.println(
        "N/A"
      );

      display.setTextSize(1);

      display.println(
        "NOT FOUND"
      );
    }


    else if (
      lightState ==
      SENSOR_FAULT
    ) {

      display.setTextSize(2);

      display.println(
        "ERROR"
      );

      display.setTextSize(1);

      display.println(
        "FAULT"
      );
    }


    else {

      display.setTextSize(2);

      display.print(
        lightLux,
        0
      );

      display.println(
        " lux"
      );

      display.setTextSize(1);

      display.println(
        sensorStateToString(
          lightState
        )
      );
    }
  }


  // ----------------------------------------------------------
  // PAGE 5 - WEBSITE
  // ----------------------------------------------------------

  else if (oledPage == 5) {

    display.println(
      "Web Dashboard"
    );


    display.setTextSize(2);


    if (wifiActive) {

      display.println(
        "ONLINE"
      );

    } else {

      display.println(
        "OFF"
      );
    }


    display.setTextSize(1);


    display.println();


    if (wifiActive) {

      display.println(
        "192.168.4.1"
      );

    } else {

      display.println(
        "Press button"
      );
    }
  }


  display.display();
}


// ============================================================
//                        OLED TASK
// ============================================================

void oledTask() {

  if (
    millis() -
    oledLastChange >=
    OLED_SLIDE_TIME
  ) {

    oledLastChange =
      millis();


    oledPage++;


    if (
      oledPage >=
      OLED_PAGE_COUNT
    ) {

      oledPage = 0;
    }


    showOLED();
  }
}


// ============================================================
//                     SYSTEM LOGIC
// ============================================================

void systemLogicTask() {

  if (
    systemMode ==
    NORMAL_MODE
  ) {

    if (
      alertConditionDetected()
    ) {

      enterAlertMode();
    }
  }


  else {

    if (
      alertConditionCleared()
    ) {

      exitAlertMode();
    }
  }
}


// ============================================================
//                          SETUP
// ============================================================

void setup() {

  Serial.begin(
    115200
  );


  Serial.println();
  Serial.println(
    "================================"
  );

  Serial.println(
    " SMART PLANT MONITORING SYSTEM"
  );

  Serial.println(
    "================================"
  );

  Serial.println();


  // ==========================================================
  // GPIO
  // ==========================================================

  pinMode(
    BUTTON_PIN,
    INPUT_PULLUP
  );


  pinMode(
    RGB_R_PIN,
    OUTPUT
  );

  pinMode(
    RGB_G_PIN,
    OUTPUT
  );

  pinMode(
    RGB_B_PIN,
    OUTPUT
  );


  pinMode(
    BUZZER_PIN,
    OUTPUT
  );


  setRGB(
    false,
    false,
    false
  );


  // ==========================================================
  // LittleFS
  // ==========================================================

  if (
    !LittleFS.begin(true)
  ) {

    Serial.println(
      "ERROR: LittleFS initialization failed!"
    );
  }

  else {

    Serial.println(
      "LittleFS initialized."
    );
  }


  // ==========================================================
  // I2C
  // ==========================================================

  Wire.begin(
    I2C_SDA,
    I2C_SCL
  );


  // ==========================================================
  // OLED
  // ==========================================================

  if (
    display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    )
  ) {

    oledDetected = true;

    Serial.println(
      "OLED detected."
    );


    display.clearDisplay();


    display.setTextColor(
      SSD1306_WHITE
    );


    display.setTextSize(1);


    display.setCursor(
      20,
      20
    );


    display.println(
      "SMART PLANT"
    );


    display.setCursor(
      28,
      35
    );


    display.println(
      "STARTING..."
    );


    display.display();

  }

  else {

    oledDetected = false;

    Serial.println(
      "OLED NOT FOUND."
    );
  }


  // ==========================================================
  // STARTUP BEEP
  // ==========================================================

  startupBeep();


  // ==========================================================
  // AHT20
  // ==========================================================

  if (
    aht.begin()
  ) {

    ahtDetected = true;

    ahtState =
      SENSOR_RESTING;

    Serial.println(
      "AHT20 detected."
    );

  }

  else {

    ahtDetected = false;

    ahtState =
      SENSOR_NOT_FOUND;

    Serial.println(
      "AHT20 NOT FOUND."
    );
  }


  // ==========================================================
  // BH1750
  // ==========================================================

  if (
    bh1750.begin(
      BH1750::CONTINUOUS_HIGH_RES_MODE
    )
  ) {

    bhDetected = true;

    lightState =
      SENSOR_RESTING;

    Serial.println(
      "BH1750 detected."
    );

  }

  else {

    bhDetected = false;

    lightState =
      SENSOR_NOT_FOUND;

    Serial.println(
      "BH1750 NOT FOUND."
    );
  }


  // ==========================================================
  // SOIL
  // ==========================================================

  soilState =
    SENSOR_RESTING;


  // ==========================================================
  // WEB SERVER ROUTES
  // ==========================================================

  setupServerRoutes();


  // ==========================================================
  // START FIRST SENSOR
  // ==========================================================

  startSensor(
    SOIL_SENSOR
  );


  // ==========================================================
  // WIFI
  // ==========================================================

  WiFi.mode(
    WIFI_OFF
  );

  wifiActive = false;


  // IMPORTANT:
  // Wi-Fi starts immediately at boot.

  startWiFiAP();


  // ==========================================================
  // OLED
  // ==========================================================

  showOLED();


  Serial.println();
  Serial.println(
    "System ready."
  );

  Serial.println(
    "Wi-Fi starts automatically."
  );

  Serial.println(
    "If no device connects for 1 minute,"
  );

  Serial.println(
    "Wi-Fi will turn OFF."
  );

  Serial.println(
    "GPIO 27 -> GND wakes Wi-Fi again."
  );

  Serial.println();
}


// ============================================================
//                           LOOP
// ============================================================

void loop() {

  // Button
  checkButton();


  // Wi-Fi
  wifiTask();


  // Buzzer
  updateBuzzer();


  // Sensors
  if (
    systemMode ==
    NORMAL_MODE
  ) {

    normalSensorTask();

  }

  else {

    alertSensorTask();
  }


  // Alert logic
  systemLogicTask();


  // OLED
  oledTask();


  // RGB
  updateRGB();
}