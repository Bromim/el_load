#include "Arduino.h"
#include <SPI.h>
#include "ad7799lib.h"

#define	MISO	19
#define SPI_MODE SPI_MODE3

static const int spiClk = 1000000;
SPIClass * vspi = NULL;
static uint8_t CS = 5;
static float VRef = 3.3;


void SPI_init(void){
	vspi = new SPIClass(VSPI);
	vspi->begin();
	pinMode(CS, OUTPUT); 
}

void AD7799_reset(void){
	vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
	digitalWrite(CS, LOW);//низкий уровень на CS ad7799
	vspi->transfer(0xFF);  
	vspi->transfer(0xFF);  
	vspi->transfer(0xFF);  
	vspi->transfer(0xFF);  
	digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
	vspi->endTransaction();
}

void AD7799_init(uint8_t pinCS, float voltage, AD7799_ModeReg modeReg){
	CS = pinCS;
	VRef=voltage;
	SPI_init();
	AD7799_reset();//перезапустим ad7799
	delay(1);
	AD7799_CommunicationReg CommunicationReg; 
	CommunicationReg._byte = 0;
	CommunicationReg._bits.RS = AD7799_REG_MODE;
	vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
	digitalWrite(CS, LOW);//низкий уровень на CS ad7799
	vspi->transfer(CommunicationReg._byte);// выбираем mode register (адрес: RS2=0,RS1=0,RS0=1)
	vspi->transfer(modeReg._bytes.h);// отправляем старший байт
	vspi->transfer(modeReg._bytes.l);// отправляем младший байт
	digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
	vspi->endTransaction();
}
uint8_t AD7799_getID(){
  uint8_t result = 0;
  AD7799_CommunicationReg CommunicationReg; 
  CommunicationReg._byte = 0;
  CommunicationReg._bits.RS = AD7799_REG_ID;
  CommunicationReg._bits.RW = 1;
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
  digitalWrite(CS, LOW);//низкий уровень на CS ad7799
  vspi->transfer(CommunicationReg._byte);
  result = vspi->transfer(0xFF);
  digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
  vspi->endTransaction();
  return result;//возвращаем результат преобразования
}

uint8_t AD7799_getByte(AD7799_CommunicationReg CommunicationReg){
	uint8_t result = 0;
	vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
	digitalWrite(CS, LOW);//низкий уровень на CS ad7799
	vspi->transfer(CommunicationReg._byte);
	result = vspi->transfer(0xFF);
	digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
	vspi->endTransaction();
	return result;//возвращаем результат преобразования
}
uint16_t AD7799_get2Byte(AD7799_CommunicationReg CommunicationReg){
	uint16_t result = 0;
  uint8_t low_byte,high_byte;
	vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
	digitalWrite(CS, LOW);//низкий уровень на CS ad7799
	vspi->transfer(CommunicationReg._byte);
	high_byte = vspi->transfer(0xFF);
	low_byte = vspi->transfer(0xFF);
	digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
	vspi->endTransaction();
	result = ((uint16_t) high_byte << 8)  | ((uint16_t) low_byte);
	return result;//возвращаем результат преобразования
}
uint32_t AD7799_get3Byte(AD7799_CommunicationReg CommunicationReg){
  uint16_t result = 0;
  uint8_t low_byte, average_byte, high_byte;
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
  digitalWrite(CS, LOW);//низкий уровень на CS ad7799
  vspi->transfer(CommunicationReg._byte);
  high_byte = vspi->transfer(0xFF);
  average_byte = vspi->transfer(0xFF);
  low_byte = vspi->transfer(0xFF);
  digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
  vspi->endTransaction();
  result = ((uint32_t) high_byte << 16) | ((uint32_t) average_byte << 8) | ((uint32_t) low_byte);
  return result;//возвращаем результат преобразования
}

void AD7799_setByte(AD7799_CommunicationReg CommunicationReg, uint8_t dByte){
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
  digitalWrite(CS, LOW);//низкий уровень на CS ad7799
  vspi->transfer(CommunicationReg._byte);// выбираем mode register (адрес: RS2=0,RS1=0,RS0=1)
  vspi->transfer(dByte);// отправляем старший байт
  digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
  vspi->endTransaction();
}

void AD7799_set2Byte(AD7799_CommunicationReg CommunicationReg, uint8_t hByte,uint8_t lByte){
  vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
  digitalWrite(CS, LOW);//низкий уровень на CS ad7799
  vspi->transfer(CommunicationReg._byte);// выбираем mode register (адрес: RS2=0,RS1=0,RS0=1)
  vspi->transfer(hByte);// отправляем старший байт
  vspi->transfer(lByte);// отправляем старший байт
  digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
  vspi->endTransaction();
}

uint32_t AD7799_getData(AD7799_ConfReg ConfReg)//функция принятия данных от ацп
{
	uint32_t result = 0;//переменная где будет храниться результат преобразования (24-бита)
	uint8_t low_byte, average_byte, high_byte;//младший,средний,старший байты данных преобразования
	AD7799_CommunicationReg CommunicationReg; 
	CommunicationReg._byte = 0;
	CommunicationReg._bits.RS = AD7799_REG_CONF;
	vspi->beginTransaction(SPISettings(spiClk, MSBFIRST, SPI_MODE));
	digitalWrite(CS, LOW);//низкий уровень на CS ad7799
	vspi->transfer(CommunicationReg._byte);
	vspi->transfer(ConfReg._bytes.h);
	vspi->transfer(ConfReg._bytes.l);

 /*
 CommunicationReg._byte = 0;
 CommunicationReg._bits.RS = AD7799_REG_MODE;
 vspi->transfer(CommunicationReg._byte);// выбираем mode register (адрес: RS2=0,RS1=0,RS0=1)
 vspi->transfer(0x30);// отправляем старший байт
  vspi->transfer(0x01);// отправляем младший байт
*/
  
	while(digitalRead(MISO));
	vspi->transfer(AD7799_READ_DATA);// следущая операция будет чтение из регистра данных(R/W=1, RS2=0, RS1=1, RS0=1)
	high_byte = vspi->transfer(0xFF);//принимаем старший байт
	average_byte = vspi->transfer(0xFF);//принимаем средний байт
	low_byte = vspi->transfer(0xFF);//принимаем младший байт
	digitalWrite(CS, HIGH);//высокий уровень на CS ad7799
	vspi->endTransaction();
	result = ((uint32_t) high_byte << 16) | ((uint32_t) average_byte << 8) | ((uint32_t) low_byte);
	return result;//возвращаем результат преобразования
}

uint32_t AD7799_getData2(AD7799_ConfReg ConfReg)//функция принятия данных от ацп
{
  uint32_t result = 0;//переменная где будет храниться результат преобразования (24-бита)
  uint8_t low_byte, average_byte, high_byte;//младший,средний,старший байты данных преобразования
  AD7799_CommunicationReg CommunicationReg; 
  CommunicationReg._byte = 0;
  CommunicationReg._bits.RS = AD7799_REG_CONF;
  AD7799_set2Byte(CommunicationReg, ConfReg._bytes.h, ConfReg._bytes.l);
  CommunicationReg._byte = 0;
  CommunicationReg._bits.RS = AD7799_REG_STAT;
  while(AD7799_getByte(CommunicationReg) & 0x80);
  CommunicationReg._byte = 0;
  CommunicationReg._bits.RS = AD7799_READ_DATA;
  result = AD7799_get3Byte(CommunicationReg);
  return result;//возвращаем результат преобразования
}

double AD7799_getVoltage(uint8_t Channel)
{
	AD7799_ConfReg ConfReg;
	ConfReg._16bit = 0x1000;
	ConfReg._bits.CH = Channel;
	double voltage = AD7799_getData(ConfReg) * (VRef / 0xFFFFFF);
	return voltage;
}
