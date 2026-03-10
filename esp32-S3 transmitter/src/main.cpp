#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <esp_err.h>
#include <SPI.h>

uint8_t receiverAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
bool sent = true;

#define MCP3008_CS   10 
#define MCP3008_CLK  13
#define MCP3008_MOSI 11
#define MCP3008_MISO 12

const int LeftXADC = 0;
const int LeftYADC = 1;
const int RightXADC = 6;
const int RightYADC = 7;

// statis led color def, format [RED, GREEN, BLUE] inverted, so 0 means full bright and 255 means off
const u8_t OnNotYetConected[] = {255, 255, 0};
const u8_t connectedNoIssue[] = {255, 0, 255};
const u8_t transmitterLowBatWarn[] = {0, 0, 255};
const u8_t telemiteryWarn[] = {0, 255, 0};
const u8_t lostConection[] = {255, 255, 0};

const int statisLEDRedPin = 3;
const int statisLEDGreenPin = 2;
const int statisLEDBluePin = 1;

void setStatisLED(const u8_t color[3]) {
  analogWrite(statisLEDRedPin, color[0]);
  analogWrite(statisLEDGreenPin, color[1]);
  analogWrite(statisLEDBluePin, color[2]);
}

class MCP3008 {
private:
    int csPin;
    SPIClass* spi;
    
public:
    MCP3008(int cs) : csPin(cs) {
        spi = &SPI;  // Use default SPI instance
    }
    
    void begin() {
        pinMode(csPin, OUTPUT);
        digitalWrite(csPin, HIGH);
        
        // Initialize SPI with custom pins for ESP32-S3
        spi->begin(MCP3008_CLK, MCP3008_MISO, MCP3008_MOSI, csPin);
    }
    
    uint16_t readChannel(uint8_t channel) {
        if (channel > 7) return 0;
        
        digitalWrite(csPin, LOW);
        
        // MCP3008 command format:
        // First byte: 0b00000001 (start bit)
        // Second byte: 0b10000000 | (channel << 4) - for single-ended mode
        // Where the channel bits are in positions 4-6 of the second byte
        
        spi->transfer(0x01);
        
        // Send mode and channel (second byte)
        // Single-ended mode (bit 7 = 1) + channel (bits 6-4)
        uint8_t commandByte = 0x80 | (channel << 4);
        uint8_t data = spi->transfer(commandByte);
        
        // Read the last byte (third byte)
        uint8_t data2 = spi->transfer(0x00);
        
        digitalWrite(csPin, HIGH);
        
        // Combine the results: 
        // data contains the 2 LSBs in bits 7-6, data2 contains the 8 LSBs
        uint16_t result = ((data & 0x03) << 8) | data2;
        
        return result;
    }
};

MCP3008 adc(MCP3008_CS);

typedef struct struct_message {
  s8_t SendRightX;
  s8_t SendRightY;

} struct_message;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Serial.print("\r\nLast Packet Send Status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  if(status == ESP_NOW_SEND_SUCCESS) {
    setStatisLED(connectedNoIssue);
  } else {
    setStatisLED(lostConection);
  }
  sent = true;
}

struct_message myData;

esp_now_peer_info_t peerInfo;

void setup() {

  pinMode(statisLEDRedPin, OUTPUT);
  pinMode(statisLEDGreenPin, OUTPUT);
  pinMode(statisLEDBluePin, OUTPUT);
  
  setStatisLED(OnNotYetConected);

  Serial.begin(115200);

  adc.begin();

  WiFi.mode(WIFI_STA);

  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, receiverAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK){
  Serial.println("Failed to add peer");
  return;
  }
}

u16_t rawRightX; //actualy a 10bit int ranging from 0-1023
u16_t rawRightY;
s8_t scaledRightX;
s8_t scaledRightY;

const int deadZoneX = 2;
const int deadZoneY = 2;

void loop() {
  rawRightX = adc.readChannel(RightXADC);
  rawRightY = adc.readChannel(RightYADC);
  scaledRightX = constrain(map(rawRightX, 234, 3111, -127, 127), -127, 127);
  scaledRightY = constrain(map(rawRightY, 204, 3111, -127, 127), -127, 127);

  if(scaledRightX >= -deadZoneX && scaledRightX <= deadZoneX) {
    scaledRightX = 0;
  }

  if(scaledRightY >= -deadZoneY && scaledRightY <= deadZoneY) {
    scaledRightY = 0;
  }


  Serial.print(">TX: ");
  Serial.print(scaledRightX);
  Serial.print("right y: ");
  Serial.println(scaledRightY);
  //Serial.print("raw right x: ");
  //Serial.println(rawRightX);
  //Serial.print("raw right y: ");
  //Serial.println(rawRightY);
  
  myData.SendRightX = scaledRightX;
  myData.SendRightY = scaledRightY;
  while(sent == false) {
    //Serial.println("wait on send");
    delay(1);
  }
  esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));
  sent = false;
  //Serial.println(esp_err_to_name(result));
}


