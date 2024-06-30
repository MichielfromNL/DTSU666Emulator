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
#include <DTSU666.h>

// the DTSU register definition, with some default.
// We can't put it in progmem, progranm will crash (don;t really know why)
const registerDef DTSU666Regs[NUM_DTSU666_REGS] = {
{ 0x0,REG_WORD,"REV.","Software version",204} ,
{ 0x1,REG_WORD,"UCode", "Programming code",701} ,
{ 0x2,REG_WORD,"ClrE", "Power reset",0} ,
{ 0x3,REG_WORD,"nET", "Network selection",0} ,
{ 0x6,REG_WORD,"Ct", "Current transformer rate",1} ,
{ 0x7,REG_WORD,"Pt", "Voltage transformer rate",10 } ,
{ 0xa,REG_WORD,"Disp", "Rotating Display Time",0 } ,
{ 0xc,REG_WORD,"Endian", "Reserved",0} ,
{ 0x2c,REG_WORD,"Prot", "Protocol stopbits",3 } ,
{ 0x2d,REG_WORD,"bAud", "Communication baudrate",3 } ,
{ 0x2e,REG_WORD,"Addr", "Communication address",1 } ,
// Electricity
{ 0x101E,REG_FLOAT,"ImpEp", "(Current) positive total active energy",0 } ,
{ 0x1028,REG_FLOAT,"ExpEp", "(Current) negative total active energy",0 } ,
// 
{ 0x2000,REG_FLOAT,"Uab", "Three phase line voltage",0 } ,
{ 0x2002,REG_FLOAT,"Ubc", "Three phase line voltage",0 } ,
{ 0x2004,REG_FLOAT,"Uca", "Three phase line voltage",0 } ,
{ 0x2006,REG_FLOAT,"Ua",  "Three phase phase voltage",0 } ,
{ 0x2008,REG_FLOAT,"Ub",  "Three phase phase voltage",0 } ,
{ 0x200a,REG_FLOAT,"Uc",  "Three phase phase voltage",0 } ,
{ 0x200c,REG_FLOAT,"Ia",  "Three phase current",0} ,
{ 0x200e,REG_FLOAT,"Ib",  "Three phase current",0 } ,
{ 0x2010,REG_FLOAT,"Ic",  "Three phase current",0 } ,
{ 0x2012,REG_FLOAT,"Pt",  "Combined active power",0 } ,
{ 0x2014,REG_FLOAT,"Pa",  "A phase active power",0} ,
{ 0x2016,REG_FLOAT,"Pb",  "B phase active power",0} ,
{ 0x2018,REG_FLOAT,"Pc",  "C phase active power",0 } ,
{ 0x201A,REG_FLOAT,"Qt",  "Combined reactive power",0 } ,
{ 0x201C,REG_FLOAT,"Qa",  "A Phase reactive power",0 } ,
{ 0x201E,REG_FLOAT,"Qb",  "B Phase reactive power",0 } ,
{ 0x2020,REG_FLOAT,"Qc",  "C Phase reactive power",0 } ,
{ 0x202A,REG_FLOAT,"PFt", "Combined power factor",0 } ,
{ 0x202C,REG_FLOAT,"PFa", "A Phase power factor",0 } ,
{ 0x202E,REG_FLOAT,"PFc", "B Phase power factor",0 } ,
{ 0x2030,REG_FLOAT,"PFc", "C Phase power factor",0 } ,
{ 0x2044,REG_FLOAT,"Freq","Frequency unit",4999 },
{ 0xffff,REG_WORD,"-","--",0} 
};

static_assert(NUM_DTSU666_REGS == ARRAY_SIZE(DTSU666Regs ));
const size_t NUM_DEFS = NUM_DTSU666_REGS;

/**
 * @brief Convert Modbus RTU registers to a ESP8266 float
 * @param register array from Modbus RTU . Endianness need to be converted
 * @return float 
 */
float DTSU666::regs2Float(word reg1, word reg2) {
  bfloat.regs[1] = reg1;
  bfloat.regs[0] = reg2;

  return bfloat.fval;
}

// And the other way around
void DTSU666::float2Regs (float val, word & reg1, word  & reg2 ) {
  bfloat.fval = val;

  reg1 = bfloat.regs[1];
  reg2 = bfloat.regs[0];
}

// saves a value 
void DTSU666::setReg(word address, float val) {
  size_t i;
  for (i=0; i< NUM_DEFS-1 && DTSU666Regs[i].address != address ; i++);
  if ( DTSU666Regs[i].type == REG_WORD) {
      mb.Hreg(DTSU666Regs[i].address,word2Reg((word)val));
    } else {
      // float
      word reg1, reg2;
      float2Regs(val,reg1,reg2);
      mb.Hreg(DTSU666Regs[i].address,reg1);
      mb.Hreg(DTSU666Regs[i].address+1,reg2);
    }
}

// Print data.
void DTSU666::printRegs (word startAddress, size_t numregs) {
  size_t i=0;
  for (i=0; i< NUM_DEFS-1 && DTSU666Regs[i].address < startAddress; i++);
  for (; i < NUM_DEFS-1 && numregs-- > 0; i++) {
    //word * raw = rawIndex(DTSU666Regs[i].address);
    word address = DTSU666Regs[i].address;
    if (DTSU666Regs[i].type == REG_FLOAT) {
      Serial.printf("0x%04x (%6s\t%40s) = %.1f\n", 
        address,DTSU666Regs[i].code,DTSU666Regs[i].name,regs2Float(mb.Hreg(address),mb.Hreg(address+1)));
      numregs--;
    } else {
      Serial.printf("0x%04x (%6s\t%40s) = %d\n", 
        address,DTSU666Regs[i].code,DTSU666Regs[i].name, reg2Word(mb.Hreg(address)));
    }
  }
}

/**
 * @brief read a number of consecutive registers, caller needs to make sure that
 *        the entire block is readable
 * 
 * @param startAddress 
 * @param numRegs 
 * @return int 
 */
size_t DTSU666::readBlock(uint slaveId, word startAddress, size_t numRegs) {

  bool status = false;

  Serial.printf("Pulling %d registers from %d at %04x : ",numRegs,slaveId,startAddress);
  mb.pullHreg(slaveId, startAddress, startAddress, numRegs, 
    // use a lambda as callback, pass ref to status
    [&](Modbus::ResultCode event, uint16_t Id, void*) {
      status = event == Modbus::EX_SUCCESS;
      if (status) {
        Serial.printf(" OK\n");
      } else {
        Serial.printf("failed, status=0x%02X\n",event);  // Display Modbus error code
      }
      return status;
  }); 
  // Send Read Hreg from Modbus Server
  while(mb.slave()) { // Check if transaction is active
    mb.task();
    delay(10);
    yield();
  }
  //Serial.println("Block read completed");
  return status ? numRegs : 0;
}

/**
 * @brief read a section, is application specific
 * 
 * @param startAddress 
 * @param endAddress 
 * @return number of read registers 
 *  
 */
size_t DTSU666::readSection(uint slaveId, word startAddress, word endAddress) {
  
  word blockStart = startAddress;
  size_t i = 0;
  int numRegs = 0;
  int numReads = 0;
  
  // Find first valid entry in section
  for (i=0; i < NUM_DEFS && DTSU666Regs[i].address != startAddress ; i++ );
  // First blockstart found. now find consecutive blocks in this section 
  blockStart = DTSU666Regs[i].address;
  while (i<NUM_DEFS && (DTSU666Regs[i].address <= endAddress) ) {
    //Serial.printf("Block start address 0x%0x found : ",blockStart);
    // Now see how many regs we can read
    // by checking if next address corresponds with added reg sizes
    numRegs = 0;
    while (i<NUM_DEFS && blockStart + numRegs == DTSU666Regs[i].address && 
                      DTSU666Regs[i].address < endAddress &&  numRegs < 16) {
      numRegs+= DTSU666Regs[i].type == REG_FLOAT ? 2 : 1;
      i++;
    }
    //Serial.printf(" can read %d consecutive registers\n",numRegs);
    numReads += readBlock(slaveId,blockStart,numRegs);
    // set to next entry, which is 0xffff if last
    blockStart = DTSU666Regs[i].address;
    // give other tasks some time 
    yield();
  }
  return numReads;
}

// reads data from remote meter
size_t DTSU666::readMeterData(uint slaveId, bool config) {
    size_t r, regsread = 0 ;
    
    // read initial data
    if (config) {
        r = readSection(slaveId,0x0,0x100);
        //Serial.printf("section at 0x0: %d entries read\n",r);
        regsread = r;
    }
    r = readSection(slaveId,0x1000,0x1fff);
    //Serial.printf("section at 0x1000: %d entries read\n",r);
    regsread += r;

    r = readSection(slaveId,0x2000,0x2046);
    //Serial.printf("section at 0x2000: %d entries read\n",r);
    regsread += r;
    
    return regsread;
}


// Setup our meter image 
// 
void DTSU666::begin(SoftwareSerial * S, int16_t re_depin, uint slaveid) {

  if (_slaveid == 0) _slaveid = slaveid;  // set if not already initialized, optional slaveid defaults to 0

  mb.begin(S,re_depin);
  delay(500);

  // setup registers and set some initial data
  for (size_t i=0; i<NUM_DEFS-1; i++) {
    mb.addHreg(DTSU666Regs[i].address,0,DTSU666Regs[i].type == REG_WORD ? 1 : 2);
    setReg(DTSU666Regs[i].address,DTSU666Regs[i].defval);
  }

  // are we a master or slave?
  if (_slaveid == 0) {
    mb.master();
    Serial.println(F("DTSU is a master "));
  } else {
    Serial.print(F("DTSU is a slave with Id ")) ; Serial.println(_slaveid);
    mb.slave(_slaveid);

    // register a callback, use a lambda
    mb.onRequest( 
      [this] (Modbus::FunctionCode fc, const Modbus::RequestData data) {
        //Serial.printf("PRE Function for slave %d: %02X\n", _slaveid, fc);
      
        if (fc == Modbus::FC_READ_REGS ) {
          Serial.printf("Reading %d registers at 0x%0x (slaveId %d)\n",data.regCount,data.reg.address,this->_slaveid);
          //this->printRegs(data.reg.address,data.regCount);
          return Modbus::EX_SUCCESS;
        } 
        Serial.printf("Function 0x%02x not supported \n",fc);
        return Modbus::EX_ILLEGAL_FUNCTION;
      }) ;
  }
}

// copy data from one meter to another 
void DTSU666::copyTo(DTSU666 & dest) {
  word address;
  //word * index;

  for (size_t i=0; i<NUM_DEFS-1; i++) {
    address = DTSU666Regs[i].address;
    //index = rawIndex(address);
    dest.mb.Hreg(address,mb.Hreg(address));
    if ( DTSU666Regs[i].type == REG_FLOAT) {
      dest.mb.Hreg(address+1, mb.Hreg(address+1));
    }
  }
}
