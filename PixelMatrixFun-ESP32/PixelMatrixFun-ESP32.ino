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
int lastClearSeq = -1;

#define SERVICE_UUID                    "5e92c674-3f45-420c-bb53-af2bcaff68b2"
#define CHARACTERISTIC_UUID_RGB_BITMAP  "2de8d042-9a37-45f0-8233-5160faea472d"
#define CHARACTERISTIC_UUID_HELO        "3ca58a89-0754-44d3-83e5-371aac29ca36"
#define CHARACTERISTIC_UUID_PIXEL_WRITE "5038a8b8-5e8e-4f1f-97dd-8cf76c8e90e1"
#define CHARACTERISTIC_UUID_CLEAR       "2ae4ad40-f1e2-4f4d-893c-f0109c024381"

// How many leds in your strip?
#define NUM_LEDS 256

//Really dark colors are shown much lighter so we better put them black.
#define MIN_BRIGHTNESS 30

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
      // Serial.print("[");Serial.print(x);Serial.print(",");Serial.print(y);Serial.print("]=[");Serial.print(r);Serial.print(",");Serial.print(g);Serial.print(",");Serial.print(b);Serial.println("]");
      unsigned long pixPos;
      if (x % 2 == 0) {
        pixPos = x*16+y;
      } else {
        pixPos = x*16+(15-y);
      }

      // Make dark color darker by squaring them as decimal part.
      r = r*r/255;
      g = g*g/255;
      b = b*b/255;
      
      if ((r+g+b)/3 < MIN_BRIGHTNESS) {
        leds[pixPos] = CRGB(0, 0, 0); //Really dark colors are shown much lighter so we better put them black.
      } else {
        leds[pixPos] = CRGB(r, g, b);
      }
    }
  }
  Serial.println("drawing!!!");
  FastLED.show();
}


class BitmapWriteCallback: public BLECharacteristicCallbacks {
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

class PixelWriteCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i += 5) {
          unsigned char x = rxValue[i];
          unsigned char y = rxValue[i+1];
          unsigned char r = rxValue[i+2];
          unsigned char g = rxValue[i+3];
          unsigned char b = rxValue[i+4];
          Serial.print("[");Serial.print(x);Serial.print(",");Serial.print(y);Serial.print("]=[");Serial.print(r);Serial.print(",");Serial.print(g);Serial.print(",");Serial.print(b);Serial.print("], totalBytes=");Serial.println(rxValue.length());
          EEPROM.write((x*16+y)*3+0, r);
          EEPROM.write((x*16+y)*3+1, g);
          EEPROM.write((x*16+y)*3+2, b);
        }
        EEPROM.commit();
        drawFromFlash();
      }
    }
};

class ClearCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      // if (!pCharacteristic->getUUID().equals(BLEUUID::fromString(CHARACTERISTIC_UUID_CLEAR))) {
      //  Serial.println("UUID Mistake!");
      //}
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() >= 4) {
        unsigned char r = rxValue[0];
        unsigned char g = rxValue[1];
        unsigned char b = rxValue[2];
        unsigned char s = rxValue[3];
        if (s == lastClearSeq) {
          Serial.println("Repeated clear received");
          return;
        } else {
          lastClearSeq = s;
        }
        Serial.print("[fill]=[");Serial.print(r);Serial.print(",");Serial.print(g);Serial.print(",");Serial.print(b);Serial.print("], totalBytes=");Serial.println(rxValue.length());
        for (int x = 0; x < 16; x++) {
          for (int y = 0; y < 16; y++) {
            EEPROM.write((x*16+y)*3+0, r);
            EEPROM.write((x*16+y)*3+1, g);
            EEPROM.write((x*16+y)*3+2, b);
          }
        }
        EEPROM.commit();
        drawFromFlash();
      }
    }
};

void setup() { 
  Serial.begin(115200);

  if (!EEPROM.begin(EEPROM_SIZE)) {
    Serial.println("failed to initialise EEPROM"); delay(1000000);
  }

	LEDS.addLeds<WS2812B,DATA_PIN,GRB>(leds,NUM_LEDS);
  LEDS.setBrightness(10);
  FastLED.setCorrection(CRGB(240,200,255));

  drawFromFlash();

   // Create the BLE Device
  BLEDevice::init("PixelMatrixFun");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  startService();

  // Start advertising
  pServer->getAdvertising()->start();
  
  Serial.println("Waiting a client connection to notify...");

}

void startService() {
  Serial.println("startService()");
    // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                   CHARACTERISTIC_UUID_HELO,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pBitmapCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RGB_BITMAP,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pBitmapCharacteristic->setCallbacks(new BitmapWriteCallback());

  BLECharacteristic * pPixelCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_PIXEL_WRITE,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pPixelCharacteristic->setCallbacks(new PixelWriteCallback());

  BLECharacteristic * pClearCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_CLEAR,
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pClearCharacteristic->setCallbacks(new ClearCallback());

  // Start the service
  pService->start();
}

void stopService() {
  Serial.println("stopService()");
  BLEService *pService = pServer->getServiceByUUID(SERVICE_UUID);
  if (pService != 0) {
    pService->stop();
  }
}

unsigned int count = 0;

unsigned long nextHelo = 0;
const unsigned long heloEvery = 2 * 1000;

void handleBLE() {
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        stopService();
        delay(500); // give the bluetooth stack the chance to get things ready
        startService();
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    }
    oldDeviceConnected = deviceConnected;
}

void loop() {
  count++;
  if (deviceConnected && (count % 100) == 0) {
    long t0 = millis();
    if (t0 > nextHelo) {
      pTxCharacteristic->setValue(txValue, 6);
      pTxCharacteristic->notify();
      nextHelo = millis() + heloEvery;
    }
  }
  handleBLE();
}
