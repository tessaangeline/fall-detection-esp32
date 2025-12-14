#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

/* ================= WIFI ================= */
const char* ssid = "5B23";
const char* password = "rajagirics";
const char* serverURL = "http://192.168.1.14:5000/predict";

/* ================= MPU ================= */
Adafruit_MPU6050 mpu;

/* ================= WINDOW CONFIG ================= */
#define WINDOW_SIZE 50      // ~1 second at 50 Hz
float accX[WINDOW_SIZE], accY[WINDOW_SIZE], accZ[WINDOW_SIZE];
float gyrX[WINDOW_SIZE], gyrY[WINDOW_SIZE], gyrZ[WINDOW_SIZE];
int bufferIndex = 0;

/* ================= STATS FUNCTIONS ================= */
float mean(float arr[], int n) {
  float s = 0;
  for (int i = 0; i < n; i++) s += arr[i];
  return s / n;
}

float stddev(float arr[], int n, float m) {
  float s = 0;
  for (int i = 0; i < n; i++)
    s += (arr[i] - m) * (arr[i] - m);
  return sqrt(s / n);
}

float maxVal(float arr[], int n) {
  float m = arr[0];
  for (int i = 1; i < n; i++) if (arr[i] > m) m = arr[i];
  return m;
}

float minVal(float arr[], int n) {
  float m = arr[0];
  for (int i = 1; i < n; i++) if (arr[i] < m) m = arr[i];
  return m;
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("ESP32 BOOTING...");

  if (!mpu.begin()) {
    Serial.println("MPU6050 NOT FOUND!");
    while (1);
  }
  Serial.println("MPU6050 READY");

  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi CONNECTED");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
}

/* ================= LOOP ================= */
void loop() {
  if (WiFi.status() != WL_CONNECTED) return;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  accX[bufferIndex] = a.acceleration.x;
  accY[bufferIndex] = a.acceleration.y;
  accZ[bufferIndex] = a.acceleration.z;

  gyrX[bufferIndex] = g.gyro.x;
  gyrY[bufferIndex] = g.gyro.y;
  gyrZ[bufferIndex] = g.gyro.z;

  bufferIndex++;

  if (bufferIndex < WINDOW_SIZE) {
    delay(20);     // ~50 Hz
    return;
  }

  /* ===== FEATURE EXTRACTION (15 FEATURES) ===== */
  float axMean = mean(accX, WINDOW_SIZE);
  float ayMean = mean(accY, WINDOW_SIZE);
  float azMean = mean(accZ, WINDOW_SIZE);

  float axStd  = stddev(accX, WINDOW_SIZE, axMean);
  float ayStd  = stddev(accY, WINDOW_SIZE, ayMean);
  float azStd  = stddev(accZ, WINDOW_SIZE, azMean);

  float axMax  = maxVal(accX, WINDOW_SIZE);
  float ayMax  = maxVal(accY, WINDOW_SIZE);
  float azMax  = maxVal(accZ, WINDOW_SIZE);

  float axMin  = minVal(accX, WINDOW_SIZE);
  float ayMin  = minVal(accY, WINDOW_SIZE);
  float azMin  = minVal(accZ, WINDOW_SIZE);

  float gxMean = mean(gyrX, WINDOW_SIZE);
  float gyMean = mean(gyrY, WINDOW_SIZE);
  float gzMean = mean(gyrZ, WINDOW_SIZE);

  /* ===== SEND TO SERVER ===== */
  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverURL);
  http.useHTTP10(true);
  http.addHeader("Content-Type", "application/json");

  String payload = "{\"features\":[" +
    String(axMean) + "," + String(ayMean) + "," + String(azMean) + "," +
    String(axStd)  + "," + String(ayStd)  + "," + String(azStd)  + "," +
    String(axMax)  + "," + String(ayMax)  + "," + String(azMax)  + "," +
    String(axMin)  + "," + String(ayMin)  + "," + String(azMin)  + "," +
    String(gxMean) + "," + String(gyMean) + "," + String(gzMean) +
  "]}";

  int httpCode = http.POST(payload);
  Serial.print("HTTP Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    String response = http.getString();
    Serial.println(response);

    if (response.indexOf("\"result\":1") != -1)
      Serial.println("🚨 FALL DETECTED");
    else
      Serial.println("✅ NORMAL ACTIVITY");
  }

  http.end();
  bufferIndex = 0;     // reset window
  delay(500);
}
