#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Requires ArduinoMqtt library: https://www.arduino.cc/reference/en/libraries/arduinomqtt/
#include <MqttClient.h>

/** IMPORTANT
 * Please ensure that the configuration at "credentials.example.h" has been
 * copied and filled into "credentials.h".
*/
#include "credentials.h"

char *ssid = WIFI_SSID;
char *pass = WIFI_PASSWORD;

String deviceId = DEVICE_ID;
WiFiClient client;

// For serial communication
int BAUD_RATE = 9600;

// Creates a new System object since MqttClient::System is abstract;
class System: public MqttClient::System {
public:
  unsigned long millis() const {
    return millis();
  }
};

// Will be filled during setup()
MqttClient *mqtt = NULL;

char draft[128];

// Character position on draft
int charPos = 0;

// Clears message
void clearDraft(){
  int i = 0;
  for (i = 0; i < 128; i++){
    draft[i] = 0;
  }

  charPos = 0;
}

// Send message
void sendMessage(){
  // Create new message
  MqttClient::Message message;
  
  // Configure message (Quality of Service, etc.)
  message.qos = MqttClient::QOS0;
  message.retained = false;
  message.dup = false;
  message.payload = (void*) draft;
  message.payloadLen = charPos; // No need for strlen()

  // Publish and clear draft
  mqtt->publish(DEVICE_ID, message);
  clearDraft();

  Serial.println("Successfully sent.");
}

void setup(){
  Serial.begin(BAUD_RATE);

  // Config WiFi
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());

  // Setup MqttClient
  MqttClient::System *mqttSystem = System();
  MqttClient::Logger *mqttLogger = new MqttClient::LoggerImpl<HardwareSerial>(Serial);
  MqttClient::Network *mqttNetwork = new MqttClient::NetworkClientImpl<WiFiClient>(client, *mqttSystem);
  // ... to send and receive 128 bits (256 is too large according to NodeMCU)
  MqttClient::Buffer *mqttSendBuffer = new MqttClient::ArrayBuffer<128>();
  MqttClient::Buffer *mqttRecvBuffer = new MqttClient::ArrayBuffer<128>();
  MqttClient::MessageHandlers *mqttMessageHandlers = new MqttClient::MessageHandlersImpl<2>();
  // ... and set command timeout to 10 seconds (10000 ms)
  MqttClient::Options mqttOptions;
  mqttOptions.commandTimeoutMs = 10000;
  
  // Initialize the mqtt object
  mqtt = new MqttClient(
    mqttOptions, *mqttLogger, *mqttSystem, *mqttNetwork, *mqttSendBuffer,
    *mqttRecvBuffer, *mqttMessageHandlers
  );

  // Clear draft
  clearDraft();
}

void loop() {
  // Check connection status
  if (mqtt->isConnected()){
    // Detect input and paste it to draft
    while (Serial.available() > 0){
      char rx = Serial.read();
      if (rx == '\n'){
        sendMessage();
      } else {
        draft[charPos] = rx;
        charPos++;
        if (charPos == 128) sendMessage();
      }
    }
  } else {
    // Close and make a new TCP connection
    Serial.println("Reconnecting...");
    client.connect(MQTT_HOST, MQTT_PORT);

    // Start new MQTT connection
    MqttClient::ConnectResult connectResult;

    // Set MQTT packet options
    MQTTPacket_connectData options = MQTTPacket_connectData_initializer;
    options.MQTTVersion = 4;
    options.clientID.cstring = (char*)DEVICE_ID;
    options.cleansession = true;
    options.keepAliveInterval = 15; // 15 seconds

    // Check for error codes
    MqttClient::Error::type rc = mqtt->connect(options, connectResult);
    if (rc != MqttClient::Error::SUCCESS) {
      Serial.printf("Connection error: %i\n", rc);
      return;
    }
  }
}
