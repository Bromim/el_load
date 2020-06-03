// реализовать мютекс на чтение dishargeprofile
#include <SPI.h>
#include <OneWire.h>
#include "AD7799lib.h"
#include "PID.h"
#include "SDWebServer.h"


#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif
// use first channel of 16 channels (started from zero)
#define LEDC_CHANNEL_0     0
#define LEDC_CHANNEL_1     1
// use 13 bit precission for LEDC timer
#define LEDC_TIMER_13_BIT  13

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ     20000


#define Home()                    Serial.print("\e[H")
#define clrscr()                  Serial.print("\e[1;1H\e[2J") 
#define gotoxy(x,y)               Serial.printf("\e[%d;%dH", y, x)
#define visible_cursor()          Serial.print("\e[?251")
#define resetcolor()              Serial.print("\e[0m")
#define set_display_atrib(color)  Serial.printf("\e[%dm",color)



#define LED_PIN1          25
#define LED_PIN2          26
#define PWM2              22
#define PWM1              32
#define AD7799_CS         5

#define VREF              3.3
#define VOLT_DIVIDER      0.7353
#define AMP               6.556
#define R_SHUNT1          0.22
#define R_SHUNT2          47
#define PWM_UPPER_LIMIT   8191
#define BUF_LEN           512
#define MAX_SIZE_PROFILE  16

OneWire  ds(16); //ds18b20 D16

struct  {
  String ssid = "ESP32";
  String password = "12345678";
  String host = "esp32sd";
  String defPIN = "0123ASDC";
} conf; 

struct  {
  bool measurment = false;
  int reqKeyId = 0;
  String profileId = "";
  String measureId = "";
  uint32_t maxDuration_S = 0;
  uint32_t lasts_S = 0;
  uint32_t profileMas[MAX_SIZE_PROFILE][2];
  uint8_t profileMasLen = 0;
  uint32_t minBat_mV = 0;
} measure; 

struct  {
  bool ledGreen = false;
  bool ledRed = true;
} semaphore; 

static union{
  struct{
    uint64_t l:32;
    uint64_t h:32;
  }_bytes;
  uint8_t bytes[8];
  uint64_t uu;
  double d;
}Converter;

uint8_t idProgress = 0;
String progress[] = {"off","\\","/","-"};

double P = 0.25;
double I = 0.1;
double D = 0;
PID regulator(P,I,D, 10);

hw_timer_t * timer = NULL;

double voltage = 0; 
double current = 0; 
double setCurrent = 0;
String input = "";
double PIDout = 0;
boolean toogleMode = 0;
boolean toogle = 0;
double celsius = 0;

void TEST_Run(){
    measure.measureId = "Test";
    measure.profileMas[0][0] = 4000;
    measure.profileMas[0][1] = 100000;
    measure.profileMas[1][0] = 4000;
    measure.profileMas[1][1] = 100000;
    measure.profileMas[2][0] = 4000;
    measure.profileMas[2][1] = 200000;
    measure.profileMas[3][0] = 500;
    measure.profileMas[3][1] = 0;
    measure.profileMasLen = 1;
    measure.maxDuration_S = 0;
    measure.minBat_mV = 900;
    SDWeb_SDcreateF("/measurments/Test1.msmnt", {0}, 0);
    SDWeb_SDcreateF("/measurments/Test1.binlog", {0}, 0);
    measure.measurment = true;
}

void serialMenu()
{
  Home();
  //taskYIELD();
  Serial.printf("voltage: %.6f  | current: %.6f | set current: %.6f | setpoint: %.6f\n\r", voltage, current, setCurrent, regulator.setpoint );
 // taskYIELD();
  Serial.printf("P: %.6f | I: %.6f | D: %.6f | PIDout: %.6f |Time_S: %i\n\r", P, I, D,  PIDout, millis()/1000);
  if(measure.measurment){
    idProgress++;
    if(idProgress >= 4)
      idProgress = 1;
  }
  else
    idProgress = 0;
  Serial.printf("Temp: %.6f | Measure %s\n\r", celsius, progress[idProgress]);
 // taskYIELD();
  Serial.printf("%-10s",input);
  gotoxy(0,5);

  if (Serial.available() > 0)
  {
    int temp = Serial.read();
    if (temp != 13)
    {
      if(!toogleMode)
        input += (char)temp;
      else{
        if(toogle)
          toogle = 0;
        else
          toogle = 1;
      }
    } 
    else{
      toogleMode = 0;
      if(input[0] == '`')
      {
        input.toLowerCase();
        double number = (input.substring(2)).toFloat();
        if(input[1] == 'c')
        {
          setCurrent = number;
          regulator.setpoint = setCurrent;
        } else if(input[1] == 't')
        {
          toogleMode = 1;
          input = "toogleMode";
        } else { 
          if (input[1] == 'p')
          {
            P = number;
          } else if (input[1] == 'i')
          {
            I = number;
          } else if (input[1] == 'd')
          {
            D = number;
          }
          regulator.tune(P, I, D);
        }
      }
      if(!toogleMode)
        input="";
    }
  }
}


volatile SemaphoreHandle_t commandSemaphore;
volatile SemaphoreHandle_t SDSemaphore;
volatile SemaphoreHandle_t MeasureSemaphore;
void TaskADC( void *pvParameters );
void TaskServer( void *pvParameters );

void setup()
{
  Serial.begin(115200);

  
  pinMode(LED_PIN1, OUTPUT); 
  pinMode(LED_PIN2, OUTPUT); 
  digitalWrite(LED_PIN1, LOW);
  digitalWrite(LED_PIN2, LOW);
  // Setup timer and attach timer to a led pin
  ledcSetup(LEDC_CHANNEL_0, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(PWM1, LEDC_CHANNEL_0);

  ledcSetup(LEDC_CHANNEL_1, LEDC_BASE_FREQ, LEDC_TIMER_13_BIT);
  ledcAttachPin(PWM2, LEDC_CHANNEL_1);

  clrscr();
  visible_cursor();


  AD7799_ModeReg reg;
  reg._16bit=0x1001;
  AD7799_init(AD7799_CS, VREF, reg);
  AD7799_CommunicationReg CommunicationReg; 
  CommunicationReg._byte = 0;
  CommunicationReg._bits.RS = AD7799_REG_MODE;
  CommunicationReg._bits.RW = 1;
  AD7799_get2Byte(CommunicationReg);
    
  regulator.setDirection(NORMAL); // направление регулирования (NORMAL/REVERSE). ПО УМОЛЧАНИЮ СТОИТ NORMAL
  regulator.setLimits(0, 1);    // пределы
  regulator.setpoint = 0;        // сообщаем регулятору ток, который он должен поддерживать
  regulator.setDt(2);
  
  commandSemaphore = xSemaphoreCreateMutex();
  MeasureSemaphore = xSemaphoreCreateMutex();
  SDSemaphore = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(
    TaskADC
    ,  "TaskADC"   // A name just for humans
    ,  8192  // This stack size can be checked & adjusted by reading the Stack Highwater
    ,  NULL
    ,  1  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskServer
    ,  "TaskServer"
    ,  16640  // Stack size
    ,  NULL
    ,  1  // Priority
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);
}
int i = 0;
void loop() {
  taskYIELD();
}

void TaskADC(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  static uint8_t buf[BUF_LEN];
  int j = 0;
  uint8_t i;
  byte data[12];
  byte addr[8];
  byte present = 0;
  byte type_s;
  bool ds_read = false;
  uint32_t startTime = 0;
  uint32_t durationTime = 0;
  uint8_t numCurr = 0;
  double old_mAh = 0;
  double mAh = 0;
  double voltageCalm = 0;
  uint32_t testTime = millis();
  uint32_t bufMeasure[MAX_SIZE_PROFILE];
  for (;;) // A Task shall never return or exit.
  {

    #define TEMP_READ_VAL_PERIOD_MS 1000
    static uint32_t prevTempRead = millis();
    
    #define SERIALMENU_WRITE_PERIOD_MS 100
    static uint32_t prevSerialMenuWrite = millis();
    
    #define VOLTAGE_READ_VAL_PERIOD_MS 5
    static uint32_t prevVoltageValReadout = millis();
    
    #define CURRENT_READ_VAL_PERIOD_MS 5
    static uint32_t prevCurrentValReadout = millis();
    
    uint32_t currTime = millis();
    
    if( xSemaphoreTake( commandSemaphore, ( TickType_t ) 0 ) == pdTRUE ){
      if(semaphore.ledGreen)
        digitalWrite(LED_PIN1, LOW);
      else
        digitalWrite(LED_PIN1, HIGH);
      if(semaphore.ledRed)
        digitalWrite(LED_PIN2, LOW);
      else
        digitalWrite(LED_PIN2, HIGH);
    }
    
    if (currTime >= prevVoltageValReadout + VOLTAGE_READ_VAL_PERIOD_MS)
    {
       prevVoltageValReadout = currTime;
       voltage = AD7799_getVoltage(0) / VOLT_DIVIDER;
    }
    if (currTime >= prevCurrentValReadout + CURRENT_READ_VAL_PERIOD_MS)
    {
      taskYIELD();
      //setCurrent > 0.05
      if(setCurrent > 0.03)
        current = AD7799_getVoltage(1) / AMP / R_SHUNT1;
      else
        current = AD7799_getVoltage(2) / R_SHUNT2;
      if(measure.measurment){
        mAh += (current * 1000 *(currTime - prevCurrentValReadout) * 0.0000002778);
        measure.lasts_S = (currTime - startTime)/1000;
        //log_i("mAh: %f, time: %i" , mAh,  (currTime - startTime)/1000);
        Converter._bytes.l = (uint32_t)(current * 1000000);
        Converter._bytes.h = (uint32_t)(voltage * 1000);
        //0xffffff - voltage
        buf[j] = Converter.bytes[6];
        buf[j+1] = Converter.bytes[5];
        buf[j+2] = Converter.bytes[4];
        //0xffffff - current
        buf[j+3] = Converter.bytes[2];
        buf[j+4] = Converter.bytes[1];
        buf[j+5] = Converter.bytes[0];
        int16_t temper = (int16_t)(celsius * 10);
        buf[j+6] = (temper >> 8) & 0xff;
        buf[j+7] = temper & 0xff;
        j += 8;
        if(j >=BUF_LEN){
          j = 0;
          String path =  "/measurments/" + (String)( measure.measureId) + ".binlog";
          SDWeb_SDappend( path.c_str (), buf, BUF_LEN);
         
        }
        if(startTime == 0) 
          startTime = millis();
        if(durationTime == 0) 
          durationTime = millis();
        if(currTime >= measure.profileMas[numCurr][0] + durationTime){
          bufMeasure[numCurr] = (uint32_t)(voltage * 1000);
          if(mAh >= old_mAh + 1){
              old_mAh = mAh;
              uint8_t bufAppend[measure.profileMasLen + 5];
              Converter._bytes.l = (uint32_t) mAh;
              //0xffffff - current
              bufAppend[0] = Converter.bytes[2];
              bufAppend[1] = Converter.bytes[1];
              bufAppend[2] = Converter.bytes[0];

              int16_t temper = (int16_t)(celsius * 10);
              bufAppend[3] = (temper >> 8) & 0xff;
              bufAppend[4] = temper & 0xff;

              for(i = 0; i < measure.profileMasLen; i++)
                bufAppend[i+5] = bufMeasure[i];
              if(measure.profileMas[numCurr][1] != 0 ){
                String path =  "/measurments/" + (String) (measure.measureId) + ".msmnt";
                if( xSemaphoreTake( MeasureSemaphore, ( TickType_t ) 10 ) == pdTRUE ){
                  measure.reqKeyId += 1;
                  xSemaphoreGive(MeasureSemaphore); 
                }
                
                SDWeb_SDappend(path.c_str (), bufAppend, measure.profileMasLen + 5);
              }
          }
          
          durationTime = millis();
          if(measure.profileMas[numCurr][1] == 0 || numCurr >= measure.profileMasLen){
            if(( measure.maxDuration_S != 0 && measure.maxDuration_S * 1000 + startTime < currTime ) || (uint32_t)(voltage * 1000) <= measure.minBat_mV){
              measure.measurment = false;
              measure.profileId = "";
              measure.measureId = "";
              measure.maxDuration_S = 0;

              measure.minBat_mV = 0;
              mAh = 0;
              old_mAh = 0;
              startTime = 0;
            }
            numCurr = 0;
          }
          else{
            numCurr++;
          }
        }
          
        
        if(measure.measurment)
          setCurrent =  (double) measure.profileMas[numCurr][1] / 1000000;
        else
          setCurrent = 0;
        
        PIDout = regulator.getResult(setCurrent , current); 
      }
      else{
        measure.profileId = "";
        measure.measureId = "";
        measure.maxDuration_S = 0;

        measure.minBat_mV = 0;
        mAh = 0;
        old_mAh = 0;
        startTime = 0;
        if(!toogleMode)
          PIDout = regulator.getResult(setCurrent , current); // value [0..1]
        else
          PIDout = regulator.getResult(setCurrent*toogle , current); // value [0..1]
      }
      //log_e("Error: %f", (setCurrent - current));
      if(setCurrent == 0)
        PIDout = 0;
      if(setCurrent > 0.01){//set MOSFET
         
        ledcWrite(LEDC_CHANNEL_1, 0);
        //(setCurrent - current)< 0.0003 && (setCurrent - current)> -0.0003
        //if((setCurrent - current)< 0.03 && (setCurrent - current)> -0.03)
        //  ledcWrite(LEDC_CHANNEL_0, PWM_UPPER_LIMIT * PIDout);
       // else
          ledcWrite(LEDC_CHANNEL_0, PWM_UPPER_LIMIT * (-0.0014 * setCurrent * setCurrent + 0.0691 * setCurrent + 0.0065) * (toogle || !toogleMode)); //ВАХ IRL540NS 
      }
      else{//set NPN
        ledcWrite(LEDC_CHANNEL_0, 0);
        ledcWrite(LEDC_CHANNEL_1, PWM_UPPER_LIMIT * PIDout);
      }
      
      prevCurrentValReadout = currTime;
      
    }
    if (currTime >= prevSerialMenuWrite + SERIALMENU_WRITE_PERIOD_MS && Serial)
    {
      prevSerialMenuWrite = currTime;
      serialMenu();
    }
    
    if(currTime >= prevTempRead + TEMP_READ_VAL_PERIOD_MS)
    {
      
      
      prevTempRead = currTime;
       
      if(!ds_read){
        if ( !ds.search(addr)) {
          log_i("No DS18B20 address.");
          ds.reset_search();
        }
        //log_i("DS18B20 = %X%X%X%X%X%X%X%X", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7]);
      
        if (OneWire::crc8(addr, 7) != addr[7]) {
            log_e("CRC is not valid!");
        }

        ds.reset();
        ds.select(addr);
        ds.write(0x44, 1);        // start conversion, with parasite power on at the end
      }
      else{        
        present = ds.reset();
        ds.select(addr);    
        ds.write(0xBE);         // Read Scratchpad
  
        for ( i = 0; i < 9; i++) {           // we need 9 bytes
          data[i] = ds.read();
        }
      //log_i("temp = %X %X %X %X %X %X %X %X %X", data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
        // Convert the data to actual temperature
        // because the result is a 16 bit signed integer, it should
        // be stored to an "int16_t" type, which is always 16 bits
        // even when compiled on a 32 bit processor.
        int16_t raw = (data[1] << 8) | data[0];
        if (type_s) {
          raw = raw << 3; // 9 bit resolution default
          if (data[7] == 0x10) {
            // "count remain" gives full 12 bit resolution
            raw = (raw & 0xFFF0) + 12 - data[6];
          }
        } else {
          byte cfg = (data[4] & 0x60);
          // at lower res, the low bits are undefined, so let's zero them
          if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
          else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
          else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
          //// default is 12 bit resolution, 750 ms conversion time
        }
        celsius = (float)raw / 16.0;
      }
      ds_read = !ds_read;
    }
    if(currTime >= testTime + 2000 && currTime <= testTime + 2100){
        //=======================================[TEST]======================================
   
        
      // TEST_Run();
        
 
      
    
    //=================================================================================== 
    } 
  }
}
void TaskServer(void *pvParameters)  // This is a task.
{
  (void) pvParameters;
  vTaskDelay(100);
  SDWeb_init();
  //SDWeb_SDtest();
  for (;;) // A Task shall never return or exit.
  {
    SDWeb_loop();
  }
}
