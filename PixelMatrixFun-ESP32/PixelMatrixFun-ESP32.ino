#include <FastLED.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <EEPROM.h>

#define EEPROM_SIZE 4096

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue[7] = "HELO\r\n";

#define SERVICE_UUID           "6ff4913c-ea8a-4e5b-afdc-9f0f0e488ab1"
#define CHARACTERISTIC_UUID_RX "6ff4913c-ea8a-4e5b-afdc-9f0f0e488ab2"
#define CHARACTERISTIC_UUID_TX "6ff4913c-ea8a-4e5b-afdc-9f0f0e488ab3"

// How many leds in your strip?
#define NUM_LEDS 256

// For led chips like Neopixels, which have a data line, ground, and power, you just
// need to define DATA_PIN.  For led chipsets that are SPI based (four wires - data, clock,
// ground, and power), like the LPD8806, define both DATA_PIN and CLOCK_PIN
#define DATA_PIN 13
#define CLOCK_PIN 14

// Define the array of leds
CRGB leds[NUM_LEDS];

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Connected");
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      Serial.println("Disconnected");
      deviceConnected = false;
    }
};

unsigned long lastReceiveMillis = 0;
unsigned long receiveOffset = 0;
const unsigned long resetReceiveMillis = 5000;


void drawFromFlash() {
  Serial.println("drawing...");
  for (int x = 0; x < 16; x++) {
    for (int y = 0; y < 16; y++) {
      unsigned char r=EEPROM.read((x*16+y)*3);
      unsigned char g=EEPROM.read((x*16+y)*3+1);
      unsigned char b=EEPROM.read((x*16+y)*3+2);
      Serial.print("[");Serial.print(x);Serial.print(",");Serial.print(y);Serial.print("]=[");Serial.print(r);Serial.print(",");Serial.print(g);Serial.print(",");Serial.print(b);Serial.println("]");
      unsigned long pixPos;
      if (x % 2 == 0) {
        pixPos = x*16+y;
      } else {
        pixPos = x*16+(15-y);
      }
      leds[pixPos].g = r;
      leds[pixPos].r = g;
      leds[pixPos].b = b;
    }
  }
  Serial.println("drawing!!!");
  FastLED.show();
}


class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        if (millis() - lastReceiveMillis > resetReceiveMillis) {
          receiveOffset = 0;
        }
        lastReceiveMillis = millis();
        
        for (int i = 0; i < rxValue.length(); i++) {
          unsigned char v = rxValue[i];
          EEPROM.write(receiveOffset+i, v);
          Serial.print("[");Serial.print(receiveOffset+i);Serial.print("]=");Serial.println(v);
        }
        EEPROM.commit();
        receiveOffset += rxValue.length();
        Serial.println(".");
      }
      Serial.print("receiveOffset=");Serial.println(receiveOffset);
      if (receiveOffset >= 16*16*3) {
        drawFromFlash();
        receiveOffset=0;
      }
    }
};

void setup() { 
  Serial.begin(115200);

  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }

	LEDS.addLeds<WS2812B,DATA_PIN,RGB>(leds,NUM_LEDS);
	LEDS.setBrightness(16);

  drawFromFlash();

   // Create the BLE Device
  BLEDevice::init("PixelMatrixFun");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                   CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  
  Serial.println("Waiting a client connection to notify...");

}

void fadeall() { for(int i = 0; i < NUM_LEDS; i++) { leds[i].nscale8(250); } }

unsigned int count = 0;

void handleBLE() {
    if (deviceConnected && (count++ % 100) == 0) {
        pTxCharacteristic->setValue(txValue, 6);
        pTxCharacteristic->notify();
        delay(10); // bluetooth stack will go into congestion, if too many packets are sent
  }

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}

void loop() {
  handleBLE();
}
