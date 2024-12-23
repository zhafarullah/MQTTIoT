#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

const char* ssid = "punyaaaf";
const char* password = "12345678";

const char* mqtt_server = "20.2.168.202"; // VM API
const int mqtt_port = 1883;  //BROKER PORT                 

const char* temp_topic = "esp32/temperature";  
const char* hum_topic = "esp32/humidity";     
const char* duty_cycle_topic = "esp32/fan/duty";       
const char* manual_control_topic = "esp32/fan/manual";
const char* fan_off_topic = "esp32/fan/off";     
unsigned long lastUpdate = 0;
const unsigned long updateInterval = 1000;
#define DHTPIN 33
#define DHTTYPE DHT11
#define RELAYPIN 32

#define FAN_PWM_PIN 25
#define PWM_FREQ 10000
#define PWM_RESOLUTION 8

int dutyCycle = 255;
bool manualControl = false;
DHT dht(DHTPIN, DHTTYPE);

WiFiClient espClient;
PubSubClient client(espClient);


void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.print("Message received on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  Serial.println(message);

  if (String(topic) == manual_control_topic) {
    if (message == "true") {
      manualControl = true;
      Serial.println("Manual control: ON");
    } else if (message == "false") {
      manualControl = false;
      Serial.println("Manual control: OFF");
    }
    return; 
  }

  if (!manualControl) {
    Serial.println("Manual control OFF, abaikan perintah.");
    return;
  }

  if (String(topic) == fan_off_topic) {
    if (message == "true") {
      digitalWrite(RELAYPIN, HIGH);
      Serial.println("Relay dipaksa HIGH karena onoff: ON");
    } else if (message == "false") {
      digitalWrite(RELAYPIN, LOW);
      Serial.println("Relay dipaksa LOW karena onoff: OFF");
    }
  }

  if (String(topic) == duty_cycle_topic) {
    int newDutyCycle = message.toInt();
    if (newDutyCycle >= 0 && newDutyCycle <= 255) {
      dutyCycle = newDutyCycle;
      ledcWrite(FAN_PWM_PIN, dutyCycle);
      Serial.print("Duty cycle diupdate: ");
      Serial.println(dutyCycle);
    } else {
      Serial.println("Duty cycle tidak valid.");
    }
  }
}


void controlFanByHumidity(float humidity) {
  if (!manualControl) {
    if (humidity < 60) {
      digitalWrite(RELAYPIN, LOW);       
      ledcWrite(FAN_PWM_PIN, 0);    
      Serial.println("Kipas mati karena kelembapan di bawah 60%");
    } else {
      digitalWrite(RELAYPIN, HIGH);    
      ledcWrite(FAN_PWM_PIN, dutyCycle); 
      Serial.println("Kipas hidup karena kelembapan di atas 60%");
    }
  } else {
    Serial.println("Fan kontrol otomatis diabaikan karena manual control: ON");
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();

  pinMode(RELAYPIN, OUTPUT);
  ledcAttach(FAN_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);

  digitalWrite(RELAYPIN, LOW); 
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  client.setServer(mqtt_server, mqtt_port);

  client.setCallback(callback);
  connectToBroker();
}


void loop() {
  if (!client.connected()) {
    connectToBroker();
  }
  client.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdate >= updateInterval) {
    lastUpdate = currentMillis; 

    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
      return;
    }

    String temp_payload = String(temperature);
    client.publish(temp_topic, temp_payload.c_str());
    Serial.println("Temperature sent: " + temp_payload);

    String hum_payload = String(humidity);
    client.publish(hum_topic, hum_payload.c_str());
    Serial.println("Humidity sent: " + hum_payload);

    controlFanByHumidity(humidity);
  }

}

void connectToBroker() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    if (client.connect("ESP32_Client")) { 
      Serial.println("Connected");

      client.subscribe(duty_cycle_topic);
      client.subscribe(manual_control_topic);
      client.subscribe(fan_off_topic);
      Serial.println("Subscribed to topics:");
      Serial.println(duty_cycle_topic);
      Serial.println(manual_control_topic);
      Serial.println(fan_off_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(10);
    }
  }
}
