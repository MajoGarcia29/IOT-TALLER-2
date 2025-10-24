#ifndef CONFIG_H
#define CONFIG_H

// wifi
#define WIFI_SSID "iPhone 14 pro de Axel"
#define WIFI_PASS "12345678"

//mqtt
#define MQTT_BROKER "test.mosquitto.org"
#define MQTT_PORT 1883
#define MQTT_TOPIC_INSTRUCCIONES "carro/instrucciones"
#define MQTT_TOPIC_SENSOR "carro/distancia"

// pines motores L298N 
#ifndef ENA
#define ENA 32
#endif
#ifndef ENB
#define ENB 33
#endif
#ifndef IN1
#define IN1 25
#endif
#ifndef IN2
#define IN2 26
#endif
#ifndef IN3
#define IN3 27
#endif
#ifndef IN4
#define IN4 14
#endif

// pines sensor HC-SR04
#ifndef TRIG_PIN
#define TRIG_PIN 35
#endif
#ifndef ECHO_PIN
#define ECHO_PIN 34
#endif

// PWM
#define PWM_FREQ 5000
#define PWM_RES 8
#define CH_A 0
#define CH_B 1

#endif
