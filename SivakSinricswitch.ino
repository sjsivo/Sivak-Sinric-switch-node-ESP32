/*
 Version 0.1 - March 17 2018
*/ 

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebSocketsClient.h> //  https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <ArduinoJson.h> // https://github.com/kakopappa/sinric/wiki/How-to-add-dependency-libraries
#include <StreamString.h>
#include <analogWrite.h>

#include <DallasTemperature.h> // Include one wire libraries
#include <OneWire.h>
#define ONE_WIRE_BUSA 16 // Temperature 1 connected to Pin 16
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWireA(ONE_WIRE_BUSA);
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensorsA(&oneWireA);
// arrays to hold device address
DeviceAddress insideThermometer;

WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
WiFiClient client;

#define MyApiKey "sinricapinr" // TODO: Change to your sinric API Key. Your API Key is displayed on sinric.com dashboard
#define MySSID "wifi" // TODO: Change to your Wifi network SSID
#define MyWifiPassword "password" // TODO: Change to your Wifi network password

#define HEARTBEAT_INTERVAL 300000 // 5 Minutes 
#define LAMP_PIN 4
#define FAN_PIN 17
#define TV_PIN 18

uint64_t heartbeatTimestamp = 0;
bool isConnected = false;

#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

// an IR detector/demodulator is connected to GPIO pin 15
uint16_t RECV_PIN = 15;

IRrecv irrecv(RECV_PIN);

decode_results results;

#include <IRsend.h>

const uint16_t kIrLed = 12;  // ESP32 GPIO pin to use. Pin 12.

IRsend irsend(kIrLed);  // Set the GPIO to be used to sending the message.
//LG On TV
//Decoded NEC: 20DF10EF (32 bits)
// (68): {8960, 4346608, 492606, 492606, 1620608, 492604, 496602, 496602, 496604, 496602, 1626604, 1620608, 492604, 1620608, 1618604, 1620606, 1622604, 1618600, 500610, 490606, 494606, 1618608, 492606, 494606, 494604, 498600, 1622610, 1616606, 1620600, 500600, 1626600, 1626604, 1620608, 1618600};

//samsung
//uint16_t LGrawData[68] = {8960, 4346608, 492606, 492606, 1620608, 492604, 496602, 496602, 496604, 496602, 1626604, 1620608, 492604, 1620608, 1618604, 1620606, 1622604, 1618600, 500610, 490606, 494606, 1618608, 492606, 494606, 494604, 498600, 1622610, 1616606, 1620600, 500600, 1626600, 1626604, 1620608, 1618600};

//hyundai
uint16_t LGrawData[22] ={900, 8661794, 864898, 868906, 860902, 868906, 17501788, 870904, 1756906, 8581792, 864910};

void brightnessSet(String deviceId,String brightnesstext) {
 if (deviceId == "5e22deb90c04793a3a7fa25b") // Device ID of first device Lamp
  {  
    Serial.print("Adjustment device id: ");
    Serial.println(deviceId);
    Serial.println(brightnesstext);
    analogWrite(LAMP_PIN,brightnesstext.toInt(),100);
  } 
  else if (deviceId == "5e22defa0c04793a3a7fa25e") // Device ID of second device Fan
  { 
    Serial.print("Adjustment device id: ");
    Serial.println(deviceId);
    Serial.println(brightnesstext);
    analogWrite(FAN_PIN,brightnesstext.toInt(),100);
  }
  else {
    Serial.print("Adjustment for unknown device id: ");
    Serial.println(deviceId);    
    Serial.println(brightnesstext);
  }     
}
 
void turnOn(String deviceId) {
  //5e22e03a0c04793a3a7fa29c TV
  if (deviceId == "5e22e03a0c04793a3a7fa29c") // Device ID of first device TV
  {  
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
   // analogWrite(12,0,100);
   irsend.sendRC5(0x4C,12);
   // irsend.sendRaw(LGrawData, 22, 38);  // Send a raw data capture at 38kHz.
    //---------------------------------------------------------------------------------------------------------------------------
  } 
  else
  if (deviceId == "5e22deb90c04793a3a7fa25b") // Device ID of first device Lampe
  {  
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
    analogWrite(LAMP_PIN,100,100);
  } 
  else if (deviceId == "5e22defa0c04793a3a7fa25e") // Device ID of second device Fan
  { 
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
    analogWrite(FAN_PIN,100,100);
  }
  else {
    Serial.print("Turn on for unknown device id: ");
    Serial.println(deviceId);    
  }     
}

void turnOff(String deviceId) {

  if (deviceId == "5e22e03a0c04793a3a7fa29c") // Device ID of first device TV
  {  
    Serial.print("Turn off device id: ");
    Serial.println(deviceId);
    //analogWrite(12,0,0);
   //irsend.sendRaw(LGrawData, 22, 38);  // Send a raw data capture at 38kHz.
    irsend.sendRC5(0x4C,12);
    //---------------------------------------------------------------------------------------------------------------------------
  } 
  else
   if (deviceId == "5e22deb90c04793a3a7fa25b") // Device ID of first device Lampe
   {  
     Serial.print("Turn off Device ID: ");
     Serial.println(deviceId);
     analogWrite(LAMP_PIN,0,100);
   }
   else if (deviceId == "5e22defa0c04793a3a7fa25e") // Device ID of second device Fan
   { 
     Serial.print("Turn off Device ID: ");
     Serial.println(deviceId);
     analogWrite(FAN_PIN,0,100);
  }
  else {
     Serial.print("Turn off for unknown device id: ");
     Serial.println(deviceId);    
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      isConnected = false;    
      Serial.printf("[WSc] Webservice disconnected from sinric.com!\n");
      break;
    case WStype_CONNECTED: {
      isConnected = true;
      Serial.printf("[WSc] Service connected to sinric.com at url: %s\n", payload);
      Serial.printf("Waiting for commands from sinric.com ...\n");        
      }
      break;
    case WStype_TEXT: {
        Serial.printf("[WSc] get text: %s\n", payload);
        // Example payloads

        // For Switch  types
        // {"deviceId":"xxx","action":"action.devices.commands.OnOff","value":{"on":true}} // https://developers.google.com/actions/smarthome/traits/onoff
        // {"deviceId":"xxx","action":"action.devices.commands.OnOff","value":{"on":false}}

#if ARDUINOJSON_VERSION_MAJOR == 5
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject((char*)payload);
#endif
#if ARDUINOJSON_VERSION_MAJOR == 6        
        DynamicJsonDocument json(1024);
        deserializeJson(json, (char*) payload);      
#endif        
        String deviceId = json ["deviceId"];     
        String action = json ["action"];
        if(action == "action.devices.commands.BrightnessAbsolute") { // Brightness 
            String value = json ["value"]["brightness"];
            Serial.println(value); 
                brightnessSet(deviceId,value);
        }
        else
        if(action == "action.devices.commands.OnOff") { // Switch 
            String value = json ["value"]["on"];
            Serial.println(value); 
            
            if(value == "true") {
                turnOn(deviceId);
            } else {
                turnOff(deviceId);
            }
        }
        else if (action == "test") {
            Serial.println("[WSc] received test command from sinric.com");
        }
      }
      break;
    case WStype_BIN:
      Serial.printf("[WSc] get binary length: %u\n", length);
      break;
    default: break;
  }
}

//eg: setTargetTemperatureOnServer("deviceid", "25.0", "CELSIUS")
void setTargetTemperatureOnServer(String deviceId, String value, String scale) {
  DynamicJsonDocument jsonBuffer(1024);
  //DynamicJsonBuffer jsonBuffer;
  JsonObject root; // = jsonBuffer.createObject()
  root["action"] = "temperatureAmbientCelsius";//"SetTargetTemperature";
  root["deviceId"] = deviceId;
  
  JsonObject valueObj = root.createNestedObject("value");
  JsonObject targetSetpoint = valueObj.createNestedObject("targetSetpoint");//("targetSetpoint");
  targetSetpoint["value"] = value;
  targetSetpoint["scale"] = scale;

   String databuf;
  serializeJson(root, databuf);
  //StreamString databuf;
  //root.printTo(databuf);
  
  webSocket.sendTXT(databuf);
}

void setup() {
  Serial.begin(115200);
  pinMode(LAMP_PIN,OUTPUT);
  pinMode(FAN_PIN,OUTPUT);
  pinMode(TV_PIN,OUTPUT);
  pinMode(12,OUTPUT);
  analogWriteFrequency(120); // set frequency to 10 KHz for all pins
  WiFiMulti.addAP(MySSID, MyWifiPassword);
  Serial.println();
  Serial.print("Connecting to Wifi: ");
  Serial.println(MySSID);  

  // Waiting for Wifi connect
  while(WiFiMulti.run() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if(WiFiMulti.run() == WL_CONNECTED) {
    Serial.println("");
    Serial.print("WiFi connected. ");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }

Serial.print("Locating devices...");
  sensorsA.begin();
  Serial.print("Found ");
  Serial.print(sensorsA.getDeviceCount(), DEC);
  Serial.println(" devices.");

  // report parasite power requirements
  Serial.print("Parasite power is: "); 
  if (sensorsA.isParasitePowerMode()) Serial.println("ON");
  else Serial.println("OFF");
  
  
  if (!sensorsA.getAddress(insideThermometer, 0)) Serial.println("Unable to find address for Device 0"); 
  
  
  Serial.print("Device 0 Address: ");
    for (uint8_t i = 0; i < 8; i++)
  {
    if (insideThermometer[i] < 16) Serial.print("0");
    Serial.print(insideThermometer[i], HEX);
  }
 // printAddress();
  Serial.println();

  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensorsA.setResolution(insideThermometer, 9);
 
  Serial.print("Device 0 Resolution: ");
  Serial.print(sensorsA.getResolution(insideThermometer), DEC); 
  Serial.println();

sensorsA.requestTemperatures();
      float temperature = (float) (sensorsA.getTempCByIndex(0) ); //
      Serial.println(temperature);
     
      
irrecv.enableIRIn();  // Start the receiver
irsend.begin();
      
  // server address, port and URL
  webSocket.begin("iot.sinric.com", 80, "/"); //"iot.sinric.com", 80

  // event handler
  webSocket.onEvent(webSocketEvent);
  webSocket.setAuthorization("apikey", MyApiKey);
  
  // try again every 5000ms if connection has failed
  webSocket.setReconnectInterval(5000);   // If you see 'class WebSocketsClient' has no member named 'setReconnectInterval' error update arduinoWebSockets

   setTargetTemperatureOnServer("5e22e0bc0c04793a3a7fa2c2", String(temperature,2),  "CELSIUS");
   //Serial.println(String(temperature,2));
}

void dump(decode_results *results) {
  // Dumps out the decode_results structure.
  // Call this after IRrecv::decode()
  uint16_t count = results->rawlen;
  if (results->decode_type == UNKNOWN) {
    Serial.print("Unknown encoding: ");
  } else if (results->decode_type == NEC) {
    Serial.print("Decoded NEC: ");
  } else if (results->decode_type == SONY) {
    Serial.print("Decoded SONY: ");
  } else if (results->decode_type == RC5) {
    Serial.print("Decoded RC5: ");
  } else if (results->decode_type == RC5X) {
    Serial.print("Decoded RC5X: ");
  } else if (results->decode_type == RC6) {
    Serial.print("Decoded RC6: ");
  } else if (results->decode_type == RCMM) {
    Serial.print("Decoded RCMM: ");
  } else if (results->decode_type == PANASONIC) {
    Serial.print("Decoded PANASONIC - Address: ");
    Serial.print(results->address, HEX);
    Serial.print(" Value: ");
  } else if (results->decode_type == LG) {
    Serial.print("Decoded LG: ");
  } else if (results->decode_type == JVC) {
    Serial.print("Decoded JVC: ");
  } else if (results->decode_type == AIWA_RC_T501) {
    Serial.print("Decoded AIWA RC T501: ");
  } else if (results->decode_type == WHYNTER) {
    Serial.print("Decoded Whynter: ");
  } else if (results->decode_type == NIKAI) {
    Serial.print("Decoded Nikai: ");
  }
  serialPrintUint64(results->value, 16);
  Serial.print(" (");
  Serial.print(results->bits, DEC);
  Serial.println(" bits)");
  Serial.print("Raw (");
  Serial.print(count, DEC);
  Serial.print("): {");

  for (uint16_t i = 1; i < count; i++) {
    if (i % 100 == 0)
      yield();  // Preemptive yield every 100th entry to feed the WDT.
    if (i & 1) {
      Serial.print(results->rawbuf[i] * kRawTick, DEC);
    } else {
      Serial.print(", ");
      Serial.print((uint32_t) results->rawbuf[i] * kRawTick, DEC);
    }
  }
  Serial.println("};");
}

void loop() {
  webSocket.loop();
  
  if(isConnected) {
      uint64_t now = millis();
      
      // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
      if((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL) {
          heartbeatTimestamp = now;
          webSocket.sendTXT("H");  
           // read Datawire type analog inputs - comment out if using Thermistor
        sensorsA.requestTemperatures();
      float temperature = (float) (sensorsA.getTempCByIndex(0) ); //
      Serial.println(temperature);
      setTargetTemperatureOnServer("5e22e0bc0c04793a3a7fa2c2", String(temperature,2),  "CELSIUS");    

          // irsend.sendRaw(LGrawData, 68, 38);  // Send a raw data capture at 38kHz.
      }

     
  }



      if (irrecv.decode(&results)) {
    dump(&results);
    Serial.println("DEPRECATED: Please use IRrecvDumpV2.ino instead!");
    irrecv.resume();  // Receive the next value
  }
  
}
 
