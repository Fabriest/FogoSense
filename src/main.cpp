#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <cJSON.h>
#include "secrets.h"

#define PIN_SUELO 33
#define PIN_LLUVIA 34
#define PIN_MQ2 32
#define PIN_LDR 35

#define PIN_LLAMA 4          
#define PIN_BOTON_PANICO 5   
#define PIN_INCLINACION 18   

#define PIN_LED 15
#define PIN_BUZZER 16

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;
WiFiClient espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;
const long intervalo = 10000;

bool ahtListo = false;
bool bmpListo = false;

void conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int intentos = 0;
  // Máximo 20 intentos (10 segundos) para no congelar el ESP32 si no hay red
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Conectado.");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nSin WiFi. Iniciando en Modo Local (Offline)...");
  }
}

void reconectarMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    Serial.print("Intentando conectar a Raspberry Pi (MQTT)...");
    String clientId = "ESP32Gateway-" + String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      Serial.println(" OK");
    } else {
      Serial.print(" No disponible (rc=");
      Serial.print(client.state());
      Serial.println("). Operando localmente.");
    }
  }
}

float obtenerValorFosberg(float tempC, float hum, int luz) {
  float tempF = (tempC * 9.0 / 5.0) + 32.0;
  float m = 0;
  
  if (hum < 10.0) m = 0.03229 + (0.281073 * hum) - (0.000578 * hum * tempF);
  else if (hum <= 50.0) m = 2.22749 + (0.160107 * hum) - (0.01478 * tempF);
  else m = 21.0606 + (0.005565 * hum * hum) - (0.00035 * hum * tempF) - (0.483199 * hum);
  
  float m30 = m / 30.0;
  float n = 1.0 - (2.0 * m30) + (1.5 * m30 * m30) - (0.5 * m30 * m30 * m30);
  float f = n * (sqrt(1.0 + 25.0) / 0.3002);
  
  if (luz > 3000) f += 10.0; 
  return f;
}

String evaluarRiesgo(float ffwi, float tempC, int lluvia, int suelo, int gas, int llama, int inclinacion, int boton) {
  if (tempC > 28.0 || gas > 2000 || llama == LOW || inclinacion == LOW || boton == LOW) {
    return "Extremo";
  }
  
  if (lluvia < 2000) return "Bajo";

  if (ffwi > 50.0 || suelo > 3500) return "Extremo";
  else if (ffwi > 30.0) return "Alto";
  else if (ffwi > 15.0) return "Moderado";
  
  return "Bajo";
}

void enviar_telemetria_aws(float temp, float hum, float presion, int suelo, int lluvia, int gas, int luz, float ffwi, String riesgo, int llama, int inclinacion, int boton) {
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
  cJSON_AddNumberToObject(telemetria, "indice_ffwi", ffwi);
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
  delay(1000); 
  
  Serial.println("\n--- Iniciando Nodo IoT FogoSense ---");

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_BUZZER, LOW);
  
  pinMode(PIN_LLAMA, INPUT);               
  pinMode(PIN_INCLINACION, INPUT_PULLUP);  
  pinMode(PIN_BOTON_PANICO, INPUT_PULLUP); 

  Serial.println("Inicializando sensores I2C...");
  ahtListo = aht.begin();
  if (!ahtListo) Serial.println("-> Aviso: Error AHT20 (Revisa cables SDA/SCL)");
  
  bmpListo = bmp.begin(0x76);
  if (!bmpListo) Serial.println("-> Aviso: Error BMP280 en 0x76");
  
  conectarWiFi();
  
  client.setBufferSize(512);
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setSocketTimeout(2); 
}

void loop() {
  if (client.connected()) {
    client.loop();
  }

  unsigned long ahora = millis();
  if (ahora - ultimoEnvio > intervalo) {
    ultimoEnvio = ahora;

    float tempVal = 25.0;
    float humVal = 50.0;
    float presion = 1013.0;

    if (ahtListo) {
      sensors_event_t humedad, temperatura;
      aht.getEvent(&humedad, &temperatura);
      tempVal = temperatura.temperature;
      humVal = humedad.relative_humidity;
    }
    if (bmpListo) {
      presion = bmp.readPressure() / 100.0F;
    }

    int valorSuelo = analogRead(PIN_SUELO);
    int valorLluvia = analogRead(PIN_LLUVIA);
    int valorGas = analogRead(PIN_MQ2);
    int valorLuz = analogRead(PIN_LDR);
    
    int alertaLlama = digitalRead(PIN_LLAMA);
    int alertaInclinacion = digitalRead(PIN_INCLINACION);
    int alertaBoton = digitalRead(PIN_BOTON_PANICO); 

    float indiceFFWI = obtenerValorFosberg(tempVal, humVal, valorLuz);
    String estadoRiesgo = evaluarRiesgo(indiceFFWI, tempVal, valorLluvia, valorSuelo, valorGas, alertaLlama, alertaInclinacion, alertaBoton);

    if (estadoRiesgo == "Extremo") {
      digitalWrite(PIN_LED, HIGH);
      digitalWrite(PIN_BUZZER, HIGH);
    } else {
      digitalWrite(PIN_LED, LOW);
      digitalWrite(PIN_BUZZER, LOW);
    }

    Serial.println("--------------------------------");
    Serial.printf("Temp: %.2f C | Hum: %.2f %% | Pres: %.2f hPa\n", tempVal, humVal, presion);
    Serial.printf("Analog -> Suelo: %d | Lluvia: %d | Gas: %d | Luz: %d\n", valorSuelo, valorLluvia, valorGas, valorLuz);
    Serial.printf("Digital-> Llama: %d | Inclinación: %d | Botón: %d\n", alertaLlama, alertaInclinacion, alertaBoton);
    Serial.printf("Fosberg FFWI: %.2f | Riesgo: %s\n", indiceFFWI, estadoRiesgo.c_str());

    if (WiFi.status() == WL_CONNECTED) {
      if (!client.connected()) {
        reconectarMQTT();
      }
      if (client.connected()) {
        enviar_telemetria_aws(tempVal, humVal, presion, valorSuelo, valorLluvia, valorGas, valorLuz, indiceFFWI, estadoRiesgo, alertaLlama, alertaInclinacion, alertaBoton);
      }
    }
  }
}