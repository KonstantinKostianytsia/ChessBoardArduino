#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <FastLED.h>

// Common constants
#define NUM_CELLS_IN_ROW       8 
#define NUM_ROWS               8
#define SEND_BOARD_STATE_DELAY 500
#define DELAY_BETWEEN_HALL_SENSORS_READING 20

// Parser constants
#define CHARACTER_TO_SEPARATE_COMMANDS  ';'
#define CHARACTER_TO_SEPARATE_FIELDS    '_'

// BLE constants
#define SERVICE_UUID                        "5f47f8ff-fbb4-47d4-ac92-8520ef9fed17"
#define WRITE_COLOR_CHAR                    "17e8a436-de30-47dd-bfb8-baf4d82afbdb"
#define BOARD_STATE_NOTIFICATIONS_CHAR      "7d09ef8c-b14f-4bbb-9d48-4b1c1f7f8044"

// LEDs constants
int BOARD_ROW_LED_PINS[NUM_ROWS] = {16, 17, 5, 18, 19, 21, 22, 13};
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
#define BRIGHTNESS 250

/// Hall sensors constats
#define ANALOG_READ_RESOLUTION      6
#define s0Pin                       25
#define s1Pin                       26
#define s2Pin                       27
#define s3Pin                       14

#define s0AnalogPin                   33 /// read only
#define s1AnalogPin                   35 /// read only
#define s2AnalogPin                   32 /// read only
#define s3AnalogPin                   34 /// read only


// FastLEDs variables
CRGB leds[NUM_ROWS][NUM_CELLS_IN_ROW];

// BLE variables
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristicWriteColor = NULL;
BLECharacteristic* pCharacteristicBoardState = NULL;
BLEDescriptor *pDescr;
BLE2902 *pBLE2902;

// other variables
bool deviceConnected = false;
bool oldDeviceConnected = false;


class BLEServerConnectCallback: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      digitalWrite(LED_BUILTIN, HIGH);
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      FastLED.clear();
      digitalWrite(LED_BUILTIN, LOW);
      deviceConnected = false;
    }
};

class BLEOnWriteColorCallback: public BLECharacteristicCallbacks {
  void processCommand(String command) {
    Serial.println(command);
    String color;
    int rowIndex = -1, columnIndex = -1;
    int currentStartIndex = 0;
    for(int i = 0; i < command.length(); ++i){
      if(command[i] == CHARACTER_TO_SEPARATE_FIELDS) {
        if(color.length() == 0) {
          color = command.substring(currentStartIndex, i);
        } else if(rowIndex == -1) {
          rowIndex = command.substring(currentStartIndex, i).toInt();
        } else if(columnIndex == -1) {
          columnIndex = command.substring(currentStartIndex, i).toInt();
        }
        currentStartIndex = i + 1;
        continue;
      }
    }
    columnIndex = command.substring(currentStartIndex, command.length()).toInt();
    Serial.println(color);
    Serial.println(rowIndex);
    Serial.println(columnIndex);
    color = color.substring(1);
    String prefix = String("0x");
    uint8_t red = strtoul((prefix + color.substring(0,2)).c_str(), NULL, 16);
    leds[rowIndex][columnIndex].red = red;

    uint8_t green = strtoul((prefix + color.substring(2,4)).c_str(), NULL, 16);
    leds[rowIndex][columnIndex].green = green;

    uint8_t blue = strtoul((prefix + color.substring(4,6)).c_str(), NULL, 16);
    leds[rowIndex][columnIndex].blue = blue;
    FastLED.show();
    Serial.println("COLOR SUCCESSFULLY CHANGED");
    Serial.println(red);
    Serial.println(green);
    Serial.println(blue);
  }

  std::vector<String> processWriteString(String receivedString) {
    Serial.println("Received string: " + receivedString);
    std::vector<String> commands = std::vector<String>();
    int currentCommandStartIndex = 0;
    for(int i = 0; i < receivedString.length(); i++) {
      if(receivedString[i] == CHARACTER_TO_SEPARATE_COMMANDS) {
        commands.push_back(receivedString.substring(currentCommandStartIndex, i));
        currentCommandStartIndex = i + 1;
        continue;
      } 
    }
    commands.push_back(receivedString.substring(currentCommandStartIndex, receivedString.length()));
    return commands;
  }

  void onWrite(BLECharacteristic *pChar) override { 
    std::string pChar_value_stdstr = pChar->getValue();
    String pChar_value_string = String(pChar_value_stdstr.c_str());
    std::vector<String> commands = processWriteString(pChar_value_string);
    for(auto it = begin(commands); it != end(commands); ++it) {
      processCommand(*it);
    }
  }
};

void setupLeds() {
  pinMode(LED_BUILTIN, OUTPUT);
  // 16, 17, 5, 18, 19, 21, 22, 13
  FastLED.addLeds<LED_TYPE, 16, COLOR_ORDER>(leds[0], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 17, COLOR_ORDER>(leds[1], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 5,  COLOR_ORDER>(leds[2], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 18, COLOR_ORDER>(leds[3], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 19, COLOR_ORDER>(leds[4], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 21, COLOR_ORDER>(leds[5], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 22, COLOR_ORDER>(leds[6], NUM_CELLS_IN_ROW);
  FastLED.addLeds<LED_TYPE, 13, COLOR_ORDER>(leds[7], NUM_CELLS_IN_ROW);
  FastLED.setBrightness(BRIGHTNESS);
}

void setupBLEService() {
    // Create the BLE Device
  BLEDevice::init("ESP32_CHESS");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BLEServerConnectCallback());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristicWriteColor = pService->createCharacteristic(
                      WRITE_COLOR_CHAR,
                      BLECharacteristic::PROPERTY_WRITE  
                    );  
  pCharacteristicBoardState = pService->createCharacteristic(
                      BOARD_STATE_NOTIFICATIONS_CHAR,
                      BLECharacteristic::PROPERTY_NOTIFY  
                    );  
                  
  pDescr = new BLEDescriptor((uint16_t)0x2901);
  pCharacteristicBoardState->addDescriptor(pDescr);

  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);

  pCharacteristicBoardState->addDescriptor(pBLE2902);

  // After defining the desriptors, set the callback functions
  pCharacteristicWriteColor->setCallbacks(new BLEOnWriteColorCallback());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x00);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}

void setupHallSensors() {
  /// Initialize pins to control multiplexor
  pinMode(s0Pin, OUTPUT);
  pinMode(s1Pin, OUTPUT);
  pinMode(s2Pin, OUTPUT);
  pinMode(s3Pin, OUTPUT);

  pinMode(s0AnalogPin, INPUT);
  pinMode(s1AnalogPin, INPUT);
  pinMode(s2AnalogPin, INPUT);
  pinMode(s3AnalogPin, INPUT);

  /// Set initial values
  digitalWrite(s0Pin, LOW);
  digitalWrite(s1Pin, LOW);
  digitalWrite(s2Pin, LOW);
  digitalWrite(s3Pin, LOW);

  analogReadResolution(ANALOG_READ_RESOLUTION);
}

void setup() {
  Serial.begin(115200);

  setupLeds();
  setupHallSensors();
  setupBLEService();
}

void handleHallSensors() {
  String commandString[4] = {"","","",""};
  /// To get sensors values are used 4 multiplexor
  /// 4 multiplexor are connected dirrectly to hall sensors 
  /// Different analog read pins are used for each multiplexor
  for (int i = 0; i < 16; i++) {
    if(i != 0) {
      for(int j = 0; j < 4; j++) {
        commandString[j].concat(CHARACTER_TO_SEPARATE_COMMANDS);
      }
    }
    // Set values to controll input of the multiplexor
    digitalWrite(s0Pin, i & 0x01);
    digitalWrite(s1Pin, (i >> 1) & 0x01);
    digitalWrite(s2Pin, (i >> 2) & 0x01);
    digitalWrite(s3Pin, (i >> 3) & 0x01);
    // A delay for signal stabilization
    delay(DELAY_BETWEEN_HALL_SENSORS_READING);
    // Reading signal value
    int sensorValues[4] = {analogRead(s0AnalogPin), analogRead(s1AnalogPin),analogRead(s2AnalogPin), analogRead(s3AnalogPin)};
    byte rowIndex = i / NUM_CELLS_IN_ROW;
    byte columnIndex = i % NUM_CELLS_IN_ROW;
    
    for(int j = 0; j < 4; j++) {
      commandString[j].concat(j * 2 + rowIndex);
      commandString[j].concat(CHARACTER_TO_SEPARATE_FIELDS);
      commandString[j].concat(columnIndex);
      commandString[j].concat(CHARACTER_TO_SEPARATE_FIELDS);
      commandString[j].concat(sensorValues[j]);
    }
  }
  String result = commandString[0] + CHARACTER_TO_SEPARATE_COMMANDS + 
                  commandString[1] + CHARACTER_TO_SEPARATE_COMMANDS + 
                  commandString[2] + CHARACTER_TO_SEPARATE_COMMANDS + 
                  commandString[3];
  Serial.println(result.c_str());
  pCharacteristicBoardState->setValue(result.c_str());
  pCharacteristicBoardState->notify();
}

void testHallSensors() {
  String commandString = "";
  /// To get sensors values are used 4 multiplexor
  /// 4 multiplexor are connected dirrectly to hall sensors 
  /// Different analog read pins are used for each multiplexor
  for (int i = 0; i < 16; i++) {
    if(i != 0) {
      commandString.concat(CHARACTER_TO_SEPARATE_COMMANDS);
    }
    // Set values to controll input of the multiplexor
    digitalWrite(s0Pin, i & 0x01);
    digitalWrite(s1Pin, (i >> 1) & 0x01);
    digitalWrite(s2Pin, (i >> 2) & 0x01);
    digitalWrite(s3Pin, (i >> 3) & 0x01);
    // A delay for signal stabilization
    delay(10);
    // Reading signal value
    int sensorValue = analogRead(s1AnalogPin);

    int value = sensorValue;

    byte rowIndex = i / NUM_CELLS_IN_ROW;
    byte columnIndex = i % NUM_CELLS_IN_ROW;
    
    byte numOfMultiplexor = 0;
      commandString.concat(numOfMultiplexor * 2 + rowIndex );
      commandString.concat(CHARACTER_TO_SEPARATE_FIELDS);
      commandString.concat(columnIndex);
      commandString.concat(CHARACTER_TO_SEPARATE_FIELDS);
      commandString.concat(value);
  }
  String result = commandString + CHARACTER_TO_SEPARATE_COMMANDS;
  Serial.println(result.c_str());
  // pCharacteristicBoardState->setValue(result.c_str());
  // pCharacteristicBoardState->notify();
}

void loop() {
    // notify changed value
    if (deviceConnected) {
        handleHallSensors();
        // testHallSensors();
        // delay(SEND_BOARD_STATE_DELAY);
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
    delay(500);
}
