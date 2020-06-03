/*AD7799 Registers*/
#ifndef AD7799_LIB_H_
#define AD7799_LIB_H_

  #define AD7799_REG_COMM				0 /* Communications Register(WO, 8-bit) */
  #define AD7799_REG_STAT				0 /* Status Register        (RO, 8-bit) */
  #define AD7799_REG_MODE				1 /* Mode Register          (RW, 16-bit */
  #define AD7799_REG_CONF				2 /* Configuration Register (RW, 16-bit)*/
  #define AD7799_REG_DATA				3 /* Data Register          (RO, 16-/24-bit) */
  #define AD7799_REG_ID				4 /* ID Register            (RO, 8-bit) */
  #define AD7799_REG_IO				5 /* IO Register            (RO, 8-bit) */
  #define AD7799_REG_OFFSET			6 /* Offset Register        (RW, 24-bit */
  #define AD7799_REG_FULLSALE			7 /* Full-Scale Register    (RW, 24-bit */
  
  #define AD7799_READ_DATA			0x58
 
  typedef union 
  {
    struct 
    {
      uint8_t  	 		: 2;
      uint8_t CREAD 		: 1;
      uint8_t RS  		: 3;
      uint8_t RW  		: 1;
      uint8_t WEN 		: 1;
    } _bits;
    
    uint8_t _byte;
  } AD7799_CommunicationReg;
  
  
  
  typedef union 
  {
    struct 
    {
      uint16_t FS 		: 4;
      uint16_t   			: 8;
      uint16_t PSW  		: 1;
      uint16_t MD  		: 3;
    } _bits;
    struct{
          unsigned char l;
          unsigned char h;
    }_bytes;
    uint16_t _16bit;
  } AD7799_ModeReg;
  
  typedef union 
  {
    struct 
    {
      uint16_t CH 		: 3;
      uint16_t 			: 1;
      uint16_t BUF  		: 1;
  	uint16_t REF_DET  	: 1;
  	uint16_t 			: 2;
      uint16_t G  		: 3;
  	uint16_t 			: 2;
  	uint16_t U_B  		: 1;
  	uint16_t BO  		: 1;
  	uint16_t 			: 1;
    } _bits;
    struct{
          unsigned char l;
          unsigned char h;
    }_bytes;
    uint16_t _16bit;
  } AD7799_ConfReg;
  
  void AD7799_reset(void);
  void AD7799_init(uint8_t pinCS, float voltage, AD7799_ModeReg modeReg);
  uint8_t AD7799_getByte(AD7799_CommunicationReg CommunicationReg);
  uint16_t AD7799_get2Byte(AD7799_CommunicationReg CommunicationReg);
  uint32_t AD7799_getData(AD7799_ConfReg ConfReg);
  double AD7799_getVoltage(uint8_t Channel);

#endif
