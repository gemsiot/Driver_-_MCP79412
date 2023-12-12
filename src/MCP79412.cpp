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

© 2023 Regents of the University of Minnesota. All rights reserved.
******************************************************************************/

// #include "Arduino.h"
#include "MCP79412.h"
#include <Wire.h>

enum Regs : uint8_t //Define time write/read registers
{
	Seconds = 0x00,
	Minutes = 0x01,
	Hours = 0x02,
	WeekDay = 0x03,
	Date = 0x04,
	Month = 0x05,
	Year = 0x06,
};

const uint8_t AlarmOffset = 0x07; //Offset between ALM0 and ALM1 regs
const uint8_t BlockOffset = 0x0A; //Offset from time regs to ALM regs

// #define RETRO_ON_MANUAL //Debug include 


MCP79412::MCP79412()
{

}

/**
 * Initializes the system, starts up I2C and turns on the oscilator and sets up to use battery backup 
 *
 * @return the I2C status value (if any error occours) or if oscilator does not start properly 
 */
int MCP79412::begin(bool UseExtOsc)
{
	#if defined(ARDUINO) && ARDUINO >= 100 
		Wire.begin();
	#elif defined(PARTICLE)
		if(!Wire.isEnabled()) Wire.begin(); //Only initialize I2C if not done already //INCLUDE FOR USE WITH PARTICLE 
	#endif

	Timestamp initTime = getRawTime();
    if(initTime.year < 2022) throwError(ANCIENT_TIME);
    if(initTime.year == 2000 || initTime.month == 0 || initTime.mday == 0) {
        throwError(NONREAL_TIME); 
        setTime(2001, 1, 1, 0, 0, 0); //If the current time is less than 00:00:00 2000/1/1 (if month/day is set to zero in correctly), set time to default time so alarms work 
    }
	
	// Wire.beginTransmission(ADR);
	// Wire.write(0x0E); //Write values to Control reg
	// Wire.write(0x24); //Start oscilator, turn off BBSQW, Turn off alarms, turn on convert
	// return Wire.endTransmission(); //return result of begin, reading is optional
	if(readBit(Regs::WeekDay, 3) == 0) throwError(RTC_POWER_LOSS); //If this bit is set back to 0, all power to the RTC must have been lost
	setBit(Regs::WeekDay, 3); //Turn backup battery enable

	writeByte(Control, 0x00); //Clear control reg //DEBUG! Prevent issue where square wave is erroniously enabled on multi-purpose pin
	writeByte(Control + 1, 0x00); //Clear trim register //DEBUG! Prevent where trim value is erroniously set
	if(!UseExtOsc) {
		bool OscError = startOsc();
		return OscError; //Return oscilator status
	}
	else {
		clearBit(0, 7); //Clear bit 7 of reg 0 (turn off ST bit)
		int Error = setBit(Control, 3); //Turn on external oscilator input
		if(Error == 0) return 1; //Return pass if I2C comunication is good, FIX??
		else return 0; //Return fail if any other I2C error code 
	}
	
	// Serial.print("Oscillator State:"); //DEBUG!
	// Serial.println(OscError); //DEBUG! 
	// Serial.print("Reg Sates:"); //DEBUG!
	// Serial.print(ReadByte(Control), HEX); //DEBUG!
	// Serial.print(","); //DEBUG!
	// // Serial.print(ReadByte(TimeRegs::Seconds), HEX); //DEBUG!
	// Serial.print(ReadByte(0x0A), HEX); //DEBUG!
	// Serial.print(","); //DEBUG!
	// // Serial.println(ReadByte(TimeRegs::WeekDay), HEX); //DEBUG!
	// Serial.println(ReadByte(0x0D), HEX); //DEBUG!
}

/**
 * Set the time of the device
 *
 * @param Year, the current year, either in 2 digit form, or 4 digit, automatically adjusts (until year 2100)
 * @param Month, the current month (1~12)
 * @param Day, the current day of the month(1~31)
 * @param DoW, the day of the week, staring on Monday (1~7)
 * @param Hour, the current hour (0~24)
 * @param Min, the current minute (0~60)
 * @param Sec, the current second (0~60)
 * @return the I2C status value (if any error occours)
 */
int MCP79412::setTime(int Year, int Month, int Day, int DoW, int Hour, int Min, int Sec)
{
	bool stVal = readBit(0x00, 7); //Read the ST value from the seconds register to check for the current value
	int Error = 0; //Default to no error 
	if(Year > 999) {
		Year = Year - 2000; //FIX! Add compnesation for centry 
	}
	int TimeDate [7]={Sec,Min,Hour,DoW,Day,Month,Year};
	for(int i=0; i<=6;i++){
		if(i == 3) {
			uint8_t DoW_Temp = readByte(Regs::WeekDay); //Read in current value
			DoW_Temp = DoW_Temp & 0xF8; //Clear lower 3 bits (day of week portion of register)
			DoW_Temp = DoW_Temp | (DoW & 0x07); //Set lower 3 bits from DoW input
			TimeDate[i] = DoW_Temp; //Return value  
		}
		else { //Otherwise write method for other regs
			int b = TimeDate[i]/10;
			int a = TimeDate[i]-b*10;
			// if(i == 2){
			// 	if (b==2)
			// 		b=B00000010;
			// 	else if (b==1)
			// 		b=B00000001;
			// }	
			TimeDate[i]= a+(b<<4);
			if(i == 0 && stVal) TimeDate[i] = TimeDate[i] | 0x80; //Set ST bit to keep oscilator running if previously set

			//FIX! Test for leap year and set LPYR bit
		}
		
		#if defined(RETRO_ON_MANUAL)
			Serial.print(i);
			Serial.print(":");
			Serial.println(TimeDate[i], HEX);
		#endif
		int Val = writeByte(i, TimeDate[i]); //Grab error value
		if(Val != 0) Error = Val; //Assign error value for any write error encountered 
		// Wire.beginTransmission(ADR);
		// Wire.write(i); //Write values starting at reg 0x00
		// Wire.write(TimeDate[i]); //Write time date values into regs
		// Wire.endTransmission(); //return result of begin, reading is optional
  }

  //Read back time to test result of write??
  return Error; //Return write error value
}

/**
 * Set the time of the device (DoW ommited)
 *
 * @param Year, the current year, either in 2 digit form, or 4 digit, automatically adjusts (until year 2100)
 * @param Month, the current month (1~12)
 * @param Day, the current day of the month(1~31)
 * @param DoW, the day of the week, staring on Monday (1~7)
 * @param Hour, the current hour (0~24)
 * @param Min, the current minute (0~60)
 * @param Sec, the current second (0~60)
 * @return the I2C status value (if any error occours)
 */
int MCP79412::setTime(int Year, int Month, int Day, int Hour, int Min, int Sec)
{
	return setTime(Year, Month, Day, 0, Hour, Min, Sec); //Pass to full funciton, force WeekDay to zero 
}

MCP79412::Timestamp MCP79412::getRawTime() {
	int TimeDate [7]; //second,minute,hour,weekday,monthday,month,year
	Wire.beginTransmission(ADR); //Ask 1 byte of data
	Wire.write(0x00); //Read values starting at reg 0x00
	Wire.endTransmission();
	Wire.requestFrom(ADR, 7);	//WAIT FOR DATA BACK FIX!!
	Timestamp ts;

	for(int i=0; i<=6;i++){
		int n = Wire.read(); //Read value of reg

		//Process results
		int a=n & B00001111;
		int high = (n >> 4) & B1111;
		switch (i) {
		case 0: // seconds
			// break;
		case 1: // minutes
			a += (high & B0111) * 10;
			break;
		case 2: // hour (24-hour time)
			// break;
		case 4: // day of month
			a += (high & B0011) * 10;
			break;
		case 3: // day of week
			a &= B0111;
			break;
		case 5: // month of year
			a += (high & B0001) * 10;
			break;
		default: // year
			a += high * 10;
			break;
		}
		TimeDate[i] = a;
	}

	ts.year = (uint16_t)(TimeDate[6] + 2000);
	ts.month = (uint8_t)TimeDate[5];
	ts.mday = (uint8_t)TimeDate[4];
	ts.wday = (uint8_t)TimeDate[3];
	ts.hour = (uint8_t)TimeDate[2];
	ts.min = (uint8_t)TimeDate[1];
	ts.sec = (uint8_t)TimeDate[0];

	return ts;
	// return {
	// 	.year  = (uint16_t)(TimeDate[6] + 2000),
	// 	.month = (uint8_t)TimeDate[5],
	// 	.mday  = (uint8_t)TimeDate[4],
	// 	.wday  = (uint8_t)TimeDate[3],
	// 	.hour  = (uint8_t)TimeDate[2],
	// 	.min   = (uint8_t)TimeDate[1],
	// 	.sec   = (uint8_t)TimeDate[0]
	// };
}

/**
 * Return current time from device, formatted
 *
 * @param Mode, used to set which value is returned 
 * @return String of current time/date in the requested format 
 */
String MCP79412::getTime(Format mode)
{
	Timestamp t = getRawTime();
	char str[32];

	Time_Date[5] = t.sec; //FIX!
	Time_Date[4] = t.min;
	Time_Date[3] = t.hour;
	Time_Date[2] = t.mday;
	Time_Date[1] = t.month;
	Time_Date[0] = t.year;
	//Format raw results into appropriate string
	switch (mode) {
	case Format::Scientific: // Return in order Year, Month, Day, Hour, Minute, Second (Scientific Style)
		sprintf(str, "%04d/%02d/%02d %02d:%02d:%02d", t.year, t.month, t.mday, t.hour, t.min, t.sec);
		break;
	case Format::Civilian: // Return in order Month, Day, Year, Hour, Minute, Second (US Civilian Style)
		sprintf(str, "%02d/%02d/%04d %02d:%02d:%02d", t.month, t.mday, t.year, t.hour, t.min, t.sec);
		break;
	case Format::US: { // Return in order Month, Day, Year, Hour (12 hour), Minute, Second
			uint8_t twelveHour = t.hour % 12;
			if (twelveHour == 0) twelveHour = 12;
			sprintf(str, "%02d/%02d/%04d %02d:%02d:%02d %cM", t.month, t.mday, t.year, twelveHour, t.min, t.sec, t.hour >= 12 ? 'P' : 'A');
			break;
		}
	case Format::ISO_8601: // Return in ISO 8601 standard (UTC)
		// FIX! Hard code for UTC time, allow for a fix??
		sprintf(str, "%04d-%02d-%02dT%02d:%02d:%02dZ", t.year, t.month, t.mday, t.hour, t.min, t.sec);
		break;
	case Format::Stardate: { // Returns in order Year, Day (of year), Hour, Minute, Second (Stardate)
			int DayOfYear = t.mday;
			int MonthDay[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
			if (t.year % 4 == 0) MonthDay[2] = 29;
			for(int m = 1; m < t.month; m++) {
				DayOfYear += MonthDay[m];
			}
			sprintf(str, "%04d.%d %02d.%02d.%02d", t.year, DayOfYear, t.hour, t.min, t.sec);
			break;
		}
	default:
		return "Invalid Input";
	}
	return str;
}

/**
 * Return current time of the device, Unix time
 *
 * @return unsigned long of current Unix timestamp
 */
time_t MCP79412::getTimeUnix()
{
	// Timestamp t = getRawTime(); //Get updated time
	// struct tm timeinfo = {0}; //Create struct in C++ time land
	// time_t rawtime;
	// time ( &rawtime );
	// timeinfo = *localtime ( &rawtime );
	//Copy the time to C++ time land

	int TimeDate [7] = {0,0,0,0,0,0,0}; //second,minute,hour,weekday,monthday,month,year
	Wire.beginTransmission(ADR); //Ask 1 byte of data
	Wire.write(0x00); //Read values starting at reg 0x00
	Wire.endTransmission();
	Wire.requestFrom(ADR, 7);	//WAIT FOR DATA BACK FIX!!
	// Timestamp ts;

	for(int i=0; i<=6;i++){
		int n = Wire.read(); //Read value of reg

		//Process results
		int a=n & B00001111;
		int high = (n >> 4) & B1111;
		switch (i) {
		case 0: // seconds
			// break;
		case 1: // minutes
			a += (high & B0111) * 10;
			break;
		case 2: // hour (24-hour time)
			// break;
		case 4: // day of month
			a += (high & B0011) * 10;
			break;
		case 3: // day of week
			a &= B0111;
			break;
		case 5: // month of year
			a += (high & B0001) * 10;
			break;
		default: // year
			a += high * 10;
			break;
		}
		TimeDate[i] = a;
	}
	// timeinfo.tm_year = TimeDate[6] + 100; //Years since 1900, not 2000
	// timeinfo.tm_mon = TimeDate[5] - 1; //Months since january
	// timeinfo.tm_mday = TimeDate[4];
	// timeinfo.tm_hour = TimeDate[2];
	// timeinfo.tm_min = TimeDate[1];
	// timeinfo.tm_sec = TimeDate[0];

	// timeinfo.tm_year = t.year - 1900; //Years since 1900
	// timeinfo.tm_mon = t.month - 1; //Months since january
	// timeinfo.tm_mday = t.mday;
	// timeinfo.tm_hour = t.hour;
	// timeinfo.tm_min = t.min;
	// timeinfo.tm_sec = t.sec;
	// t.~Timestamp(); //Call destructor

	// time_t rawTime = timegm(&timeinfo); //Convert struct to unix time
	// return rawTime;
	// return 0xDEADBEEF; //DEBUG!
	return cstToUnix(TimeDate[6] + 2000, TimeDate[5], TimeDate[4], TimeDate[2], TimeDate[1], TimeDate[0]);
	// return 0; //Return dummy value
}

/**
 * Return specific time date value to not be forced to parse string 
 *
 * @param n, which value to be returned (0:Year, 1:Month, 2:Day, 3:Hour, 4:Minute, 5:Second)
 * @return int, the desired time date value in numerical form 
 */
int MCP79412::getValue(int n)	// n = 0:Year, 1:Month, 2:Day, 3:Hour, 4:Minute, 5:Second
{
	getTime(MCP79412::Format::ISO_8601); //Update time
	return Time_Date[n]; //Return desired value 
}

/**
 * Setup the output mode of the system, using normal or inverted polarity. See page 27 of MCP79412 (Table 5-10 and Table 5-9)
 *
 * @param Val, which selects either Normal or Inverted polarity
 * @return int, the result of the I2C write 
 */
int MCP79412::setMode(Mode Val) 
{
	if(Val == Mode::Normal) return clearBit(Regs::WeekDay + BlockOffset, 7); //Clear bit 7 of reg 0x0D (will be mirrored by hardware in reg 0x14)
	if(Val == Mode::Inverted) return setBit(Regs::WeekDay + BlockOffset, 7); //Set bit 7 of reg 0x14 (will be mirrored by hardware in reg 0x14)
	else return -1; //Return unknown input error 
}

/**
 * Set alarm for a given number of seconds from current time 
 *
 * @param Delta, how many seconds from now the alarm should be set for 
 * @param bool, AlarmVal, determine which alarm to be set
 * @return int, the I2C status value (if any error occours)
 */
int MCP79412::setAlarm(unsigned int Delta, bool AlarmNum) //Set alarm from current time to x seconds from current time 
{ 
	//DEFINE LIMITS FOR FUNCTION!!
	uint8_t RegOffset = BlockOffset; 
	if(AlarmNum == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1

	// ClearBit(Control, 4 + AlarmNum); //Turn off desired alarm bit (ALM0 or ALM1)
	enableAlarm(false, AlarmNum); //Disable desired alarm

	// if(Seconds == 60) { //Will trigger every minute, on the minute 
	// 	// uint8_t AlarmMask = 0x07; //nibble for A1Mx values

	// 	// // Wire.beginTransmission(ADR);
	// 	// // Wire.write(0x0E); //Write values to control reg
	// 	// // Wire.write(0x40); //Turn on 1 Hz square wave
	// 	// // Wire.endTransmission(); 

	// 	// Wire.beginTransmission(ADR);
	// 	// Wire.write(0x0E); //Write values to control reg
	// 	// Wire.write(0x06); //Turn on INTCN and Alarm 2
	// 	// Wire.endTransmission(); 

	// 	// //DEBUG!
	// 	// Wire.beginTransmission(ADR);
	// 	// Wire.write(0x0F); //Write values to control reg
	// 	// Wire.write(0x00); //Clear any alarm flags, set oscilator to run
	// 	// Wire.endTransmission(); 

	// 	// for(int i=0; i < 3;i++){
	// 	// 	Wire.beginTransmission(ADR);
	// 	// 	Wire.write(0x0B + i); //Write values starting at reg 0x0B
	// 	// 	// Wire.write(((AlarmMask & (1 << i)) << 8)); //Write time date values into regs
	// 	// 	Wire.write(0x80); 
	// 	// 	Wire.endTransmission(); //return result of begin, reading is optional
	// 	// }
	// 	ClearBit(Control, 4 + AlarmVal); //Turn off desired alarm bit (ALM0 or ALM1)
	// 	uint8_t AlarmRegTemp = ReadByte(AlarmRegs::WeekDay + RegOffset); //Read in week day alarm reg for other values in reg
	// 	AlarmRegTemp = AlarmRegTemp & 0x8F; //Clear mask bits, match only seconds
	// 	WriteByte(AlarmRegs::WeekDay + RegOffset, AlarmRegTemp); //Write back config reg
	// 	WriteByte(AlarmRegs::Seconds, 0x00); //Write for alarm to trigger at 0 seconds 
	// 	SetBit(Control, 4 + AlarmVal); //Turn desired alarm (ALM0 or ALM1) back on
	// }

	// else {
	//Currently can not set timer for more than 24 hours
	// uint8_t AlarmMask = 0x08; //nibble for A1Mx values
	// uint8_t DY = 0; //DY/DT value 
	// GetTime(MCP79412::Format::ISO_8601);
	Timestamp t = getRawTime();

	Time_Date[5] = t.sec; //FIX!
	Time_Date[4] = t.min;
	Time_Date[3] = t.hour;
	Time_Date[2] = t.mday;
	Time_Date[1] = t.month;
	Time_Date[0] = t.year;

	int AlarmTime[7] = {Time_Date[5], Time_Date[4], Time_Date[3], t.wday, Time_Date[2], Time_Date[1], Time_Date[0]};
	// int AlarmVal[7] = {Delta % 60, ((Delta - (Delta % 60))/60) % 60, ((Delta - (Delta % 3600))/3600) % 24, ((Delta - (Delta % 3600))/3600) % 24, ((Delta - (Delta % 86400))/86400), 0, 0};  //Remove unused elements?? FIX!
	int AlarmVal[7] = {Delta % 60, ((Delta - (Delta % 60))/60) % 60, ((Delta - (Delta % 3600))/3600) % 24, ((Delta - (Delta % 86400))/86400), ((Delta - (Delta % 86400))/86400), 0, 0};  //Remove unused elements?? FIX!
	int CarryIn = 0; //Carry value
	int CarryOut = 0; 
	int MonthDay[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};  //Use??
	//Check if the next year is a leap year 
	// bool LeapYear = false; //Flag to check if it is a leap year
	// if(AlarmTime[6] % 400 == 0) LeapYear = true; //If year is divisable by 400, is leap year, set leap year flag
	// else if((AlarmTime[6] % 4 == 0) && (AlarmTime[6] % 100 != 0)) LeapYear = true; //Otherwise, if IS dividsable by 4, but NOT multiple of 100, is leap year, set leap year flag

	

	if(readBit(Regs::Month, 5)) MonthDay[2] = 29; //If LPYR is set, then adjust number of days in Febuary //FIX! Check if this is correct in terms of setting an alarm into next year 
	// if(LeapYear) {
	// 	MonthDay[2] = 29; //Correct days in Febuary
	// }

	// Wire.beginTransmission(ADR);
	// Wire.write(0x0E); //Write values to control reg
	// Wire.write(0x05); //Turn on INTCN and Alarm 1
	// Wire.endTransmission(); 
	

	//Calc seconds
	if(AlarmTime[0] + AlarmVal[0] >= 60) CarryOut = 1;
	AlarmTime[0] = (AlarmTime[0] + AlarmVal[0]) % 60;
	CarryIn = CarryOut; //Copy over prevous carry

	//Calc minutes
	if(AlarmTime[1] + AlarmVal[1] + CarryIn >= 60) CarryOut = 1;
	else CarryOut = 0;
	AlarmTime[1] = (AlarmTime[1] + AlarmVal[1] + CarryIn) % 60;
	CarryIn = CarryOut; //Copy over prevous carry

	//Calc hours
	if(AlarmTime[2] + AlarmVal[2] + CarryIn >= 24) CarryOut = 1; //OUT OF RANGE??
	else CarryOut = 0;
	AlarmTime[2] = (AlarmTime[2] + AlarmVal[2] + CarryIn) % 24;
	CarryIn = CarryOut; //Copy over prevous carry

	// //Calc DoW
	// if(AlarmTime[3] + AlarmVal[3] + CarryIn >= 7) CarryOut = 1; //OUT OF RANGE??
	// else CarryOut = 0;
	// AlarmTime[3] = (AlarmTime[3] + AlarmVal[3] + CarryIn) % 7;
	// CarryIn = CarryOut; //Copy over prevous carry

	//Calc DoW
	// if(AlarmTime[3] + AlarmVal[3] + CarryIn >= 7) CarryOut = 1; //OUT OF RANGE??
	// else CarryOut = 0;
	AlarmTime[3] = ((AlarmTime[3] + AlarmVal[3] + CarryIn - 1) % 7) + 1; //Calc DoW change, no need to carry
	// CarryIn = CarryOut; //Copy over prevous carry

	//Calc days 
	if(AlarmTime[4] + AlarmVal[4] + CarryIn > MonthDay[AlarmTime[5]]) CarryOut = 1;  //Carry out if result pushes you beyond current month 
	else CarryOut = 0;
	AlarmTime[4] = (AlarmTime[4] + AlarmVal[4] + CarryIn) % (MonthDay[AlarmTime[5]] + 1);
	if(AlarmTime[4] == 0) AlarmTime[4] = 1; //FIX! Find more elegant way to do this

	//Calc Months
	AlarmTime[5] = ((AlarmTime[5] + CarryOut - 1) % 12) + 1; //If needed, push into next month, if this rolls over into the next year, simply roll over 
	// AlarmTime[5] = ((AlarmTime[5] + CarryOut) % 13) + int((AlarmTime[5] + CarryOut)/13); //If needed, push into next month, if this rolls over into the next year, simply roll over 

	// int q = 5; //DEBUG!
	// for(int i = 0; i < 7; i++) { //DEBUG!
	// 	if(i != 3) {
	// 		Serial.print(Time_Date[q]); 
	// 		q--;
	// 	}
	// 	if(i == 3) Serial.print(t.wday);
	// 	Serial.print('\t');
	// 	Serial.println(AlarmTime[i]);
	// 	// if(i != 3) q--; //Do no decrement for DoW
	// 	// q--;
	// }


	//ADD FAILURE NOTIFICATION FOR OUT OF RANGE??
	for(int i=0; i<=5;i++){
		if(i == 3) {
			uint8_t DoW_Temp = readByte(Regs::WeekDay + RegOffset); //Read in current value
			DoW_Temp = DoW_Temp & 0xF8; //Clear lower 3 bits (day of week portion of register)
			DoW_Temp = DoW_Temp | 0x70 | (AlarmTime[i] & 0x07); //Set lower 3 bits from DoW input, set MSK bits to configure for full match
			AlarmTime[i] = DoW_Temp; //Return value  
		}
		else { //Otherwise write method for other regs
			int b = AlarmTime[i]/10;
			int a = AlarmTime[i]-b*10;
			if(i == 2){
				if (b==2)
					b=B00000010;
				else if (b==1)
					b=B00000001;
			}	
			AlarmTime[i]= a+(b<<4);
			// if(i == 0) AlarmTime[i] = AlarmTime[i] | 0x80; //Set ST bit to keep oscilator running 

			//FIX! Test for leap year and set LPYR bit
		}
		writeByte(Regs::Seconds + RegOffset + i, AlarmTime[i]); //Write AlarmTime back to specified alarm register 
		// Serial.println(AlarmTime[i], HEX); //DEBUG!
		// delay(1);
		// delayMicroseconds(150); //DEBUG!
	}


	// int Offset = 0;
	// for(int i=0; i<=6;i++){
	// 	if(i==3) i++;
	// 	int b= AlarmTime[i]/10;
	// 	int a= AlarmTime[i]-b*10;
	// 	if(i==2){
	// 		if (b==2)
	// 			b=B00000010;
	// 		else if (b==1)
	// 			b=B00000001;
	// 	}	
	// 	AlarmTime[i]= a+(b<<4);
		  
	// 	Wire.beginTransmission(ADR);
	// 	Wire.write(0x07 + Offset); //Write values starting at reg 0x07
	// 	Wire.write(AlarmTime[i] | ((AlarmMask & (1 << Offset)) << 8)); //Write time date values into regs
	// 	Wire.endTransmission(); //return result of begin, reading is optional
	// 	Offset++;
	// }
	// }
	// }

	//FIX! Should the alarm be turned on before status is cleared, or vise-versa??
	// SetBit(Control, 4 + AlarmNum); //Turn desired alarm (ALM0 or ALM1) back on 
	int Error = enableAlarm(true, AlarmNum); //Re-enable alarm
	clearAlarm(AlarmNum); //Clear any existing alarm
	return Error; //Return the error from enabling the alarm

}

/**
 * Set alarm for to trigger once a minute at a given second period offset
 *
 * @param Offset, how many seconds to offset on the minute alarm (if set to 30, the alarm will trigger every minute on the half minute)
 * @param bool, AlarmVal, determine which alarm to be set
 * @return int, the I2C status value (if any error occours)
 */
int MCP79412::setMinuteAlarm(unsigned int Offset, bool AlarmVal) //Set alarm from current time to x seconds from current time 
{ 
	uint8_t RegOffset = BlockOffset; 
	if(AlarmVal == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1

	// ClearBit(Control, 4 + AlarmVal); //Turn off desired alarm bit (ALM0 or ALM1)
	enableAlarm(false, AlarmVal); //Disable desired alarm
	uint8_t AlarmRegTemp = readByte(Regs::WeekDay + RegOffset); //Read in week day alarm reg for other values in reg
	AlarmRegTemp = AlarmRegTemp & 0x8F; //Clear mask bits, match only seconds
	writeByte(Regs::WeekDay + RegOffset, AlarmRegTemp); //Write back config reg

	uint8_t SecondsOffset = (Offset % 0x0A) | (uint8_t(floor(Offset/10)) << 4); //Convert offset to BCD
	writeByte(Regs::Seconds + RegOffset, SecondsOffset); //Write for alarm to trigger at offset period  
	// SetBit(Control, 4 + AlarmVal); //Turn desired alarm (ALM0 or ALM1) back on
	int Error = enableAlarm(true, AlarmVal); //Re-enable alarm
	clearAlarm(AlarmVal); //Clear any existing alarm
	return Error; //Return the error from enabling the alarm
}

/**
 * Set alarm for to trigger once a hour at a given minutes period offset
 *
 * @param Offset, how many minutes to offset on the minute alarm (if set to 30, the alarm will trigger every hour on the half hour)
 * @param bool, AlarmVal, determine which alarm to be set
 * @return int, the I2C status value (if any error occours)
 */
int MCP79412::setHourAlarm(unsigned int Offset, bool AlarmVal) //Set alarm from current time to x seconds from current time 
{ 
	uint8_t RegOffset = BlockOffset; 
	if(AlarmVal == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1

	// ClearBit(Control, 4 + AlarmVal); //Turn off desired alarm bit (ALM0 or ALM1)
	enableAlarm(false, AlarmVal); //Disable desired alarm
	uint8_t AlarmRegTemp = readByte(Regs::WeekDay + RegOffset); //Read in week day alarm reg for other values in reg
	AlarmRegTemp = AlarmRegTemp & 0x8F; //Clear mask bits
	AlarmRegTemp = AlarmRegTemp | 0x10; //Set ALMxMSK0, match only minutes
	writeByte(Regs::WeekDay + RegOffset, AlarmRegTemp); //Write back config reg

	uint8_t MinuteOffset = (Offset % 0x0A) | (uint8_t(floor(Offset/10)) << 4); //Convert offset to BCD
	writeByte(Regs::Minutes + RegOffset, MinuteOffset); //Write for alarm to trigger at offset period  
	// SetBit(Control, 4 + AlarmVal); //Turn desired alarm (ALM0 or ALM1) back on
	int Error = enableAlarm(true, AlarmVal); //Re-enable alarm
	clearAlarm(AlarmVal); //Clear any existing alarm
	return Error; //Return the error from enabling the alarm
}

/**
 * Set alarm for to trigger once a day at a given hour period offset
 *
 * @param Offset, how many seconds to offset on the minute alarm (if set to 6, the alarm will trigger every day at 6AM)
 * @param bool, AlarmVal, determine which alarm to be set
 * @return int, the I2C status value (if any error occours)
 */
int MCP79412::setDayAlarm(unsigned int Offset, bool AlarmVal) //Set alarm from current time to x seconds from current time 
{ 
	uint8_t RegOffset = BlockOffset; 
	if(AlarmVal == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1

	// ClearBit(Control, 4 + AlarmVal); //Turn off desired alarm bit (ALM0 or ALM1)
	enableAlarm(false, AlarmVal); //Disable desired alarm
	uint8_t AlarmRegTemp = readByte(Regs::WeekDay + RegOffset); //Read in week day alarm reg for other values in reg
	AlarmRegTemp = AlarmRegTemp & 0x8F; //Clear mask bits
	AlarmRegTemp = AlarmRegTemp | 0x20; //Set ALMxMSK1, match only hours
	writeByte(Regs::WeekDay + RegOffset, AlarmRegTemp); //Write back config reg

	uint8_t HourOffset = (Offset % 0x0A) | (uint8_t(floor(Offset/10)) << 4); //Convert offset to BCD 
	writeByte(Regs::Hours + RegOffset, HourOffset); //Write for alarm to trigger at offset period  
	// SetBit(Control, 4 + AlarmVal); //Turn desired alarm (ALM0 or ALM1) back on
	int Error = enableAlarm(true, AlarmVal); //Re-enable alarm
	clearAlarm(AlarmVal); //Clear any existing alarm
	return Error; //Return the error from enabling the alarm
}

/**
 * Set the register bit to clear any current alarm flags, effectively disables alarm until SetAlarm() is called again
 *
 * @param bool, AlarmVal, determine which alarm to be set
 * @return int, the I2C status value (if any error occours)
 */
int MCP79412::clearAlarm(bool AlarmVal) {  //Clear registers to stop alarm, must call SetAlarm again to get it to turn on again
	// Wire.beginTransmission(ADR);
	// Wire.write(0x0F); //Write values to status reg
	// Wire.write(0x00); //Clear all flags
	// Wire.endTransmission(); //return result of begin, reading is optional
	uint8_t RegOffset = BlockOffset; 
	if(AlarmVal == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1
	return clearBit(Regs::WeekDay + RegOffset, 3); //Clear interrupt flag bit of the desired alarm register 
}

/**
 * Turns on the desired alarm to allow them to generate interrupts 
 *
 * @param bool, State, if the alarm should be enabled or disabled (enables by default)
 * @param bool, AlarmVal, determine which alarm to be set (alarm 0 by default)
 * @return int, the I2C status value (if any error occours)
 */
int MCP79412::enableAlarm(bool State, bool AlarmVal) {  //Clear registers to stop alarm, must call SetAlarm again to get it to turn on again
	uint8_t RegOffset = BlockOffset; 
	clearBit(Control, 6); //If an alarm is in use, disable square wave output //DEBUG! 
	if(AlarmVal == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1
	if(State) return setBit(Control, 4 + AlarmVal); //Set enable bit of desired alarm
	else if(!State) return clearBit(Control, 4 + AlarmVal); //Clear enable bit of desired alarm
	else return -1; //Return on unknown input, should never occour 
}

/**
 * Read the value of the given alarm flags, which are set when the alarm is triggere d 
 *
 * @param bool, AlarmVal, determine which alarm to be set
 * @return bool, the state of the alarm flag
 */
bool MCP79412::readAlarm(bool AlarmVal) {  //Clear registers to stop alarm, must call SetAlarm again to get it to turn on again
	// Wire.beginTransmission(ADR);
	// Wire.write(0x0F); //Write values to status reg
	// Wire.write(0x00); //Clear all flags
	// Wire.endTransmission(); //return result of begin, reading is optional
	uint8_t RegOffset = BlockOffset; 
	if(AlarmVal == 1) RegOffset = AlarmOffset + BlockOffset; //Set offset if using ALM1
	return readBit(Regs::WeekDay + RegOffset, 3); //Read interrupt flag bit of the desired alarm register 
}

/**
 * Read the UUID from the memory on the RTC and report back as string
 *
 * @return String, a '-' seperated hex encoded UUID
 */
String MCP79412::getUUIDString() {
	uint8_t val = 0; 
	String uuid = "";
	Wire.beginTransmission(ADR_EEPROM); //EEPROM address
	Wire.write(0xF0); //Begining of EUI-64 data
	int error = Wire.endTransmission();
	if(error == 0) { //Only attempt to read in if there are no errors
		Wire.requestFrom(ADR_EEPROM, 8); //EEPROM address
		for(int i = 0; i < 8; i++) {
			val = Wire.read();
			uuid = uuid + String(val, HEX); //Concatonate into full UUID
			if(i < 7) uuid = uuid + '-'; //Print formatting chracter, don't print on last pass
		}
		return uuid; //Only return UUID if read was good
	}
	else {
		throwError(RTC_EEPROM_READ_FAIL);
		return "null"; //Otherwise return null state
	}
}

/**
 * Read the UUID from the memory on the RTC and report back as number
 *
 * @return uint64_t, the 64 bit value of the UUID
 */
uint64_t MCP79412::getUUID() {
	uint8_t val = 0; 
	uint64_t uuid = 0; 
	Wire.beginTransmission(ADR_EEPROM); //EEPROM address
	Wire.write(0xF0); //Begining of EUI-64 data
	int error = Wire.endTransmission();
	if(error == 0) {
		Wire.requestFrom(ADR, 8); //EEPROM address
		for(int i = 0; i < 8; i++) {
			val = Wire.read();
			uuid = uuid | (val << (8 - i)); //Concatonate into full UUID
			// Serial.print(Val, HEX); //Print each hex byte from left to right
			if(i < 7) Serial.print('-'); //Print formatting chracter, don't print on last pass
		}
		return uuid;
	}
	else {
		throwError(RTC_EEPROM_READ_FAIL);
		return 0; //Otherwise return null state
	}
	// Serial.print("\n");
}

/**
 * Starts the crystal oscilator connected to the device (required to keep time)
 *
 * @return bool, state of oscilator at end of startup (1 = running, 0 = not running, error)
 */
bool MCP79412::startOsc() //Turn on oscilator, returs TRUE if oscilator is set properly, false otherwise 
{
	uint8_t ControlTemp = readByte(Control);
	ControlTemp = ControlTemp & 0xF7; //Clear EXTOSC bit to enable and external oscilator 
	uint8_t SecTemp = readByte(Regs::Seconds); //Read value from seconds register to use as mask
	SecTemp = SecTemp | 0x80; //Set ST bit to start oscilator
	writeByte(Control, ControlTemp); //Write back value of temp control register
	writeByte(Regs::Seconds, SecTemp); //Write back value of seconds register (for ST bit)
	delay(5); //Wait for oscilator to start 
	// Serial.println(ControlTemp, HEX); //DEBUG!
	// Serial.println(SecTemp, HEX); //DEBUG!
	// Serial.println(Control, HEX); //DEBUG!
	// Serial.println(TimeRegs::Seconds, HEX); //DEBUG!
	return readBit(Regs::WeekDay, 5); //Return the OSCRUN bit of the weekday register to test if oscilator is running
}

/**
 * Helper function, reads byte and given register location
 *
 * @param Reg, the register to read the byte from 
 * @return uint8_t, returns the desired byte
 */
uint8_t MCP79412::readByte(int Reg)
{
	Wire.beginTransmission(ADR); //Point to desired register 
	Wire.write(Reg);
	Wire.endTransmission();

	Wire.requestFrom(ADR, 1); //Ask for 1 byte from RTC
	const unsigned long Timeout = 5; 
	unsigned long LocalTime = millis();
	while(Wire.available() < 1 && (millis() - LocalTime) < Timeout); //Wait at most 5ms for byte to arrive
	if(Wire.available() >= 1) return Wire.read(); //If got byte, return value
	else return 0; //Otherwise return zero 
}

/**
 * Helper function, write value (byte) to register at given register location
 *
 * @param Reg, the location of the register to write the data to
 * @param Val, the value of the data to write to the register 
 * @return int, I2C status
 */
int MCP79412::writeByte(int Reg, uint8_t Val)
{
	Wire.beginTransmission(ADR);
	Wire.write(Reg);
	Wire.write(Val); //Write value to register
	return Wire.endTransmission(); //Return I2C status 
}

/**
 * Helper function, reads bit value from given byte and retuns this as bool
 *
 * @param Reg, the location of the register to read the data from
 * @param Pos, the position of the bit to return
 * @return bool, the value of the bit at the ith location 
 */
bool MCP79412::readBit(int Reg, uint8_t Pos)
{
	uint8_t Val = readByte(Reg);
	return (Val >> Pos) & 0x01; //Return single reguested bit
}

/**
 * Helper function, sets a bit at a given location in a register 
 *
 * @param Reg, the location of the register to set the bit in
 * @param Pos, the position of the bit to set
 * @return int, I2C status 
 */
int MCP79412::setBit(int Reg, uint8_t Pos)
{
	uint8_t ValTemp = readByte(Reg);
	ValTemp = ValTemp | (1 << Pos); //Set desired bit
	// Serial.println(ValTemp, HEX); //DEBUG!
	return writeByte(Reg, ValTemp); //Write value back in place
}

/**
 * Helper function, clears a bit at a given location in a register 
 *
 * @param Reg, the location of the register to clear the bit in
 * @param Pos, the position of the bit to clear
 * @return int, I2C status 
 */
int MCP79412::clearBit(int Reg, uint8_t Pos)
{
	uint8_t ValTemp = readByte(Reg); //Grab register
	uint8_t Mask = ~(1 << Pos); //Creat mask to clear register
	ValTemp = ValTemp & Mask; //Clear desired bit
	return writeByte(Reg, ValTemp); //Write value back
}

/**
 * Reports the current array of errors 
 * 
 * @param errors, array of errors to pass out
 * @return uint8_t, number of errors reported, Note: can exceed MAX_NUM_ERRORS, in this case an overrun has occoured 
 */
uint8_t MCP79412::getErrorsArray(uint32_t errors_[]) 
{
    for(int i = 0; i < min(MAX_NUM_ERRORS, numErrors); i++) { //Interate over used element of array without exceeding bounds
		// output = output + String(errors[i]) + ","; //Add each error code
		errors_[i] = errors[i]; //Copy over errors
        errors[i] = 0; //Clear as you go
	}
    uint8_t numErrorsCurrent = numErrors; //Store temporarily so global can be cleared
    numErrors = 0; //Clear error count once dumped 
    return numErrorsCurrent; //Return the number of values written into array
}

/**
 * Adds the specified error to the growing list of errors
 * 
 * @param errors, new error value to write add to global array
 * @return int, current total number of errors
 */
int MCP79412::throwError(uint32_t error)
{
	errors[(numErrors++) % MAX_NUM_ERRORS] = error; //Write error to the specified location in the error array
	// if(numErrors > MAX_NUM_ERRORS) errorOverwrite = true; //Set flag if looping over previous errors 
	return numErrors;
}

time_t MCP79412::timegm(struct tm *tm)
{
    time_t ret;
    char *tz;

   tz = getenv("TZ");
    setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz)
        setenv("TZ", tz, 1);
    else
        unsetenv("TZ");
    tzset();
    return ret;
}

time_t MCP79412::cstToUnix(int year, int month, int day, int hour, int minute, int second)
{
    unsigned long unixDate = day - 32075 + 1461*(year + 4800 + (month - 14)/12)/4 + 367*(month - 2 - (month - 14)/12*12)/12 - 3*((year + 4900 + (month - 14)/12)/100)/4 - 2440588; //Stolen from Communications of the ACM in October 1968 (Volume 11, Number 10), Henry F. Fliegel and Thomas C. Van Flandern - offset from Julian Date. Why mess with success? 
    return unixDate*86400 + hour*3600 + minute*60 + second; //Convert unixDate to seconds, sum partial seconds from the current day

}