#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#include <dht11.h>

#define buz 8                // Buzzer pin
#define analogInPin A0       // Analog input pin for battery voltage
#define chargingPin 12       // Pin to detect charging status
#define flamePin A1          // Analog input pin for flame sensor
#define dht_apin 11          // DHT11 data pin

// Calibration factor for voltage calculation
float calibration = 0.36;
int sensorValue;
float voltage;
int bat_percentage;
bool isCharging;
bool flameDetected;

// LCD configuration
LiquidCrystal lcd(2, 3, 4, 5, 6, 7);

// DHT11 configuration
dht11 dhtObject;

// Wi-Fi and ThingSpeak configuration
#define RX 9
#define TX 10
String AP = "elegant";
String PASS = "smartwork";
String API = "QK5NSQGJB0FIW2XS";
String HOST = "api.thingspeak.com";
String PORT = "80";

// Software serial for ESP8266
SoftwareSerial esp8266(RX, TX);

void setup() {
  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("WELCOME");
  delay(1000);
  lcd.clear();
  lcd.print("BATTERY MANAGEMENT ");
  lcd.setCursor(0, 1);
  lcd.print("   FOR EV");
  delay(1000);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buz, OUTPUT);
  pinMode(chargingPin, INPUT);
  pinMode(flamePin, INPUT);
  Serial.begin(9600);
  esp8266.begin(115200);

  sendCommand("AT", 5, "OK");
  sendCommand("AT+CWMODE=1", 5, "OK");
  sendCommand("AT+CWJAP=\"" + AP + "\",\"" + PASS + "\"", 20, "OK");
}

void loop() {
  // Reading temperature and humidity from DHT11
  String temperature = getTemperatureValue();
  String humidity = getHumidityValue();

  // Reading battery voltage and percentage
  sensorValue = analogRead(analogInPin);
  voltage = (((sensorValue * 3.3) / 1024) * 2 + calibration);
  bat_percentage = mapfloat(voltage, 2.8, 4.2, 0, 100);

  // Clamping battery percentage
  if (bat_percentage >= 100) bat_percentage = 100;
  if (bat_percentage <= 0) bat_percentage = 1;

  // Charging detection
  isCharging = digitalRead(chargingPin);

  // Flame detection (analog input)
  int flameValue = analogRead(flamePin);
  flameDetected = (flameValue < 300);  // Adjust threshold as needed

  // Display flame sensor value on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Flame Val: ");
  lcd.print(flameValue);
  delay(1000);

  // Flame detection alert and handling
  if (flameDetected) {
    // Stop charging if charging is in progress
    if (isCharging) {
      digitalWrite(chargingPin, LOW);  // Turn off charging relay
    }
    // Activate buzzer for flame detection
    digitalWrite(buz, HIGH);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("FLAME DETECTED!");
    lcd.setCursor(0, 1);
    lcd.print("CHARGING OFF");
    Serial.println("Warning: Flame Detected! Charging Stopped.");

    // Sending flame alert to ThingSpeak
    String flameAlert = "GET /update?api_key=" + API + "&field5=FlameDetected";
    sendCommand("AT+CIPMUX=1", 5, "OK");
    sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 15, "OK");
    sendCommand("AT+CIPSEND=0," + String(flameAlert.length() + 4), 4, ">");
    esp8266.println(flameAlert);
    delay(1500);
    sendCommand("AT+CIPCLOSE=0", 5, "OK");

    delay(1000);  // Buzzer on delay
    digitalWrite(buz, LOW);  // Turn off buzzer
  }

  // Displaying battery and charging status on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: " + temperature + "C");
  lcd.setCursor(0, 1);
  lcd.print("Hum: " + humidity + "%");
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Batt: ");
  lcd.print(voltage);
  lcd.print("V");
  lcd.setCursor(0, 1);
  lcd.print("Chg: ");
  lcd.print(isCharging ? "Yes" : "No");
  lcd.print(" ");
  lcd.print(bat_percentage);
  lcd.print("%");
  delay(1000);

  // Sending data to ThingSpeak
  String getData = "GET /update?api_key=" + API + "&field1=" + temperature + "&field2=" + humidity + "&field3=" + String(voltage) + "&field4=" + String(bat_percentage);
  sendCommand("AT+CIPMUX=1", 5, "OK");
  sendCommand("AT+CIPSTART=0,\"TCP\",\"" + HOST + "\"," + PORT, 15, "OK");
  sendCommand("AT+CIPSEND=0," + String(getData.length() + 4), 4, ">");
  esp8266.println(getData);
  delay(1500);
  sendCommand("AT+CIPCLOSE=0", 5, "OK");

  // Low battery alert
  if (bat_percentage < 20 && !isCharging) {
    Serial.println("Warning: Low Battery! Please recharge soon.");
    digitalWrite(buz, HIGH);  // Activate buzzer
    delay(1000);
    digitalWrite(buz, LOW);   // Deactivate buzzer
  }

  delay(2000);
}

String getTemperatureValue() {
  dhtObject.read(dht_apin);
  float temp = dhtObject.temperature;
  Serial.print("Temperature: ");
  Serial.println(temp);
  return String(temp);
}

String getHumidityValue() {
  dhtObject.read(dht_apin);
  float humidity = dhtObject.humidity;
  Serial.print("Humidity: ");
  Serial.println(humidity);
  return String(humidity);
}

void sendCommand(String command, int maxTime, char readReply[]) {
  Serial.println(command);
  int countTimeCommand = 0;
  bool found = false;
  while (countTimeCommand < (maxTime * 1)) {
    esp8266.println(command);
    if (esp8266.find(readReply)) {
      found = true;
      break;
    }
    countTimeCommand++;
  }
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
