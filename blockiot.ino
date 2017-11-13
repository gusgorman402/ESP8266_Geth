#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//Older OneWire libraries may cause errors
// ESP8266 ESP-01 GPIO2 make sure this is correct for your ESP8266 version
#define ONE_WIRE_BUS 2

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature DS18B20(&oneWire);
int temperatureF;
int temperatureC;
int counter;
int currentTemperature;

void setup() {
  counter = 0;
  //ESP8266 ESP-01 GPIO0
  //green 5mm LED attached to GPIO0
  pinMode(0, OUTPUT);
  
  Serial.begin(115200); //Serial connection
  DS18B20.begin();
  WiFi.begin("blockchain", "ethereum");   //WiFi connection
 
  while (WiFi.status() != WL_CONNECTED) {  //Wait for the WiFI connection completion
 
    delay(500);
    Serial.println("Waiting for connection");
 
  }
    Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
 
}


void getTemperature() {
  float tempC;
  float tempF;
  do {
    DS18B20.requestTemperatures(); 
    tempC = DS18B20.getTempCByIndex(0);
    temperatureC = tempC;
    tempF = DS18B20.getTempFByIndex(0);
    temperatureF = tempF;
    delay(100);
  } while (tempC == 85.0 || tempC == (-127.0));
}


String callGeth(String inputJSON)
{
  HTTPClient http;    //Declare object of class HTTPClient
 
    http.begin("http://192.168.2.1:8545/");      //TRAILING SLASH AT END REQUIRED!!!
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
 
    int httpCode = http.POST(inputJSON);   //Send the request and get http Code
    String JSONResult = http.getString();  //Get the response from Geth JSONRPC
    
    http.end();
    return JSONResult;
}

 
void loop() {
  counter++;
  if (WiFi.status() == WL_CONNECTED) { //Check WiFi connection status

    StaticJsonBuffer<1000> JSONbuffer;   //Declaring static JSON buffer and set high value maybe 500 per call
    JsonObject& gethQueryJSON = JSONbuffer.createObject(); 
    gethQueryJSON["jsonrpc"] = "2.0";
    gethQueryJSON["method"] = "eth_call";
    JsonArray&  gethQueryParams = gethQueryJSON.createNestedArray("params");
    JsonObject& gethCallParams = JSONbuffer.createObject();
    gethCallParams["to"] = "0x513c67ef8dd393A423900aaFCc78A6878e465aE5";
    gethCallParams["data"] = "0x9de4d683";
    gethQueryParams.add(gethCallParams);
    gethQueryParams.add("latest");
    gethQueryJSON["id"] = 1;
 
    String gethStringJSON;
    gethQueryJSON.printTo(gethStringJSON);
    Serial.println("First Geth query JSON message isLightTurnedOn function: ");
    Serial.println(gethStringJSON);
     
    String gethResult = callGeth(gethStringJSON);  //Get the response from Geth JSONRPC
    JsonObject& gethJSONRPC = JSONbuffer.parseObject(gethResult);
    String lightOnString = gethJSONRPC["result"];
    lightOnString.remove(0,2);
    
    
    //parsing & converting Geth JSON-RPC hex results is not easy
    long int lightOn = strtol(lightOnString.c_str(), NULL, 16);

    Serial.println("Geth JSON-RPC response: ");
    Serial.println(gethResult);    //Print request response payload
    //Serial.println("Hex function input: ");
    //Serial.println(lightOnString);
    //Serial.println("Function result: ");
    //Serial.println(lightOn);
    
    
    //***************************** second call to geth RPC function ****************************
    gethCallParams["data"] = "0x455f1a4c";
    String tempQuery;
    gethQueryJSON.printTo(tempQuery);
    Serial.println("Second Geth query JSON message isTempCurrent function: ");
    Serial.println(tempQuery);

    String tempStatus = callGeth(tempQuery);  //Get the response from Geth JSONRPC
    JsonObject& tempRPC = JSONbuffer.parseObject(tempStatus);
    String tempString = tempRPC["result"];
    tempString.remove(0,2);
    
    long int tempUpdate = strtol(tempString.c_str(), NULL, 16);

    Serial.println("Second Geth JSON-RPC response: ");
    Serial.println(tempStatus);    //Print request response payload
    //if the JSON buffer is too small, they'll be no or bunk output
    //hence the serial debugs
    //Serial.println("Hex Function input: ");
    //Serial.println(tempString);
    //Serial.println("Function result: ");
    //Serial.println(tempUpdate);
    

    //Turns on LED if blockchain state has been changed
    if( lightOn ){
      digitalWrite(0, HIGH);   
    } else {
      digitalWrite(0, LOW);
    }


    //***********************************third call to RPC to send Temperature*******************
    //sends temperature every 10min, or if smart contract activates, or if temp changes by 1 degree
    getTemperature();

    if( !tempUpdate || counter % 40 == 0 || currentTemperature != temperatureF ){
      if( !tempUpdate ){ Serial.println("**************** User requested temperature"); }
      if( counter % 40 == 0 ){ Serial.println("*********** 10 minute Temperature Update"); }
      if( currentTemperature != temperatureF )
      {
        Serial.println("*********** Temperature changed by at least one degreeF: ");
        Serial.println(temperatureF);
        currentTemperature = temperatureF;
      }
      
      if( counter == 4000 ){ counter = 0; } //keep counter in check

      //creating another JSON buffer to avoid confusion with the first 
      StaticJsonBuffer<1000> JSONbufferTwo;  
      JsonObject& uploadJSON = JSONbufferTwo.createObject(); 
      uploadJSON["jsonrpc"] = "2.0";
      uploadJSON["method"] = "personal_sendTransaction";
      
      JsonArray&  uploadQueryParams = uploadJSON.createNestedArray("params");
      
      JsonObject& callTxParams = JSONbufferTwo.createObject();
      callTxParams["from"] = "0xbca66e58394E730b70593020c4D10819C613755f";
      callTxParams["to"] = "0x513c67ef8dd393A423900aaFCc78A6878e465aE5";
      callTxParams["gas"] = "0x30D40"; //hex value for 200000 -high gas limit for good measure          
      callTxParams["gasPrice"] = "0x6FC23AC00"; //hex value 30 Gwei gasprice 21gwei is typical

      //convert temperature to hex and pad with zeroes
      String hexTemperature = String(temperatureF, HEX);
      String paddedZeroes;
      for(int x = 0; x < (64 - hexTemperature.length()); x++)
      {
        paddedZeroes = String(paddedZeroes + "0");
      }
      hexTemperature = String(paddedZeroes + hexTemperature);
      //sha3 hash of setTempDeviceOnly function name
      hexTemperature = String("0x0e56dbea" + hexTemperature);
      callTxParams["data"] = hexTemperature;
      
      uploadQueryParams.add(callTxParams);
      uploadQueryParams.add("testpasswd");
      uploadJSON["id"] = 1;

      String uploadString;
      uploadJSON.printTo(uploadString);
      Serial.println("Temperature Geth CALL JSON message setTempDeviceOnly function: ");
      Serial.println(uploadString);

      //!!!! This RPC call is insecure plain text password over the wifi LAN !!!!!!
      //Secure firewall and secure environment for private key/geth node is expected
      //I am looking for better ways to push data off IoT devices with minimum resources and open ports
      //NodeJS unlocking the accounts via IPC on the wifiRouter node seems best for now
      //I am looking for microcontroller IoT way to remotely securely unlock sign transactions without NodeJS
      //Suggestions?
      String uploadResult = callGeth(uploadString);  //Get the response from Geth JSONRPC
      JsonObject& gethJSONRPC = JSONbuffer.parseObject(uploadResult);
      Serial.println("**** Set Temp Call result TX hash: ");
      Serial.println(uploadResult);      
    }

   
    Serial.println("Counter: ");
    Serial.println(counter);
    Serial.println("Temperature in F: ");
    Serial.println(temperatureF);
    Serial.println("Temperature in C: ");
    Serial.println(temperatureC);
        
  } else {
 
    Serial.println("Error in WiFi connection");
 
  }
 
  delay(15000);  //Send a request every 15ish seconds
 
}
