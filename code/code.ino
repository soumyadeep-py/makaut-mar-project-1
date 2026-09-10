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
#define OLED_LEFT_PIN  32
#define OLED_RIGHT_PIN 4
#define OLED_UP_PIN    16
#define OLED_DOWN_PIN  17

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

extern SensorState soilState;
extern SensorState ahtState;
extern SensorState lightState;


// ============================================================
//                     SYSTEM MODES
// ============================================================

enum SystemMode {
  NORMAL_MODE,
  ALERT_MODE
};

enum StartupPhase {
  STARTUP_BOOT,
  STARTUP_SENSOR_CHECK,
  STARTUP_READY
};

SystemMode systemMode = NORMAL_MODE;
StartupPhase startupPhase = STARTUP_BOOT;

bool startupSensorCheckPassed = false;
unsigned long startupCheckLastAttempt = 0;
unsigned long startupWarningLastBeep = 0;
unsigned long startupResultDisplayUntil = 0;
unsigned long sensorRecoveryLastAttempt = 0;

const unsigned long STARTUP_SENSOR_CHECK_INTERVAL = 1000UL;
const unsigned long STARTUP_WARNING_REPEAT_TIME = 1500UL;
const unsigned long STARTUP_RESULT_DISPLAY_TIME = 2000UL;
const unsigned long SENSOR_RECOVERY_INTERVAL = 1000UL;


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
// AHT21B  → 3s ACTIVE → 5s REST
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

const unsigned long OLED_SLIDE_TIME = 4000UL;
const unsigned long OLED_FRAME_TIME = 40UL;
const unsigned long OLED_TRANSITION_TIME = 500UL;

unsigned long oledLastChange = 0;
unsigned long oledLastFrame = 0;
unsigned long oledTransitionStart = 0;

int oledPage = 0;

const int OLED_PAGE_COUNT = 6;

bool oledTransitionActive = false;
bool oledTransitionFromLeft = false;

uint8_t oledOutgoingFrame[
  SCREEN_WIDTH * SCREEN_HEIGHT / 8
];


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
unsigned long oledButtonChange[4] = {0, 0, 0, 0};

const unsigned long DEBOUNCE_TIME = 50UL;
const unsigned long OLED_MANUAL_OVERRIDE_TIME = 600000UL;

const uint8_t OLED_MIN_CONTRAST = 20;
const uint8_t OLED_MAX_CONTRAST = 255;
const float OLED_BRIGHTNESS_REFERENCE_LUX = 65535.0;

uint8_t oledContrast = 128;
unsigned long oledManualOverrideStart = 0;
bool oledManualOverrideActive = false;

void setOLEDContrast();
void updateAutomaticOLEDContrast();


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
    red ? LOW : HIGH
  );

  digitalWrite(
    RGB_G_PIN,
    green ? LOW : HIGH
  );

  digitalWrite(
    RGB_B_PIN,
    blue ? LOW : HIGH
  );
}


bool hasSensorProblem() {

  return
    soilState == SENSOR_NOT_FOUND ||
    soilState == SENSOR_FAULT ||
    !ahtDetected ||
    ahtState == SENSOR_NOT_FOUND ||
    ahtState == SENSOR_FAULT ||
    !bhDetected ||
    lightState == SENSOR_NOT_FOUND ||
    lightState == SENSOR_FAULT;
}


bool allSensorsHealthy() {

  bool soilOk =
    soilState != SENSOR_NOT_FOUND &&
    soilState != SENSOR_FAULT &&
    !isnan(soilPercent);

  bool ahtOk =
    ahtDetected &&
    ahtState != SENSOR_NOT_FOUND &&
    ahtState != SENSOR_FAULT &&
    !isnan(temperature) &&
    !isnan(humidity);

  bool lightOk =
    bhDetected &&
    lightState != SENSOR_NOT_FOUND &&
    lightState != SENSOR_FAULT &&
    !isnan(lightLux);

  return soilOk && ahtOk && lightOk;
}


void updateRGB() {

  // Startup validation has priority before normal operation
  if (startupPhase == STARTUP_SENSOR_CHECK) {

    if (startupSensorCheckPassed) {
      setRGB(false, true, false);
    } else {
      setRGB(false, false, true);
    }

    return;
  }


  // A connection problem takes priority over threshold alerts.
  if (hasSensorProblem()) {
    setRGB(false, false, true);
    return;
  }


  // ALERT always takes priority: red means something is wrong
  if (systemMode == ALERT_MODE) {

    setRGB(true, false, false);

    return;
  }


  // GREEN only when every required sensor is connected
  // and each one is returning a valid reading.
  if (allSensorsHealthy()) {

    setRGB(false, true, false);

    return;
  }


  // Blue means a sensor needs attention. Red remains reserved for plant alerts.
  setRGB(false, false, true);
}


void startupSensorCheckTask() {

  if (startupPhase != STARTUP_SENSOR_CHECK)
    return;


  if (
    millis() - startupCheckLastAttempt <
    STARTUP_SENSOR_CHECK_INTERVAL
  ) {
    return;
  }

  startupCheckLastAttempt = millis();

  // Retry I2C initialisation too, so a sensor plugged in during boot is found.
  sensorRecoveryLastAttempt = millis() - SENSOR_RECOVERY_INTERVAL;
  sensorRecoveryTask();

  readSoil();
  readAHT();
  readBH1750();

  bool soilOk =
    soilState == SENSOR_ACTIVE &&
    !isnan(soilPercent);

  bool ahtOk =
    ahtDetected &&
    ahtState == SENSOR_ACTIVE &&
    !isnan(temperature) &&
    !isnan(humidity);

  bool lightOk =
    bhDetected &&
    lightState == SENSOR_ACTIVE &&
    !isnan(lightLux);

  startupSensorCheckPassed =
    soilOk && ahtOk && lightOk;

  if (startupSensorCheckPassed) {

    startupResultDisplayUntil = millis() + STARTUP_RESULT_DISPLAY_TIME;

    startupPhase = STARTUP_READY;

    startupHappyBeep();

    Serial.println();
    Serial.println("All sensors OK. Starting normal monitoring.");

    startWiFiAP();

    return;
  }


  Serial.println();
  Serial.println("Sensor check failed. Check sensor connection.");

  if (
    millis() - startupWarningLastBeep >=
    STARTUP_WARNING_REPEAT_TIME
  ) {
    startupWarningLastBeep = millis();
    startBeep();
  }
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


// A short rising three-note confirmation for a successful sensor check.
void startupHappyBeep() {

  const int notes[] = { 1600, 2100, 2800 };

  for (int index = 0; index < 3; index++) {
    tone(BUZZER_PIN, notes[index]);
    delay(90);
    noTone(BUZZER_PIN);
    delay(45);
  }
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
//                        AHT21B SENSOR
// ============================================================

SensorState ahtState = SENSOR_NOT_FOUND;


bool i2cDevicePresent(uint8_t address) {

  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}


// ------------------------------------------------------------
// Read AHT21B
// ------------------------------------------------------------

void readAHT() {

  if (!ahtDetected || !i2cDevicePresent(0x38)) {

    ahtState = SENSOR_NOT_FOUND;
    ahtDetected = false;
    temperature = NAN;
    humidity = NAN;

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
    ahtDetected = false;
    temperature = NAN;
    humidity = NAN;

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

  if (!bhDetected || !i2cDevicePresent(0x23)) {

    lightState = SENSOR_NOT_FOUND;
    bhDetected = false;
    lightLux = NAN;

    return;
  }


  float lux =
    bh1750.readLightLevel();


  if (lux < 0) {

    lightState = SENSOR_FAULT;
    bhDetected = false;

    lightLux = NAN;

    return;
  }


  lightLux = lux;

  lightState = SENSOR_ACTIVE;

  updateAutomaticOLEDContrast();


  Serial.print(
    "Light: "
  );

  Serial.print(
    lightLux,
    1
  );

  Serial.println(" lux");
}


// Re-initialise a disconnected I2C sensor and re-test a bad soil reading.
// This lets the monitor automatically return to normal as soon as it is fixed.
void sensorRecoveryTask() {

  if (millis() - sensorRecoveryLastAttempt < SENSOR_RECOVERY_INTERVAL) {
    return;
  }

  sensorRecoveryLastAttempt = millis();

  if (!ahtDetected || ahtState == SENSOR_NOT_FOUND || ahtState == SENSOR_FAULT) {
    if (aht.begin(&Wire)) {
      ahtDetected = true;
      readAHT();
    } else {
      ahtDetected = false;
      ahtState = SENSOR_NOT_FOUND;
      temperature = NAN;
      humidity = NAN;
    }
  }

  if (!bhDetected || lightState == SENSOR_NOT_FOUND || lightState == SENSOR_FAULT) {
    if (bh1750.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
      bhDetected = true;
      readBH1750();
    } else {
      bhDetected = false;
      lightState = SENSOR_NOT_FOUND;
      lightLux = NAN;
    }
  }

  if (soilState == SENSOR_NOT_FOUND || soilState == SENSOR_FAULT) {
    readSoil();
  }
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
// GPIO 32 ---- OLED LEFT BUTTON ---- GND
// GPIO 4  ---- OLED RIGHT BUTTON --- GND
// GPIO 16 ---- OLED UP BUTTON ------ GND
// GPIO 17 ---- OLED DOWN BUTTON ---- GND
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


        wifiDisconnectTime = 0;


        if (!wifiActive) {

          startWiFiAP();
        }
      }
    }
  }
}


void changeOLEDPage(int direction) {

  unsigned long now = millis();

  oledLastChange = now;

  if (oledDetected) {
    memcpy(
      oledOutgoingFrame,
      display.getBuffer(),
      sizeof(oledOutgoingFrame)
    );
  }

  oledPage += direction;

  if (oledPage >= OLED_PAGE_COUNT) {
    oledPage = 0;
  }

  if (oledPage < 0) {
    oledPage = OLED_PAGE_COUNT - 1;
  }

  oledTransitionStart = now;
  oledTransitionFromLeft = direction < 0;
  oledTransitionActive = true;
}


void setOLEDContrast() {

  if (!oledDetected) {
    return;
  }

  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(oledContrast);
}


void updateAutomaticOLEDContrast() {

  if (oledManualOverrideActive) {
    if (
      millis() - oledManualOverrideStart <
      OLED_MANUAL_OVERRIDE_TIME
    ) {
      return;
    }

    oledManualOverrideActive = false;
  }

  float brightnessPercent =
    constrain(
      lightLux / OLED_BRIGHTNESS_REFERENCE_LUX * 100.0,
      0.0,
      100.0
    );

  oledContrast =
    (uint8_t)map(
      (long)brightnessPercent,
      0,
      100,
      OLED_MIN_CONTRAST,
      OLED_MAX_CONTRAST
    );

  setOLEDContrast();
}


void startOLEDManualOverride() {

  oledManualOverrideActive = true;
  oledManualOverrideStart = millis();
}


void checkOLEDButton() {

  const int buttonPins[4] = {
    OLED_LEFT_PIN,
    OLED_RIGHT_PIN,
    OLED_UP_PIN,
    OLED_DOWN_PIN
  };

  static bool lastReading[4] = {HIGH, HIGH, HIGH, HIGH};
  static bool stableState[4] = {HIGH, HIGH, HIGH, HIGH};

  for (int index = 0; index < 4; index++) {
    bool reading = digitalRead(buttonPins[index]);

    if (reading != lastReading[index]) {
      oledButtonChange[index] = millis();
      lastReading[index] = reading;
    }

    if (
      millis() - oledButtonChange[index] >=
      DEBOUNCE_TIME &&
      reading != stableState[index]
    ) {
      stableState[index] = reading;

      if (stableState[index] == LOW) {
        if (index == 0) {
          changeOLEDPage(-1);
        } else if (index == 1) {
          changeOLEDPage(1);
        } else if (index == 2) {
          oledContrast = min(
            (int)OLED_MAX_CONTRAST,
            (int)oledContrast + 40
          );
          startOLEDManualOverride();
          setOLEDContrast();
        } else if (index == 3) {
          oledContrast = max(
            (int)OLED_MIN_CONTRAST,
            (int)oledContrast - 40
          );
          startOLEDManualOverride();
          setOLEDContrast();
        }

        Serial.println("OLED BUTTON PRESSED");
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

void showLegacyOLED() {

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


  if (oledTransitionActive) {

    unsigned long transitionElapsed =
      millis() - oledTransitionStart;

    if (transitionElapsed >= OLED_TRANSITION_TIME) {

      oledTransitionActive = false;

    } else {

      float transitionProgress =
        (float)transitionElapsed /
        OLED_TRANSITION_TIME;

      float easedProgress =
        1.0 -
        (
          (1.0 - transitionProgress) *
          (1.0 - transitionProgress) *
          (1.0 - transitionProgress)
        );

      int revealedHeight =
        (int)(easedProgress * SCREEN_HEIGHT);

      display.fillRect(
        0,
        revealedHeight,
        SCREEN_WIDTH,
        SCREEN_HEIGHT - revealedHeight,
        SSD1306_BLACK
      );
    }
  }


  display.display();
}


void oledCenteredText(
  const String& text,
  int y,
  int textSize
) {

  int16_t x1;
  int16_t y1;
  uint16_t width;
  uint16_t height;

  display.setTextSize(textSize);
  display.getTextBounds(
    text,
    0,
    y,
    &x1,
    &y1,
    &width,
    &height
  );

  display.setCursor(
    (SCREEN_WIDTH - width) / 2,
    y
  );

  display.print(text);
}


void oledSensorFooter(
  SensorState state
) {

  display.setTextSize(1);

  if (state == SENSOR_NOT_FOUND) {
    oledCenteredText("SENSOR NOT FOUND", 51, 1);
  }

  else if (state == SENSOR_FAULT) {
    oledCenteredText("SENSOR FAULT", 51, 1);
  }

  else {
    oledCenteredText(
      sensorStateToString(state),
      51,
      1
    );
  }
}


void oledProgressBar(
  float value,
  float maximum
) {

  const int barX = 12;
  const int barY = 43;
  const int barWidth = 104;
  const int barHeight = 6;

  display.drawRoundRect(
    barX,
    barY,
    barWidth,
    barHeight,
    2,
    SSD1306_WHITE
  );

  if (isnan(value) || value <= 0.0) {
    return;
  }

  float limitedValue = value;

  if (limitedValue > maximum) {
    limitedValue = maximum;
  }

  int fillWidth =
    (int)(
      (limitedValue / maximum) *
      (barWidth - 4)
    );

  if (fillWidth > 0) {
    display.fillRoundRect(
      barX + 2,
      barY + 2,
      fillWidth,
      barHeight - 4,
      1,
      SSD1306_WHITE
    );
  }
}


void oledPageHeader() {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(3, 1);
  display.print("PLANT MONITOR");

  display.setCursor(109, 1);
  display.print(oledPage + 1);
  display.print("/");
  display.print(OLED_PAGE_COUNT);

  display.drawLine(
    0,
    11,
    SCREEN_WIDTH - 1,
    11,
    SSD1306_WHITE
  );
}


void oledPageDots() {

  const int dotsWidth = OLED_PAGE_COUNT * 6 - 1;
  const int startX = (SCREEN_WIDTH - dotsWidth) / 2;
  const int dotY = 62;

  for (int index = 0; index < OLED_PAGE_COUNT; index++) {
    if (index == oledPage) {
      display.fillRect(
        startX + index * 6,
        dotY,
        5,
        2,
        SSD1306_WHITE
      );
    } else {
      display.drawPixel(
        startX + index * 6 + 2,
        dotY,
        SSD1306_WHITE
      );
    }
  }
}


void oledSensorCheckScreen(bool allFound) {

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(3, 1);
  display.print("SMART PLANT");
  display.drawLine(0, 11, SCREEN_WIDTH - 1, 11, SSD1306_WHITE);

  oledCenteredText(allFound ? "ALL SENSORS FOUND" : "CHECK SENSOR", 16, 1);
  display.drawRoundRect(1, 27, SCREEN_WIDTH - 2, 31, 3, SSD1306_WHITE);

  display.setCursor(8, 31);
  display.print(soilState == SENSOR_ACTIVE || soilState == SENSOR_RESTING ? "+ SOIL" : "! SOIL MISSING");

  display.setCursor(8, 40);
  display.print(ahtDetected && ahtState != SENSOR_FAULT ? "+ AHT21B" : "! AHT21B MISSING");

  display.setCursor(8, 49);
  display.print(bhDetected && lightState != SENSOR_FAULT ? "+ BH1750" : "! BH1750 MISSING");

  display.display();
}


void showOLED() {

  if (!oledDetected) {
    return;
  }

  if (
    startupPhase != STARTUP_READY ||
    millis() < startupResultDisplayUntil
  ) {
    oledSensorCheckScreen(startupSensorCheckPassed);
    return;
  }

  // Never leave a stale environmental reading on-screen after a sensor drops.
  if (hasSensorProblem()) {
    oledSensorCheckScreen(false);
    return;
  }

  oledPageHeader();

  display.drawRoundRect(
    1,
    15,
    SCREEN_WIDTH - 2,
    44,
    3,
    SSD1306_WHITE
  );

  if (oledPage == 0) {
    oledCenteredText("SYSTEM STATUS", 19, 1);

    if (systemMode == ALERT_MODE) {
      oledCenteredText("ALERT", 27, 2);
      oledCenteredText("CHECK PLANT", 47, 1);
    } else {
      oledCenteredText("NORMAL", 27, 2);
      oledCenteredText("ALL SYSTEMS OK", 47, 1);
    }
  }

  else if (oledPage == 1) {
    oledCenteredText("SOIL MOISTURE", 19, 1);

    if (soilState == SENSOR_NOT_FOUND || soilState == SENSOR_FAULT) {
      oledCenteredText(
        soilState == SENSOR_NOT_FOUND ? "N/A" : "ERROR",
        28,
        2
      );
    } else {
      oledCenteredText(String(soilPercent, 1) + "%", 27, 2);
      oledProgressBar(soilPercent, 100.0);
    }

    oledSensorFooter(soilState);
  }

  else if (oledPage == 2) {
    oledCenteredText("AIR TEMPERATURE", 19, 1);

    if (ahtState == SENSOR_NOT_FOUND || ahtState == SENSOR_FAULT) {
      oledCenteredText(
        ahtState == SENSOR_NOT_FOUND ? "N/A" : "ERROR",
        28,
        2
      );
    } else {
      oledCenteredText(String(temperature, 1) + " C", 27, 2);
    }

    oledSensorFooter(ahtState);
  }

  else if (oledPage == 3) {
    oledCenteredText("AIR HUMIDITY", 19, 1);

    if (ahtState == SENSOR_NOT_FOUND || ahtState == SENSOR_FAULT) {
      oledCenteredText(
        ahtState == SENSOR_NOT_FOUND ? "N/A" : "ERROR",
        28,
        2
      );
    } else {
      oledCenteredText(String(humidity, 1) + "%", 27, 2);
      oledProgressBar(humidity, 100.0);
    }

    oledSensorFooter(ahtState);
  }

  else if (oledPage == 4) {
    oledCenteredText("AMBIENT LIGHT", 19, 1);

    if (lightState == SENSOR_NOT_FOUND || lightState == SENSOR_FAULT) {
      oledCenteredText(
        lightState == SENSOR_NOT_FOUND ? "N/A" : "ERROR",
        28,
        2
      );
    } else {
      float lightPercent = constrain(
        lightLux / OLED_BRIGHTNESS_REFERENCE_LUX * 100.0,
        0.0,
        100.0
      );

      oledCenteredText(
        String(lightLux, 0) + " lux  " + String(lightPercent, 0) + "%",
        27,
        1
      );
      oledProgressBar(lightPercent, 100.0);
    }

    oledSensorFooter(lightState);
  }

  else {
    oledCenteredText("WI-FI DASHBOARD", 19, 1);

    if (wifiActive) {
      oledCenteredText("ONLINE", 27, 2);
      oledCenteredText("192.168.4.1", 47, 1);
    } else {
      oledCenteredText("OFFLINE", 27, 2);
      oledCenteredText("PRESS BUTTON", 47, 1);
    }
  }

  oledPageDots();

  if (oledTransitionActive) {
    unsigned long transitionElapsed =
      millis() - oledTransitionStart;

    if (transitionElapsed >= OLED_TRANSITION_TIME) {
      oledTransitionActive = false;
    } else {
      float transitionProgress =
        (float)transitionElapsed /
        OLED_TRANSITION_TIME;

      float easedProgress =
        1.0 -
        (
          (1.0 - transitionProgress) *
          (1.0 - transitionProgress) *
          (1.0 - transitionProgress)
        );

      int incomingWidth =
        (int)(easedProgress * SCREEN_WIDTH);

      int splitX = oledTransitionFromLeft
        ? incomingWidth
        : SCREEN_WIDTH - incomingWidth;

      uint8_t* currentFrame = display.getBuffer();

      for (
        int row = 0;
        row < SCREEN_HEIGHT / 8;
        row++
      ) {
        for (
          int column = 0;
          column < SCREEN_WIDTH;
          column++
        ) {
          if (
            (oledTransitionFromLeft && column >= splitX) ||
            (!oledTransitionFromLeft && column < splitX)
          ) {
            int byteIndex =
              row * SCREEN_WIDTH + column;

            currentFrame[byteIndex] =
              oledOutgoingFrame[byteIndex];
          }
        }
      }

      display.drawLine(
        splitX,
        12,
        splitX,
        SCREEN_HEIGHT - 1,
        SSD1306_WHITE
      );
    }
  }

  display.display();
}


// ============================================================
//                        OLED TASK
// ============================================================

void oledTask() {

  unsigned long now = millis();
  bool pageChanged = false;

  if (
    now -
    oledLastChange >=
    OLED_SLIDE_TIME
  ) {

    changeOLEDPage(1);
    pageChanged = true;
  }


  if (
    startupPhase != STARTUP_READY ||
    millis() < startupResultDisplayUntil ||
    hasSensorProblem() ||
    pageChanged ||
    (
      oledTransitionActive &&
      now - oledLastFrame >= OLED_FRAME_TIME
    )
  ) {

    oledLastFrame = now;

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


  pinMode(OLED_LEFT_PIN, INPUT_PULLUP);
  pinMode(OLED_RIGHT_PIN, INPUT_PULLUP);
  pinMode(OLED_UP_PIN, INPUT_PULLUP);
  pinMode(OLED_DOWN_PIN, INPUT_PULLUP);


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
  setOLEDContrast();

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
  // AHT21B
  // ==========================================================

  if (
    aht.begin(&Wire)
  ) {

    ahtDetected = true;

    ahtState =
      SENSOR_RESTING;

    Serial.println(
      "AHT21B detected."
    );

  }

  else {

    ahtDetected = false;

    ahtState =
      SENSOR_NOT_FOUND;

    Serial.println(
      "AHT21B NOT FOUND."
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

  if (bhDetected) {
    readBH1750();
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


  // ==========================================================
  // SENSOR STARTUP CHECK
  // ==========================================================

  startupPhase = STARTUP_SENSOR_CHECK;
  startupSensorCheckPassed = false;
  startupCheckLastAttempt = 0;
  startupWarningLastBeep = 0;


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

  Serial.println(
    "OLED controls: GPIO 32 left, 4 right, 16 up, 17 down."
  );

  Serial.println(
    "Up/down change contrast by 10."
  );

  Serial.println();
}


// ============================================================
//                           LOOP
// ============================================================

void loop() {

  // Button
  checkButton();
  checkOLEDButton();


  if (startupPhase == STARTUP_SENSOR_CHECK) {
    startupSensorCheckTask();
    updateBuzzer();
    oledTask();
    updateRGB();
    return;
  }

  if (startupPhase == STARTUP_READY) {
    if (millis() < startupResultDisplayUntil) {
      updateBuzzer();
      oledTask();
      updateRGB();
      return;
    }
  }


  // Wi-Fi
  wifiTask();


  // Buzzer
  updateBuzzer();


  // Sensors
  sensorRecoveryTask();

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

