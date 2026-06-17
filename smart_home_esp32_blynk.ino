/*
 * ============================================================
 *  SMART HOME AUTOMATION - ESP32 + BLYNK IoT Cloud
 * ============================================================
 *  Components:
 *    - ESP32 Dev Board
 *    - 2x Servo Motors       (windows / doors)
 *    - 1x Buzzer             (rain alert)
 *    - 1x DHT11 Sensor       (temperature & humidity)
 *    - 1x Raindrop Sensor    (analog + digital output)
 *    - 1x 16x2 I2C LCD       (SDA→GPIO21, SCL→GPIO22)
 *
 *  Blynk Virtual Pins:
 *    V0  → Temperature        (SuperChart / Value Display)
 *    V1  → Humidity           (SuperChart / Value Display)
 *    V2  → Rain Status        (LED Widget: 1=rain, 0=dry)
 *    V3  → Window/Door Status (Value Display: "OPEN"/"CLOSED")
 *    V4  → Manual Servo Override Toggle (Switch Widget)
 *    V5  → Buzzer Mute Toggle (Switch Widget)
 *    V6  → Rain Analog Level  (Gauge Widget 0–4095)
 *
 *  Blynk Dashboard Setup:
 *    1. Create new template in Blynk Cloud (blynk.cloud)
 *    2. Add device → ESP32
 *    3. Add widgets as listed above
 *    4. Copy BLYNK_TEMPLATE_ID, BLYNK_TEMPLATE_NAME, BLYNK_AUTH_TOKEN
 *       and paste them below
 *
 *  Required Libraries (install via Arduino Library Manager):
 *    1. Blynk               by Volodymyr Shymanskyy  (v1.3.2+)
 *    2. ESP32Servo          by Kevin Harrington
 *    3. DHT sensor library  by Adafruit
 *    4. Adafruit Unified Sensor by Adafruit
 *    5. LiquidCrystal_I2C   by Frank de Brabander
 * ============================================================
 */

// ─────────────────────────────────────────
//  BLYNK CREDENTIALS  ← FILL THESE IN
// ─────────────────────────────────────────
#define BLYNK_TEMPLATE_ID "TMPL697AOriwD"
#define BLYNK_TEMPLATE_NAME "ESP32 Smart Home "
#define BLYNK_AUTH_TOKEN "DAo9eecSp750AR2nd5Etyg9gJwk0GFAa"



// ─────────────────────────────────────────
//  Wi-Fi CREDENTIALS  ← FILL THESE IN
// ─────────────────────────────────────────
#define WIFI_SSID     ""
#define WIFI_PASSWORD ""

// ─────────────────────────────────────────
//  BLYNK VIRTUAL PINS
// ─────────────────────────────────────────
#define VPIN_TEMPERATURE    V0
#define VPIN_HUMIDITY       V1
#define VPIN_RAIN_STATUS    V2
#define VPIN_DOOR_CONTROL   V3   // Switch widget: receives open/close command
#define VPIN_DOOR_STATUS    V7   // Value Display: shows "OPEN"/"CLOSED"
#define VPIN_MANUAL_OVERRIDE V4
#define VPIN_MUTE_BUZZER    V5
#define VPIN_RAIN_ANALOG    V6

// ─────────────────────────────────────────
//  INCLUDES
// ─────────────────────────────────────────
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ESP32Servo.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ─────────────────────────────────────────
//  PIN DEFINITIONS
// ─────────────────────────────────────────
#define SERVO1_PIN        23
#define SERVO2_PIN        19
#define BUZZER_PIN        17
#define DHT_PIN           4
#define RAIN_DIGITAL_PIN  35
#define RAIN_ANALOG_PIN   34

// ─────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────
#define DHT_TYPE          DHT11
#define RAIN_THRESHOLD    2000   // Analog: below this = rain (tune as needed)
#define OPEN_ANGLE        0
#define CLOSE_ANGLE       90
#define LCD_ADDRESS       0x27   // Try 0x3F if LCD stays blank
#define LCD_COLS          16
#define LCD_ROWS          2

// ─────────────────────────────────────────
//  TIMING (ms)
// ─────────────────────────────────────────
#define DHT_INTERVAL        2000
#define RAIN_INTERVAL       500
#define BLYNK_SEND_INTERVAL 2000   // Send data to Blynk every 3 s
#define BUZZER_ON           200
#define BUZZER_OFF          200

// ─────────────────────────────────────────
//  OBJECTS
// ─────────────────────────────────────────
Servo servo1;
Servo servo2;
int lastServo1State = -1;
int lastServo2State = -1;
DHT   dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
BlynkTimer blynkTimer;

// ─────────────────────────────────────────
//  STATE
// ─────────────────────────────────────────
bool  isRaining       = false;
bool  lastRainState   = false;
bool  buzzerState     = false;
bool  manualOverride  = false;   // V4: manual open/close from app
bool  buzzerMuted     = false;   // V5: mute buzzer from app
bool  manualOpen      = true;    // true = open, false = closed (manual)

float temperature     = 0.0;
float humidity        = 0.0;

unsigned long lastDHTCheck   = 0;
unsigned long lastRainCheck  = 0;
unsigned long lastBuzzerTog  = 0;

void setServo1(int angle) {
  if (angle != lastServo1State) {
    servo1.write(angle);
    lastServo1State = angle;
  }
}

void setServo2(int angle) {
  if (angle != lastServo2State) {
    servo2.write(angle);
    lastServo2State = angle;
  }
}

// Custom LCD chars
byte thermChar[8] = { 0b00100,0b01010,0b01010,0b01110,0b01110,0b11111,0b11111,0b01110 };
byte dropChar[8]  = { 0b00100,0b00100,0b01010,0b01010,0b10001,0b10001,0b10001,0b01110 };
byte wifiChar[8]  = { 0b00000,0b01110,0b10001,0b00100,0b01010,0b00000,0b00100,0b00000 };

// ─────────────────────────────────────────
//  BLYNK: Manual Override Switch (V4)
//  Widget: Switch → sends 1 (ON) or 0 (OFF)
// ─────────────────────────────────────────
BLYNK_WRITE(VPIN_MANUAL_OVERRIDE) {
  manualOverride = param.asInt();   // 1 = manual mode on
  Serial.printf("[Blynk] Manual Override: %s\n", manualOverride ? "ON" : "OFF");

  if (manualOverride) {
    // When switching to manual, default to current state
    lcd.setCursor(0, 1);
    lcd.print("Manual Mode ON  ");
  } else {
    lcd.setCursor(0, 1);
    lcd.print("Auto Mode ON    ");
  }
}

// ─────────────────────────────────────────
//  BLYNK: Manual Door/Window Control (V3)
//  Use a Button Widget (PUSH) on V3 in manual mode
//  Or repurpose a Switch widget for open/close
// ─────────────────────────────────────────
BLYNK_WRITE(VPIN_DOOR_STATUS) {
  if (!manualOverride) return;  // Ignore if not in manual mode

  int val = param.asInt();      // 1 = open command, 0 = close command
  manualOpen = (val == 1);

  if (manualOpen) {
    servo1.write(OPEN_ANGLE);
    setServo2(OPEN_ANGLE);
    Serial.println("[Blynk] Manual → OPEN (90°)");
    Blynk.virtualWrite(VPIN_DOOR_STATUS, "OPEN");
  } else {
    servo1.write(CLOSE_ANGLE);
    setServo2(CLOSE_ANGLE);
    Serial.println("[Blynk] Manual → CLOSED (0°)");
    Blynk.virtualWrite(VPIN_DOOR_STATUS, "CLOSED");
  }
}

BLYNK_WRITE(VPIN_DOOR_CONTROL) {   // <-- V3, receives commands
  if (!manualOverride) return;
  int val = param.asInt();
  manualOpen = (val == 1);

  if (manualOpen) {
    setServo1(OPEN_ANGLE);
    setServo2(OPEN_ANGLE);
    Blynk.virtualWrite(VPIN_DOOR_STATUS, "OPEN");   // writes to V7
  } else {
    setServo1(CLOSE_ANGLE);
    setServo2(CLOSE_ANGLE);
    Blynk.virtualWrite(VPIN_DOOR_STATUS, "CLOSED"); // writes to V7
  }
}

// ─────────────────────────────────────────
//  BLYNK: Mute Buzzer Toggle (V5)
//  Widget: Switch
// ─────────────────────────────────────────
BLYNK_WRITE(VPIN_MUTE_BUZZER) {
  buzzerMuted = param.asInt();
  Serial.printf("[Blynk] Buzzer: %s\n", buzzerMuted ? "MUTED" : "UNMUTED");

  if (buzzerMuted) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
  }
}

// ─────────────────────────────────────────
//  SEND DATA TO BLYNK
// ─────────────────────────────────────────
void sendToBlynk() {
  if (!Blynk.connected()) return;

  Blynk.virtualWrite(VPIN_TEMPERATURE, temperature);
  Blynk.virtualWrite(VPIN_HUMIDITY,    humidity);
  Blynk.virtualWrite(VPIN_RAIN_STATUS, isRaining ? 1 : 0);
  Blynk.virtualWrite(VPIN_RAIN_ANALOG, analogRead(RAIN_ANALOG_PIN));
  Blynk.virtualWrite(VPIN_DOOR_STATUS, isRaining ? "CLOSED" : "OPEN");

  Serial.printf("[Blynk] Sent → Temp:%.1f Hum:%.1f Rain:%s\n",
                temperature, humidity, isRaining ? "YES" : "NO");
}

// ─────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n Smart Home + Blynk Starting...");

  // --- LCD ---
  Wire.begin(21, 22);
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, thermChar);
  lcd.createChar(1, dropChar);
  lcd.createChar(2, wifiChar);

  lcd.setCursor(0, 0);
  lcd.print(" Smart Home IoT ");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi.");
  delay(500);

  // --- DHT11 ---
  dht.begin();

  // --- Servos ---
  servo1.attach(SERVO1_PIN, 500, 2400);
  servo2.attach(SERVO2_PIN, 500, 2400);
  servo1.write(OPEN_ANGLE);
  servo2.write(OPEN_ANGLE);

  // --- Buzzer ---
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // --- Rain Sensor ---
  pinMode(RAIN_DIGITAL_PIN, INPUT);

  // --- Blynk + WiFi ---
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD);

  // Show WiFi connected on LCD
  lcd.setCursor(0, 1);
  lcd.write(byte(2));
  lcd.print(" WiFi OK!       ");
  delay(1500);
  lcd.clear();

  // --- Blynk Timer: push sensor data every 3 s ---
  delay(2000);   // Let DHT11 stabilize
  blynkTimer.setInterval(BLYNK_SEND_INTERVAL, sendToBlynk);

  Serial.println("System Ready!");
  showReadyLCD();
}

// ─────────────────────────────────────────
//  LOOP
// ─────────────────────────────────────────
void loop() {
  Blynk.run();          // Keep Blynk connection alive
  blynkTimer.run();     // Fire scheduled Blynk sends

  unsigned long now = millis();

  // Rain sensor check
  if (now - lastRainCheck >= RAIN_INTERVAL) {
    lastRainCheck = now;
    checkRainSensor();
  }

  // DHT11 + LCD update
  if (now - lastDHTCheck >= DHT_INTERVAL) {
    lastDHTCheck = now;
    readDHT11();
    updateLCD();
  }

  // Non-blocking buzzer
  if (isRaining && !buzzerMuted && !manualOverride) {
    handleBuzzerBeep(now);
  }
}

// ─────────────────────────────────────────
//  RAIN SENSOR CHECK
// ─────────────────────────────────────────
void checkRainSensor() {
  int  analogVal  = analogRead(RAIN_ANALOG_PIN);
  int  digitalVal = digitalRead(RAIN_DIGITAL_PIN);

  bool rainNow = (digitalVal == LOW) || (analogVal < RAIN_THRESHOLD);

  if (rainNow != lastRainState) {
    lastRainState = rainNow;
    isRaining     = rainNow;

    if (isRaining) onRainDetected();
    else           onRainCleared();
  }
}

// ─────────────────────────────────────────
//  RAIN DETECTED
// ─────────────────────────────────────────
void onRainDetected() {
  Serial.println("RAIN DETECTED!");

  if (!manualOverride) {
    servo1.write(CLOSE_ANGLE);
    servo2.write(CLOSE_ANGLE);
  }

  // Blynk LED widget: turn on rain indicator
  Blynk.virtualWrite(VPIN_RAIN_STATUS, 1);
  Blynk.logEvent("rain_alert", "Rain detected! Windows & doors closed.");

  lcd.setCursor(0, 1);
  lcd.print("Rain! Closed    ");
}

// ─────────────────────────────────────────
//  RAIN CLEARED
// ─────────────────────────────────────────
void onRainCleared() {
  Serial.println("Rain cleared!");

  if (!manualOverride) {
    servo1.write(OPEN_ANGLE);
    servo2.write(OPEN_ANGLE);
  }

  digitalWrite(BUZZER_PIN, LOW);
  buzzerState = false;

  Blynk.virtualWrite(VPIN_RAIN_STATUS, 0);
  Blynk.logEvent("rain_clear", "Rain stopped. Windows & doors opened.");

  lcd.setCursor(0, 1);
  lcd.print("                ");
}

// ─────────────────────────────────────────
//  NON-BLOCKING BUZZER BEEP
// ─────────────────────────────────────────
void handleBuzzerBeep(unsigned long now) {
  unsigned long phase = now % (BUZZER_ON + BUZZER_OFF);
  bool shouldBeep = (phase < BUZZER_ON);

  if (shouldBeep != buzzerState) {
    buzzerState = shouldBeep;
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
  }
}

// ─────────────────────────────────────────
//  READ DHT11
// ─────────────────────────────────────────
void readDHT11() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    temperature = t;
    humidity    = h;
    Serial.printf("[DHT11] Temp: %.1f°C | Hum: %.1f%%\n", t, h);
  } else {
    Serial.println("[DHT11] Read failed.");
  }
}

// ─────────────────────────────────────────
//  UPDATE LCD
// ─────────────────────────────────────────
void updateLCD() {
  // Row 0: Temperature
  lcd.setCursor(0, 0);
  lcd.write(byte(0));
  lcd.print(" Temp: ");
  lcd.print(temperature, 1);
  lcd.print((char)223);    // °
  lcd.print("C  ");

  // Row 1: Humidity (only when not raining & not in manual mode LCD msg)
  if (!isRaining && !manualOverride) {
    lcd.setCursor(0, 1);
    lcd.write(byte(1));
    lcd.print(" Hum:  ");
    lcd.print(humidity, 1);
    lcd.print("%    ");
  }
}

// ─────────────────────────────────────────
//  READY SCREEN
// ─────────────────────────────────────────
void showReadyLCD() {
  lcd.clear();
  lcd.setCursor(1, 0);
  lcd.print("System Ready");
  lcd.setCursor(0, 1);
  lcd.print("Blynk Connected ");
  delay(2000);
  lcd.clear();
}
