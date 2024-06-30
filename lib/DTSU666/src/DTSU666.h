/**
 * @file DTSU666.h  
 * @author Michiel STeltman (git: michielfromNL, msteltman@disway.nl 
 * @brief  meter Emulator definitions 
 * @version 0.1
 * @date 2024-06-17
 * 
 * @copyright Copyright (c) 2024, MIT license
 *  DTSU666 manual: https://www.solaxpower.com/uploads/file/dtsu666-user-manual-en.pdf
 */
#include <Arduino.h>
#include <ModbusRTU.h>
#include <SoftwareSerial.h>

// Register definitions
//
enum regType {  REG_WORD = 1,  REG_FLOAT = 2 } ;
typedef struct registerDef {
  word    address;
  regType type;
  const char *  code;
  const char *  name;
  const float   defval;
} registerDef;

#define ARRAY_SIZE(a) sizeof(a)/sizeof(a[0])
#define NUM_DTSU666_REGS 36 // ideally compiler calculates the # entries, but we need in the class definition

// class def for virtual DTSU666 power meter
// A meter serves as a slave. And has routines to set the register data: either from a JSON source 
// or from another source 
class DTSU666 {
public: 
  DTSU666() {};  // master, or set slave later
  DTSU666(uint slave_id) : _slaveid(slave_id) {};
  void    begin(SoftwareSerial * S, int16_t en_pin, uint slaveid = 0);
  void    setReg(word address, float value);
  size_t  readMeterData(uint slaveId,bool config = false);
  void    printRegs(word start, size_t numregs);
  void    task() { mb.task(); } 
  bool    isBusy() { return mb.slave(); }
  void    copyTo(DTSU666 & Meter);  /// operator = later
  //void    requestCb(Modbus::ResultCode cbPreRequest(Modbus::FunctionCode fc, const Modbus::RequestData data));

protected:
  ModbusRTU      mb;

private:
  //word *  rawIndex(word address);
  float   regs2Float(word reg1, word reg2);
  void    float2Regs (float val, word &reg1, word  &reg2 );
  word    reg2Word(word reg) { return reg; }
  word    word2Reg (word val) { return val; }
  size_t  readBlock(uint slaveId, word startAddress, size_t numRegs);
  size_t  readSection(uint slaveId, word startAddress, word endAddress);
  
  uint    _slaveid = 0;

  union {
    word regs[2];
    float fval; 
  } bfloat;
};