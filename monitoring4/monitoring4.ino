#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h> // Gunakan ESP32Servo bukannya Servo.h
#include <Firebase_ESP_Client.h>
#include <WiFi.h> // Untuk ESP32
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define API_KEY "AIzaSyC9kIkLAGkB0xIS31vXQ8mtqMXED9TnSQc"
#define DATABASE_URL "https://monitoring-2f6fc-default-rtdb.asia-southeast1.firebasedatabase.app"

const char* ssid = "POCOF5";
const char* password = "okela234";

namespace pin {
  const byte tds_sensor = 35;        // Sensor TDS pada pin D35
  const byte one_wire_bus = 32;      // Sensor Dallas pada pin D32
  const byte servo_pin = 26;         // Servo pada pin D26
  const int potPin = 34;             // Sensor pH pada pin D34
}

namespace device {
  float aref = 3.3;
}

namespace sensor {
  float ec = 0;
  unsigned int tds = 0;
  float waterTemp = 0;
  float ecCalibration = 1;
  float ph = 0;
  float pHVoltage = 0;
}

OneWire oneWire(pin::one_wire_bus);
DallasTemperature dallasTemperature(&oneWire);
Servo servo;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

unsigned long previousMillis = 0;
const long interval = 1000;

void baca_Tds();
void baca_pH();
void uploadToFirebase();
void controlServoFromFirebase();
void uploadServoStatusToFirebase(bool status);

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected to Wi-Fi, IP Address: " + WiFi.localIP().toString());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Firebase sign up successful");
    signupOK = true;
  } else {
    Serial.printf("Firebase sign up failed: %s\n", config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  dallasTemperature.begin();
  servo.attach(pin::servo_pin);
  servo.write(0);  
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    baca_Tds();
    baca_pH();
    uploadToFirebase();
  }

  controlServoFromFirebase();
}

void baca_Tds() {
  dallasTemperature.requestTemperatures();
  sensor::waterTemp = dallasTemperature.getTempCByIndex(0);
  float rawEC = analogRead(pin::tds_sensor) * device::aref / 4095.0; // untuk ESP32 gunakan resolusi 12-bit (4095)
  float temperatureCoefficient = 1.0 + 0.02 * (sensor::waterTemp - 25.0);

  sensor::ec = (rawEC / temperatureCoefficient) * sensor::ecCalibration;
  sensor::tds = (133.42 * pow(sensor::ec, 3) - 255.86 * sensor::ec * sensor::ec + 857.39 * sensor::ec) * 0.5;

  Serial.print(F("TDS:")); Serial.println(sensor::tds);
  Serial.print(F("EC:")); Serial.println(sensor::ec, 2);
  Serial.print(F("Temperature:"));
  Serial.println(sensor::waterTemp, 2);
  
  display.clearDisplay();
  display.setCursor(5, 0);
  display.setTextSize(2);
  display.print("TDS:" + String(sensor::tds));
  display.setTextSize(1);
  display.print(" ppm");

  display.setCursor(5, 20);
  display.setTextSize(2);
  display.print("pH:" + String(sensor::ph, 2)); // Mengubah EC menjadi pH
  display.setTextSize(1);
  display.print(" pH");

  display.setCursor(5, 45);
  display.setTextSize(2);
  display.print("T:" + String(sensor::waterTemp, 2) + " C");
  display.display();
}

/* Backup
void baca_pH() {
  float Value = analogRead(pin::potPin);
  sensor::pHVoltage = Value * (device::aref / 4095.0);
  sensor::ph = 3.3 * sensor::pHVoltage;

  Serial.print("pH Voltage: "); Serial.println(sensor::pHVoltage);
  Serial.print("pH Value: "); Serial.println(sensor::ph);
}
*/
void baca_pH() {
  // Baca tegangan dari sensor
  float Value = analogRead(pin::potPin);
  sensor::pHVoltage = Value * (device::aref / 4095.0);

  // Data kalibrasi (tegangan disesuaikan dengan pengukuran sensor pada buffer pH)
  float voltage_at_pH9_14 = 2.5; // Tegangan aktual pada buffer pH 9.14
  float voltage_at_pH6_68 = 2.0; // Tegangan aktual pada buffer pH 6.68
  float pH_at_pH9_14 = 9.14;
  float pH_at_pH6_68 = 6.68;

  // Hitung gradien (m) dan offset (c)
  float m = (pH_at_pH9_14 - pH_at_pH6_68) / (voltage_at_pH9_14 - voltage_at_pH6_68);
  float c = pH_at_pH9_14 - m * voltage_at_pH9_14;

  // Hitung pH menggunakan kalibrasi
  sensor::ph = m * sensor::pHVoltage + c;

  // Debugging
  Serial.print("pH Voltage: "); Serial.println(sensor::pHVoltage);
  Serial.print("pH Value: "); Serial.println(sensor::ph);
}

void uploadToFirebase() {
  if (Firebase.RTDB.setInt(&fbdo, "sensorData/TDS", sensor::tds)) {
    Serial.println("TDS data sent to Firebase");
  } else {
    Serial.println("Failed to send TDS data: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setFloat(&fbdo, "sensorData/EC", sensor::ec)) {
    Serial.println("EC data sent to Firebase");
  } else {
    Serial.println("Failed to send EC data: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setFloat(&fbdo, "sensorData/Temperature", sensor::waterTemp)) {
    Serial.println("Temperature data sent to Firebase");
  } else {
    Serial.println("Failed to send Temperature data: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setFloat(&fbdo, "sensorData/pH", sensor::ph)) {
    Serial.println("pH data sent to Firebase");
  } else {
    Serial.println("Failed to send pH data: " + fbdo.errorReason());
  }
}

void controlServoFromFirebase() {
  if (Firebase.RTDB.getBool(&fbdo, "servoControl/status")) {
    bool status = fbdo.boolData();
    Serial.println("Current servo status: " + String(status));

    if (status) {
      servo.write(180);
      delay(500);
      Serial.println("Servo ON");
    } else {
      servo.write(0);
      delay(500);
      Serial.println("Servo OFF");
    }
  } else {
    Serial.println("Failed to get servo status: " + fbdo.errorReason());
  }
}

void uploadServoStatusToFirebase(bool status) {
  if (Firebase.RTDB.setBool(&fbdo, "servoControl/status", status)) {
    Serial.println("Servo status (" + String(status) + ") sent to Firebase");
  } else {
    Serial.println("Failed to send servo status: " + fbdo.errorReason());
  }
}
