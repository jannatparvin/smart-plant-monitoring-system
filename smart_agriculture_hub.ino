/*
 * Smart Agriculture Hub - IIEST Shibpur Final Build
 * Logic: Holistic Multi-Parameter Diagnosis (Stacked)
 * Fault Tolerance: 6 vs 7 Parameter Fusion (pH Delay Neglect)
 */

#define BLYNK_TEMPLATE_ID "TMPL3Ff9xhAjs"
#define BLYNK_TEMPLATE_NAME "Smart Agriculture Hub"
#define BLYNK_AUTH_TOKEN "YOUR BLYNK TOKEN"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

char auth[] = BLYNK_AUTH_TOKEN;
char ssid[] = "YOUR SSID"; 
char pass[] = "YOUR PASSWORD";

// Pin Definitions
#define RELAY_PIN 26        
#define MOISTURE_PIN 34     
#define DHT_PIN 15          
#define LDR_PIN 35          
#define EC_PIN 32           
#define EC_PULSE_PIN 25     
#define ONE_WIRE_BUS 4      

DHT dht(DHT_PIN, DHT22);
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
BlynkTimer timer;

// Data Variables
int currentMode = 0;        
float labviewPH = -1.0;     // Initialized to -1.0 (Flag for "No Data")
float greenRatio = 0.0;     
String plantStatus = "---";

BLYNK_WRITE(V10) { currentMode = param.asInt(); }

void updateSystem() {
  if(currentMode !=0) {
    digitalWrite(RELAY_PIN,HIGH);
  }
  sensors.requestTemperatures();
  float soilT = sensors.getTempCByIndex(0);
  float airT = dht.readTemperature();
  float hum = dht.readHumidity();
  int light = analogRead(LDR_PIN);
  
  // DIY EC Reading with Pulse Logic (Prevents Corrosion)
  digitalWrite(EC_PULSE_PIN, HIGH);
  delay(20);
  float vOut = analogRead(EC_PIN) * (3.3 / 4095.0);
  digitalWrite(EC_PULSE_PIN, LOW);
  float ecValue = (vOut > 0.1) ? ((1.0 / (((3.3 * 1000.0) / vOut) - 1000.0)) * 0.75 * 1000.0) : 0;

  // Moisture Mapping
  int mPercent = constrain(map(analogRead(MOISTURE_PIN), 4095, 1500, 0, 100), 0, 100);

  // Send raw CSV to LabVIEW for Telemetry
  Serial.print(airT); Serial.print(","); Serial.print(hum); Serial.print(",");
  Serial.print(mPercent); Serial.print(","); Serial.print(light); Serial.print(",");
  Serial.print(ecValue); Serial.print(","); Serial.println(soilT);

  // Push raw values to individual Blynk Widgets
  Blynk.virtualWrite(V0, mPercent);
  Blynk.virtualWrite(V2, airT);
  Blynk.virtualWrite(V3, hum);
  Blynk.virtualWrite(V4, light);
  Blynk.virtualWrite(V5, ecValue);
  Blynk.virtualWrite(V6, soilT);

  // --- MODE-BASED LOGIC ---
  
  if (currentMode == 0) { // MODE 0: Auto Irrigation (Hysteresis)
    if (mPercent < 30) {
    digitalWrite(RELAY_PIN, LOW);
    Blynk.virtualWrite(V1, "PUMP: ON"); // Force the text update
} else if (mPercent > 70) {
    digitalWrite(RELAY_PIN, HIGH);
    Blynk.virtualWrite(V1, "PUMP: OFF"); // Force the text update
}
  }
  
  else if (currentMode == 1) { // MODE 1: Stacked Diagnostic (6 vs 7 Parameters)
    String diagnostic = ""; 

    // 1. Check the 6 Local Hardware Parameters
    if (mPercent < 30)       diagnostic += "Dry Soil. ";
    if (ecValue > 2.5)       diagnostic += "High Salt. ";
    if (airT > 40)           diagnostic += "Too Hot. ";
    if (airT < 10)           diagnostic += "Too Cold. ";
    if (hum < 25)            diagnostic += "Dry Air. ";
    if (light < 150)         diagnostic += "Low Light. ";
    
    // 2. Check the 7th Parameter (LabVIEW pH) ONLY IF VALID (pH > 1.0)
    // If labviewPH is -1.0, the system NEGLECTS this part of the diagnosis
    if (labviewPH > 1.0 && labviewPH < 14.0) {
        if (labviewPH < 5.5)  diagnostic += "Acidic. ";
        if (labviewPH > 8.5)  diagnostic += "Alkaline. ";
    }
    
    // 3. Final Output Logic
    if (diagnostic == "") diagnostic = "SYSTEM HEALTHY";

    Blynk.virtualWrite(V7, (labviewPH > 0 ? labviewPH : 0)); // Send pH to Gauge
    Blynk.virtualWrite(V12, diagnostic); // Send combined report to Phone
  }
  
  else if (currentMode == 2) { // MODE 2: Vision Feedback
    Blynk.virtualWrite(V8, greenRatio); 
    Blynk.virtualWrite(V9, plantStatus);
  }
}

// Data Handling for Incoming LabVIEW Packets
void receiveLabVIEW() {
  if (Serial.available() > 0) {
    String incoming = Serial.readStringUntil('\n');
    char header = incoming.charAt(0);
    String payload = incoming.substring(1);

    if (header == 'P') {
      labviewPH = payload.toFloat(); // Update the pH value
    } 
    else if (header == 'G') {
      int comma = payload.indexOf(',');
      if (comma != -1) {
        greenRatio = payload.substring(0, comma).toFloat();
        plantStatus = payload.substring(comma + 1);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(EC_PULSE_PIN, OUTPUT);
  dht.begin(); 
  sensors.begin(); 
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(2000L, updateSystem); 
}

void loop() {
  Blynk.run();
  timer.run();
  receiveLabVIEW(); 
}