#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

namespace pin {
  const byte tds_sensor = A0;
  const byte one_wire_bus = D5;
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

//====================================
void setup() {
  Serial.begin(115200);
  dallasTemperature.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  delay(1000);
}

//====================================
void loop() {
  baca_Tds();
  delay(1000);
}

//====================================
void baca_Tds() {
  dallasTemperature.requestTemperatures();
  sensor::waterTemp = dallasTemperature.getTempCByIndex(0);
  float rawEC = analogRead(pin::tds_sensor) * device::aref / 1024.0;
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
  display.setTextColor(WHITE);
  display.print("TDS:" + String(sensor::tds));
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print(" ppm");
  display.setCursor(5, 20);
  display.setTextSize(2);
  display.print("EC:" + String(sensor::ec, 2));
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.print(" S/m");
  display.setCursor(5, 45);
  display.setTextSize(2);
  display.print("T:" + String(sensor::waterTemp, 2) + " C");
  display.display();
}
