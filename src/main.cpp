#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <cJSON.h>
#include "secrets.h"

#define PIN_SUELO 33
#define PIN_LLUVIA 34
// #define PIN_MQ2 32
// #define PIN_LDR 35

//#define PIN_LLAMA 4          
//#define PIN_BOTON_PANICO 5   
//#define PIN_INCLINACION 18   

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

void enviar_telemetria_aws(float temp, float hum, float presion, int suelo, int lluvia, int gas, int luz, String riesgo, int llama, int inclinacion, int boton) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "device_id", "device_equipo2");

  cJSON *telemetria = cJSON_CreateObject();
  cJSON_AddNumberToObject(telemetria, "temperatura", temp);
  cJSON_AddNumberToObject(telemetria, "humedad", hum);
  cJSON_AddNumberToObject(telemetria, "presion", presion);
  cJSON_AddNumberToObject(telemetria, "suelo", suelo);
  cJSON_AddNumberToObject(telemetria, "lluvia", lluvia);
  cJSON_AddNumberToObject(telemetria, "gas_mq2_ppm", gas);
  cJSON_AddNumberToObject(telemetria, "luz", luz);
  cJSON_AddStringToObject(telemetria, "estado_riesgo", riesgo.c_str());
  cJSON_AddNumberToObject(telemetria, "alerta_llama", llama);
  cJSON_AddNumberToObject(telemetria, "alerta_inclinacion", inclinacion);
  cJSON_AddNumberToObject(telemetria, "boton_panico", boton);

  cJSON_AddItemToObject(root, "telemetria", telemetria);
  char *json_string = cJSON_PrintUnformatted(root);
  
  client.publish("seccion20/fogosense/datos_aws", json_string);
  
  cJSON_Delete(root);
  free(json_string);
}

void setup() {
  Serial.begin(115200);
  
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  
  // pinMode(PIN_BUZZER, OUTPUT);
  // digitalWrite(PIN_BUZZER, LOW);
  
  // pinMode(PIN_LLAMA, INPUT);
  // pinMode(PIN_INCLINACION, INPUT);
  // pinMode(PIN_BOTON_PANICO, INPUT_PULLUP); 

  Serial.println("\n--- Iniciando Nodo IoT (Fase 03) ---");

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
    
    int valorGas = 0;
    int valorLuz = 0;
    // valorGas = analogRead(PIN_MQ2);
    // valorLuz = analogRead(PIN_LDR);
    
    // int alertaLlama = digitalRead(PIN_LLAMA);
    // int alertaInclinacion = digitalRead(PIN_INCLINACION);
    //  int alertaBoton = digitalRead(PIN_BOTON_PANICO); 

    // --- CÁLCULO TEMPORAL BASADO EN TEMPERATURA ---
    String estadoRiesgo = "Normal";
    if (temperatura.temperature > 28.0) {
      estadoRiesgo = "Extremo";
    }

    // Lógica de anulación por sensores digitales
    if (alertaLlama == HIGH || alertaInclinacion == HIGH || alertaBoton == LOW) {
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
    Serial.printf("Temp: %.2f C | Hum: %.2f %% | Pres: %.2f hPa\n", temperatura.temperature, humedad.relative_humidity, presion);
    // Serial.printf("Analog -> Suelo: %d | Lluvia: %d | Gas: %d | Luz: %d\n", valorSuelo, valorLluvia, valorGas, valorLuz);
    // Serial.printf("Digital-> Llama: %d | Inclinación: %d | Botón: %d\n", alertaLlama, alertaInclinacion, alertaBoton);
    Serial.printf("Riesgo: %s\n", estadoRiesgo.c_str());

    if (client.connected()) {
      enviar_telemetria_aws(temperatura.temperature, humedad.relative_humidity, presion);
      // enviar_telemetria_aws(temperatura.temperature, humedad.relative_humidity, presion, valorSuelo, valorLluvia, valorGas, valorLuz, estadoRiesgo, alertaLlama, alertaInclinacion, alertaBoton);
    }
  }
}