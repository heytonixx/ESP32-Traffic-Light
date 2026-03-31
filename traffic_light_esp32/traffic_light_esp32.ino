// ================================================================
// ESP32 Traffic Light → Firebase Realtime Database
//
// UPDATED: Dashboard now owns the auto-cycle. ESP32 is a pure
// signal follower — it reads /trafficLight/signal and lights the
// correct LED regardless of mode. No internal timer loop.
//
// Sequence (driven by dashboard):
//   GREEN (configurable) → YELLOW (5 s fixed) → RED (configurable)
//   → GREEN  (yellow is skipped after red)
//
// Required Library (install via Arduino Library Manager):
//   Firebase ESP32 Client  →  by Mobizt
// ================================================================
#include <WiFi.h>
#include <FirebaseESP32.h>

// ----------------------------------------------------------------
// Pin Definitions
// ----------------------------------------------------------------
#define PIN_RED    2    // D2  → RED LED
#define PIN_YELLOW 4    // D4  → YELLOW LED
#define PIN_GREEN  16   // D16 → GREEN LED

// ----------------------------------------------------------------
// WiFi & Firebase credentials
// ----------------------------------------------------------------
#define WIFI_SSID     "iPhone"
#define WIFI_PASSWORD "heytonixx"

#define FIREBASE_HOST "traffic-light-esp32.firebaseapp.com"
#define FIREBASE_AUTH "AIzaSyCjNL9JvwRLOZy4oE9fWuMCZODr6rZTpgM"

// Poll Firebase this often (ms).
// Kept short so LEDs react quickly to dashboard changes.
#define POLL_INTERVAL 500

// ----------------------------------------------------------------
FirebaseData   fbData;
FirebaseAuth   fbAuth;
FirebaseConfig fbConfig;

// ----------------------------------------------------------------
// State
// ----------------------------------------------------------------
String currentSignal = "";   // last applied signal — "" forces first apply
String currentMode   = "";   // "auto" | "manual"

// ----------------------------------------------------------------
// Apply signal to LEDs — only writes pins when signal actually
// changes so we don't spam digitalWrite on every poll tick.
// ----------------------------------------------------------------
void applySignal(const String& signal) {
  if (signal == currentSignal) return;   // nothing changed
  currentSignal = signal;

  digitalWrite(PIN_RED,    signal == "red"    ? HIGH : LOW);
  digitalWrite(PIN_YELLOW, signal == "yellow" ? HIGH : LOW);
  digitalWrite(PIN_GREEN,  signal == "green"  ? HIGH : LOW);

  Serial.println("[LED] Signal applied → " + signal);
}

// ================================================================
void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED,    OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN,  OUTPUT);

  // Boot blink test — verifies all three LEDs are wired correctly
  Serial.println("Boot LED test...");
  digitalWrite(PIN_RED,    HIGH); delay(300); digitalWrite(PIN_RED,    LOW);
  digitalWrite(PIN_YELLOW, HIGH); delay(300); digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_GREEN,  HIGH); delay(300); digitalWrite(PIN_GREEN,  LOW);

  // ── Connect WiFi ─────────────────────────────────────────────
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected! IP: " + WiFi.localIP().toString());

  // ── Connect Firebase ─────────────────────────────────────────
  fbConfig.host = FIREBASE_HOST;
  fbConfig.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&fbConfig, &fbAuth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase connected!");

  // ── Announce online ──────────────────────────────────────────
  Firebase.setBool  (fbData, "/trafficLight/esp32Online", true);
  Firebase.setString(fbData, "/trafficLight/ip",          WiFi.localIP().toString());
  Firebase.setInt   (fbData, "/trafficLight/uptime",      0);

  // NOTE: We do NOT write signal/mode/durations here anymore.
  // The dashboard is the authority — let it keep whatever values
  // it already has in Firebase. We just read and follow.

  Serial.println("ESP32 ready — following dashboard signal.");
}

// ================================================================
void loop() {

  // ── Poll Firebase for current signal ─────────────────────────
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll >= POLL_INTERVAL) {
    lastPoll = millis();

    // Read mode (for logging only — we apply signal in BOTH modes)
    if (Firebase.getString(fbData, "/trafficLight/mode")) {
      currentMode = fbData.stringData();
    }

    // Always read and apply signal regardless of mode.
    // In AUTO mode  → dashboard writes signal on its timer cycle.
    // In MANUAL mode → dashboard writes signal on button press.
    // ESP32 just follows in both cases.
    if (Firebase.getString(fbData, "/trafficLight/signal")) {
      String demanded = fbData.stringData();
      if (demanded.length() > 0) {
        applySignal(demanded);   // no-op if unchanged
      }
    }

    // Log poll status
    if (fbData.httpCode() == 200) {
      Serial.println("Poll OK | mode=" + currentMode + " | signal=" + currentSignal);
    } else {
      Serial.println("Poll ERR: " + fbData.errorReason());
    }
  }

  // ── Heartbeat every 10 s ─────────────────────────────────────
  // Writes uptime, online flag, and IP so dashboard can display them.
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    Firebase.setInt   (fbData, "/trafficLight/uptime",      (int)(millis() / 1000));
    Firebase.setBool  (fbData, "/trafficLight/esp32Online", true);
    Firebase.setString(fbData, "/trafficLight/ip",          WiFi.localIP().toString());
    Serial.println("[HB] uptime=" + String((int)(millis() / 1000)) + "s");
  }

  // ── WiFi watchdog ────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Lost — reconnecting...");
    WiFi.reconnect();
    delay(3000);
  }
}
