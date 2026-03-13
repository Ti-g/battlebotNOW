#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_err.h>
#include <SPI.h>
#include <Adafruit_MCP3008.h>
#include "main.h"

#define DEBUG

const char *TAG = "Transmitter";

const uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static QueueHandle_t statusLEDQueue = NULL;
static TaskHandle_t xHandle = NULL;

bool sent;

Adafruit_MCP3008 adc;

struct_message myData;

static void vStatusLED(void *pvParameters) {
  statusLEDEvent evt;

  while (xQueueReceive(statusLEDQueue, &evt, portMAX_DELAY) == pdTRUE)
    {
      setStatusLED(evt.color);
    }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  statusLEDEvent evt;
#ifdef DEBUG
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success"
  //                                               : "Delivery Fail");
  #endif
  if(status == ESP_NOW_SEND_SUCCESS) {
    memcpy(evt.color, connectedNoIssue, sizeof(u_int8_t) * 3);
  } else {
    memcpy(evt.color, lostConection, sizeof(u_int8_t) * 3);
  }
  xQueueSend(statusLEDQueue, &evt, 512);
  sent = true;
}

void setup() {
  delay(1000);
  esp_now_peer_info_t peerInfo = {};
  sent = true;
  pinMode(statusLEDRedPin, OUTPUT);
  pinMode(statusLEDGreenPin, OUTPUT);
  pinMode(statusLEDBluePin, OUTPUT);

  setStatusLED(onNotYetConnected);

  Serial.begin(115200);

  adc.begin(MCP3008_CLK, MCP3008_MOSI, MCP3008_MISO, MCP3008_CS);

  WiFi.mode(WIFI_STA);
  ESP_ERROR_CHECK(esp_now_init());

  ESP_ERROR_CHECK(esp_now_register_send_cb(OnDataSent));

  // Register peer
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  Serial.println(esp_err_to_name(result));
  BaseType_t xReturned;

  statusLEDQueue = xQueueCreate(STATUSLED_QUEUE_SIZE, sizeof(statusLEDEvent));
  if(statusLEDQueue == NULL) {
    ESP_LOGE(TAG, "Create Status LED queue fail");
  }

  xReturned = xTaskCreate(vStatusLED,
                          "STATUSLED",
                          2000,
                          (void *)1,
                          tskIDLE_PRIORITY,
                          &xHandle);

  delay(1000);
}

void loop() {
  static u16_t rawRightX; //actualy a 10bit int ranging from 0-1023
  static u16_t rawRightY;
  static s8_t scaledRightX;
  static s8_t scaledRightY;
  static u_long prev_sent;
  static esp_err_t result;

  const int deadZoneX = 2;
  const int deadZoneY = 2;

  rawRightX = adc.readADC(RightXADCPin);
  rawRightY = adc.readADC(RightYADCPin);
  scaledRightX = constrain(map(rawRightX, 235, 799, -127, 127), -127, 127);
  scaledRightY = constrain(map(rawRightY, 204, 3111, -127, 127), -127, 127);

  if(scaledRightX >= -deadZoneX && scaledRightX <= deadZoneX) {
    scaledRightX = 0;
  }

  if(scaledRightY >= -deadZoneY && scaledRightY <= deadZoneY) {
    scaledRightY = 0;
  }

  // #ifdef DEBUG
  // Serial.print(">TX: ");
  // Serial.println(scaledRightX);
  // Serial.print("right y: ");
  // Serial.println(scaledRightY);
  // Serial.print("raw right x: ");
  // Serial.println(rawRightX);
  // Serial.print("raw right y: ");
  // Serial.println(rawRightY);
  // #endif

  myData.SendRightX = scaledRightX;
  myData.SendRightY = scaledRightY;
  if (sent == true) {
#ifdef DEBUG
    // Serial.println("wait on send");
#endif
    result =
        esp_now_send(receiverAddress, (uint8_t *)&myData, sizeof(myData));
    Serial.printf("TTS: %ld\n", millis() - prev_sent);
    prev_sent = millis();
    sent = false;
  }
#ifdef DEBUG
  // Serial.println(esp_err_to_name(result));
#endif
}
