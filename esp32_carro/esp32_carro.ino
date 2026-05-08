#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Wire.h>

// ========== CONFIGURABLE CONSTANTS ==========
const char* SSID = "CarroESP32_ARTYOM";
const char* PASSWORD = "12345678A";
const char* WS_HOST = "192.168.4.2";
const uint16_t WS_PORT = 8000;
const char* WS_PATH = "/ws/car";

const int IN1 = 26;
const int IN2 = 27;
const int ENA = 25;
const int IN3 = 14;
const int IN4 = 12;
const int ENB = 13;

const unsigned long ADELANTE_DURATION = 800;
const unsigned long GIRAR_DURATION = 450;
const unsigned long STOP_DURATION = 200;

const int MPU6050_ADDR = 0x68;
const float GYRO_SCALE = 131.0;

// ========== GLOBAL VARIABLES ==========
WebSocketsClient webSocket;
bool wsConnected = false;
bool gyroAvailable = false;
float heading = 0.0;       // heading continuo 0-360° (0=NORTE, 90=ESTE, 180=SUR, 270=OESTE)
int logicalHeading = 0;    // 0=NORTE, 1=ESTE, 2=SUR, 3=OESTE

// ========== MPU6050 ==========
void initGyro() {
  Wire.begin(21, 22);
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  if (Wire.endTransmission() == 0) {
    gyroAvailable = true;
    Serial.println("Gyro OK");
  }
}

bool readGyro(float& gz) {
  if (!gyroAvailable) return false;
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x47);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 2);
  if (Wire.available() >= 2) {
    int16_t val = Wire.read() << 8 | Wire.read();
    gz = val / GYRO_SCALE;
    return true;
  }
  return false;
}

// ========== MOTORS ==========
void initMotors() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);
  stopMotors();
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  analogWrite(ENA, 0);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  analogWrite(ENB, 0);
}

void moveForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  analogWrite(ENA, 200);
}

void moveBackward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  analogWrite(ENA, 200);
}

void steerLeft() {
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  analogWrite(ENB, 180);
}

void steerRight() {
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  analogWrite(ENB, 180);
}

void centerSteering(bool fromRight) {
  if (fromRight) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  } else {
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  }
  analogWrite(ENB, 150);
  delay(100);
  stopMotors();
}

// ========== COMMANDS ==========
int getLogicalHeading(const String& dir) {
  if (dir == "GIRAR_NORTE") return 0;
  if (dir == "GIRAR_ESTE") return 1;
  if (dir == "GIRAR_SUR") return 2;
  if (dir == "GIRAR_OESTE") return 3;
  return -1;
}

int headingToDir(float h) {
  int dir = round(h / 90.0);
  while (dir < 0) dir += 4;
  while (dir >= 4) dir -= 4;
  return dir;
}

void executeGirar(const String& direction) {
  int targetDir = getLogicalHeading(direction);
  if (targetDir < 0) return;

  float targetAngle = targetDir * 90.0;
  float diff = targetAngle - heading;
  while (diff > 180)  diff -= 360;
  while (diff < -180) diff += 360;

  if (fabs(diff) < 2.0) {
    logicalHeading = targetDir;
    return;
  }

  bool turnLeft = (diff > 0);
  float targetTurn = fabs(diff);
  float angleTurned = 0;
  unsigned long lastRead = millis();

  if (turnLeft) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    analogWrite(ENA, 180);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    analogWrite(ENB, 160);
  } else {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    analogWrite(ENA, 180);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    analogWrite(ENB, 160);
  }

  if (gyroAvailable) {
    unsigned long startWait = millis();
    unsigned long timeout = (targetTurn > 100) ? 2000 : 1000;
    while (angleTurned < targetTurn && millis() - startWait < timeout) {
      float gyroZ;
      if (readGyro(gyroZ)) {
        unsigned long dt = millis() - lastRead;
        angleTurned += fabs(gyroZ) * (dt / 1000.0);
        lastRead = millis();
      }
      delay(5);
    }
  } else {
    delay((targetTurn > 100) ? GIRAR_DURATION * 2 : GIRAR_DURATION);
  }

  stopMotors();

  heading += turnLeft ? angleTurned : -angleTurned;
  while (heading < 0)   heading += 360;
  while (heading >= 360) heading -= 360;

  if (gyroAvailable) {
    float error = targetAngle - heading;
    while (error > 180)  error -= 360;
    while (error < -180) error += 360;

    if (fabs(error) > 8.0) {
      bool correctLeft = (error > 0);
      if (correctLeft) {
        digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
      } else {
        digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
      }
      analogWrite(ENB, 150);
      delay(fabs(error) * 5);
      stopMotors();
      delay(60);
    }
  }

  logicalHeading = headingToDir(heading);
  centerSteering(!turnLeft);
}

void executeCommand(const String& cmd) {
  Serial.print("Exec: ");
  Serial.println(cmd);

  if (cmd == "ADELANTE") {
    moveForward();
    delay(ADELANTE_DURATION);
    stopMotors();
  } else if (cmd == "ATRAS") {
    moveBackward();
    delay(ADELANTE_DURATION);
    stopMotors();
  } else if (cmd == "STOP") {
    stopMotors();
  } else if (cmd.startsWith("GIRAR_")) {
    executeGirar(cmd);
  }
}

// ========== WEBSOCKET ==========
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("Disconnected");
      wsConnected = false;
      break;
    case WStype_CONNECTED:
      Serial.println("Connected");
      wsConnected = true;
      break;
    case WStype_TEXT: {
      Serial.print("RX: ");
      Serial.println((char*)payload);

      String msg = String((char*)payload);
      int cmdStart = msg.indexOf("[\"");
      if (cmdStart >= 0) {
        int cmdEnd = msg.lastIndexOf("\"]");
        if (cmdEnd > cmdStart) {
          String cmdsStr = msg.substring(cmdStart + 2, cmdEnd);
          int pos = 0;
          int nextPos;
          while ((nextPos = cmdsStr.indexOf("\",\"", pos)) >= 0) {
            String cmd = cmdsStr.substring(pos, nextPos);
            cmd.trim();
            if (cmd.length() > 0) executeCommand(cmd);
            pos = nextPos + 3;
          }
          String cmd = cmdsStr.substring(pos);
          cmd.trim();
          if (cmd.length() > 0) executeCommand(cmd);
        }
      }
      break;
    }
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("=== ESP32 Carro A* ===");

  initMotors();
  initGyro();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  delay(2000);
  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();
  delay(10);
}