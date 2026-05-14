#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>

// ========== CONFIG RED ==========
const char* SSID     = "CarroESP8266_ARTYOM";
const char* PASSWORD = "12345678A";
const char* WS_HOST  = "192.168.4.2";
const uint16_t WS_PORT = 8000;
const char* WS_PATH  = "/ws/car";

// ========== PINES MOTORES ==========
// Motor A (izquierda): ENA=D4, IN1=D7, IN2=D6
// Motor B (derecha):   ENB=D3, IN3=D5, IN4=D8
const int ENA = D4; const int IN1 = D7; const int IN2 = D6;
const int ENB = D3; const int IN3 = D5; const int IN4 = D8;

// ========== VARIABLES DE VELOCIDAD ==========
// Rango válido: 0–255
const int SPEED_ADELANTE = 160;   // Velocidad de avance/retroceso
const int SPEED_GIRO     = 180;   // Velocidad al girar

// ========== DURACIONES ==========
const unsigned long DUR_ADELANTE  = 350;   // ms que dura un paso adelante/atrás
const unsigned long DUR_GIRO      = 210;   // ms que dura el giro (ajusta para calibrar 90°)
const unsigned long DUR_POST_GIRO = 1000;  // ms de pausa DESPUÉS de cada giro
const unsigned long DUR_PAUSA     = 180;   // ms entre comandos consecutivos

// ========== RAMPA SUAVE ==========
// Incremento de PWM por paso durante aceleración/frenado
const int RAMP_STEP  = 15;   // cuánto sube/baja el PWM por tick
const int RAMP_DELAY = 15;   // ms entre cada tick de rampa

// ========== GLOBALES ==========
WebSocketsClient webSocket;
bool wsConnected = false;

// ========== MOTORES — BAJO NIVEL ==========
void initMotors() {
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);
  stopMotors();
}

// Para suavidad: sube de 0 al target con rampa
void rampUp(int targetSpd) {
  for (int s = 0; s <= targetSpd; s += RAMP_STEP) {
    analogWrite(ENA, s);
    analogWrite(ENB, s);
    delay(RAMP_DELAY);
  }
  analogWrite(ENA, targetSpd);
  analogWrite(ENB, targetSpd);
}

// Baja del target hasta 0 con rampa
void rampDown(int fromSpd) {
  for (int s = fromSpd; s >= 0; s -= RAMP_STEP) {
    analogWrite(ENA, s);
    analogWrite(ENB, s);
    delay(RAMP_DELAY);
  }
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
}

void stopMotors() {
  // Freno suave solo si los motores están activos
  // (llamada directa detiene sin rampa; úsala para paradas de emergencia)
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW); analogWrite(ENA, 0);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW); analogWrite(ENB, 0);
}

void setDirectionForward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // Motor A adelante
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // Motor B adelante
}

void setDirectionBackward() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // Motor A atrás
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // Motor B atrás
}

void setDirectionSpinLeft() {
  digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH);  // Motor A atrás
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);   // Motor B adelante
}

void setDirectionSpinRight() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);   // Motor A adelante
  digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH);  // Motor B atrás
}

// ========== COMANDOS ==========
// nextCmd permite decidir si hacer pausa o continuar fluido
void executeCommand(const String& cmd, const String& nextCmd) {
  Serial.print("[CMD] "); Serial.print(cmd);
  Serial.print("  ->  next: "); Serial.println(nextCmd.length() ? nextCmd : "(fin)");

  bool nextIsSame = (nextCmd == cmd);

  if (cmd == "ADELANTE") {
    setDirectionForward();
    if (!nextIsSame) rampUp(SPEED_ADELANTE);      // aceleración solo al inicio de ráfaga
    else             analogWrite(ENA, SPEED_ADELANTE), analogWrite(ENB, SPEED_ADELANTE);
    delay(DUR_ADELANTE);
    if (!nextIsSame) {
      rampDown(SPEED_ADELANTE);
      stopMotors();
      if (nextCmd.length()) delay(DUR_PAUSA);
    }

  } else if (cmd == "ATRAS") {
    setDirectionBackward();
    if (!nextIsSame) rampUp(SPEED_ADELANTE);
    else             analogWrite(ENA, SPEED_ADELANTE), analogWrite(ENB, SPEED_ADELANTE);
    delay(DUR_ADELANTE);
    if (!nextIsSame) {
      rampDown(SPEED_ADELANTE);
      stopMotors();
      if (nextCmd.length()) delay(DUR_PAUSA);
    }

  } else if (cmd == "IZQUIERDA") {
    setDirectionSpinLeft();
    analogWrite(ENA, SPEED_GIRO);
    analogWrite(ENB, SPEED_GIRO);
    delay(DUR_GIRO);
    stopMotors();
    delay(DUR_POST_GIRO);   // ← 2 segundos de pausa post-giro

  } else if (cmd == "DERECHA") {
    setDirectionSpinRight();
    analogWrite(ENA, SPEED_GIRO);
    analogWrite(ENB, SPEED_GIRO);
    delay(DUR_GIRO);
    stopMotors();
    delay(DUR_POST_GIRO);   // ← 2 segundos de pausa post-giro

  } else if (cmd == "STOP") {
    rampDown(SPEED_ADELANTE);
    stopMotors();
  }
}

// ========== PARSER DE COMANDOS ==========
void parseAndExecute(const String& msg) {
  // Espera formato: {"commands": ["ADELANTE", "DERECHA", "ADELANTE"]}
  // o el formato array simple del código original: ["ADELANTE","DERECHA"]
  String cmdList[20];
  int cmdCount = 0;

  int arrayStart = msg.indexOf("[\"");
  if (arrayStart >= 0) {
    int arrayEnd = msg.lastIndexOf("\"]");
    if (arrayEnd > arrayStart) {
      String content = msg.substring(arrayStart + 2, arrayEnd);
      content.replace("\", \"", "\",\"");
      content.replace(" ", "");
      int pos = 0, nextPos;
      while ((nextPos = content.indexOf("\",\"", pos)) >= 0 && cmdCount < 20) {
        String c = content.substring(pos, nextPos);
        if (c.length()) cmdList[cmdCount++] = c;
        pos = nextPos + 3;
      }
      String c = content.substring(pos);
      if (c.length() && cmdCount < 20) cmdList[cmdCount++] = c;
    }
  }

  Serial.print("[PARSER] "); Serial.print(cmdCount); Serial.println(" comandos");
  for (int i = 0; i < cmdCount; i++) {
    String next = (i + 1 < cmdCount) ? cmdList[i + 1] : "";
    executeCommand(cmdList[i], next);
  }
}

// ========== WEBSOCKET ==========
void webSocketEvent(WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Desconectado");
      wsConnected = false;
      break;
    case WStype_CONNECTED:
      Serial.println("[WS] Conectado");
      wsConnected = true;
      break;
    case WStype_TEXT:
      Serial.print("[WS] RX: ");
      Serial.println((char*)payload);
      parseAndExecute(String((char*)payload));
      break;
    default:
      break;
  }
}

// ========== SETUP / LOOP ==========
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n=== ESP8266 Carro — Sin Giroscopio ===");

  initMotors();

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID, PASSWORD);
  Serial.print("[WIFI] AP IP: ");
  Serial.println(WiFi.softAPIP());

  delay(1000);
  webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  Serial.println("[INFO] Velocidad avance: " + String(SPEED_ADELANTE));
  Serial.println("[INFO] Velocidad giro:   " + String(SPEED_GIRO));
  Serial.println("[INFO] Duración giro:    " + String(DUR_GIRO) + "ms");
  Serial.println("[INFO] Pausa post-giro:  " + String(DUR_POST_GIRO) + "ms");
  Serial.println("[INFO] Listo.");
}

void loop() {
  webSocket.loop();
  delay(5);
}