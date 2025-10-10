#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// ---------- CONFIG ----------
const char* WIFI_SSID = "iPhone 14 pro de Axel";
const char* WIFI_PASS = "12345678";

const char* MQTT_BROKER = "test.mosquitto.org"; 
const int   MQTT_PORT   = 1883;
const char* MQTT_TOPIC  = "carro/instrucciones";

// Pines L298N 
#define IN1 25
#define IN2 26
#define IN3 27
#define IN4 14
#define ENA 32 // PWM pin motor A
#define ENB 33 // PWM pin motor B

// PWM config (ESP32 uses canales)
const int PWM_FREQ = 5000;
const int PWM_RES  = 8; // 8 bits -> 0..255
const int CH_A = 0;
const int CH_B = 1;

// ---------- Estado de movimiento ----------
String currentCmd = "S"; // S = stop
int currentSpeed = 0;
unsigned long moveEnd = 0; // millis() cuando debe detenerse

// ---------- Objetos red y mqtt ----------
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ---------- Prototipos ----------
void setupWiFi();
void reconnectMQTT();
void setMovement(const String &cmd, int speed);
void stopMotors();
void handleMove();
void handleStatus();
void publishMQTTInstruction(const String &cmd, int speed, int duration, const String &clientIp);

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);

  // Pines como salida
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // Configurar PWM
ledcAttach(ENA, 5000, 8); // pin, frecuencia , resoluci n (8 bits = 0-255)                                                                                                            ledcAttach(ENB, 5000, 8);                                                                                                                                                                                                           detener();
  stopMotors();
  setupWiFi();

  // Rutas HTTP
  server.on("/move", HTTP_GET, handleMove);    // acepta GET con query params: ?cmd=F&speed=150&duration=2
  server.on("/move", HTTP_POST, handleMove);   
  server.on("/status", HTTP_GET, handleStatus);

  server.begin();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  Serial.println("Servidor HTTP iniciado. Esperando comandos...");
}

// ---------- Loop ----------
void loop() {
  server.handleClient();

  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }

  // Control no bloqueante: detener si tiempo expiró
  if (currentCmd != "S" && millis() >= moveEnd) {
    stopMotors();
    currentCmd = "S";
    Serial.println("Duración finalizada -> motores detenidos");
  }
}

// ---------- FUNCIONES ----------

void setupWiFi() {
  Serial.printf("Conectando a %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("Conectado. IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nNo fue posible conectar a WiFi.");
  }
}

void reconnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.print("Conectando a MQTT...");
  while (!mqttClient.connected()) {
    if (mqttClient.connect("ESP32CarroClient")) {
      Serial.println("conectado");
    } else {
      Serial.print("fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" reintentando en 2s");
      delay(2000);
    }
  }
}

void setMovement(const String &cmd, int speed) {
  // Limitar speed
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;

  if (cmd == "F" || cmd == "f") {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_A, speed); ledcWrite(CH_B, speed);
  } else if (cmd == "B" || cmd == "b") {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    ledcWrite(CH_A, speed); ledcWrite(CH_B, speed);
  } else if (cmd == "L" || cmd == "l") {
    // giro a la izquierda: motor A más lento
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_A, max(0, speed/2)); ledcWrite(CH_B, speed);
  } else if (cmd == "R" || cmd == "r") {
    // giro a la derecha: motor B más lento
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_A, speed); ledcWrite(CH_B, max(0, speed/2));
  } else {
    stopMotors();
  }
  currentCmd = cmd;
  currentSpeed = speed;
}

void stopMotors() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
  ledcWrite(CH_A, 0); ledcWrite(CH_B, 0);
  currentSpeed = 0;
}

// Handler del endpoint /move
void handleMove() {
  // Obtener IP cliente
  IPAddress clientIP = server.client().remoteIP();
  String clientIpStr = clientIP.toString();

  // Parámetros por GET (query) o POST (raw body JSON/simple)
  String cmd = "";
  int speed = 150;
  int duration = 1;

  if (server.method() == HTTP_GET) {
    if (server.hasArg("cmd")) cmd = server.arg("cmd");
    if (server.hasArg("speed")) speed = server.arg("speed").toInt();
    if (server.hasArg("duration")) duration = server.arg("duration").toInt();
  } else if (server.method() == HTTP_POST) {
    // Intentamos parsear simple JSON: {"cmd":"F","speed":150,"duration":2}
    String body = server.arg("plain");
    if (body.length()) {
      // parse simple (sin ArduinoJson por simplicidad)
      int p1 = body.indexOf("\"cmd\"");
      if (p1>=0) {
        int q = body.indexOf(":", p1);
        int q1 = body.indexOf("\"", q);
        int q2 = body.indexOf("\"", q1+1);
        if (q1>=0 && q2>q1) cmd = body.substring(q1+1, q2);
      }
      int p2 = body.indexOf("\"speed\"");
      if (p2>=0) {
        int q = body.indexOf(":", p2);
        speed = body.substring(q+1).toInt();
      }
      int p3 = body.indexOf("\"duration\"");
      if (p3>=0) {
        int q = body.indexOf(":", p3);
        duration = body.substring(q+1).toInt();
      }
    }
  }

  // Validaciones
  if (cmd.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Falta parametro cmd\"}");
    return;
  }
  if (duration <= 0) duration = 1;
  if (duration > 5) duration = 5; // regla: max 5s
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;

  // Ejecutar movimiento (no bloqueante)
  setMovement(cmd, speed);
  moveEnd = millis() + (unsigned long)duration * 1000UL;

  // Publicar en MQTT
  publishMQTTInstruction(cmd, speed, duration, clientIpStr);

  // Responder
  String resp = "{\"status\":\"ok\",\"cmd\":\"" + cmd + "\",\"speed\":" + String(speed) + ",\"duration\":" + String(duration) + "}";
  server.send(200, "application/json", resp);
}

// Handler /status
void handleStatus() {
  String body = "{\"status\":\"up\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", body);
}

void publishMQTTInstruction(const String &cmd, int speed, int duration, const String &clientIp) {
  if (!mqttClient.connected()) {
    // intentar reconectar rápido
    reconnectMQTT();
  }
  String payload = "{\"cmd\":\"" + cmd + "\",\"speed\":" + String(speed) + ",\"duration\":" + String(duration) + ",\"client_ip\":\"" + clientIp + "\",\"timestamp\":" + String(millis()) + "}";
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC, payload.c_str());
    Serial.println("Publicado MQTT: " + payload);
  } else {
    Serial.println("No conectado a MQTT, no se publicó");
  }
}
