#include <WiFi.h>

#include <KiwisIoT.h>

#include <DHT.h>

#include <Wire.h>

#include <BH1750.h>

// -------- WIFI SETTINGS --------

const char* ssid = "YOUR_WIFI_NAME";

const char* pass = "YOUR_WIFI_PASSWORD";

// Replace with your actual KiwisIoT panel topic

const char* topic = "dash_YOUR_PANEL_ID";

// -------- GPIO CONNECTIONS --------

#define SOIL_PIN 34

#define PH_PIN 35

#define DHT_PIN 4

#define RELAY_PIN 26

#define DHTTYPE DHT22

// -------- SENSOR OBJECTS --------

DHT dht(DHT_PIN, DHTTYPE);

BH1750 lightSensor;

KiwisIoT kiwisiot(ssid, pass, topic);

// -------- CALIBRATION --------

// Replace these with your measured soil sensor values.

const int DRY_VALUE = 3000;

const int WET_VALUE = 1400;

// Example pH calibration coefficients.

// Replace after calibration using pH 4 and pH 7 buffers.

const float PH_SLOPE = -5.70;

const float PH_OFFSET = 21.34;

// -------- PUMP SETTINGS --------

#define RELAY_ON LOW

#define RELAY_OFF HIGH

const int DRY_THRESHOLD = 35;

const int WET_THRESHOLD = 55;

const unsigned long MAX_PUMP_TIME = 15000;

const unsigned long PUMP_COOLDOWN = 30000;

bool autoMode = true;

bool pumpState = false;

unsigned long pumpStart = 0;

unsigned long lastPumpStop = 0;

unsigned long lastSend = 0;

// -------- PUMP CONTROL --------

void pumpOn() {

if (pumpState) return;

pumpState = true;

pumpStart = millis();

digitalWrite(RELAY_PIN, RELAY_ON);

Serial.println("Water Pump ON");

}

void pumpOff() {

if (!pumpState) return;

pumpState = false;

lastPumpStop = millis();

digitalWrite(RELAY_PIN, RELAY_OFF);

Serial.println("Water Pump OFF");

}

// -------- DASHBOARD COMMANDS --------

void onControl(String ch, String val) {

// Channel 3: Manual Pump Control

if (ch == "3" && !autoMode) {

if (val == "1") {

pumpOn();

} else {

pumpOff();

}

}

// Channel 4: Auto / Manual Mode

if (ch == "4") {

autoMode = (val == "1");

if (autoMode) {

pumpOff();

Serial.println("Automatic Mode");

} else {

pumpOff();

Serial.println("Manual Mode");

}

}

}

// -------- SETUP --------

void setup() {

Serial.begin(115200);

pinMode(SOIL_PIN, INPUT);

pinMode(PH_PIN, INPUT);

pinMode(RELAY_PIN, OUTPUT);

digitalWrite(RELAY_PIN, RELAY_OFF);

analogReadResolution(12);

analogSetPinAttenuation(SOIL_PIN, ADC_11db);

analogSetPinAttenuation(PH_PIN, ADC_11db);

dht.begin();

Wire.begin(21, 22);

if (!lightSensor.begin()) {

Serial.println("BH1750 sensor not detected");

}

kiwisiot.begin();

kiwisiot.onReceive(onControl);

Serial.println("Crop Health Monitoring Started");

}

// -------- MAIN LOOP --------

void loop() {

// Process cloud commands

kiwisiot.run();

// Read sensors

int soilRaw = analogRead(SOIL_PIN);

int phRaw = analogRead(PH_PIN);

float temperature = dht.readTemperature();

float humidity = dht.readHumidity();

float lux = lightSensor.readLightLevel();

// Soil moisture percentage

int soilMoisture = map(

soilRaw,

DRY_VALUE,

WET_VALUE,

0,

100

);

soilMoisture = constrain(soilMoisture, 0, 100);

// Approximate pH calculation

float phVoltage = phRaw * (3.3 / 4095.0);

float soilPH =

PH_SLOPE * phVoltage + PH_OFFSET;

// -------- AUTOMATIC IRRIGATION --------

if (autoMode) {

if (soilMoisture <= DRY_THRESHOLD &&

!pumpState &&

millis() - lastPumpStop >= PUMP_COOLDOWN) {

pumpOn();

}

if (soilMoisture >= WET_THRESHOLD) {

pumpOff();

}

}

// -------- PUMP SAFETY TIMER --------

if (pumpState &&

millis() - pumpStart >= MAX_PUMP_TIME) {

pumpOff();

Serial.println("Pump safety timeout!");

}

// -------- SEND DATA TO KIWISIOT --------

if (millis() - lastSend >= 3000) {

lastSend = millis();

kiwisiot.send("0", soilMoisture);

if (!isnan(temperature)) {

kiwisiot.send("1", temperature);

}

if (!isnan(humidity)) {

kiwisiot.send("2", humidity);

}

if (!isnan(lux) && lux >= 0) {

kiwisiot.send("6", lux);

}

// Send only after pH module calibration

kiwisiot.send("7", soilPH);

Serial.println("\n--- CROP HEALTH DATA ---");

Serial.print("Soil Moisture: ");

Serial.print(soilMoisture);

Serial.println("%");

Serial.print("Soil pH: ");

Serial.println(soilPH);

Serial.print("Temperature: ");

Serial.println(temperature);

Serial.print("Humidity: ");

Serial.println(humidity);

Serial.print("Light Intensity: ");

Serial.print(lux);

Serial.println(" lux");

Serial.print("Pump: ");

Serial.println(pumpState ? "ON" : "OFF");

}

delay(100);

}