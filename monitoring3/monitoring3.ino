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
  const byte ph_sensor = A0;  // Po ke A0
  const byte ph_digital = D3; // Do ke D3
  const byte one_wire_bus = D5;
  const byte servo_pin = D6;
}

namespace device {
  float aref = 3.3;
}

namespace sensor {
  float pH = 0;
  float waterTemp = 0;
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
bool servoStatus = false;

void baca_pH();
void uploadToFirebase();
void controlServoFromFirebase();
void uploadServoStatusToFirebase(bool status);
void tampilkanOLED();

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

  pinMode(pin::ph_digital, INPUT);  // Mengatur pin D3 sebagai input untuk Do sensor pH
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 1000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    baca_pH();
    uploadToFirebase();
  }

  controlServoFromFirebase();
  tampilkanOLED();
}

void baca_pH() {
  dallasTemperature.requestTemperatures();
  sensor::waterTemp = dallasTemperature.getTempCByIndex(0);

  // Membaca nilai dari Po (Analog) untuk pH
  int analogValue = analogRead(pin::ph_sensor);
  float voltage = analogValue * (device::aref / 1024.0);
  sensor::pH = 3.5 * voltage;  // Formula sederhana untuk pH

  // Membaca nilai dari Do (Digital)
  int digitalValue = digitalRead(pin::ph_digital);

  Serial.print(F("pH: ")); Serial.println(sensor::pH);
  Serial.print(F("Temperature: ")); Serial.println(sensor::waterTemp, 2);
  Serial.print(F("pH Digital Value (Do): ")); Serial.println(digitalValue);  // Menampilkan nilai digital
}

void uploadToFirebase() {
  if (Firebase.RTDB.setFloat(&fbdo, "sensorData/pH", sensor::pH)) {
    Serial.println("pH data sent to Firebase");
  } else {
    Serial.println("Failed to send pH data: " + fbdo.errorReason());
  }

  if (Firebase.RTDB.setFloat(&fbdo, "sensorData/Temperature", sensor::waterTemp)) {
    Serial.println("Temperature data sent to Firebase");
  } else {
    Serial.println("Failed to send Temperature data: " + fbdo.errorReason());
  }
}

void controlServoFromFirebase() {
  if (Firebase.RTDB.getBool(&fbdo, "servoControl/status")) {
    servoStatus = fbdo.boolData();
    Serial.println("Current servo status: " + String(servoStatus));

    if (servoStatus) {
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

void tampilkanOLED() {
  display.clearDisplay();
  
  // Menampilkan nilai pH
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.print("pH:");
  display.print(sensor::pH);

  // Menampilkan suhu air
  display.setCursor(0, 20);
  display.setTextSize(2);
  display.print("T:");
  display.print(sensor::waterTemp, 2);
  display.print(" C");

  // Menampilkan status servo
  display.setCursor(0, 40);
  display.setTextSize(2);
  display.print("Servo: ");
  if (servoStatus) {
    display.print("ON");
  } else {
    display.print("OFF");
  }

  display.display();
}
