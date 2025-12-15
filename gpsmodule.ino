#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>

// ---------------- GPS ----------------
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);   // UART2

// ---------------- WIFI ----------------
const char* ssid = "anitta";
const char* password = "newpassword";

WebServer server(80);

// ---------------- SAFE ZONE ----------------
double homeLat = 10.004961;
double homeLon = 76.363041;
double safeRadius = 5.0;   // meters (balcony-friendly)

// ---------------- DATA ----------------
double currentLat = 0.0;
double currentLon = 0.0;
double distanceFromHome = 0.0;
String geoStatus = "WAITING FOR GPS FIX";

// ---------------- DISTANCE ----------------
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  double R = 6371000.0;
  double dLat = radians(lat2 - lat1);
  double dLon = radians(lon2 - lon1);

  double a = sin(dLat / 2) * sin(dLat / 2) +
             cos(radians(lat1)) * cos(radians(lat2)) *
             sin(dLon / 2) * sin(dLon / 2);

  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return R * c;
}

// ---------------- WEB PAGE ----------------
void handleRoot() {
  String page = "SMART COMPANION – GEOFENCING STATUS\n\n";

  page += "Latitude  : " + String(currentLat, 6) + "\n";
  page += "Longitude : " + String(currentLon, 6) + "\n";
  page += "Distance  : " + String(distanceFromHome) + " meters\n\n";
  page += "STATUS    : " + geoStatus + "\n";

  server.send(200, "text/plain", page);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // -------- WIFI FIRST (IMPORTANT) --------
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  int wifiTimeout = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeout < 20) {
    delay(500);
    Serial.print(".");
    wifiTimeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi FAILED");
  }

  // -------- WEB SERVER --------
  server.on("/", handleRoot);
  server.begin();
  Serial.println("Web server started");

  // -------- GPS --------
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  Serial.println("Waiting for GPS...");
}

void loop() {

  // Handle browser requests
  server.handleClient();

  // Read GPS safely
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  // If GPS FIX is valid
  if (gps.location.isValid()) {

    currentLat = gps.location.lat();
    currentLon = gps.location.lng();

    distanceFromHome = calculateDistance(
      currentLat,
      currentLon,
      homeLat,
      homeLon
    );

    if (distanceFromHome > safeRadius) {
      geoStatus = " GEOFENCE BREACHED";
    } else {
      geoStatus = "INSIDE SAFE ZONE";
    }
  }
}
