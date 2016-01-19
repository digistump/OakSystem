//#define DEBUG_SETUP

#include <Ticker.h>
#include <ESP8266WiFi.h>
#include "handshake.h"
#include "rsa.h"
#include "setup-extras.h"
#include "dsakeygen.h"


#ifdef __cplusplus
extern "C" {
#endif
  #include <c_types.h>
  #include <user_interface.h>
  #include <mem.h>
  #include <osapi.h>
  #include "espmissingincludes.h"
  #include "oakboot.h"
#ifdef __cplusplus
}
#endif


#define MAX_COMMAND_LENGTH 14
/* Length in bytes of DER-encoded 1024-bit RSA private key */
#define PRIVATE_KEY_LENGTH    (612)
/* Length in bytes of DER-encoded 2048-bit RSA public key */
#define SERVER_PUBLIC_KEY_LENGTH   (294)
#define SERVER_DOMAIN_LENGTH   (253)
#define OTA_CONNECT_TIMEOUT 10000
#define OTA_READ_TIMEOUT 5000
#define MAX_ROM_SIZE (0x100000-0x2000)

//void *freeholder;


Ticker LEDFlip;
#ifdef DEBUG_SETUP
      Ticker FreeHeap;
#endif

uint8_t LEDState = 0;
char ssid_ap[12]; //AcornFFFFFF
//WiFiClient ota_client;

typedef enum
{
  IP_ADDRESS = 0, DOMAIN_NAME = 1, INVALID_INTERNET_ADDRESS = 0xff
} Internet_Address_TypeDef;

typedef struct __attribute__ ((__packed__)) ServerAddress_ {
  uint8_t addr_type;
  uint8_t length;
  union __attribute__ ((__packed__)) {
    char domain[127];
    uint32_t ip;
  };
} ServerAddress;


 typedef struct {
  //can cut off here if needed
  char device_id[25];     //device id in hex
  char claim_code[65];   // server public key
  uint8 claimed;   // server public key
  uint8 device_private_key[1216];  // device private key
  uint8 device_public_key[384];   // device public key
  uint8 server_public_key[768]; //also contains the server address at offset 384
  uint8 server_address_type;   //domain or ip of cloud server
  uint8 server_address_length;   //domain or ip of cloud server
  char server_address_domain[254];   //domain or ip of cloud server
  uint8 padding;
  uint32 server_address_ip;   //[4]//domain or ip of cloud server
  unsigned short firmware_version;  
  unsigned short system_version;     //
  char version_string[33];    //
  uint8 reserved_flags[32];    //
  uint8 reserved1[32];
  uint8 product_store[24];    
  char ssid[33]; //ssid and terminator
  char passcode[65]; //passcode and terminator
  uint8 channel; //channel number
  int32 third_party_id;    //
  char third_party_data[256];     //
  char first_update_domain[65];
  char first_update_url[65];
  char first_update_fingerprint[60];
  uint8 current_rom_scheme[1];
  uint8 padding2[1];
  uint8 magic;
  uint8 chksum; 
  //uint8 reserved2[698]; 
} oak_config; 

#define SECTOR_SIZE 0x1000
#define DEVICE_CONFIG_SECTOR 256
#define DEVICE_BACKUP_CONFIG_SECTOR 512
#define DEVICE_CHKSUM_INIT 0xee
#define DEVICE_MAGIC 0xf0

#define BOOT_CONFIG_SIZE 92
#define DEVICE_CONFIG_SIZE 3398

uint8 boot_buffer[BOOT_CONFIG_SIZE];
rboot_config *bootConfig = (rboot_config*)boot_buffer;
uint8 config_buffer[DEVICE_CONFIG_SIZE];
oak_config *deviceConfig = (oak_config*)config_buffer;

//esp code
ESP8266WebServer server(80);
WiFiServer telnet(5609);
WiFiClient telnetClient;

String JSONScan;
//String HTMLScan;
uint32_t lastBlink = 0;
uint32_t lastRoutine = 0;
bool deviceIdSet = false;

void FlipLED(){
   LEDState = !LEDState;
   digitalWrite(5, LEDState);
}


void deviceConfigInit(){
  if(!readDeviceConfig()){
    #ifdef DEBUG_SETUP
      Serial.println("NEW DEVICE CONFIG");
    #endif
    ets_memset(deviceConfig, 0x00, sizeof(oak_config));
    deviceConfig->magic = DEVICE_MAGIC;
    deviceConfig->chksum = calc_device_chksum((uint8*)deviceConfig, (uint8*)&deviceConfig->chksum);
    writeDeviceConfig();
  }
  if(bootConfig->first_boot == 2){
    #ifdef DEBUG_SETUP
      Serial.println("SKIP KEYS");
    #endif
    return;
  }

  if(deviceConfig->device_private_key[0] == 0x00 || deviceConfig->device_private_key[0] == 0xFF){
    digitalWrite(5,HIGH);
    if(!provisionKeys()){
      digitalWrite(5,LOW);
      LEDFlip.attach(2, FlipLED);
      while(1){
        yield();
      }
    }
    digitalWrite(5,LOW);
  }

  if(deviceConfig->server_public_key[0] == 0x00){
    #ifdef DEBUG_SETUP
      Serial.println("LOAD IN PUB KEY");
    #endif
    setResponse(String(particle_key_json));
    setResponse(String(first_update));
  }
}

void initVariant() {
    WiFi.disconnect();
    noInterrupts();
    spi_flash_erase_sector(1020);
    spi_flash_erase_sector(1021); 
    interrupts();
}

  #ifdef DEBUG_SETUP
void ShowFreeHeap(){
      Serial.println(ESP.getFreeHeap());
      }
    #endif

void setup(){



  
  //free(freeholder);
  Serial.begin(9600);





  if(!readBootConfig()){
    #ifdef DEBUG_SETUP
      Serial.println("NO BOOT CONFIG");
    #endif

    Serial.println("\r\nFAIL - SELF TEST FAIL\r\n");

    ESP.restart();
  }


  if(bootConfig->first_boot == 2){
    if(check_image(bootConfig->roms[0]) == 0){
      Serial.println("\r\nFAIL - SELF TEST FAIL\r\n");
      while(1);
    }
    else if(check_image(bootConfig->roms[2]) == 0){
      Serial.println("\r\nFAIL - SELF TEST FAIL\r\n");
      while(1);
    }
    else{
      Serial.println("\r\nOK - SELF TEST OK\r\n\r\n");
    }

  }

    //free(freeholder);
  
  //Serial.setDebugOutput(true);
  #ifdef DEBUG_SETUP
      Serial.println("START");
      FreeHeap.attach(5, ShowFreeHeap);
    #endif

  pinMode(5,OUTPUT);
  deviceConfigInit();
  
  sprintf(ssid_ap, "ACORN-%06x", ESP.getChipId());
  pinMode(4,OUTPUT);
  
  pinMode(0,OUTPUT);
  pinMode(2,OUTPUT);
  pinMode(15,OUTPUT);
  pinMode(13,OUTPUT);
  pinMode(12,OUTPUT);
  pinMode(14,OUTPUT);
  pinMode(16,OUTPUT);
  

  


  if(deviceConfig->device_id[0] == 'd' || deviceConfig->device_id[0] == 'D'){
    deviceIdSet = true;
    LEDFlip.attach(0.5, FlipLED);
  }

  #ifdef DEBUG_SETUP
      Serial.println("START AP");
  #endif
  setupAccessPoint();
  #ifdef DEBUG_SETUP
      Serial.println("STARTED");
  #endif

  #ifdef DEBUG_SETUP
      Serial.println("GOTO LOOP");
  #endif
      Serial.begin(115200);
}



uint8_t blinkRoutineIndex = 0;

void blinkRoutine(){
  
  if(millis()>lastBlink+100){
    lastBlink = millis();
    LEDState = !LEDState;
    digitalWrite(5,LEDState);
  }

  if(millis()>lastRoutine+40 && !LEDState){
    lastRoutine = millis();
    if(blinkRoutineIndex == 0){
      digitalWrite(0,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 1){
      digitalWrite(0,LOW);
      digitalWrite(2,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 2){
      digitalWrite(2,LOW);
      digitalWrite(4,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 3){
      digitalWrite(4,LOW);
      digitalWrite(12,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 4){
      digitalWrite(12,LOW);
      digitalWrite(13,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 5){
      digitalWrite(13,LOW);
      digitalWrite(14,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 6){
      digitalWrite(14,LOW);
      digitalWrite(15,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 7){
      digitalWrite(15,LOW);
      digitalWrite(16,HIGH);
      blinkRoutineIndex++;
    }
    else if(blinkRoutineIndex == 8){
      digitalWrite(16,LOW);
      blinkRoutineIndex = 0;
    }
  }

}



void loop(){  

  if(!deviceIdSet){
    blinkRoutine();
  }
  server.handleClient();
  yield();
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
  //server.on("/provision-keys", displayProvisionKeysPage);
  server.onNotFound(handleNotFound);
}

uint8_t hex_nibble(unsigned char c) {
    if (c<'0')
        return 0;
    if (c<='9')
        return c-'0';
    if (c<='Z')
        return c-'A'+10;
    if (c<='z')
        return c-'a'+10;
    return 0;
}

size_t hex_decode(uint8_t* buf, size_t len, const char* hex) {
    unsigned char c = '0'; // any non-null character
    size_t i;
    for (i=0; i<len && c; i++) {
        uint8_t b;
        if (!(c = *hex++))
            break;
        b = hex_nibble(c)<<4;
        if (c) {
            c = *hex++;
            b |= hex_nibble(c);
        }
        *buf++ = b;
    }
    return i;
}

int decrypt(char* plaintext, int max_plaintext_len, char* hex_encoded_ciphertext) {
    const size_t len = 256;
    uint8_t buf[len];
    hex_decode(buf, len, hex_encoded_ciphertext);

    // reuse the hex encoded buffer
    int plaintext_len = decrypt_rsa(buf, deviceConfig->device_private_key, (uint8_t*)plaintext, max_plaintext_len);
    return plaintext_len;
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
    sendHTML(configureAp(input));
  }
  else if(streamType == 2){
    sendSerial(configureAp(input));
  }
  else{
    sendTelnet(configureAp(input));
  }

}

String configureAp(String json){

  int16_t valueStart = json.indexOf("\"ssid\":\"");
  if(valueStart<0)
    return String("{\"r\":-1}");

  int16_t valueEnd = json.indexOf('"',valueStart+8);
  if(valueEnd-valueStart < 1)
    return String("{\"r\":-1}");
  String ssid = json.substring(valueStart+8,valueEnd);
  ssid.trim();
  if(ssid.length()>64)
    return String("{\"r\":-1}");
  ssid.toCharArray(deviceConfig->ssid,ssid.length()+1);
  deviceConfig->ssid[ssid.length()] = '\0';

  valueStart = json.indexOf("\"ch\":");
  uint8_t ch = 0;
  if(valueStart>=0){
    valueEnd = json.indexOf(',',valueStart+5);
    ch = json.substring(valueStart+5,valueEnd).toInt();
  }
  deviceConfig->channel = ch;

  valueStart = json.indexOf("\"sec\":");
  uint8_t sec = false;
  if(valueStart>=0){
    valueEnd = json.indexOf(',',valueStart+6);
    if(json.substring(valueStart+6,valueEnd).equals(String("0")))
      sec = false;
    else
      sec = true;
  }

  char passcode[65];
  if(sec == true){
    valueStart = json.indexOf("\"pwd\":\"");
    if(valueStart<0)
      return String("{\"r\":-1}");

    valueEnd = json.indexOf('"',valueStart+7);
    if(valueEnd-(valueStart+7) != 256)
      return String("{\"r\":-1}");
    char encodedPasscode[257];
    String passcodeString = json.substring(valueStart+7,valueEnd);
    passcodeString.trim();
    passcodeString.toCharArray(encodedPasscode,257);
    int decodeLength = decrypt((char*)deviceConfig->passcode, 65, encodedPasscode);
    deviceConfig->passcode[decodeLength] = '\0';
  }
  else{
    deviceConfig->passcode[0] = '\0';
  }
  writeDeviceConfig();
  return String("{\"r\":0}");

  
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

String setResponse(String json){

  String valueString;
  uint8_t gotSomething = false;
  int16_t valueStart = json.indexOf("\"cc\":\"");
  int16_t valueEnd;
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+6);
    valueString = json.substring(valueStart+6,valueEnd);
    valueString.trim();
    if(valueString.length()!=64){
      return String("{\"r\":-1}");
    }
    gotSomething = true;
    valueString.toCharArray(deviceConfig->claim_code,65);
    deviceConfig->claim_code[64] = '\0';
  }

  valueStart = json.indexOf("\"device-id\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+13);
    valueString = json.substring(valueStart+13,valueEnd);
    valueString.trim();
    if(valueString.length()!=24){

      return String("{\"r\":-1}");
    }
    #ifdef DEBUG_SETUP
      Serial.print("=");
      Serial.print(valueString);
      Serial.println("=");
    #endif
    deviceIdSet = true;
    bootConfig->first_boot = 1;
    writeBootConfig();
    LEDFlip.attach(0.5, FlipLED);
    gotSomething = true;
    valueString.toCharArray(deviceConfig->device_id,25);
    #ifdef DEBUG_SETUP
      Serial.print("=");
      Serial.print(deviceConfig->device_id);
      Serial.println("=");
    #endif
    deviceConfig->device_id[24] = '\0';
    #ifdef DEBUG_SETUP
      Serial.print("=");
      Serial.print(deviceConfig->device_id);
      Serial.println("=");
    #endif
  }

  valueStart = json.indexOf("\"meta-id\":");
  if(valueStart>=0){
    valueEnd = json.indexOf(',',valueStart+10);
    valueString = json.substring(valueStart+10,valueEnd);
    gotSomething = true;
    deviceConfig->third_party_id = valueString.toInt();
  }

  valueStart = json.indexOf("\"first-update-domain\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+23);
    valueString = json.substring(valueStart+23,valueEnd);
    valueString.trim();
    if(valueString.length()>64){
      return String("{\"r\":-1}");
    }
    gotSomething = true;
    valueString.toCharArray(deviceConfig->first_update_domain,valueString.length()+1);
    deviceConfig->first_update_domain[valueString.length()] = '\0';
  }

  valueStart = json.indexOf("\"first-update-url\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+20);
    valueString = json.substring(valueStart+20,valueEnd);
    valueString.trim();
    if(valueString.length()>64){
      return String("{\"r\":-1}");
    }
    gotSomething = true;
    valueString.toCharArray(deviceConfig->first_update_url,valueString.length()+1);
    deviceConfig->first_update_url[valueString.length()] = '\0';
  }

  valueStart = json.indexOf("\"first-update-fingerprint\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+28);
    valueString = json.substring(valueStart+28,valueEnd);
    valueString.trim();
    if(valueString.length()>59){
      return String("{\"r\":-1}");
    }
    gotSomething = true;
    valueString.toCharArray(deviceConfig->first_update_fingerprint,valueString.length()+1);
    deviceConfig->first_update_fingerprint[valueString.length()] = '\0';
  }

  valueStart = json.indexOf("\"meta-data\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+13);
    valueString = json.substring(valueStart+13,valueEnd);
    valueString.trim();
    if(valueString.length()>255){
      return String("{\"r\":-1}");
    }
    gotSomething = true;
    valueString.toCharArray(deviceConfig->third_party_data,valueString.length()+1);
    deviceConfig->third_party_data[valueString.length()] = '\0';
  }

  valueStart = json.indexOf("\"server-address\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+18);
    valueString = json.substring(valueStart+18,valueEnd);
    valueString.trim();
    if(valueString.length()>253){
      return String("{\"r\":-1}");
    }
    //get server address type
    valueStart = json.indexOf("\"server-address-type\":");
    if(valueStart<0){
      //server-address-type not set
      return String("{\"r\":-1}");
    }
    char server_address_type = json.charAt(valueStart+22);
    if(server_address_type == '0'){
      gotSomething = true;
      IPAddress bufIP;
      IPAddressFromString(bufIP,valueString.c_str());
      deviceConfig->server_address_ip = bufIP;
      deviceConfig->server_address_length = '\0';
    }
    else if(server_address_type == '1'){
      gotSomething = true;
      valueString.toCharArray(deviceConfig->server_address_domain,valueString.length()+1);
      deviceConfig->server_address_domain[valueString.length()] = '\0';
      deviceConfig->server_address_domain[253] = '\0';
    }
    else
      return String("{\"r\":-1}");
    
    
  }

  valueStart = json.indexOf("\"server-public-key\":\"");
  if(valueStart>=0){
    valueEnd = json.indexOf('"',valueStart+21);
    valueString = json.substring(valueStart+21,valueEnd);
    if(valueString.length()>2046){
      return String("{\"r\":-1}");
    }
    gotSomething = true;
    const size_t len = (valueString.length()+1)/2;
    uint8_t buf[len];
    hex_decode(buf, len, valueString.c_str());
    memcpy(deviceConfig->server_public_key,buf,SERVER_PUBLIC_KEY_LENGTH);
    
    if(len>386){
      uint8_t domainLength;
      domainLength = buf[385];
      deviceConfig->server_address_type = buf[384];
      if(deviceConfig->server_address_type == IP_ADDRESS){
        uint8_t ipBuf[4];
        memcpy(ipBuf,buf+386,4);
        deviceConfig->server_address_ip = (ipBuf[3] << 24) | (ipBuf[2] << 16) | (ipBuf[1] << 8)  |  ipBuf[0];
        deviceConfig->server_address_length = '\0';
        
      }
      else if(deviceConfig->server_address_type == DOMAIN_NAME && domainLength < 254){
        memcpy(deviceConfig->server_address_domain,buf+386,domainLength);
        deviceConfig->server_address_domain[domainLength] = '\0';
        deviceConfig->server_address_domain[253] = '\0';
        deviceConfig->server_address_length = domainLength;
      }
      else{
        return String("{\"r\":-1}");
      }

    }
  }

  if(!gotSomething)
    return String("{\"r\":-1}");
  else{
    //write config
    writeDeviceConfig();
    return String("{\"r\":0}");
  }
}

void displaySet(uint8_t streamType,String input) {
  if(streamType == 1){
    sendHTML(setResponse(input));
  }
  else if(streamType == 2){
    sendSerial(setResponse(input));
  }
  else{
    sendTelnet(setResponse(input));
  }

}


void displayTest() {
  Serial.print(analogRead(A0));
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



/*void displayProvisionKeysPage() {
  String input ="";
  displayProvisionKeys(true,input);
}

String provisionKeysResponse(String json){
  bool success = false;
  if(salt[0] != '\0')
    success = provisionKeys(force, salt)
  else
    success = provisionKeys(force, NULL)
  if(success)
    return String("{\"r\":0}");
  else
    return String("{\"r\":1}");
}

void displayProvisionKeys(uint8_t streamType,String input) {
  if(streamType == 1){
    sendHTML(provisionKeysResponse(input));
  }
  else{
    sendTelnet(provisionKeysResponse(input));
  }

}
*/

void displayInfoPage() {
  displayInfo(true);
}


void displayPublicKeyPage() {
  displayPublicKey(true);
}

String publicKeyResponse(){
  const int length = 162;
  const uint8_t* data = deviceConfig->device_public_key;
  String response = "{\"b\":\"";
  for (unsigned i=length; i-->0; ) {
    uint8_t v = *data++;
    response += ascii_nibble(v>>4);
    response += ascii_nibble(v&0xF);
  }
  response += "\", \"r\":0}";
  return response;
}


bool provisionKeys(){//(bool force){
  #ifdef DEBUG_SETUP
    Serial.println("Provision Keys");
  #endif
  if(generatePrivateKey(deviceConfig->device_private_key)){
    parse_device_pubkey_from_privkey(deviceConfig->device_public_key,deviceConfig->device_private_key);
    #ifdef DEBUG_SETUP
      const int length = 612;
      const uint8_t* data = deviceConfig->device_private_key;
      for (unsigned i=length; i-->0; ) {
        uint8_t v = *data++;
        Serial.write(ascii_nibble(v>>4));
        Serial.write(ascii_nibble(v&0xF));
      }
      Serial.write('\n');

    #endif
    writeDeviceConfig();
    return true;//generated
  }

  #ifdef DEBUG_SETUP
    Serial.println("Provision Keys FAILED");
  #endif
  
  return false; //not generate
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
  #ifdef DEBUG_SETUP
    Serial.println(deviceConfig->ssid);
    Serial.println(deviceConfig->passcode);
    Serial.println(deviceConfig->channel);
  #endif
  //switch client and try to connect
  WiFi.softAPdisconnect(false);
  WiFi.mode(WIFI_STA);
  if(deviceConfig->passcode[0] != '\0' && deviceConfig->channel > 0){
    WiFi.begin(deviceConfig->ssid,deviceConfig->passcode, deviceConfig->channel);
  }
  else if(deviceConfig->passcode[0] != '\0'){
    WiFi.begin(deviceConfig->ssid,deviceConfig->passcode);
  }
  else if(deviceConfig->channel > 0){
    WiFi.begin(deviceConfig->ssid, NULL, deviceConfig->channel);
  }
  else{
    WiFi.begin(deviceConfig->ssid);
  }
  //wait for connect
  uint32_t timeoutTime = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED)
  {
    yield();
    //timeout after 15 seconds and return to main config loop - config on app will fail
    if(millis() > timeoutTime){
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        WiFi.softAP(ssid_ap);
        return;
      }
      delay(100);
  }
  //while(1){yield();}
  ////only gets here if we were able to connect to wifi with success

  startFactoryUpdate(); 
  #ifdef DEBUG_SETUP
  Serial.print("\n\RESTART GO TO UPDATE ROM\n\n");
  #endif
  ESP.restart();
  
}

void startFactoryUpdate(){
  bootConfig->current_rom = 2; // update rom
  writeBootConfig();
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

/*
void generateDeviceId(uint8_t *keyBuffer){
  if(*keyBuffer != 0xFF)
    return;


  char deviceIdHex[24];
  byte randBytes[6];
  os_get_random(randBytes, 6);
  sprintf(deviceIdHex,"d95704%06x%02x%02x%02x%02x%02x%02x",ESP.getChipId(),randBytes[0],randBytes[1],randBytes[2],randBytes[3],randBytes[4],randBytes[5]);
  memcpy(keyBuffer, deviceIdHex, 24);
  writeDeviceConfig();

}
*/

void displayDeviceIdPage() {
  displayDeviceId(true);
}

String deviceIdResponse(){
  String response = "";
  response += "{\"id\":\"";
  response += deviceConfig->device_id;
  response += "\",\"c\":";
  if(deviceConfig->claimed != 1)
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
  String response = "";
  response += "{\"id\":\"";
  response += deviceConfig->device_id;
  response += "\",\"claimed\":";
  if(deviceConfig->claimed != 1)
    response += "0,";
  else
    response += "1,";
  response += "\"claim_code\":\"";
  response += deviceConfig->claim_code;
  response += "\",\"server_address_type\":";
  if(deviceConfig->server_address_type != 1){
    response += "0,";
    response += "\"server_address_ip\":\"";
    IPAddress server_ip = IPAddress(deviceConfig->server_address_ip);
    response += String(server_ip[0]);
    response += ".";
    response += String(server_ip[1]);
    response += ".";
    response += String(server_ip[2]);
    response += ".";
    response += String(server_ip[3]);
    response += "\"";
  }
  else if(deviceConfig->server_address_type == 1){
    response += "1,";
    response += "\"server_address_domain\":\"";
    response += deviceConfig->server_address_domain;
    response += "\"";
  }
  else
    response += "-1,";

    response += ",\"firmware_version\":";
  response += deviceConfig->firmware_version;
    response += ",\"version_string\":\"";
  response += deviceConfig->version_string;
  response += "\",\"meta_id\":";
  response += deviceConfig->third_party_id;
   response += ",\"meta_data\":\"";
  response += deviceConfig->third_party_data;
  response += "\",\"first_update_domain\":\"";
  response += deviceConfig->first_update_domain;
  response += "\",\"first_update_url\":\"";
  response += deviceConfig->first_update_url;
  response += "\",\"first_update_fingerprint\":\"";
  response += deviceConfig->first_update_fingerprint;
  response += "\"}";
  return response;
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

/*void handleNotFound() {
  String content = "File Not Found\n\n";
  server.send(404, "text/plain", content);
}*/

void handleNotFound() {

  String message = "File Not Found\n\n";
  /*message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  if(server.hasArg("plain")){
    message += "PLAIN:" + server.arg ("plain");
  }*/

  server.send ( 404, "text/plain", message );

}




int rsa_random(void* p)
{
    byte randBytes[4];
    os_get_random(randBytes, 4);
    return *((long *)randBytes);
}



/**
 * Reads and generates the device's private key.
 * @param keyBuffer
 * @return
 */
bool generatePrivateKey(uint8_t *keyBuffer)//, bool force)
{
    if(*keyBuffer!=0xFF && *keyBuffer!=0x00){// && !force){
      return false;
    }
    else{
      ESP.wdtDisable();
        if (!gen_rsa_key(keyBuffer, PRIVATE_KEY_LENGTH, rsa_random, NULL)) {
            //keyBuffer + PRIVATE_KEY_LENGTH = '\0';
            ESP.wdtEnable(WDTO_8S);
            return true;
        }
      ESP.wdtEnable(WDTO_8S);
    }
    return false;
}

void writeDeviceConfig(){
    deviceConfig->chksum = calc_device_chksum((uint8*)deviceConfig,(uint8*)&deviceConfig->chksum);
    noInterrupts();
    spi_flash_erase_sector(DEVICE_CONFIG_SECTOR);
    spi_flash_write(DEVICE_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);
    spi_flash_erase_sector(DEVICE_BACKUP_CONFIG_SECTOR);
    spi_flash_write(DEVICE_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);
    interrupts();
    
  }


void writeBootConfig(){
  noInterrupts();
    bootConfig->chksum = calc_chksum((uint8*)bootConfig,(uint8*)&bootConfig->chksum);
    spi_flash_erase_sector(BOOT_CONFIG_SECTOR);
    spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);
    spi_flash_erase_sector(BOOT_BACKUP_CONFIG_SECTOR);
    spi_flash_write(BOOT_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);
    interrupts();
    
  }



static uint8 calc_device_chksum(uint8 *start, uint8 *end) {
  uint8 chksum = DEVICE_CHKSUM_INIT;
  while(start < end) {
    chksum ^= *start;
    start++;
  }
  return chksum;
}


static uint8 calc_chksum(uint8 *start, uint8 *end) {
  uint8 chksum = CHKSUM_INIT;
  while(start < end) {
    chksum ^= *start;
    start++;
  }
  return chksum;
}

static char ascii_nibble(uint8_t nibble) {
    char hex_digit = nibble + 48;
    if (57 < hex_digit)
        hex_digit += 7;
    return hex_digit;
}

bool readBootConfig(){
noInterrupts();
  spi_flash_read(BOOT_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);

  if(bootConfig->magic != BOOT_CONFIG_MAGIC || bootConfig->chksum != calc_chksum((uint8*)bootConfig, (uint8*)&bootConfig->chksum)){

    //load the backup and copy to main
    spi_flash_read(BOOT_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);
    spi_flash_erase_sector(BOOT_CONFIG_SECTOR);
    spi_flash_write(BOOT_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(boot_buffer), BOOT_CONFIG_SIZE);

  }
  interrupts();

  if(bootConfig->magic != BOOT_CONFIG_MAGIC || bootConfig->chksum != calc_chksum((uint8*)bootConfig, (uint8*)&bootConfig->chksum)){
    return false;
  }

  return true;
}

bool readDeviceConfig(){
noInterrupts();


  spi_flash_read(DEVICE_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);

  if(deviceConfig->magic != DEVICE_MAGIC || deviceConfig->chksum != calc_device_chksum((uint8*)deviceConfig, (uint8*)&deviceConfig->chksum)){
    //load the backup and copy to main
    spi_flash_read(DEVICE_BACKUP_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);
    spi_flash_erase_sector(DEVICE_CONFIG_SECTOR);
    spi_flash_write(DEVICE_CONFIG_SECTOR * SECTOR_SIZE, reinterpret_cast<uint32_t*>(config_buffer), DEVICE_CONFIG_SIZE);
  }
  interrupts();

  if(deviceConfig->magic != DEVICE_MAGIC || deviceConfig->chksum != calc_device_chksum((uint8*)deviceConfig, (uint8*)&deviceConfig->chksum)){
    return false;
  }

  return true;
}


int decrypt_rsa(const uint8_t* ciphertext, const uint8_t* private_key, uint8_t* plaintext, int plaintext_len)
{
    rsa_context rsa;
    init_rsa_context_with_private_key(&rsa, private_key);
    int err = rsa_pkcs1_decrypt(&rsa, RSA_PRIVATE, &plaintext_len, ciphertext, plaintext, plaintext_len);
    rsa_free(&rsa);
    return err ? -abs(err) : plaintext_len;
}


bool IPAddressFromString(IPAddress &ipaddress, const char *address)
{

    uint16_t acc = 0; // Accumulator
    uint8_t dots = 0;

    while (*address)
    {
        char c = *address++;
        if (c >= '0' && c <= '9')
        {
            acc = acc * 10 + (c - '0');
            if (acc > 255) {
                // Value out of [0..255] range
                return false;
            }
        }
        else if (c == '.')
        {
            if (dots == 3) {
                // Too much dots (there must be 3 dots)
                return false;
            }
            ipaddress[dots++] = acc;
            acc = 0;
        }
        else
        {
            // Invalid char
            return false;
        }
    }

    if (dots != 3) {
        // Too few dots (there must be 3 dots)
        return false;
    }
    ipaddress[3] = acc;
    return true;
}


#define NOINLINE __attribute__ ((noinline))

#define ROM_MAGIC    0xe9
#define ROM_MAGIC_NEW1 0xea
#define ROM_MAGIC_NEW2 0x04


// buffer size, must be at least 0x10 (size of rom_header_new structure)
#define BUFFER_SIZE 0x100

// functions we'll call by address
typedef void stage2a(uint32);
typedef void usercode(void);

// standard rom header
typedef struct {
  // general rom header
  uint8 magic;
  uint8 count;
  uint8 flags1;
  uint8 flags2;
  usercode* entry;
} rom_header;

typedef struct {
  uint8* address;
  uint32 length;
} section_header;

// new rom header (irom section first) there is
// another 8 byte header straight afterward the
// standard header
typedef struct {
  // general rom header
  uint8 magic;
  uint8 count; // second magic for new header
  uint8 flags1;
  uint8 flags2;
  uint32 entry;
  // new type rom, lib header
  uint32 add; // zero
  uint32 len; // length of irom section
} rom_header_new;


static uint32 check_image(uint32 readpos) {
  
  uint8 buffer[BUFFER_SIZE];
  uint8 sectcount;
  uint8 sectcurrent;
  uint8 *writepos;
  uint8 chksum = CHKSUM_INIT;
  uint32 loop;
  uint32 remaining;
  uint32 romaddr;
  
  rom_header_new *header = (rom_header_new*)buffer;
  section_header *section = (section_header*)buffer;
  
  if (readpos == 0 || readpos == 0xffffffff) {
    //ets_printf("EMPTY");
    return 0;
  }
  
  // read rom header
  //if (SPIRead(readpos, header, sizeof(rom_header_new)) != 0) {
  if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(header), sizeof(rom_header_new)) != SPI_FLASH_RESULT_OK) {
    //ets_printf("NO_HEADER");
    return 0;
  }
  
  // check header type
  if (header->magic == ROM_MAGIC) {
    // old type, no extra header or irom section to skip over
    romaddr = readpos;
    readpos += sizeof(rom_header);
    sectcount = header->count;
  } else if (header->magic == ROM_MAGIC_NEW1 && header->count == ROM_MAGIC_NEW2) {
    // new type, has extra header and irom section first
    romaddr = readpos + header->len + sizeof(rom_header_new);

    // we will set the real section count later, when we read the header
    sectcount = 0xff;
    // just skip the first part of the header
    // rest is processed for the chksum
    readpos += sizeof(rom_header);
/*
    // skip the extra header and irom section
    readpos = romaddr;
    // read the normal header that follows
    if (SPIRead(readpos, header, sizeof(rom_header)) != 0) {
      //ets_printf("NNH");
      return 0;
    }
    sectcount = header->count;
    readpos += sizeof(rom_header);
*/
  } else {
    //ets_printf("BH");
    return 0;
  }
  
  // test each section
  for (sectcurrent = 0; sectcurrent < sectcount; sectcurrent++) {
    //ets_printf("ST");
    
    // read section header
    if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(section), sizeof(section_header)) != SPI_FLASH_RESULT_OK) {
      return 0;
    }
    readpos += sizeof(section_header);

    // get section address and length
    writepos = section->address;
    remaining = section->length;
    
    while (remaining > 0) {
      // work out how much to read, up to BUFFER_SIZE
      uint32 readlen = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
      // read the block
      if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(buffer), readlen) != SPI_FLASH_RESULT_OK) {
        return 0;
      }
      // increment next read and write positions
      readpos += readlen;
      writepos += readlen;
      // decrement remaining count
      remaining -= readlen;
      // add to chksum
      for (loop = 0; loop < readlen; loop++) {
        chksum ^= buffer[loop];
      }
    }
    
//#ifdef BOOT_IROM_CHKSUM
    if (sectcount == 0xff) {
      // just processed the irom section, now
      // read the normal header that follows
      if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(header), sizeof(rom_header)) != SPI_FLASH_RESULT_OK) {
        //ets_printf("SPI");
        return 0;
      }
      sectcount = header->count + 1;
      readpos += sizeof(rom_header);
    }
//#endif
  }
  
  // round up to next 16 and get checksum
  readpos = readpos | 0x0f;

  if (spi_flash_read(readpos, reinterpret_cast<uint32_t*>(buffer), 1) != SPI_FLASH_RESULT_OK) {
    //ets_printf("CK");
    return 0;

  }
  
  // compare calculated and stored checksums
  if (buffer[0] != chksum) {
    //ets_printf("CKF");
    return 0;
  }
  
  return romaddr;
}