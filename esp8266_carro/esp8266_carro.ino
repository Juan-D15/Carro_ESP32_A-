#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <Wire.h>

// ========== CONFIG ==========
const char* SSID     = "CarroESP8266_ARTYOM";
const char* PASSWORD = "12345678A";
const char* WS_HOST  = "192.168.4.2";
const uint16_t WS_PORT = 8000;
const char* WS_PATH  = "/ws/car";

// ─────────────────────────────────────────────
//  PINES
//  SDA = D2 / GPIO4  |  SCL = D1 / GPIO5
//  Motor A (izq): ENA=D4, IN1=D7, IN2=D6
//  Motor B (der): ENB=D3, IN3=D5, IN4=D8
// ─────────────────────────────────────────────

const int ENA = D4; const int IN1 = D7; const int IN2 = D6;
const int ENB = D3; const int IN3 = D5; const int IN4 = D8;

#define MPU_ADDR 0x68
#define G_R      65.5f   // clon MPU6050 ±500°/s

// ── Ajusta estos dos valores para calibrar el giro de 90° ──
const unsigned long GIRAR_DURATION = 350;  // ms
const int           SPEED_GIRO     = 180;  // 0-255
// ───────────────────────────────────────────────────────────

const unsigned long ADELANTE_DURATION = 600;
const unsigned long STOP_DURATION     = 300;
const int           SPEED_ADELANTE    = 180;

// ========== GLOBALES ==========
WebSocketsClient webSocket;
bool  wsConnected    = false;
bool  gyroOK         = false;
float gyroOffsetZ    = 0.0f;
float yaw            = 0.0f;
int   currentHeading = 0;   // 0=N 1=E 2=S 3=O

// ========== MPU6050 ==========
bool readGyroZ(float& gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() < 6) return false;
  Wire.read(); Wire.read();   // GyX — descartar
  Wire.read(); Wire.read();   // GyY — descartar
  int16_t raw = Wire.read() << 8 | Wire.read();
  gz = raw / G_R;
  return true;
}

void initGyro() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  uint8_t err = Wire.endTransmission(true);
  Serial.print("[GYRO] Wake up → error: "); Serial.print(err);
  Serial.println(err == 0 ? " (OK)" : " (fallo)");
  if (err != 0) return;

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); Wire.write(0x00);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A); Wire.write(0x03);
  Wire.endTransmission();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1, true);
  if (Wire.available()) {
    uint8_t who = Wire.read();
    Serial.print("[GYRO] WHO_AM_I: 0x"); Serial.print(who, HEX);
    if (who == 0x68 || who == 0x70) {
      gyroOK = true;
      Serial.println(who == 0x68 ? " → OK ✓" : " → OK ✓ (clon 0x70)");
    } else {
      Serial.println(" → ERROR");
    }
  } else {
    Serial.println("[GYRO] Sin respuesta");
  }
}

void calibrateGyro() {
  Serial.println("[GYRO] Calibrando (no mover)...");
  float sum = 0; int n = 0;
  for (int i = 0; i < 200; i++) {
    float gz;
    if (readGyroZ(gz)) { sum += gz; n++; }
    delay(5);
  }
  gyroOffsetZ = (n > 0) ? sum / n : 0;
  Serial.print("[GYRO] Offset Z: "); Serial.println(gyroOffsetZ, 4);
}

void scanI2C() {
  Serial.println("[I2C] Escaneando...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print("[I2C] Dispositivo en 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (!found) Serial.println("[I2C] Ningún dispositivo");
  else { Serial.print("[I2C] Total: "); Serial.println(found); }
}

// ========== TELEMETRÍA ==========
void sendGyroTelemetry() {
  if (!wsConnected) return;
  float gz = 0.0f;
  if (gyroOK && readGyroZ(gz)) gz -= gyroOffsetZ;
  String json = "{\"type\":\"gyro\"";
  json += ",\"gyroOK\":"  + String(gyroOK ? "true" : "false");
  json += ",\"gz\":"      + String(gz, 3);
  json += ",\"yaw\":"     + String(yaw, 2);
  json += ",\"heading\":" + String(currentHeading);
  json += "}";
  webSocket.sendTXT(json);
}

// ========== MOTORES ==========
void initMotors() {
  pinMode(IN1,OUTPUT); pinMode(IN2,OUTPUT); pinMode(ENA,OUTPUT);
  pinMode(IN3,OUTPUT); pinMode(IN4,OUTPUT); pinMode(ENB,OUTPUT);
  stopMotors();
}
void stopMotors() {
  digitalWrite(IN1,LOW); digitalWrite(IN2,LOW); analogWrite(ENA,0);
  digitalWrite(IN3,LOW); digitalWrite(IN4,LOW); analogWrite(ENB,0);
}
void moveForward() {
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  analogWrite(ENA,SPEED_ADELANTE);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  analogWrite(ENB,SPEED_ADELANTE);
}
void moveBackward() {
  digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); analogWrite(ENA,SPEED_ADELANTE);
  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); analogWrite(ENB,SPEED_ADELANTE);
}
void spinLeft(int spd) {
  digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); analogWrite(ENA,spd);
  digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  analogWrite(ENB,spd);
}
void spinRight(int spd) {
  digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  analogWrite(ENA,spd);
  digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); analogWrite(ENB,spd);
}

// ========== GIRO — solo delay, sin gyro ==========
void executeGiro(const String& dir) {
  bool toLeft = (dir == "IZQUIERDA");

  Serial.print("\n[GIRO] "); Serial.print(dir);
  Serial.print("  heading: "); Serial.print(currentHeading);
  Serial.print("  speed: ");   Serial.print(SPEED_GIRO);
  Serial.print("  dur: ");     Serial.print(GIRAR_DURATION); Serial.println("ms");

  if (toLeft) spinLeft(SPEED_GIRO);
  else        spinRight(SPEED_GIRO);

  delay(GIRAR_DURATION);  // ← único control: ajusta GIRAR_DURATION y SPEED_GIRO

  stopMotors();
  delay(200);  // inercia mecánica

  // Actualizar heading y yaw (estimado, no medido)
  yaw += toLeft ? -90.0f : 90.0f;
  currentHeading = toLeft ? (currentHeading + 3) % 4 : (currentHeading + 1) % 4;

  Serial.print("[GIRO] Heading final: "); Serial.print(currentHeading);
  Serial.print("  yaw estimado: ");       Serial.println(yaw, 1);
}

// ========== COMANDOS ==========
void executeCommand(const String& cmd, const String& nextCmd) {
  Serial.print("Exec: "); Serial.print(cmd);
  Serial.print(" -> next: "); Serial.println(nextCmd);

  if (cmd == "ADELANTE") {
    moveForward();
    delay(ADELANTE_DURATION);
    if (nextCmd != "ADELANTE") {
      stopMotors();
      if (nextCmd.length() > 0) delay(STOP_DURATION);
    }
  } else if (cmd == "ATRAS") {
    moveBackward();
    delay(ADELANTE_DURATION);
    if (nextCmd != "ATRAS") {
      stopMotors();
      if (nextCmd.length() > 0) delay(STOP_DURATION);
    }
  } else if (cmd == "STOP") {
    stopMotors();
  } else if (cmd == "IZQUIERDA" || cmd == "DERECHA") {
    executeGiro(cmd);
    if (nextCmd.length() > 0) delay(STOP_DURATION);
  }
}

// ========== WEBSOCKET ==========
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED: Serial.println("WS Disconnected"); wsConnected=false; break;
    case WStype_CONNECTED:    Serial.println("WS Connected");    wsConnected=true;  break;
    case WStype_TEXT: {
      Serial.print("RX: "); Serial.println((char*)payload);
      String msg = String((char*)payload);
      String cmdList[20]; int cmdCount = 0;
      int arrayStart = msg.indexOf("[\"");
      if (arrayStart >= 0) {
        int arrayEnd = msg.lastIndexOf("\"]");
        if (arrayEnd > arrayStart) {
          String content = msg.substring(arrayStart + 2, arrayEnd);
          content.replace("\", \"", "\",\"");
          int pos = 0, nextPos;
          while ((nextPos = content.indexOf("\",\"", pos)) >= 0 && cmdCount < 20) {
            String c = content.substring(pos, nextPos); c.trim();
            if (c.length() > 0) cmdList[cmdCount++] = c;
            pos = nextPos + 3;
          }
          String c = content.substring(pos); c.trim();
          if (c.length() > 0 && cmdCount < 20) cmdList[cmdCount++] = c;
        }
      }
      for (int i = 0; i < cmdCount; i++) {
        String next = (i+1 < cmdCount) ? cmdList[i+1] : "";
        executeCommand(cmdList[i], next);
      }
      break;
    }
    default: break;
  }
}

// ========== SETUP / LOOP ==========
unsigned long lastStatus = 0;
unsigned long lastTelem  = 0;

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("\n=== ESP8266 Carro 2 Motores ===");

  Wire.begin(4, 5);
  delay(100);
  scanI2C();
  initGyro();
  if (gyroOK) calibrateGyro();

  initMotors();

  Serial.print("[GYRO] Estado: ");
  Serial.println(gyroOK ? "ACTIVO (solo telemetría)" : "NO DISPONIBLE");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  delay(2000);
  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();

  if (millis() - lastTelem > 1000) {
    lastTelem = millis();
    sendGyroTelemetry();
  }

  if (millis() - lastStatus > 3000) {
    lastStatus = millis();
    if (gyroOK) {
      float gz;
      if (readGyroZ(gz)) {
        gz -= gyroOffsetZ;
        Serial.print("[STATUS] gz="); Serial.print(gz, 3);
        Serial.print("°/s  yaw=");   Serial.print(yaw, 1);
        Serial.print("°  heading="); Serial.println(currentHeading);
      }
    } else {
      Serial.println("[STATUS] gyro=OFF");
    }
  }

  delay(10);
}