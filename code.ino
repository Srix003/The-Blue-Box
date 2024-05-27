#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h> 
#include <ThingSpeak.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>   

const char *ssid = "Deviename"; // add your netwrok name
const char *password = "ps"; // add the password 
const char *thingSpeakApiKey = "customized"; // code available on thingspeak server 
const long channelId = 99; // available on personalized server 

WiFiClient client;

const int mq7Pin = 34;
const int mq135Pin = 35;
const int relayPin = 15; 

float V_RL_MQ7 = 0.4;
float R_L_MQ7 = 10;
float S_MQ7 = 60;

float A_MQ135_CO2 = 400;
float B_MQ135_CO2 = 0.35;
float C_MQ135_CO2 = -0.48;

float Rs_Min = 30;
float Rs_Max = 200;
float NH3_Concentration = 100;
float Vc = 5.0;

const float COThreshold = 25;
const float CO2Threshold = 5000;
const float NH3Threshold = 25;

LiquidCrystal_I2C lcd(0x27, 16, 2); 

void setup() {
  Serial.begin(9600);
  Serial.println("Gas Sensor Testing:");

  pinMode(relayPin, OUTPUT); 

  connectToWiFi();

  ThingSpeak.begin(client);

  lcd.init();
  lcd.backlight();
}
void loop() {
  int mq7RawValue = analogRead(mq7Pin);
  int mq135RawValue = analogRead(mq135Pin);

  float mq7Voltage = mq7RawValue * (3.3 / 4095.0);
  float mq135Voltage = mq135RawValue * (3.3 / 4095.0);

  float mq7PPM = (mq7Voltage - V_RL_MQ7) * (R_L_MQ7 / S_MQ7);
  float mq135PPM_CO2 = A_MQ135_CO2 * pow((mq135Voltage / B_MQ135_CO2), C_MQ135_CO2);

  float RsValue = ((Vc - mq135Voltage) * Rs_Max) / (mq135Voltage * Rs_Min);
  float NH3PPM = pow((RsValue / Rs_Min), -2.303) * NH3_Concentration;

  Serial.print("MQ7 PPM (CO): ");
  Serial.print(mq7PPM);
  Serial.print("\t MQ135 PPM (CO2): ");
  Serial.print(mq135PPM_CO2);
  Serial.print("\t NH3 PPM (Ammonia): ");
  Serial.println(NH3PPM);

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO: ");
  lcd.print(mq7PPM);
  lcd.print(" PPM");

  delay(1000); 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO2: ");
  lcd.print(mq135PPM_CO2);
  lcd.print(" PPM");

  delay(1000); 

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("NH3: ");
  lcd.print(NH3PPM);
  lcd.print(" PPM");

  delay(1000); 

  checkAndSendToThingSpeak(mq7PPM, mq135PPM_CO2, NH3PPM);

  if (mq7PPM > COThreshold || mq135PPM_CO2 > CO2Threshold || NH3PPM > NH3Threshold) {
    digitalWrite(relayPin, LOW); 

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gas Alert!");

    lcd.setCursor(0, 1);
    if (mq7PPM > COThreshold) {
      lcd.print("CO in excess");
    } else if (mq135PPM_CO2 > CO2Threshold) {
      lcd.print("CO2 in excess");
    } else if (NH3PPM > NH3Threshold) {
      lcd.print("NH3 in excess");
    }
  } else {
    digitalWrite(relayPin, HIGH); 
  }

  delay(5000);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
}

void checkAndSendToThingSpeak(float mq7, float mq135_CO2, float NH3) {
  if (mq7 > COThreshold) {
    Serial.println("Beware, polluted air! High CO concentration detected!");
    triggerIFTTTEvent("Alert1");
    triggerIFTTTEvent("Alert2");
  }
  
  if (mq135_CO2 > CO2Threshold) {
    Serial.println("Beware, polluted air! High CO2 concentration detected!");
    triggerIFTTTEvent("Alert1");
    triggerIFTTTEvent("Alert2");
  }
  
  if (NH3 > NH3Threshold) {
    Serial.println("Beware, polluted air! High NH3 concentration detected!");
    triggerIFTTTEvent("Alert1");
    triggerIFTTTEvent("Alert2");
  }

  if (WiFi.status() == WL_CONNECTED) {
    ThingSpeak.setField(1, mq7);
    ThingSpeak.setField(2, mq135_CO2);
    ThingSpeak.setField(3, NH3);
    int response = ThingSpeak.writeFields(channelId, thingSpeakApiKey);
    if (response == 200) {
      Serial.println("Data sent to ThingSpeak successfully!");
    } else {
      Serial.println("Error sending data to ThingSpeak. HTTP error code " + String(response));
    }
  } else {
    Serial.println("WiFi is not connected. Data not sent to ThingSpeak.");
  }
  
}

void triggerIFTTTEvent(String eventName) {
  String url = "https://maker.ifttt.com/trigger/" + eventName + "/json/with/key/dn7horxJmSyn3Ie0VKnijD";
  HTTPClient http;
  http.begin(url);
  int httpResponseCode = http.GET();
  http.end();
  
  Serial.println("IFTTT event triggered: " + eventName);
}
