#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>
#include <Firebase_ESP_Client.h>
#include <ESP8266WiFi.h>
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
  const byte tds_sensor = A0;
  const byte one_wire_bus = D5;
  const byte servo_pin = D6;
}

namespace device {
  float aref = 3.3;
}

namespace sensor {
  float ec = 0;
  unsigned int tds = 0;
  float waterTemp = 0;
  float ecCalibration = 1;
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
    uploadToFirebase();
  }

  controlServoFromFirebase();
}

void baca_Tds() {
  dallasTemperature.requestTemperatures();
  sensor::waterTemp = dallasTemperature.getTempCByIndex(0);
  float rawEC = analogRead(pin::tds_sensor) * device::aref / 1024.0;
  float temperatureCoefficient = 1.0 + 0.02 * (sensor::waterTemp - 25.0);

  sensor::ec = (rawEC / temperatureCoefficient) * sensor::ecCalibration;
  sensor::tds = (133.42 * pow(sensor::ec, 3) - 255.86 * sensor::ec * sensor::ec + 857.39 * sensor::ec) * 0.5;

  
  // Print to serial monitor
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
  display.print("EC:" + String(sensor::ec, 2));
  display.setTextSize(1);
  display.print(" S/m");
  
  display.setCursor(5, 45);
  display.setTextSize(2);
  display.print("T:" + String(sensor::waterTemp, 2) + " C");
  display.display();
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
