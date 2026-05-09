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
//
//  MPU6050 I2C — igual que el ejemplo funcional
//  SDA = D2 / GPIO4
//  SCL = D1 / GPIO5
//
//  Motor A (izquierdo) — sin cambios
//  ENA = D4 / GPIO2   PWM
//  IN1 = D7 / GPIO13
//  IN2 = D6 / GPIO12
//
//  Motor B (derecho) — sin cambios
//  ENB = D3 / GPIO0   PWM
//  IN3 = D5 / GPIO14
//  IN4 = D8 / GPIO15
// ─────────────────────────────────────────────

// Motor A
const int ENA = D4; const int IN1 = D7; const int IN2 = D6;
// Motor B
const int ENB = D3; const int IN3 = D5; const int IN4 = D8;

// MPU6050
#define MPU_ADDR 0x68
#define G_R      131.0f    // ±250°/s → 131 LSB/°/s

// Tiempos y velocidades
const unsigned long ADELANTE_DURATION = 1200;
const unsigned long GIRAR_DURATION    = 400;
const unsigned long STOP_DURATION     = 100;
const int SPEED_ADELANTE = 200;
const int SPEED_GIRO     = 200;
const int SPEED_CORREC   = 150;

// ========== GLOBALES ==========
WebSocketsClient webSocket;
bool  wsConnected  = false;
bool  gyroOK       = false;
float gyroOffsetZ  = 0.0f;
float yaw          = 0.0f;   // ángulo acumulado
int   currentHeading = 0;    // 0=N 1=E 2=S 3=O

// ========== MPU6050 ==========
// Lee solo GyZ — igual que el ejemplo pero extrae solo el eje Z
bool readGyroZ(float& gz) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);               // registro inicio: GyX_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 6, true);
  if (Wire.available() < 6) return false;
  Wire.read(); Wire.read();       // GyX — descartar
  Wire.read(); Wire.read();       // GyY — descartar
  int16_t raw = Wire.read() << 8 | Wire.read(); // GyZ
  gz = raw / G_R;
  return true;
}

void initGyro() {
  // Wire.begin ya fue llamado en setup() antes de esta función
  // Wake up
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); Wire.write(0x00);
  uint8_t err = Wire.endTransmission(true);
  Serial.print("[GYRO] Wake up → error: "); Serial.print(err);
  Serial.println(err == 0 ? " (OK)" : " (fallo — verificar SDA/SCL/VCC/ADO)");
  if (err != 0) return;

  // Rango giroscopio ±250°/s
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B); Wire.write(0x00);
  Wire.endTransmission();

  // Filtro DLPF nivel 3 → ~44Hz
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A); Wire.write(0x03);
  Wire.endTransmission();

  // Verificar WHO_AM_I
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x75);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, 1, true);
  if (Wire.available()) {
    uint8_t who = Wire.read();
    Serial.print("[GYRO] WHO_AM_I: 0x"); Serial.print(who, HEX);
    if (who == 0x68 || who == 0x70) {  // 0x70 = clon MPU6050
      gyroOK = true;
      Serial.println(who == 0x68 ? " → OK ✓" : " → OK ✓ (clon, who=0x70)");
    } else {
      Serial.println(" → ERROR (valor desconocido)");
    }
  } else {
    Serial.println("[GYRO] Sin respuesta en WHO_AM_I");
  }
}

void calibrateGyro() {
  Serial.println("[GYRO] Calibrando offset (no mover el carro)...");
  float sum = 0; int n = 0;
  for (int i = 0; i < 200; i++) {
    float gz;
    if (readGyroZ(gz)) { sum += gz; n++; }
    delay(5);
  }
  gyroOffsetZ = (n > 0) ? sum / n : 0;
  Serial.print("[GYRO] Offset Z: "); Serial.print(gyroOffsetZ, 4);
  Serial.println(" °/s");
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

// ========== GIRO CON CORRECCIÓN GYRO ==========
void executeGiro(const String& dir) {
  bool toLeft = (dir == "IZQUIERDA");
  float targetDeg = 90.0f;
  float turned    = 0.0f;
  unsigned long lastT = millis();

  Serial.print("\n[GIRO] "); Serial.print(dir);
  Serial.print("  heading: "); Serial.print(currentHeading);
  Serial.print("  yaw: ");     Serial.print(yaw, 1);
  Serial.print("°  gyro: ");   Serial.println(gyroOK ? "ON" : "OFF→delay fijo");

  if (toLeft) spinLeft(SPEED_GIRO);
  else        spinRight(SPEED_GIRO);

  if (gyroOK) {
    unsigned long startT = millis();
    int logN = 0;
    while (turned < targetDeg && millis() - startT < 1500) {
      float gz;
      if (readGyroZ(gz)) {
        gz -= gyroOffsetZ;                        // aplicar calibración
        unsigned long dtMs = millis() - lastT;
        turned += fabs(gz) * (dtMs / 1000.0f);   // integrar ángulo

        // Log cada ~40ms
        if (++logN % 8 == 0) {
          Serial.print("  gz=");     Serial.print(gz, 2);
          Serial.print("°/s  girado="); Serial.print(turned, 1);
          Serial.print("/");         Serial.print(targetDeg, 0); Serial.println("°");
        }

        // Si detecta rotación contraria al esperado → parar
        // (si gz siempre aparece con signo contrario, intercambia los signos aquí)
        if ((toLeft && gz > 3.0f) || (!toLeft && gz < -3.0f)) {
          Serial.println("  [!] Rotación inversa detectada — deteniendo");
          break;
        }
        lastT = millis();
      }
      delay(5);
    }
  } else {
    delay(GIRAR_DURATION);
  }

  stopMotors();
  yaw += toLeft ? -turned : turned;

  // Corrección fina
  if (gyroOK) {
    float error = targetDeg - turned;
    Serial.print("[GIRO] Girado: "); Serial.print(turned, 1);
    Serial.print("°  error: ");      Serial.print(error, 1); Serial.println("°");

    if (fabs(error) > 8.0f) {
      Serial.print("[CORREC] error="); Serial.print(error, 1);
      Serial.print("° → girando ");
      // error>0: faltó ángulo → continuar en misma dirección
      // error<0: se pasó   → revertir dirección
      bool corrLeft = (error > 0) ? toLeft : !toLeft;
      Serial.println(corrLeft ? "IZQ" : "DER");
      if (corrLeft) spinLeft(SPEED_CORREC);
      else          spinRight(SPEED_CORREC);
      delay((int)(fabs(error) * 4));
      stopMotors();
      delay(50);
    } else {
      Serial.println("[CORREC] Dentro de tolerancia (<8°)");
    }
  }

  currentHeading = toLeft ? (currentHeading + 3) % 4 : (currentHeading + 1) % 4;
  Serial.print("[GIRO] Heading final: "); Serial.print(currentHeading);
  Serial.print("  yaw total: "); Serial.println(yaw, 1);
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

void scanI2C() {
  Serial.println("[I2C] Escaneando bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print("[I2C] Dispositivo en 0x");
      if (addr < 16) Serial.print("0");
      Serial.println(addr, HEX);
      found++;
    }
  }
  if (found == 0) Serial.println("[I2C] Ningún dispositivo encontrado");
  else { Serial.print("[I2C] Total: "); Serial.println(found); }
}

void setup() {
  Serial.begin(115200);
  delay(5000);  // 5s para abrir el serial monitor
  Serial.println("\n=== ESP8266 Carro 2 Motores + Gyro ===");

  // Gyro PRIMERO — antes de que initMotors toque los pines
  Wire.begin(4, 5);  // GPIO4=SDA(D2), GPIO5=SCL(D1)
  delay(100);
  scanI2C();         // ver qué hay en el bus antes de init
  initGyro();
  if (gyroOK) calibrateGyro();

  initMotors();      // motores después del gyro

  Serial.print("[GYRO] Estado final: ");
  Serial.println(gyroOK ? "ACTIVO ✓" : "NO DISPONIBLE — usando delays fijos");

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

  // Status gyro cada 3 segundos
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