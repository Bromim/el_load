#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>

#include <ESPmDNS.h>
#include <CRC32.h>
#include <FS.h>
#include <SD_MMC.h>

#include <Arduino.h>
#include <esp32-hal-log.h>
#include <ArduinoJson.h>

#include "SDWebServer.h"


SPIClass * hspi = NULL;


static int token;
WebServer server(80);

static bool hasSD = false;

const char * headerKeys[] = {"Cookie"};
const size_t numberOfHeaders = 1;

//--------------------------------------------[init directory]---------------------------------------
void SDWeb_initFS(){
  if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
    File file = SD_MMC.open("/discharge-profiles");
    if(!file){
      SD_MMC.mkdir("/discharge-profiles");
    }
    file = SD_MMC.open("/measurments");
    if(!file){
      SD_MMC.mkdir("/measurments");
    }
    file.close();
    xSemaphoreGive(SDSemaphore); 
  }
}
//--------------------------------------------[read profile]---------------------------------------
void SDWeb_readProfile(String profileId){
  if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
    File file = SD_MMC.open("/discharge-profiles/" + profileId + ".dpl");
    if(file){
      String ProfileName = "";
      for(int i = 0; i < 256; i++)
      {
        ProfileName += (char) file.read();
      }
      //length mas
      file.read(); 
      file.read();
      file.read();
      file.read();
     
      SDWeb_uint32 int32;
      int i = 0;
      while(file.available()){
          int32.bytes[3] = file.read();
          int32.bytes[2] = file.read();
          int32.bytes[1] = file.read();
          int32.bytes[0] = file.read();
          measure.profileMas[i][0] = int32._4byte;// duration_ms
          
          int32.bytes[3] = 0x00;
          int32.bytes[2] = file.read();
          int32.bytes[1] = file.read();
          int32.bytes[0] = file.read();
          measure.profileMas[i][1] = int32._4byte;// current
          i++;
      }
      measure.profileMasLen = i;
      
    }
    
    file.close();
    xSemaphoreGive(SDSemaphore); 
  }
}


//--------------------------------------------[read sd config]---------------------------------------
void SDWeb_readConf(){
  StaticJsonDocument<600> confJson;
  if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
    File file = SD_MMC.open("/conf.bin");
    if(!file){
      file = SD_MMC.open("/conf.bin", FILE_WRITE);
      confJson["ssid"] = conf.ssid;
      confJson["password"] = conf.password;
      confJson["host"] = conf.host;
      confJson["defPIN"] = conf.defPIN;
      serializeJsonPretty(confJson, file);
    }
    else{
      String json = "";
      while(file.available()){
          json+=(char) file.read();
      }
      DeserializationError error = deserializeJson(confJson, json);
      if (error) {
        log_e("deserializeJson() failed: %s \n%s\n", error.c_str(), json);
        return;
      }
      String temp1 = confJson["ssid"];
      conf.ssid = temp1;
      String temp2 = confJson["password"];
      conf.password = temp2;
      String temp3 = confJson["host"];
      conf.host = temp3;
      String temp4 = confJson["defPIN"];
      conf.defPIN = temp4;
    }
  
    file.close();
    xSemaphoreGive(SDSemaphore); 
  }
}


void SDWeb_SDtest() {
  Serial.printf("\n\n\n\n\n\n======TEST=======\n");
  uint8_t mas[512];
  uint32_t full_time = millis();
  uint32_t delta_time = 0;
  uint32_t max_time = 0;
  uint32_t ran = esp_random();
  if(hasSD){
    for (int i=0; i<100; i++){
      
      uint32_t read_time = millis();
      File testFile = SD_MMC.open("/some.txt");
      testFile.seek(ran);
      testFile.read(&mas[0],512);
      testFile.close();
      read_time = millis() - read_time;
      if(max_time < read_time)
        max_time = read_time;
      if(delta_time == 0)
        delta_time = read_time;
      else
        delta_time = (read_time + delta_time)/2;
    }
    
  }
  full_time = millis() - full_time;
  Serial.printf("\n\n\n\n\n full time: %i\n delta time: %i\n max time: %i\n", full_time, delta_time, max_time);
}

void SDWeb_SDappend(const char * path, uint8_t* mas, uint16_t len) {
  if(hasSD){
    if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 40 ) == pdTRUE ){
      File file = SD_MMC.open(path, FILE_APPEND);
      file.write(mas, len);
      file.close();
      xSemaphoreGive(SDSemaphore); 
    }
  }
}
void SDWeb_SDcreateF(const char * path, uint8_t* mas, uint16_t len) {
  if(hasSD){
    if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 40 ) == pdTRUE ){
      File file = SD_MMC.open(path, FILE_WRITE);
      file.write(mas, len);
      file.close();
      xSemaphoreGive(SDSemaphore); 
    }
  }
}

void SDWeb_SDcreateD(const char * path) {
  if(hasSD){
    if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 40 ) == pdTRUE ){
      File file = SD_MMC.open(path);
      if(!file){
        SD_MMC.mkdir(path);
      }
      file.close();
      xSemaphoreGive(SDSemaphore); 
    }
  }
}

bool SDWeb_loadFromSdCard(String path) {
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.htm";
  }

  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".htm")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } else if (path.endsWith(".xml")) {
    dataType = "text/xml";
  } else if (path.endsWith(".pdf")) {
    dataType = "application/pdf";
  } else if (path.endsWith(".zip")) {
    dataType = "application/zip";
  }
  if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
    File dataFile = SD_MMC.open(path.c_str());
    if (dataFile.isDirectory()) {
      path += "/index.htm";
      dataType = "text/html";
      dataFile = SD_MMC.open(path.c_str());
    }
  
    if (!dataFile) {
      return false;
    }
  
    if (server.hasArg("download")) {
      dataType = "application/octet-stream";
    }
  
    if (server.streamFile(dataFile, dataType) != dataFile.size()) {
  	  log_e("Sent less data than expected!");
    }
  
    dataFile.close();
    xSemaphoreGive(SDSemaphore); 
  }
  return true;
}





void SDWeb_handleNotFound() {
  if (hasSD && SDWeb_loadFromSdCard(server.uri())) {
    return;
  }
  String message = "SDCARD Not Detected\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " NAME:" + server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  log_i("%s", message);
}


//=============================================================[setup]=======================================================================
void SDWeb_init() {
  token = esp_random();
  log_i("\nSetting AP (Access Point)...");
  hspi = new SPIClass(HSPI);
  if (SD_MMC.begin()) {
    Serial.println("SD Card initialized.");
    hasSD = true;
    semaphore.ledGreen= true;
    semaphore.ledRed = false;
    xSemaphoreGive( commandSemaphore );
  }
  else{
    log_e("SD Card is missing.");
    while(1)
      taskYIELD();
  }
  SDWeb_initFS();
  SDWeb_readConf();
  

  WiFi.softAP(&conf.ssid[0], &conf.password[0],1,0);

  IPAddress IP = WiFi.softAPIP();
  log_i("AP IP address: %s", IP.toString());

  

  // Wait for connection
  uint8_t i = 0;

	log_i("Connected! IP address: %s", WiFi.localIP().toString());

  if (MDNS.begin(&conf.host[0])) {
    MDNS.addService("http", "tcp", 80);
  	log_i("MDNS responder started");
  	log_i("You can now connect to http://%s.local", conf.host);
  }
  log_i("Token: %i", token);

  server.on("/authorize", HTTP_POST, SDWeb_handleAuthorize);
  server.on("/authorize", HTTP_GET, SDWeb_handleAuthorizeGet);
  server.on("/discharge-profiles", HTTP_POST, SDWeb_handleDischargeSet);
  server.on("/discharge-profiles", HTTP_GET, SDWeb_handleDischargeGet);
  server.on("/measurments", HTTP_POST, SDWeb_handleMeasurmentsSet);
  server.on("/measurments", HTTP_GET, SDWeb_handleMeasurmentsGet);
  server.on("/manual-control", HTTP_POST, SDWeb_handleManual);
  server.on("/parametrs", HTTP_PUT, SDWeb_handleParametrsPut);
  server.on("/parametrs", HTTP_GET, SDWeb_handleParametrsGet);

  
  server.onNotFound(SDWeb_handleNotFound);

  server.begin();

  server.collectHeaders(headerKeys, numberOfHeaders);
  Serial.println("HTTP server started");

  
  
}

void SDWeb_loop(void) {
 server.handleClient();
}




//================================================[HTTP]===========================================================


void SDWeb_returnOK() {
  server.send(200, "text/plain", "");
}

void SDWeb_returnFail(String msg) {
  server.send(500, "text/plain", msg + "\r\n");
}

bool is_authentified(){
  if (server.hasHeader("Cookie")){
    String cookie = server.header("Cookie");
	  log_i("Found cookie: %s", cookie);
    if (cookie.indexOf("authToken=" + String(token)) != -1) {
		  log_i("Authentification Successful");
      return true;
    }
  }
  log_i("Authentification Failed");
  return false;
}

//---------------------------------------[Authorize]---------------------------------------------------------
void SDWeb_handleAuthorize() {
  StaticJsonDocument<200> request;
  StaticJsonDocument<200> responce;
  
  WiFiClient client = server.client();
  if (server.args() == 0) {
    return SDWeb_returnFail("BAD_ARGS");
  }
  String currentLine = server.arg(0);

  DeserializationError error = deserializeJson(request, currentLine);

  if (error) {
    log_e("deserializeJson() failed: %s \n%s\n", error.c_str(), currentLine);
  }
  else{
    const char* pin = request["PIN"];
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    String str = conf.defPIN;
    
    if(str.equals(pin)){
      responce["result"] = "RESULT_OK";
      server.sendHeader("Set-Cookie","authToken=" + String(token)+"; Expires=Wed, 09 Jun 2021 10:18:14 GMT");
      
    }
    else{
      responce["result"] = "ERROR_WRONG_PIN";
    }
    String out;
    serializeJson(responce, out);
    server.send(200, "text/json", out);

  }
}
//---------------------------------------[AuthorizeGet]---------------------------------------------------------
void SDWeb_handleAuthorizeGet() {
  StaticJsonDocument<200> request;
  StaticJsonDocument<200> responce;
  WiFiClient client = server.client();

  if(is_authentified()){
   responce["result"] = "RESULT_OK";
  }
  else{
    responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
  }
  String out;
  serializeJson(responce, out);
  server.send(200, "text/json", out);
}
//---------------------------------------[DischargeSet]---------------------------------------------------------
void SDWeb_handleDischargeSet() {
  StaticJsonDocument<1024> request;
  StaticJsonDocument<200> responce;
  uint32_t i;
  WiFiClient client = server.client();
  if (server.args() == 0) {
    return SDWeb_returnFail("BAD_ARGS");
  }
  String currentLine = server.arg(0);
  if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
      
    File root = SD_MMC.open("/discharge-profiles");
    if(!root){
        SD_MMC.mkdir("/discharge-profiles");
    }
    root.close();
    xSemaphoreGive(SDSemaphore); 
  }
  if(is_authentified()){
    DeserializationError error = deserializeJson(request, currentLine);

    if (error) {
      log_e("deserializeJson() failed: %s \n%s\n", error.c_str(), currentLine);
    }
    else{
      String dischargeProfileName = request["dischargeProfileName"];
      JsonArray dataArray =  request["data"].as<JsonArray>();
      
      uint32_t checksum = CRC32::calculate(&dischargeProfileName, dischargeProfileName.length());
      if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
      
        File file = SD_MMC.open("/discharge-profiles/" + String(checksum, HEX) + ".dpl");
        if(!file){
          file = SD_MMC.open("/discharge-profiles/" + String(checksum, HEX) + ".dpl", FILE_WRITE);
          for(i = 0; i < 256; i++)
          {
            if(i < dischargeProfileName.length())
              file.write(dischargeProfileName.charAt(i));
            else
              file.write(0x00);
          }
          SDWeb_uint32 int32;
          
          int32._4byte = dataArray.size();
          file.write(int32.bytes[3]);
          file.write(int32.bytes[2]);
          file.write(int32.bytes[1]);
          file.write(int32.bytes[0]);
          log_i("dataArray.size:  %i", dataArray.size());
          for(i = 0; i < 2; i++){
            //offset ms
            uint32_t temp1 =  request["data"][i][0];
            double temp2 =  request["data"][i][1];
            int32._4byte = temp1;
            file.write(int32.bytes[3]);
            file.write(int32.bytes[2]);
            file.write(int32.bytes[1]);
            file.write(int32.bytes[0]);
            //current
            int32._4byte = temp2 * 1000;
            file.write(int32.bytes[2]);
            file.write(int32.bytes[1]);
            file.write(int32.bytes[0]);
          }
          responce["result"] = "RESULT_OK";
        }
        else
          responce["result"] = "ERROR_SUCH_PROFILE_ALREADY_EXISTS";
        file.close();  
        xSemaphoreGive(SDSemaphore); 
      }    
    }
  }
  else{
    responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
  }
  String out;
  serializeJson(responce, out);
  server.send(200, "text/json", out);
}
//---------------------------------------[DischargeGet]---------------------------------------------------------
void SDWeb_handleDischargeGet() {
  StaticJsonDocument<1024> responce;
  uint32_t i;
  
  WiFiClient client = server.client();

  if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
    
    
    File root = SD_MMC.open("/discharge-profiles");
    
    if(!root){
        SD_MMC.mkdir("/discharge-profiles");
    }
    File file = root.openNextFile();
    
    
    if(is_authentified()){
      if (server.hasArg("id")) {
        String fileName = server.arg("id");
  
        file = SD_MMC.open("/discharge-profiles/" + fileName + ".dpl");
        if(file){
          responce["result"] = "RESULT_OK";
          responce["dischargeProfileId"] = fileName;
          String ProfileName = "";
          for(i = 0; i < 256; i++)
          {
            char ch = (char) file.read();
            if(ch == '\0')
              for(; i < 255; i++)
                file.read();
            else
              ProfileName += ch;
          }
          responce["dischargeProfileName"] = ProfileName;
          //length mas
          file.read(); 
          file.read();
          file.read();
          file.read();
          String out;
          serializeJson(responce, out);
          
          server.setContentLength(CONTENT_LENGTH_UNKNOWN);
          server.send(200, "text/json",out.substring(0,out.length()-1));
          //server.sendContent();
          SDWeb_uint32 int32;
          int32.bytes[3] = file.read();
          int32.bytes[2] = file.read();
          int32.bytes[1] = file.read();
          int32.bytes[0] = file.read();
          server.sendContent(",\"data\":[[" + String(int32._4byte) + ",");
          int32.bytes[3] = 0x00;
          int32.bytes[2] = file.read();
          int32.bytes[1] = file.read();
          int32.bytes[0] = file.read();
          server.sendContent(String(((double)int32._4byte)/1000) + "]");
          String dat = "";
          while(file.available()){
              dat = "";
              int32.bytes[3] = file.read();
              int32.bytes[2] = file.read();
              int32.bytes[1] = file.read();
              int32.bytes[0] = file.read();
              dat += ",[" + String(int32._4byte) + ",";
              
              int32.bytes[3] = 0x00;
              int32.bytes[2] = file.read();
              int32.bytes[1] = file.read();
              int32.bytes[0] = file.read();
              dat += String(((double)int32._4byte)/1000) + "]";
              server.sendContent(dat);
          }
          server.sendContent("]}");
          //server.send(200, "text/json", out);
        }
        else{
          responce["result"] = "ERROR_NO_SUCH_ID";
          String out;
          serializeJson(responce, out);
          server.send(200, "text/json", out);
        }
        file.close();      
  
        
      }
      else{
        //JsonArray dischargeProfilesList = responce.createNestedArray("dischargeProfilesList");
        responce["result"] = "RESULT_OK";
        String out;
        
        serializeJson(responce, out);
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/json","");
        //out = out.substring(0,out.length()-1);
        server.sendContent(out.substring(0,out.length()-1));
        server.sendContent(",\"dischargeProfilesList\":[");
      //out += ",\"dischargeProfilesList\":[";

        bool dataAddedInCycle = false;
        while(file){
            if(!file.isDirectory()){
                String ProfileName = "";
                for(i = 0; i < 256; i++)
                {
                  char ch = (char) file.read();
                  
                  if(ch == '\0')
                    i = 256;
                  else
                    ProfileName += ch;
                }
                String profileID = String( file.name()).substring( String( file.name()).lastIndexOf("/") + 1,  String( file.name()).lastIndexOf("."));
                if (dataAddedInCycle)
                  server.sendContent(",");
                server.sendContent("{\"dischargeProfileId\":\"" + profileID + "\",\"dischargeProfileName\":\"" + ProfileName + "\"}");
                dataAddedInCycle = true;
            }
            file = root.openNextFile();
        }
        file.close();
        server.sendContent("]}");
        
      }
    }
    else{
      responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
      String out;
      serializeJson(responce, out);
      server.send(200, "text/json", out);
    }
    root.close();
    xSemaphoreGive(SDSemaphore); 
  }
}
//---------------------------------------[MeasurmentsSet]---------------------------------------------------------
void SDWeb_handleMeasurmentsSet() {
  StaticJsonDocument<1024> request;
  StaticJsonDocument<200> responce;
  uint32_t i;
  WiFiClient client = server.client();
  if (server.args() == 0) {
    return SDWeb_returnFail("BAD_ARGS");
  }
  String currentLine = server.arg(0);
  if(is_authentified()){
    DeserializationError error = deserializeJson(request, currentLine);

    if (error) {
      log_e("deserializeJson() failed: %s \n%s\n", error.c_str(), currentLine);
    }
    else{
      String measurmentName = request["measurmentName"];
      String description = request["description"];
      String dischargeProfileId = request["dischargeProfileId"];
      double minBat_V = request["minBat_V"];
      uint32_t maxDuration_S = request["maxDuration_S"];


      uint32_t checksum = CRC32::calculate(&measurmentName, measurmentName.length());
      if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
        File file = SD_MMC.open("/measurments/" + String(checksum, HEX) + ".msmnt");
        if(!file){
          file = SD_MMC.open("/measurments/" + String(checksum, HEX) + ".binlog", FILE_WRITE);
          file = SD_MMC.open("/measurments/" + String(checksum, HEX) + ".msmnt", FILE_WRITE);
          
          //measurmentName
          for(i = 0; i < 256; i++)
          {
            if(i < measurmentName.length())
              file.write(measurmentName.charAt(i));
            else
              file.write(0x00);
          }
          
          //description
          for(i = 0; i < 256; i++)
          {
            if(i < description.length())
              file.write(description.charAt(i));
            else
              file.write(0x00);
          }
          
          //dischargeProfileId
          for(i = 0; i < 8; i++)
          {
            if(i < dischargeProfileId.length())
              file.write(dischargeProfileId.charAt(i));
            else
              file.write(0x00);
          }
  
          //minBat_V
          SDWeb_uint32 int32;
          int32._4byte = (uint32_t) (minBat_V * 1000);
          file.write(int32.bytes[2]);
          file.write(int32.bytes[1]);
          file.write(int32.bytes[0]);
          
          //maxDuration_S
          int32._4byte = maxDuration_S;
          file.write(int32.bytes[3]);
          file.write(int32.bytes[2]);
          file.write(int32.bytes[1]);
          file.write(int32.bytes[0]);
          
          
          responce["result"] = "RESULT_OK";
          file.close();
          
          measure.profileId = dischargeProfileId;
          measure.measureId = String(checksum, HEX);
          measure.minBat_mV = (uint32_t) minBat_V * 1000;
          SDWeb_readProfile(dischargeProfileId);
          measure.measurment = true;
          //xSemaphoreGive( measureSemaphore );
        }
        else{
          responce["result"] = "ERROR_WRONG_FIELD_VALUE";
          file.close(); 
        }     
        xSemaphoreGive(SDSemaphore); 
      }
    }
  }
  else{
    responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
  }
  String out;
  serializeJson(responce, out);
  server.send(200, "text/json", out);
}
//---------------------------------------[MeasurmentsGet]---------------------------------------------------------
void SDWeb_handleMeasurmentsGet() {
  StaticJsonDocument<1024> responce;
  uint32_t i;
  uint32_t j = 0;
  WiFiClient client = server.client();
 if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
    
    File root = SD_MMC.open("/measurments");
      if(!root){
        SD_MMC.mkdir("/measurments");
    }
    File file = root.openNextFile();
    
    
    if(is_authentified()){
      if (server.args() != 0) {
        String fileName;
        bool getFile = true;
        bool currentFile = false;
        if(server.hasArg("id")){
          fileName = server.arg("id");
        }
        else if(server.hasArg("current")){
          fileName =  measure.measureId;
          if(measure.measurment){
            int temp = 0;
            if( xSemaphoreTake( MeasureSemaphore, ( TickType_t ) 10 ) == pdTRUE ){
              temp = measure.reqKeyId;
              xSemaphoreGive(MeasureSemaphore); 
            }
            if(String(temp).equals(server.arg("reqKeyId"))){
              responce["result"] = "RESULT_NO_NEW_DATA";
              String out;
              serializeJson(responce, out);
              server.send(200, "text/json", out);
              getFile = false;
            }
            else{
              currentFile = true;

              
            }
          }
          else{
            responce["result"] = "RESULT_NO_ACTIVE_MEASURMENT";
            String out;
            serializeJson(responce, out);
            server.send(200, "text/json", out);
            getFile = false;
          }

        }



        //---------------------------------------
        if(getFile){
          File file = SD_MMC.open("/measurments/" + fileName + ".msmnt");
          if(file){
            responce["result"] = "RESULT_OK";
            if(currentFile)
              responce["newReqKeyId"] = measure.reqKeyId;
            responce["measurmentId"] = fileName;
            String measurmentName = "";
            for(i = 0; i < 256; i++)
            {
              char ch = (char) file.read();
              if(ch == '\0')
                for(; i < 255; i++)
                  file.read();
              else
                measurmentName += ch;
            }
            responce["measurmentName"] = measurmentName;

            //description
            String description = "";
            for(i = 0; i < 256; i++)
            {
              char ch = (char) file.read();
              if(ch == '\0')
                for(; i < 255; i++)
                  file.read();
              else
                description += ch;
            }
            responce["description"] = description;
                    
            //dischargeProfileId
            String dischargeProfileId = "";
            for(i = 0; i < 8; i++)
            {
              dischargeProfileId += (char) file.read();
            }
            responce["dischargeProfileId"] = dischargeProfileId;

            responce["fullLogFileName"] = "/measurments/" + fileName + ".binlog";

            String out;
            serializeJson(responce, out);
            server.setContentLength(CONTENT_LENGTH_UNKNOWN);
            server.send(200, "text/json", "" );
            server.sendContent(out.substring(0,out.length()-1));
            SDWeb_uint32 int32;
            
            //minBat_V
            int32.bytes[3] = 0;
            int32.bytes[2] = file.read();
            int32.bytes[1] = file.read();
            int32.bytes[0] = file.read();
            server.sendContent(",\"minBat_V\":" + String(((double)int32._4byte)/1000));
            
            //maxDuration_S
            int32.bytes[3] = file.read();
            int32.bytes[2] = file.read();
            int32.bytes[1] = file.read();
            int32.bytes[0] = file.read();
            server.sendContent(",\"maxDuration_S\":" + String(int32._4byte));
            bool isFin;
            if(fileName.equals(measure.measureId))
              server.sendContent(",\"isFinished\":false");
            else
              server.sendContent(",\"isFinished\":true");
            

            server.sendContent(",\"plotsData\":{");
            uint32_t profileMas[MAX_SIZE_PROFILE][2];

            File profile = SD_MMC.open("/discharge-profiles/" + dischargeProfileId + ".dpl");
            if(profile){
              String ProfileName = "";
              for(i = 0; i < 256; i++)
              {
                char ch = (char) profile.read();
                if(ch == '\0')
                  for(; i < 255; i++)
                    profile.read();
                else
                  ProfileName += ch;
              }
              //length mas
              profile.read(); 
              profile.read();
              profile.read();
              profile.read();
              
              
              while(profile.available()){
                  int32.bytes[3] = profile.read();
                  int32.bytes[2] = profile.read();
                  int32.bytes[1] = profile.read();
                  int32.bytes[0] = profile.read();
                  profileMas[j][0] = int32._4byte;// duration_ms
                  int32.bytes[3] = 0x00;
                  int32.bytes[2] = profile.read();
                  int32.bytes[1] = profile.read();
                  int32.bytes[0] = profile.read();
                  profileMas[j][1] = int32._4byte;// current
                  j++;
              }
              
            }
                      
            profile.close();  
            server.sendContent("\"colDescr\":[\"DRWN_MAH\", \"TMP_C\"");
            for(i = 0; i < j; i++)
              server.sendContent(",\"V_" + String(((double)profileMas[i][1])) + "_MA\"");
            server.sendContent("], \"rowsData\":[");
            
            bool first = true;
            String dat;
            while(file.available()){
                if (first){
                  dat = "";
                  first = false;
                }
                else
                  dat = ",";

                int32.bytes[3] = 0;
                int32.bytes[2] = file.read();
                int32.bytes[1] = file.read();
                int32.bytes[0] = file.read();
                dat += "[" + String(int32._4byte) + ",";
                
                int32.bytes[3] = 0;
                int32.bytes[2] = 0;
                int32.bytes[1] = file.read();
                int32.bytes[0] = file.read();
                dat += String(((double)int32._4byte)/10);


                for(i = 0; i < j; i++){
                  int32.bytes[3] = 0;
                  int32.bytes[2] = file.read();
                  int32.bytes[1] = file.read();
                  int32.bytes[0] = file.read();
                  dat += "," + String(((double)int32._4byte)/1000);
                }
                dat += "]";
                server.sendContent(dat);
                
            }
            server.sendContent("]}}");
          }
          else{
            responce["result"] = "ERROR_NO_SUCH_ID";
            String out;
            serializeJson(responce, out);
            server.send(200, "text/json", out);
          }
          file.close();   
        }  
      } 
      else{      //--------------------------------------------------------
        responce["result"] = "RESULT_OK";
        String out;
        serializeJson(responce, out);
        server.setContentLength(CONTENT_LENGTH_UNKNOWN);
        server.send(200, "text/json", "" );
        server.sendContent(out.substring(0,out.length()-1));

        if(1)
            server.sendContent(",\"current\":null"); 
        else
            server.sendContent(",\"current\":{\"id\": \"" + String() +  ", \"name\": \"" + String() + "\"},"); 
        server.sendContent(",\"measurments\":[");
        bool dataAddedInCycle = false;
        file = root.openNextFile();
        while(file){
            if(!file.isDirectory()){
                String measureID = String( file.name()).substring( String( file.name()).lastIndexOf("/") + 1,  String( file.name()).lastIndexOf("."));
                String measurmentName = "";
                for(i = 0; i < 256; i++)
                {
                  char ch = (char) file.read();
                  if(ch == '\0')
                    i=256;
                  else
                     measurmentName += ch;
                }
                if (dataAddedInCycle)
                  server.sendContent(",");
                server.sendContent("{\"id\":\"" + measureID + "\", \"name\": \"" + measurmentName + "\"}");
               
                dataAddedInCycle = true;
            }
            file.close();
            file = root.openNextFile();
        }
        file.close();
        server.sendContent("]}");
      }
    }
    else{
      responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
      String out;
      serializeJson(responce, out);
      server.send(200, "text/json", out);
    }
    file.close();
    root.close();
    xSemaphoreGive(SDSemaphore); 
  }
}
//---------------------------------------[Manual]---------------------------------------------------------
void SDWeb_handleManual() {
  StaticJsonDocument<512> request;
  StaticJsonDocument<200> responce;
  uint32_t i;
  WiFiClient client = server.client();
  if (server.args() == 0) {
    return SDWeb_returnFail("BAD_ARGS");
  }
  String currentLine = server.arg(0);
  
  if(is_authentified()){
    DeserializationError error = deserializeJson(request, currentLine);

    if (error) {
      log_e("deserializeJson() failed: %s \n%s\n", error.c_str(), currentLine);
    }
    else{
       StaticJsonDocument<200> arguments = request["command"][1];
      String command = request["command"][0];
      if(command.equals("CMD_STOP_MEASURMENT")){
        measure.measurment = false;
      }
      else if(command.equals("CMD_LED_SET")){
        bool red = arguments["redIsOn"];
        bool green = arguments["greenIsOn"];
        semaphore.ledGreen= green;
        semaphore.ledRed = red;
      }
      xSemaphoreGive( commandSemaphore );
      responce["result"] = "RESULT_OK";
    }
  }
  else{
    responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
  }
  String out;
  serializeJson(responce, out);
  server.send(200, "text/json", out);
}
//---------------------------------------[ParametrsPut]---------------------------------------------------------
void SDWeb_handleParametrsPut() {
  StaticJsonDocument<512> request;
  StaticJsonDocument<200> responce;
  uint32_t i;
  WiFiClient client = server.client();
  if (server.args() == 0) {
    return SDWeb_returnFail("BAD_ARGS");
  }
  String currentLine = server.arg(0);
  
  if(is_authentified()){
    DeserializationError error = deserializeJson(request, currentLine);

    if (error) {
      log_e("deserializeJson() failed: %s \n%s\n", error.c_str(), currentLine);
    }
    else{

      StaticJsonDocument<600> confJson;
	  
      confJson["ssid"] = conf.ssid;
      confJson["password"] = conf.password;
      confJson["host"] = conf.host;
      confJson["defPIN"] = conf.defPIN;
	  
      String paramName = request["paramName"];
      confJson[paramName] = request["paramValue"];
      
      String temp1 = confJson["ssid"];
      conf.ssid = temp1;
      String temp2 = confJson["password"];
      conf.password = temp2;
      String temp3 = confJson["host"];
      conf.host = temp3;
      String temp4 = confJson["defPIN"];
      conf.defPIN = temp4;
      if( xSemaphoreTake( SDSemaphore, ( TickType_t ) 1000 ) == pdTRUE ){
        File file = SD_MMC.open("/conf.bin", FILE_WRITE);
        serializeJsonPretty(confJson, file);
        file.close();
        xSemaphoreGive(SDSemaphore); 
      }
      responce["result"] = "RESULT_OK";

    }
  }
  else{
    responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
  }
  String out;
  serializeJson(responce, out);
  server.send(200, "text/json", out);
}
//---------------------------------------[ParametrsGet]---------------------------------------------------------
void SDWeb_handleParametrsGet() {
  StaticJsonDocument<600> responce;
  uint32_t i;
  WiFiClient client = server.client();
  
  if(is_authentified()){
    responce["result"] = "RESULT_OK";
    StaticJsonDocument<600> confJson;
    JsonArray parametrList = responce.createNestedArray("parametrList");
    confJson["ssid"] = conf.ssid;
    parametrList.add(confJson);
    confJson.clear();
    confJson["password"] = conf.password;
    parametrList.add(confJson);
    confJson.clear();
    confJson["host"] = conf.host;
    parametrList.add(confJson);
    confJson.clear();
    confJson["defPIN"] = conf.defPIN;
    parametrList.add(confJson);
    confJson.clear();
  }
  else{
    responce["result"] = "ERROR_NOT_AUTHORIZED_ACCESS";
  }
  String out;
  serializeJson(responce, out);
  server.send(200, "text/json", out);
}
