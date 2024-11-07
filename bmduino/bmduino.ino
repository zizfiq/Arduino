int pH_pin = A0; // Pin yang digunakan untuk sensor pH
int pH_Value;
float Voltage;
float pH;

void setup() 
{ 
  Serial.begin(9600);
  pinMode(pH_pin, INPUT); 
  Serial.println("Mulai Kalibrasi pH...");
} 
 
void loop() 
{ 
  // Membaca nilai analog dari sensor
  pH_Value = analogRead(pH_pin); 
  
  // Mengonversi nilai analog menjadi tegangan
  Voltage = pH_Value * (5.0 / 1023.0); 
  
  // Rumus sederhana untuk konversi tegangan ke pH
  // (slope dan intercept harus diperoleh dari kalibrasi)
  pH = 7.0 - ((Voltage - 2.5) / 0.18); // Contoh rumus sederhana

  // Menampilkan hasil tegangan dan nilai pH
  Serial.print("Voltage: ");
  Serial.print(Voltage);
  Serial.print(" V, ");
  Serial.print("pH: ");
  Serial.println(pH); 

  delay(500);  
}
