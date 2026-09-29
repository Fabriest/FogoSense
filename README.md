# Sistema de Detección de Riesgo de Incendio IoT 

Este repositorio contiene el código fuente y la documentación de una red de sensores IoT diseñada para la detección y alerta temprana de condiciones propicias para incendios en entornos comunitarios. 

Proyecto desarrollado para el curso CC3108 Internet of Things, Universidad del Valle de Guatemala.

## Equipo de Desarrollo
* Marco Prera 25468
* Sebastian Rodas 25038
* Fabricio Estrada 25230

## Arquitectura del Sistema
El sistema opera bajo una arquitectura *Edge-to-Cloud* de bajo costo:
1. **Nodo 1 (Sensor):** Adquiere telemetría ambiental y transmite vía WiFi/MQTT.
2. **Nodo 2 (Gateway + Sensor):** Recibe los datos, lee sus propios sensores, calcula el Índice de Riesgo de Incendio (basado en el modelo Fosberg adaptado) y determina si existen condiciones críticas.
3. **Alertas:** Activación de actuadores locales (LED y Buzzer) y notificaciones remotas vía un Bot de Telegram.

## Hardware Utilizado
* 2x ESP32
* 2x Sensores de Temperatura y Humedad Relativa (DHT11)
* 2x Sensores de Gases Inflamables y Humo (MQ-2)
* 2x Sensores Capacitivos de Humedad de Suelo (v1.2)
* 2x Sensores de Precipitación (YL-83)
* 2x Sensores de Luz (LDR)
* Buzzers Activos y LEDs de potencia.

## Software y Dependencias
El firmware está escrito en C++ (compatible con Arduino IDE y VS Code).
Las librerías requeridas son:
* `WiFi.h`: Para la conexión a la red local.
* `PubSubClient`: Para el manejo de publicación/suscripción del protocolo MQTT.
* `DHT sensor library` (por Adafruit): Para la lectura del sensor DHT11.
* `UniversalTelegramBot` (opcional, si se maneja Telegram directo desde el ESP32).

