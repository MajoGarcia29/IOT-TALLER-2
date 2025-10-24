#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include "config.h"                   //archivo donde estan las variables de procesador 


//objetos de red y mqtt
WebServer server(80);
WiFiClient espClient;
PubSubClient mqttClient(espClient);

//estado de movimiento
String currentCmd = "S";
int currentSpeed = 0;
unsigned long moveEnd = 0;
unsigned long lastSensorPublish = 0;

//prototipos
void setupWiFi();
void reconnectMQTT();
void setMovement(const String &cmd, int speed);
void stopMotors();
void handleMove();
void handleStatus();
void publishMQTTInstruction(const String &cmd, int speed, int duration, const String &clientIp);
float mockUltrasonicDistance();
void publishSensorData()


void setup() {
  Serial.begin(115200);

  // Configuración de pines motores
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);

  // Configurar PWM
  ledcSetup(CH_A, PWM_FREQ, PWM_RES);
  ledcSetup(CH_B, PWM_FREQ, PWM_RES);
  ledcAttachPin(ENA, CH_A);
  ledcAttachPin(ENB, CH_B);

  stopMotors();
  setupWiFi();

  // Rutas HTTP
  server.on("/move", HTTP_GET, handleMove);
  server.on("/move", HTTP_POST, handleMove);
  server.on("/status", HTTP_GET, handleStatus);
  server.begin();

  // MQTT
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  Serial.println("Servidor HTTP iniciado. Esperando comandos...");
}

void loop() {
  server.handleClient();

  if (!mqttClient.connected()) {
    reconnectMQTT();
  } else {
    mqttClient.loop();
  }

  // Detener al acabar tiempo
  if (currentCmd != "S" && millis() >= moveEnd) {
    stopMotors();
    currentCmd = "S";
    Serial.println("Duración finalizada -> motores detenidos");
  }

  // Publicar lectura de sensor cada 3 segundos
  if (millis() - lastSensorPublish > 3000) {
    publishSensorData();
    lastSensorPublish = millis();
  }
}

//funciones
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
  while (!mqttClient.connected()) {
    Serial.print("Conectando a MQTT...");
    if (mqttClient.connect("ESP32CarroClient")) {
      Serial.println("Conectado a broker MQTT");
      mqttClient.subscribe(MQTT_TOPIC_INSTRUCCIONES);
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Reintentando en 2s");
      delay(2000);
    }
  }
}

void setMovement(const String &cmd, int speed) {
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
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    ledcWrite(CH_A, max(0, speed/2)); ledcWrite(CH_B, speed);
  } else if (cmd == "R" || cmd == "r") {
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

// Endpoint /move
void handleMove() {
  IPAddress clientIP = server.client().remoteIP();
  String clientIpStr = clientIP.toString();

  String cmd = "";
  int speed = 150;
  int duration = 1;

  if (server.method() == HTTP_GET) {
    if (server.hasArg("cmd")) cmd = server.arg("cmd");
    if (server.hasArg("speed")) speed = server.arg("speed").toInt();
    if (server.hasArg("duration")) duration = server.arg("duration").toInt();
  } else if (server.method() == HTTP_POST) {
    String body = server.arg("plain");
    if (body.length()) {
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

  if (cmd.length() == 0) {
    server.send(400, "application/json", "{\"error\":\"Falta parametro cmd\"}");
    return;
  }
  if (duration <= 0) duration = 1;
  if (duration > 5) duration = 5;
  if (speed < 0) speed = 0;
  if (speed > 255) speed = 255;

  setMovement(cmd, speed);
  moveEnd = millis() + (unsigned long)duration * 1000UL;
  publishMQTTInstruction(cmd, speed, duration, clientIpStr);

  String resp = "{\"status\":\"ok\",\"cmd\":\"" + cmd + "\",\"speed\":" + String(speed) + ",\"duration\":" + String(duration) + "}";
  server.send(200, "application/json", resp);
}

// Endpoint /status
void handleStatus() {
  String body = "{\"status\":\"up\",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
  server.send(200, "application/json", body);
}

void publishMQTTInstruction(const String &cmd, int speed, int duration, const String &clientIp) {
  if (!mqttClient.connected()) reconnectMQTT();
  String payload = "{\"cmd\":\"" + cmd + "\",\"speed\":" + String(speed) + ",\"duration\":" + String(duration) + ",\"client_ip\":\"" + clientIp + "\",\"timestamp\":" + String(millis()) + "}";
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC_INSTRUCCIONES, payload.c_str());
    Serial.println("Publicado MQTT: " + payload);
  }
}

// sensor mock
float mockUltrasonicDistance() {
  // Simula una lectura aleatoria entre 5cm y 200cm
  float distance = random(5, 200);
  Serial.print("Distancia simulada: ");
  Serial.println(distance);
  return distance;
}

void publishSensorData() {
  float distance = mockUltrasonicDistance();
  String payload = "{\"distance\":" + String(distance, 2) + ",\"timestamp\":" + String(millis()) + "}";
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC_SENSOR, payload.c_str());
    Serial.println("Publicado sensor: " + payload);
  }
}


