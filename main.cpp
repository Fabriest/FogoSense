#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "secrets.h"

// Instancias de WiFiClientSecure y PubSubClient para la comunicación MQTT
WiFiClientSecure espClient;
PubSubClient client(espClient);

unsigned long ultimoEnvio = 0;
const long intervalo = 5000; 

// Función para conectar a la red WiFi local
void conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado exitosamente.");
  Serial.print("IP asignada: ");
  Serial.println(WiFi.localIP());
}

// Callback para procesar mensajes entrantes 
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido en [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String mensaje = "";
  for (int i = 0; i < length; i++) {
    mensaje += (char)payload[i];
  }
  Serial.println(mensaje);
}

// Reconexión automática a HiveMQ Cloud
void reconectarMQTT() {
  while (!client.connected()) {
    Serial.print("Intentando conectar a HiveMQ Cloud...");
    
    // Identificador único para el cliente
    String clientId = "ESP32Client-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)) {
      Serial.println(" ¡Conectado!");
      
      // Suscripción de prueba a los tópicos de la red
      client.subscribe("uvg/nodo1/#");
      Serial.println("Suscrito al árbol uvg/nodo1/#");
    } else {
      Serial.print(" Falló conexión. Código rc=");
      Serial.print(client.state());
      Serial.println(". Reintentando en 5 segundos...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  conectarWiFi();

  // Configuración de seguridad para HiveMQ Cloud (TLS/SSL)
  // setInsecure permite omitir la validación del certificado CA en prototipos rápidos
  espClient.setInsecure();

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  // Mantener la conexión activa
  if (!client.connected()) {
    reconectarMQTT();
  }
  client.loop();

  // Publicación periódica de prueba (Simulando el Nodo 1)
  unsigned long ahora = millis();
  if (ahora - ultimoEnvio > intervalo) {
    ultimoEnvio = ahora;

    float tempSimulada = 24.5 + (random(-20, 20) / 10.0);
    String payload = String(tempSimulada, 2);

    Serial.print("Publicando en uvg/nodo1/temp: ");
    Serial.println(payload);
    
    client.publish("uvg/nodo1/temp", payload.c_str());
  }
}