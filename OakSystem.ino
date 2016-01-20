//#define DEBUG_SETUP
SYSTEM_MODE(SEMI_AUTOMATIC)

#define OAK_SYSTEM_ROM_4F616B 82
#include "ESP8266WiFi.h"
#include "ESP8266WebServer.h"
#include <Ticker.h>

const char ok_response[] = "HTTP/1.1 200 OK \r\nContent-Type: text/html\r\nConnection: close\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\nContent-Length: ";

Ticker LEDFlip;

uint8_t LEDState = 0;
char ssid_ap[12]; //AcornFFFFFF

//esp code
ESP8266WebServer server(80);
WiFiServer telnet(5609);
WiFiClient telnetClient;

String JSONScan;

void FlipLED(){
   LEDState = !LEDState;
   digitalWrite(1, LEDState);
}

void provisionKeys(){
  digitalWrite(1,HIGH);
  if(!Particle.provisionKeys()){
    digitalWrite(1,LOW);
    LEDFlip.attach(2, FlipLED);
    while(1){
      yield();
    }
  }
  digitalWrite(1,LOW);
}

void initVariant() {
    WiFi.disconnect();
    noInterrupts();
    Oak.flashEraseSector(1020);
    Oak.flashEraseSector(1021); 
    interrupts();
    Particle.initialize(true);
}


void setup(){
  Serial.begin(115200);

  //Serial.setDebugOutput(true);
  #ifdef DEBUG_SETUP
      Serial.println("START");
  #endif

  pinMode(1,OUTPUT);
  provisionKeys();
  sprintf(ssid_ap, "ACORN-%06x", ESP.getChipId());
  LEDFlip.attach(0.5, FlipLED);

  if(!Particle.connect() || Oak.currentRom() == Oak.configRom()){ //TODO does this cover gpio?
    //if we rebooted to config on purpose or we can't connect then allow connection
    #ifdef DEBUG_SETUP
        Serial.println("START AP");
    #endif
    setupAccessPoint();
  }
  else{
    //pump those events - we are in safe mode
    
    //send safe mode event
    Particle.publish("oak/devices/stderr", "Safe Mode", 60, PRIVATE);
    while(1){
      //wait for an update or reset!
      Particle.process();
    }
  }
  #ifdef DEBUG_SETUP
      Serial.println("STARTED");
  #endif

  #ifdef DEBUG_SETUP
      Serial.println("GOTO LOOP");
  #endif
      
}



void loop(){  

  server.handleClient();
  yield();
  Particle.process();
  telnetClient = telnet.available();
  if (telnetClient) {

    if (telnetClient.connected()) {
      //telnetClient.print("HELLO\n");
      //get command
      telnetClient.setTimeout(10000);
      char command[MAX_COMMAND_LENGTH+1];
      int resLength = telnetClient.readBytesUntil('\n',command,MAX_COMMAND_LENGTH);
      if(command[resLength-1]=='\r')
        command[resLength-1] = '\0';
      if(resLength<3){
        //telnetClient.print("CL");
        telnetClient.stop();
      }
      command[resLength] = '\0';
      String requestLengthString = telnetClient.readStringUntil('\n');
      int requestLength = requestLengthString.toInt();

      char request[requestLength+1];
      if(requestLength > 0){
        resLength = telnetClient.readBytesUntil('\n',request,requestLength);
        if(resLength<2 && resLength != requestLength)
          resLength = telnetClient.readBytesUntil('\n',request,requestLength);
        request[resLength] = '\0';
        if(request[resLength-1]=='\r')
          request[resLength-1] = '\0';
        if(resLength < requestLength){
          //telnetClient.print("RL");
          telnetClient.stop();
        }
      }
      else{
        char nullChar[1];
        Serial.setTimeout(100);
        resLength = telnetClient.readBytesUntil('\n',nullChar,1);
        Serial.setTimeout(10000);
      }

        if(strcmp(command,"version")==0) 
          displayVersion(false); 
        else if(strcmp(command,"device-id") == 0)
          displayDeviceId(false); 
        else if(strcmp(command,"scan-ap") == 0)
          displayScan(false); 
        else if(strcmp(command,"configure-ap") == 0)
          displayConfigureAp(true,String(request));
        else if(strcmp(command,"connect-ap") == 0)
          displayConnectAp(false); 
        else if(strcmp(command,"public-key") == 0)
          displayPublicKey(false); 
        else if(strcmp(command,"set") == 0)
          displaySet(false,String(request)); 
        else if(strcmp(command,"hello") == 0)
          displayHello(false); 
        else if(strcmp(command,"info") == 0)
          displayInfo(false); 
        else if(strcmp(command,"system-version") == 0)
          displaySystemVersion(false); 
        
        //else if(strcmp(command,"provision-keys") == 0)
        //  displayProvisionKeys(false,request);    
        else            {          
          //telnetClient.print("NM");
          //telnetClient.print("S");
          //telnetClient.print(command);
          //telnetClient.print("E");
          telnetClient.stop();
        }

    }
  }
  else if(Serial.available()){
    //telnetClient.print("HELLO\n");
      //get command
      Serial.setTimeout(10000);
      char command[MAX_COMMAND_LENGTH+1];
      int resLength = Serial.readBytesUntil('\n',command,MAX_COMMAND_LENGTH);
      if(command[resLength-1]=='\r')
        command[resLength-1] = '\0';
      if(resLength<3){
        while(Serial.read() != -1);
          return;
      }
      command[resLength] = '\0';
      String requestLengthString = Serial.readStringUntil('\n');
      int requestLength = requestLengthString.toInt();
      char request[requestLength+1];
      if(requestLength > 0){
        resLength = Serial.readBytesUntil('\n',request,requestLength);
        if(resLength<2 && resLength != requestLength)
          resLength = Serial.readBytesUntil('\n',request,requestLength);
        request[resLength] = '\0';
        if(request[resLength-1]=='\r')
          request[resLength-1] = '\0';
        if(resLength < requestLength){
          while(Serial.read() != -1);
          return;
        }
      }
      else{
        char nullChar[1];
        Serial.setTimeout(100);
        resLength = Serial.readBytesUntil('\n',nullChar,1);
        Serial.setTimeout(10000);
      }

        if(strcmp(command,"version")==0) 
          displayVersion(2); 
        else if(strcmp(command,"device-id") == 0)
          displayDeviceId(2); 
        else if(strcmp(command,"scan-ap") == 0)
          displayScan(2); 
        else if(strcmp(command,"configure-ap") == 0)
          displayConfigureAp(2,String(request)); 
        else if(strcmp(command,"connect-ap") == 0)
          displayConnectAp(2); 
        else if(strcmp(command,"public-key") == 0)
          displayPublicKey(2); 
        else if(strcmp(command,"set") == 0)
          displaySet(2,String(request)); 
        else if(strcmp(command,"hello") == 0)
          displayHello(2);
        else if(strcmp(command,"test") == 0)
          displayTest();  
        else if(strcmp(command,"info") == 0)
          displayInfo(2);
        else if(strcmp(command,"system-version") == 0)
          displaySystemVersion(2);
        else{
          while(Serial.read() != -1);
          return;
        }

  }

}


void setupAccessPoint(void) {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  
  //HTMLScan = "";
  JSONScan = "{ \"scans\" : [";
  if(n>10)
    n = 10;
  if (n > 0){
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      JSONScan += "{ \"ssid\":\"";
      //HTMLScan += "<option>";
      //HTMLScan += WiFi.SSID(i);
      JSONScan += WiFi.SSID(i);
      JSONScan += "\",\"rssi\":";
      //HTMLScan += " (";
      //HTMLScan += WiFi.RSSI(i);
      JSONScan += WiFi.RSSI(i);
      JSONScan += ",\"sec\":";
      //HTMLScan += ")";
      //HTMLScan += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " " : "*";
      /*
       SECURITY_OPEN           = 0;          /**< Unsecured                               
        SECURITY_WEP_PSK        = 1;          /**< WEP Security with open authentication   
        SECURITY_WEP_SHARED     = 0x8001;     /**< WEP Security with shared authentication
        SECURITY_WPA_TKIP_PSK   = 0x00200002; /**< WPA Security with TKIP                  
        SECURITY_WPA_AES_PSK    = 0x00200004; /**< WPA Security with AES                   
        SECURITY_WPA2_AES_PSK   = 0x00400004; /**< WPA2 Security with AES                  
        SECURITY_WPA2_TKIP_PSK  = 0x00400002; /**< WPA2 Security with TKIP                 
        SECURITY_WPA2_MIXED_PSK = 0x00400006;
       */
      switch (WiFi.encryptionType(i)) {
        case ENC_TYPE_WEP:
          JSONScan += "1";
          break;
        case ENC_TYPE_TKIP:
          JSONScan += "2097154"; //WPA_TKIP_PSK
          break;
        case ENC_TYPE_CCMP: //WPA2_TKIP_PSK 
          JSONScan += "4194306";
          break;
        case ENC_TYPE_NONE:
          JSONScan += "0";
          break;
        case ENC_TYPE_AUTO:
          JSONScan += "4194310";
          break;
      }
      JSONScan += ",\"ch\":";
      JSONScan += String(WiFi.channel(i));
      //HTMLScan += "</option>";
      if(i!=n-1)
        JSONScan += ",\"mdr\":0},";
      else
        JSONScan += ",\"mdr\":0}";
    }
  }
  JSONScan += "]}";
  //st += "</ol>";
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(IPAddress(192,168,0,1), IPAddress(192,168,0,1), IPAddress(255,255,255,0));
  WiFi.softAP(ssid_ap);

/*
  Serial.println("\nWiFi connected");
  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());
*/

  // Start the server
  telnet.begin();
  server.begin();
  server.on("/", displayHelloPage);
  server.on("/hello", displayHelloPage);
  server.on("/version", displayVersionPage);
  server.on("/device-id", displayDeviceIdPage);
  server.on("/scan-ap", displayScanPage);
  server.on("/configure-ap", displayConfigureApPage);
  server.on("/connect-ap", displayConnectApPage);
  server.on("/public-key", displayPublicKeyPage);
  server.on("/set", displaySetPage);
  server.on("/info", displayInfoPage);
  server.on("/system-version", displaySystemVersionPage);
  //server.on("/provision-keys", displayProvisionKeysPage);
  server.onNotFound(handleNotFound);
}

void displayConfigureApPage() {
  String input;
  if(server.hasArg("plain")){
    input = server.arg ("plain");
  }
  else{
    input = "";
  }
  //TO DO GET INPUT FROM POST
  displayConfigureAp(true,input);
}

void displayConfigureAp(uint8_t streamType,String input) {
  if(streamType == 1){
    sendHTML(Oak.configureApFromJSON(input));
  }
  else if(streamType == 2){
    sendSerial(Oak.configureApFromJSON(input));
  }
  else{
    sendTelnet(Oak.configureApFromJSON(input));
  }

}


void displaySystemVersionPage() {
  displaySystemVersion(true);
}
void displayHelloPage() {
  displayHello(true);
}

void displaySetPage() {
  String input;
  if(server.hasArg("plain")){
    input = server.arg ("plain");
  }
  else{
    input = "";
  }
  //TO DO GET INPUT FROM POST
  displaySet(true,input);
}



void displaySet(uint8_t streamType,String input) {
  if(streamType == 1){
    sendHTML(Oak.setConfigFromJSON(input));
  }
  else if(streamType == 2){
    sendSerial(Oak.setConfigFromJSON(input));
  }
  else{
    sendTelnet(Oak.setConfigFromJSON(input));
  }

}


void displayTest() {
  Serial.print(analogRead(A0));
}

void displaySystemVersion(uint8_t streamType) {
  if(streamType ==1){
    sendHTML(String(OAK_SYSTEM_VERSION_INTEGER));
  }
  else if(streamType == 2){
    sendSerial(String(OAK_SYSTEM_VERSION_INTEGER));
  }
  else{
    sendTelnet(String(OAK_SYSTEM_VERSION_INTEGER));
  }

}


void displayHello(uint8_t streamType) {
  String hello = "Soft AP Setup";
  if(streamType ==1){
    sendHTML(hello);
  }
  else if(streamType == 2){
    sendSerial(hello);
  }
  else{
    sendTelnet(hello);
  }

}


void displayInfoPage() {
  displayInfo(true);
}


void displayPublicKeyPage() {
  displayPublicKey(true);
}

String publicKeyResponse(){
  String response = "{\"b\":\"";
  response += Particle.pubKey();
  response += "\", \"r\":0}";
  return response;
}





void displayPublicKey(uint8_t streamType) {
  String pubKey = publicKeyResponse();
  if(streamType == 1){
    sendHTML(pubKey);
  }
  else if(streamType == 2){
    sendSerial(pubKey);
  }
  else{
    sendTelnet(pubKey);
  }
}


void displayConnectApPage() {
  displayConnectAp(true);
}

String connectApResponse(){
  return String("{\"r\":0}");
}

void displayConnectAp(uint8_t streamType) {
  String connectAp = connectApResponse();
  if(streamType == 1){
    sendHTML(connectAp);
  }
  else if(streamType == 2){
    sendSerial(connectAp);
  }
  else if(streamType == 0){
    sendTelnet(connectAp);
  }

  //switch client and try to connect
  ////TODO MAKE THIS PART OF CLOUD INIT?
  WiFi.softAPdisconnect(false);
  WiFi.mode(WIFI_STA);
  //TODO CONNECT TO CLOUD OR JUST RESTART INTO USER? INTO CURRENT? PROBABLY USER - BUT WHAT IF USER IS HAVING ERROR, THEN IT SHOULD REBOOT TO HERE AND HAVE SETTINGS
  
}

void displayVersionPage() {
  displayVersion(true);
}

String versionResponse(){
  return String("{\"v\":1}");
}

void displayVersion(uint8_t streamType) {
  String version = versionResponse();
  if(streamType == 1){
    sendHTML(version);
  }
  else if(streamType == 2){
    sendSerial(version);
  }
  else{
    sendTelnet(version);
  }

}

void displayDeviceIdPage() {
  displayDeviceId(true);
}

String deviceIdResponse(){
  String response = "";
  response += "{\"id\":\"";
  response += Particle.deviceID();
  response += "\",\"c\":";
  if(Particle.isClaimed() != 1)
    response += "0}";
  else
    response += "1}";
  return response;
}

void displayDeviceId(uint8_t streamType) {
  String deviceId = deviceIdResponse();
  if(streamType == 1){
    sendHTML(deviceId);
  }
  else if(streamType == 2){
    sendSerial(deviceId);
  }
  else{
    sendTelnet(deviceId);
  }

}

String infoResponse(){
  return Oak.infoResponse();
}


void displayInfo(uint8_t streamType) {
  if(streamType == 1){
    sendHTML(infoResponse());
  }
  else if(streamType == 2){
    sendSerial(infoResponse());
  }
  else{
    sendTelnet(infoResponse());
  }

}

void displayScanPage() {
  displayScan(true);
}

void displayScan(uint8_t streamType) {
  if(streamType == 1){
    sendHTML(JSONScan);
  }
  else if(streamType == 2){
    sendSerial(JSONScan);
  }
  else{
    sendTelnet(JSONScan);
  }
}

void sendHTML(String content){
    server.sendContent(ok_response);
    server.sendContent(String(content.length()).c_str());
    server.sendContent("\r\n\r\n");
    server.sendContent(content);
}
void sendTelnet(String content){
    telnetClient.print("\n\n");
    telnetClient.print(content);
}
void sendSerial(String content){
    Serial.print("\n\n");
    Serial.print(content);
}


void handleNotFound() {
  String message = "File Not Found\n\n";
  server.send ( 404, "text/plain", message );
}



