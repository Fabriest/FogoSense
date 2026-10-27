#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include "secrets.h"


#define PIN_SUELO 33
#define PIN_LLUVIA 34

#define PIN_LED 15

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;
const long intervalo = 5000; 

void conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Conectado. IP Asignada: ");
  Serial.println(WiFi.localIP());
}

void reconectarMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Intentando conexión MQTT a la Raspberry Pi...");
    String clientId = "ESP32Gateway-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" ¡Conectado al Broker Mosquitto!");
    } else {
      Serial.print(" Falló la conexión, rc=");
      Serial.println(client.state());
    }
  }
}


String calcularRiesgoFosberg(float temp, float hum, int lluvia, int suelo) {
  if (temp > 28.0) {
    return "Extremo";
  } else if (temp > 25.0 && hum < 40.0) {
    return "Alto";
  }
  return "Bajo";
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  Serial.println("\n--- Iniciando Sistema IoT (Fase 03) ---");

  if (!aht.begin()) {
    Serial.println("Error: No se encuentra AHT20.");
  }
  if (!bmp.begin(0x76)) { 
    Serial.println("Error: No se encuentra BMP280 en la dirección 0x76.");
  }
  
  conectarWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (!client.connected()) {
    reconectarMQTT();
  }
  client.loop();

  unsigned long ahora = millis();
  if (ahora - ultimoEnvio > intervalo) {
    ultimoEnvio = ahora;

    sensors_event_t humedad, temperatura;
    aht.getEvent(&humedad, &temperatura);
    float presion = bmp.readPressure() / 100.0F; 

    int valorSuelo = analogRead(PIN_SUELO);
    int valorLluvia = analogRead(PIN_LLUVIA);

    String estadoRiesgo = calcularRiesgoFosberg(temperatura.temperature, humedad.relative_humidity, valorLluvia, valorSuelo);

    if (estadoRiesgo == "Extremo") {
      digitalWrite(PIN_LED, HIGH);
    } else {
      digitalWrite(PIN_LED, LOW);
    }

    Serial.println("--------------------------------");
    Serial.printf("Temp: %.2f C | Hum: %.2f %% | Presión: %.2f hPa\n", temperatura.temperature, humedad.relative_humidity, presion);
    Serial.printf("Suelo: %d | Lluvia: %d\n", valorSuelo, valorLluvia);
    Serial.printf("Riesgo Calculado: %s\n", estadoRiesgo.c_str());

    if (client.connected()) {
      client.publish("uvg/nodo1/temp", String(temperatura.temperature).c_str());
      client.publish("uvg/nodo1/presion", String(presion).c_str());
      client.publish("uvg/gateway/riesgo", estadoRiesgo.c_str());
    }
  }
}