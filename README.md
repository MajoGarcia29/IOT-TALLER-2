# IOT-TALLER-2
# CarroHTTP_MQTT

Proyecto que permite controlar un carro con un **ESP32** mediante **peticiones HTTP** y **mensajes MQTT**.  
El sistema recibe instrucciones como dirección, velocidad y duración, ejecuta el movimiento y publica los datos en un **broker MQTT** para su monitoreo.

📂 Estructura:
- `src/CarroHTTP_MQTT.ino` → código principal del ESP32  
- `docs/postman_collection.json` → pruebas con Postman  
- `README.md` → descripción del proyecto  

El objetivo es integrar comunicación **HTTP + MQTT** para control y supervisión remota de un vehículo basado en microcontrolador.
