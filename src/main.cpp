#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include "secrets.h"

#define PIN_SUELO 33
#define PIN_LLUVIA 34
// #define PIN_MQ2 32
// #define PIN_LDR 35

#define PIN_LED 15
// #define PIN_BUZZER 16

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
  Serial.println("\nWiFi Conectado.");
  Serial.println(WiFi.localIP());
}

void reconectarMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Conectando a MQTT (Raspberry Pi)...");
    String clientId = "ESP32Gateway-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" OK");
    } else {
      Serial.print(" Falló la conexión, rc=");
      Serial.println(client.state());
    }
  }
}

/*
String calcularRiesgoFosberg(float tempC, float hum, int lluvia, int suelo, int gas, int luz) {
  if (gas > 2500) return "Extremo";
  if (lluvia < 2000) return "Bajo";

  float tempF = (tempC * 9.0 / 5.0) + 32.0;
  float m = 0;
  
  if (hum < 10.0) m = 0.03229 + (0.281073 * hum) - (0.000578 * hum * tempF);
  else if (hum <= 50.0) m = 2.22749 + (0.160107 * hum) - (0.01478 * tempF);
  else m = 21.0606 + (0.005565 * hum * hum) - (0.00035 * hum * tempF) - (0.483199 * hum);
  
  float m30 = m / 30.0;
  float n = 1.0 - (2.0 * m30) + (1.5 * m30 * m30) - (0.5 * m30 * m30 * m30);
  float f = n * (sqrt(1.0 + 25.0) / 0.3002);
  
  if (luz > 3000) f += 10.0; 

  if (f > 50.0 || suelo > 3000) return "Extremo";
  else if (f > 30.0) return "Alto";
  else if (f > 15.0) return "Moderado";
  
  return "Bajo";
}
*/

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  // pinMode(PIN_BUZZER, OUTPUT);
  // digitalWrite(PIN_BUZZER, LOW);

  Serial.println("\n--- Iniciando Nodo IoT ---");

  if (!aht.begin()) Serial.println("Error AHT20");
  if (!bmp.begin(0x76)) Serial.println("Error BMP280 en 0x76");
  
  conectarWiFi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (!client.connected()) reconectarMQTT();
  client.loop();

  unsigned long ahora = millis();
  if (ahora - ultimoEnvio > intervalo) {
    ultimoEnvio = ahora;

    sensors_event_t humedad, temperatura;
    aht.getEvent(&humedad, &temperatura);
    float presion = bmp.readPressure() / 100.0F; 

    int valorSuelo = analogRead(PIN_SUELO);
    int valorLluvia = analogRead(PIN_LLUVIA);
    
    // int valorGas = analogRead(PIN_MQ2);
    // int valorLuz = analogRead(PIN_LDR);

    // --- CÁLCULO TEMPORAL BASADO EN TEMPERATURA ---
    String estadoRiesgo = "Normal";
    if (temperatura.temperature > 28.0) {
      estadoRiesgo = "Extremo";
    }

    // estadoRiesgo = calcularRiesgoFosberg(temperatura.temperature, humedad.relative_humidity, valorLluvia, valorSuelo, valorGas, valorLuz);

    if (estadoRiesgo == "Extremo") {
      digitalWrite(PIN_LED, HIGH);
      // digitalWrite(PIN_BUZZER, HIGH);
    } else {
      digitalWrite(PIN_LED, LOW);
      // digitalWrite(PIN_BUZZER, LOW);
    }

    Serial.println("--------------------------------");
    Serial.printf("Temp: %.2f C | Hum: %.2f %% | Presión: %.2f hPa\n", temperatura.temperature, humedad.relative_humidity, presion);
    Serial.printf("Suelo: %d | Lluvia: %d\n", valorSuelo, valorLluvia);
    Serial.printf("Riesgo: %s\n", estadoRiesgo.c_str());

    if (client.connected()) {
      client.publish("uvg/nodo1/temp", String(temperatura.temperature).c_str());
      client.publish("uvg/nodo1/presion", String(presion).c_str());
      client.publish("uvg/gateway/riesgo", estadoRiesgo.c_str());
      
      // client.publish("uvg/nodo1/gas", String(valorGas).c_str());
    }
  }
}