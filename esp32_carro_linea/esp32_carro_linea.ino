#include <WiFi.h>
#include <WebSocketsClient.h>
#include <Wire.h>

// =============================================================
// PLANTILLA DE PINES - reemplaza -1 por el GPIO real del ESP32.
// Evita usar pines de arranque si tu modulo no lo permite.
// =============================================================
const int PIN_L298N_IN1 = -1;  // Motor izquierdo IN1
const int PIN_L298N_IN2 = -1;  // Motor izquierdo IN2
const int PIN_L298N_ENA = -1;  // Motor izquierdo ENA/PWM
const int PIN_L298N_IN3 = -1;  // Motor derecho IN3
const int PIN_L298N_IN4 = -1;  // Motor derecho IN4
const int PIN_L298N_ENB = -1;  // Motor derecho ENB/PWM

const int PIN_TCRT_LEFT = -1;   // Sensor TCRT5000 izquierdo DO/AO digitalizado
const int PIN_TCRT_RIGHT = -1;  // Sensor TCRT5000 derecho DO/AO digitalizado

const int PIN_MPU_SDA = -1;  // SDA GY521/MPU6050
const int PIN_MPU_SCL = -1;  // SCL GY521/MPU6050

// =============================================================
// Red y WebSocket. El ESP32 crea el AP; el servidor corre en 192.168.4.2.
// =============================================================
const char* WIFI_SSID = "CarroESP32_LINEA";
const char* WIFI_PASSWORD = "12345678A";
const char* WS_HOST = "192.168.4.2";
const uint16_t WS_PORT = 8000;
const char* WS_PATH = "/ws/car";

// =============================================================
// Ajustes de movimiento. Calibra estos valores con el carro real.
// =============================================================
const bool LINE_ACTIVE_LOW = true;      // true si el TCRT marca linea con LOW
const int SPEED_BASE = 165;             // Velocidad recta
const int SPEED_CORRECTION = 75;        // Correccion al perder un sensor
const int SPEED_TURN = 150;             // Giro sobre eje
const int SPEED_SEARCH = 120;           // Busqueda suave de linea
const unsigned long NODE_TIMEOUT_MS = 6500;
const unsigned long INTERSECTION_HOLD_MS = 120;
const unsigned long TURN_TIMEOUT_90_MS = 1400;
const unsigned long TURN_TIMEOUT_180_MS = 2600;
const float TURN_ANGLE_TOLERANCE = 8.0;

const int MPU6050_ADDR = 0x68;
const float GYRO_SCALE = 131.0;

enum Heading { NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3 };

WebSocketsClient webSocket;
bool wsConnected = false;
bool pinsReady = false;
bool gyroReady = false;
Heading currentHeading = EAST;

bool pinConfigured(int pin) {
  return pin >= 0;
}

bool allPinsConfigured() {
  return pinConfigured(PIN_L298N_IN1) && pinConfigured(PIN_L298N_IN2) &&
         pinConfigured(PIN_L298N_ENA) && pinConfigured(PIN_L298N_IN3) &&
         pinConfigured(PIN_L298N_IN4) && pinConfigured(PIN_L298N_ENB) &&
         pinConfigured(PIN_TCRT_LEFT) && pinConfigured(PIN_TCRT_RIGHT) &&
         pinConfigured(PIN_MPU_SDA) && pinConfigured(PIN_MPU_SCL);
}

void setMotorRaw(int inA, int inB, int en, int speed) {
  if (!pinConfigured(inA) || !pinConfigured(inB) || !pinConfigured(en)) return;
  int pwm = constrain(abs(speed), 0, 255);
  if (speed > 0) {
    digitalWrite(inA, HIGH);
    digitalWrite(inB, LOW);
  } else if (speed < 0) {
    digitalWrite(inA, LOW);
    digitalWrite(inB, HIGH);
  } else {
    digitalWrite(inA, LOW);
    digitalWrite(inB, LOW);
  }
  analogWrite(en, pwm);
}

void setMotors(int leftSpeed, int rightSpeed) {
  if (!pinsReady) return;
  setMotorRaw(PIN_L298N_IN1, PIN_L298N_IN2, PIN_L298N_ENA, leftSpeed);
  setMotorRaw(PIN_L298N_IN3, PIN_L298N_IN4, PIN_L298N_ENB, rightSpeed);
}

void stopMotors() {
  setMotors(0, 0);
}

bool readLineSensor(int pin) {
  int value = digitalRead(pin);
  return LINE_ACTIVE_LOW ? value == LOW : value == HIGH;
}

bool lineLeft() {
  return pinsReady && readLineSensor(PIN_TCRT_LEFT);
}

bool lineRight() {
  return pinsReady && readLineSensor(PIN_TCRT_RIGHT);
}

void initPins() {
  pinsReady = allPinsConfigured();
  if (!pinsReady) {
    Serial.println("Pines sin configurar: reemplaza -1 en la plantilla.");
    return;
  }

  pinMode(PIN_L298N_IN1, OUTPUT);
  pinMode(PIN_L298N_IN2, OUTPUT);
  pinMode(PIN_L298N_ENA, OUTPUT);
  pinMode(PIN_L298N_IN3, OUTPUT);
  pinMode(PIN_L298N_IN4, OUTPUT);
  pinMode(PIN_L298N_ENB, OUTPUT);
  pinMode(PIN_TCRT_LEFT, INPUT);
  pinMode(PIN_TCRT_RIGHT, INPUT);
  stopMotors();
}

void initMPU6050() {
  if (!pinConfigured(PIN_MPU_SDA) || !pinConfigured(PIN_MPU_SCL)) return;
  Wire.begin(PIN_MPU_SDA, PIN_MPU_SCL);
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  gyroReady = Wire.endTransmission() == 0;
  Serial.println(gyroReady ? "MPU6050 OK" : "MPU6050 no detectado");
}

bool readGyroZ(float& gzDps) {
  if (!gyroReady) return false;
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x47);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom(MPU6050_ADDR, 2);
  if (Wire.available() < 2) return false;
  int16_t raw = (Wire.read() << 8) | Wire.read();
  gzDps = raw / GYRO_SCALE;
  return true;
}

float readTurnAngleAbs(unsigned long& lastMs) {
  float gz = 0.0;
  unsigned long now = millis();
  unsigned long dt = now - lastMs;
  lastMs = now;
  if (!readGyroZ(gz)) return 0.0;
  return abs(gz) * (dt / 1000.0);
}

Heading headingFromCommand(const String& command) {
  if (command == "GIRAR_NORTE") return NORTH;
  if (command == "GIRAR_ESTE") return EAST;
  if (command == "GIRAR_SUR") return SOUTH;
  if (command == "GIRAR_OESTE") return WEST;
  return currentHeading;
}

int turnDelta(Heading from, Heading to) {
  int diff = (int)to - (int)from;
  if (diff < 0) diff += 4;
  return diff;
}

void turnLeftInPlace(int speed) {
  setMotors(-speed, speed);
}

void turnRightInPlace(int speed) {
  setMotors(speed, -speed);
}

void executeTimedTurn(bool left, unsigned long durationMs) {
  if (left) turnLeftInPlace(SPEED_TURN);
  else turnRightInPlace(SPEED_TURN);
  delay(durationMs);
  stopMotors();
}

void executeGyroTurn(bool left, float targetDegrees, unsigned long timeoutMs) {
  float angle = 0.0;
  unsigned long start = millis();
  unsigned long last = millis();
  if (left) turnLeftInPlace(SPEED_TURN);
  else turnRightInPlace(SPEED_TURN);

  while (angle < targetDegrees - TURN_ANGLE_TOLERANCE && millis() - start < timeoutMs) {
    angle += readTurnAngleAbs(last);
    delay(5);
  }
  stopMotors();
}

void executeTurnToHeading(Heading targetHeading) {
  int diff = turnDelta(currentHeading, targetHeading);
  if (diff == 0) return;

  if (diff == 1) {
    gyroReady ? executeGyroTurn(false, 90, TURN_TIMEOUT_90_MS) : executeTimedTurn(false, TURN_TIMEOUT_90_MS);
  } else if (diff == 3) {
    gyroReady ? executeGyroTurn(true, 90, TURN_TIMEOUT_90_MS) : executeTimedTurn(true, TURN_TIMEOUT_90_MS);
  } else {
    gyroReady ? executeGyroTurn(false, 180, TURN_TIMEOUT_180_MS) : executeTimedTurn(false, TURN_TIMEOUT_180_MS);
  }

  currentHeading = targetHeading;
  delay(120);
}

bool atIntersection() {
  if (!lineLeft() || !lineRight()) return false;
  unsigned long start = millis();
  while (millis() - start < INTERSECTION_HOLD_MS) {
    if (!lineLeft() || !lineRight()) return false;
    delay(5);
  }
  return true;
}

void followLineStep() {
  bool left = lineLeft();
  bool right = lineRight();

  if (left && right) {
    setMotors(SPEED_BASE, SPEED_BASE);
  } else if (left && !right) {
    setMotors(SPEED_BASE - SPEED_CORRECTION, SPEED_BASE + SPEED_CORRECTION);
  } else if (!left && right) {
    setMotors(SPEED_BASE + SPEED_CORRECTION, SPEED_BASE - SPEED_CORRECTION);
  } else {
    setMotors(SPEED_SEARCH, SPEED_SEARCH / 2);
  }
}

void followLineUntilIntersection() {
  if (!pinsReady) return;
  unsigned long start = millis();

  while (millis() - start < NODE_TIMEOUT_MS) {
    followLineStep();
    if (millis() - start > 350 && atIntersection()) {
      stopMotors();
      delay(160);
      return;
    }
    webSocket.loop();
    delay(8);
  }

  stopMotors();
  Serial.println("Timeout buscando siguiente nodo/interseccion");
}

void executeCommand(const String& command) {
  Serial.print("CMD: ");
  Serial.println(command);

  if (command == "ADELANTE") {
    followLineUntilIntersection();
  } else if (command == "STOP") {
    stopMotors();
  } else if (command.startsWith("GIRAR_")) {
    executeTurnToHeading(headingFromCommand(command));
  }
}

void parseAndExecuteCommands(const String& msg) {
  int arrayStart = msg.indexOf('[');
  int arrayEnd = msg.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd <= arrayStart) return;

  int pos = arrayStart + 1;
  while (pos < arrayEnd) {
    int quoteStart = msg.indexOf('"', pos);
    if (quoteStart < 0 || quoteStart >= arrayEnd) break;
    int quoteEnd = msg.indexOf('"', quoteStart + 1);
    if (quoteEnd < 0 || quoteEnd > arrayEnd) break;

    String command = msg.substring(quoteStart + 1, quoteEnd);
    command.trim();
    if (command.length() > 0) executeCommand(command);
    pos = quoteEnd + 1;
  }
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("WebSocket conectado");
      break;
    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("WebSocket desconectado");
      break;
    case WStype_TEXT:
      parseAndExecuteCommands(String((char*)payload));
      break;
    default:
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("=== CarroESP seguidor de linea A* ===");

  initPins();
  initMPU6050();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();
  delay(10);
}
