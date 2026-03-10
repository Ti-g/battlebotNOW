#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

Servo myservo;

typedef struct struct_message {
  s8_t SendRightX;
  s8_t SendRightY;
} struct_message;

struct_message myData;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));

  myservo.write(map(myData.SendRightX, -127, 127, 0, 360));
  Serial.printf(">angle: %d\n", myData.SendRightX);
}

void setup() {

  // Initialize Serial Monitor
  Serial.begin(115200);

  myservo.attach(33);
  myservo.write(90);
  Serial.print("begin");
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully initialized, we will register our callback function to
  // get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}
 
void loop() {
  Serial.print("waiting on data");
}