#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "config.h"

#define BUTTON_PIN D3
#define BELL_PIN D2
#define STATUS_LED LED_BUILTIN
#define STATUS_NOT_MUTED HIGH
#define STATUS_MUTED LOW

#define DOORBELL_DELAY_BETWEEN_RINGS 30000
#define DOOROPEN_DELAY_BETWEEN_RINGS 30000

WiFiClient espClient;
PubSubClient mqttClient(espClient);

int muted = 0;
char last_djo_state[20];
char last_bitlair_state[20];
int button_state = 0;
long last_doorbell_ring = -DOORBELL_DELAY_BETWEEN_RINGS;
long last_dooropen_ring = -DOOROPEN_DELAY_BETWEEN_RINGS;

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(BELL_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);

  digitalWrite(BELL_PIN, LOW);
  digitalWrite(STATUS_LED, STATUS_NOT_MUTED);

  Serial.begin(115200);
  setupWIFI();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);

  button_state = digitalRead(BUTTON_PIN);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  static char value[256];
  memset(value, 0, sizeof(value));
  memcpy(value, payload, min(length, sizeof(value) - 1));

  if(strcmp(djo_state_topic, topic) == 0) {
    if (strcmp(last_djo_state, value) != 0) {
      if (muted) Serial.println("DJO changed state, unmuting");
      memcpy(last_djo_state, value, sizeof(last_djo_state));
      muted = 0;
    }
  } else if (strcmp(bitlair_state_topic, topic) == 0) {
      if (muted) Serial.println("Bitlair changed state, unmuting");
      memcpy(last_bitlair_state, value, sizeof(last_bitlair_state));
      muted = 0;
  } else if ((strcmp(bell_topic, topic) == 0) && (strcmp("1", value) == 0)) {
    if ((millis() - last_doorbell_ring) > DOORBELL_DELAY_BETWEEN_RINGS) {
      last_doorbell_ring = millis();
      Serial.println("Doorbell button pressed, ringing bell");
      ring_bell(2);
    } else {
      Serial.println("Doorbell already rang recently, ignoring");
    }
  } else if ((strcmp(door_open_topic, topic) == 0) && (strcmp("1", value) == 0)) {
    if (!muted) {
      if ((millis() - last_dooropen_ring) > DOOROPEN_DELAY_BETWEEN_RINGS) {
        last_dooropen_ring = millis();
        Serial.println("Door opened, ringing bell");
        ring_bell(1);
      } else {
        Serial.println("Door already opened recently, ignoring");
      }
    } else {
      Serial.println("Door entry notification was muted");
    }
  }
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();
  
  digitalWrite(STATUS_LED, muted ? STATUS_MUTED : STATUS_NOT_MUTED);

  int new_button_state = digitalRead(BUTTON_PIN);
  if (new_button_state != button_state) {
    button_state = new_button_state;
    delay(5); // Poor mans debouncing

    if (button_state == 0) {
      muted = !muted;
    }
  }
}

void ring_bell(int times) {
  for(int i = 0; i < times; i++) {
    digitalWrite(BELL_PIN, HIGH);
    delay(300);
    digitalWrite(BELL_PIN, LOW);
    if (i < (times - 1)) delay(700);
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("Deurbel")) {
      Serial.println("connected");
      mqttClient.subscribe(djo_state_topic);
      mqttClient.subscribe(bitlair_state_topic);
      mqttClient.subscribe(bell_topic);
      mqttClient.subscribe(door_open_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setupWIFI() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  WiFi.printDiag(Serial);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
