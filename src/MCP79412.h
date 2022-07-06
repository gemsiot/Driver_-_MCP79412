/******************************************************************************
MCP79412.cpp
A simplified library for the MCP79412, focused on data logger applications, specifically use on the Kestrel data logger
Bobby Schulz @ GEMS Sensing
6/30/2022

The MCP79412 is a low cost RTC with integrated EEPROM and UUID. This chip allows for time to be kept
over long periods of time, and waking up a logger device when required to take measurments or tend to sensors

"That's not fair. That's not fair at all. There was time now. There was, was all the time I needed..."
-Henery Bemis

Distributed as-is; no warranty is given.
******************************************************************************/

#ifndef MCP79412_h
#define MCP79412_h

#include <Particle.h>
#include <time.h>
#include <stdlib.h>
// #include "Arduino.h"
// #include <cstdint>
// #ifdef ARDUINO 
// # define NO_CSTDINT 1  // AVR arduino has no <cstdint>; but we're coding to portable C++. So substitute.
// #endif

// // unless we know otherwise, use the compiler's <cstdint>
// #ifndef NO_CSTDINT
// # include <cstdint>
// #else
// // no <cstdint> -- make sure std:: contains the things we need.
// # include <stdint.h>

// namespace std {
//   using ::int8_t;             
//   using ::uint8_t;            
                     
//   using ::int16_t;            
//   using ::uint16_t;           
                     
//   using ::int32_t;            
//   using ::uint32_t;           
// }

// #endif

// enum class AlarmRegs : uint8_t //Define time write/read registers
// {
// 	Seconds = 0x0A,
// 	Minutes = 0x0B,
// 	Hours = 0x0C,
// 	WeekDay = 0x0D,
// 	Date = 0x0E,
// 	Month = 0x0F,
// };




class MCP79412
{
    const uint32_t NONREAL_TIME = 0x500101F5; ///<RTC has been set to non-real time (day or month or year read as zero)
    const uint32_t ANCIENT_TIME = 0x500201F5; ///<RTC has been set to time before start of 2000
    constexpr static int MAX_NUM_ERRORS = 10; ///<Maximum number of errors to log before overwriting previous errors in buffer
	public:
		enum class Format: int
		{
			Scientific = 0,
			Civilian = 1,
			US = 2,
			ISO_8601 = 3,
			Stardate = 1701
		};

		enum class Mode: int
		{
			Normal = 0,
			Inverted = 1
		};

		struct Timestamp {
			uint16_t year;  // e.g. 2020
			uint8_t  month; // 1-12
			uint8_t  mday;  // Day of the month, 1-31
			uint8_t  wday;  // Day of the week, 1-7
			uint8_t  hour;  // 0-23
			uint8_t  min;   // 0-59
			uint8_t  sec;   // 0-59
		};

		MCP79412();
		int begin(bool UseExtOsc = false);
		int setTime(int Year, int Month, int Day, int DoW, int Hour, int Min, int Sec);
		int setTime(int Year, int Month, int Day, int Hour, int Min, int Sec);
		Timestamp getRawTime();
		String getTime(Format mode = Format::Scientific); //Default to scientifc
		unsigned long getTimeUnix(); 
		// float GetTemp();
		int setMode(Mode Val); 
		int getValue(int n);
		int setAlarm(unsigned int Seconds, bool AlarmNum = 0); //Default to ALM0
		int setMinuteAlarm(unsigned int Offset, bool AlarmVal = 0); //Default to ALM0
		int setHourAlarm(unsigned int Offset, bool AlarmVal = 0); //Default to ALM0
		int setDayAlarm(unsigned int Offset, bool AlarmVal = 0); //Default to ALM0
		int enableAlarm(bool State = true, bool AlarmVal = 0); //Default to ALM0, enable
		int clearAlarm(bool AlarmVal = 0); //Default to ALM0
		bool readAlarm(bool AlarmVal = 0); //Default to ALM0

		uint8_t readByte(int Reg); //DEBUG! Make private

        uint8_t getErrorsArray(uint32_t errors[]);
        int throwError(uint32_t error);
        uint32_t errors[MAX_NUM_ERRORS] = {0};
		uint8_t numErrors = 0; //Used to track the index of errors array
		

	private:
		bool startOsc();
		
		int writeByte(int Reg, uint8_t Val);
		bool readBit(int Reg, uint8_t Pos);
		int setBit(int Reg, uint8_t Pos);
		int clearBit(int Reg, uint8_t Pos);
		time_t timegm(struct tm *tm); //Portable implementation
		const int ADR = 0x6F; //Address of MCP7940 (non-variable)
		int Time_Date[6]; //Store date time values of integers 

		const uint8_t Control = 0x07;

        

};

#endif