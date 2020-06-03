#ifndef SDWEB_H_
#define SDWEB_H_

  #define SD_CS            15

  typedef union 
  {
    uint8_t bytes[4];    
    uint32_t _4byte;
  } SDWeb_uint32;


  void SDWeb_readProfile(String profileId);
  void SDWeb_returnOK(void);
  void SDWeb_returnFail(String msg);
  bool SDWeb_loadFromSdCard(String path);
  void SDWeb_handleFileUpload(void);
  void SDWeb_deleteRecursive(String path);
  void SDWeb_handleDelete(void);
  void SDWeb_handleCreate(void);
  void SDWeb_printDirectory(void);
  void SDWeb_handleNotFound(void);
  void SDWeb_init(void);
  void SDWeb_loop(void);

  void SDWeb_SDappend(const char * path, uint8_t* mas, uint16_t len);
  void SDWeb_SDcreateF(const char * path, uint8_t* mas, uint16_t len);
  void SDWeb_SDcreateD(const char * path);

#endif
