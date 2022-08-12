/*
 Name:		ClockSimulator.ino
 Created:	8/2/2022 8:36:10 AM
 Author:	ThuanVo
*/

// the setup function runs once when you press reset or power the board
#include <TM1638.h>


//PIN
#define stbPin 4
#define clkPin 7
#define dioPin 8
#define relayPin 2


#define NumberOfMode 6
//Button function
#define btnMode 1 //used for all mode
//Mode 0
#define btnDigitInc		2
#define btnDigitDec		3
#define btnCursor		4
#define btnBack			8
//Mode 1
#define btnBrightInc	2
#define btnBrightDec	3
//Mode 2
//#define btnCursor		4 (same as mode0)
//#define btnDigitInc	2 (same as mode0)
//#define btnDigitDec	3 (same as mode0)
#define btnDelayAlarm	6
#define btnAckAlarm		7
//#define btnBack		8 (same as mode0)
//Mode 3


TM1638	tm(dioPin, clkPin, stbPin);
uint8_t mode;
uint64_t startingTime = 0;
uint64_t runningMilSecs = 0;
uint8_t	brightness = 1;
struct datetime {
	uint16_t year = 2022;
	uint8_t month = 12;
	uint8_t date = 6;
	uint8_t day = 6;	//Sunday = 1
	uint8_t hour = 22;
	uint8_t minute = 2;
	uint8_t second = 0;
	uint8_t smallsec = 0;	// 100smallsec = 1s
} time;
uint8_t	arr[17]; // [0 y,y, 2 y,y, 4 m,m,6 d,d,8 day, 9 h,h,11 m,m,13 s,s,15 ss,ss,\0]
uint8_t datesOfMonth[2][12] = { { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}, 
								{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 } };	//leap year
bool btn[8];

//Mode 0 global variable
static uint8_t	cursor = 0;
//Mode 2 global variable
uint8_t alarmHour = 23;
uint8_t	alarmMinute = 15;
uint8_t	alarmCursor = 0;
uint8_t alarmLastHour, alarmLastMinute;
//Mode turn off: global variable
//static uint16_t	k = 0;

void TMInit(uint8_t brightness) {
	tm.setupDisplay(true, brightness);
	startingTime = millis();
	mode = 0;
}

void setup() {
	TMInit(brightness);

	pinMode(relayPin, OUTPUT);	//Init for alarm mode
	digitalWrite(relayPin, true); //true -> relay OFF


	Serial.begin(9600);
}

void buttonsScan() {
	bool arrBtn[2][8]; // arr[0][k] is the previous state, arr[1][k] is the next state

	byte buttons = tm.getButtons();
	for (uint8_t i = 1; i <= 8; i++) {
		if ((buttons & (0x01 << (i - 1))) > 0) {
			arrBtn[0][i - 1] = true;
		}
		else {
			arrBtn[0][i - 1] = false;
		}
	}
	delay(80);
	buttons = tm.getButtons();
	for (uint8_t i = 1; i <= 8; i++) {
		if ((buttons & (0x01 << (i - 1))) > 0) {
			arrBtn[1][i - 1] = true;
		}
		else {
			arrBtn[1][i - 1] = false;
		}
	}
	for (uint8_t i = 1; i <= 8; i++) {
		if (arrBtn[0][i - 1] == false && arrBtn[1][i - 1] == true) {
			//Serial.print("Button ");	//debug
			//Serial.print(i);
			//Serial.println(" is being pressed");
			btn[i - 1] = true;
		}
		else {
			btn[i - 1] = false;
		}
	}
}

bool isButton(uint8_t pos) {	//check if btn at pos (1:8) is pressed
	return btn[pos - 1];
}

bool isModeChanged() {
	if (isButton(btnMode)) {	//positive edge on btn 1
		mode = (++mode)%NumberOfMode;
		//clear when mode changed
		tm.clearDisplay();
		tm.setLEDs(0);

		//reset cursor in mode 0 to stop blinking
		cursor = 0;
		//reset cursor in mode 2 to stop blinking
		alarmCursor = 0;
		return true;
		//reset k in mode turn off
		//k = 2;
	}
	else{}
	return false;
}

bool isMillisOverflowed() {	
	static uint64_t time_ms = 0;		//time_ms ONLY = 0 in the first cycle
	//In case overflow of millis() (64bits)
	//Cause millis() is always bigger than time_ms
	//=> if millis() <= time_ms (overflow happens) -> reset time_ms = 0 and set new Startingtime
	if (millis() <= time_ms) {//overflow happens
		time_ms = 0;
		startingTime = millis();
		return true;
	}
	else {
		time_ms = millis();
		return false;
	}
}

bool isOneSec() {
	static uint64_t timeSec1 = 0;
	uint64_t timeSec2;

	timeSec1 = isMillisOverflowed() ? 0 : timeSec1;
	timeSec2 = runningMilSecs / 1000;
	if (timeSec2 - timeSec1 >= 1) {
		timeSec1 = timeSec2;
		return true;
	}
	return false;
}

bool isHalfSec() {	//seem not to be accurate
	static uint64_t t1 = 0;
	uint64_t t2;

	t1 = isMillisOverflowed() ? 0 : t1;
	t2 = runningMilSecs / 100;		//runningMilSecs = 1234ms => t2 = 12 = 1.2(s)
	if (t2 - t1 >= 5) {
		t1 = t2;
		return true;
	}
	return false;
}

void splitTwoDigitsNum(uint8_t value, uint8_t digit[], uint8_t startAddr) {	//2022 must be split to 20 and 22 before using this
	digit[startAddr] = value / 10;
	digit[++startAddr] = value % 10;
}

void storeDateTime() {
	//store date&time in an array
//arr	0	1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16
//		y	y	y	y	m	m	d	d	day	h	h	m	m	s	s	ss	ss
	splitTwoDigitsNum(time.year / 100, arr, 0);
	splitTwoDigitsNum(time.year % 100, arr, 2);
	splitTwoDigitsNum(time.month, arr, 4);
	splitTwoDigitsNum(time.date, arr, 6);
	arr[8] = time.day;
	splitTwoDigitsNum(time.hour, arr, 9);
	splitTwoDigitsNum(time.minute, arr, 11);
	splitTwoDigitsNum(time.second, arr, 13);
	splitTwoDigitsNum(time.smallsec, arr, 15);
}

void timeCalculator(uint64_t runningMilSecs, uint8_t *hour = &time.hour, uint8_t *minute = &time.minute, uint8_t *second = &time.second, uint8_t *smallsec = &time.smallsec, uint16_t *year = &time.year, uint8_t* month = &time.month, uint8_t* date = &time.date, uint8_t* day = &time.day) {
	* smallsec = (runningMilSecs % 1000) / 10;
	if (isOneSec()) {
		if (++time.second > 59) {
			time.second = 0; 
			if (++time.minute > 59) {
				time.minute = 0;
				if (++time.hour > 23) {
					alarmLastHour = 0; //reset alarm
					alarmLastMinute = 0;
					time.hour = 0;
					time.day = ((++time.day) % 8 == 0) ? 1 : time.day;
					if (++time.date > datesOfMonth[(time.year % 4) == 0][time.month - 1]) {
						time.date = 1;
						if (++time.month > 12) {
							time.month = 1;
							++time.year;
						}
					}			
				}
			}
		}
	}
	else {}
	storeDateTime();	
	//print date&time to serial - debug
	{
		Serial.print("Date: ");
		Serial.print(time.date);
		Serial.print("-");
		Serial.print(time.month);
		Serial.print("-");
		Serial.print(time.year);
		Serial.print("-");
		Serial.println(time.day);

		Serial.print("Time: ");
		Serial.print(time.hour);
		Serial.print(":");
		Serial.print(time.minute);
		Serial.print(":");
		Serial.print(time.second);
		Serial.print(":");
		Serial.println(time.smallsec);
		//Serial.println("");
	}
}

void mode0() {
	//display date & time to 7 segment
	tm.setDisplayDigit(arr[6], 0, false);
	tm.setDisplayDigit(arr[7], 1, true);
	tm.setDisplayDigit(arr[4], 2, false);
	tm.setDisplayDigit(arr[5], 3, true);
	tm.setDisplayDigit(arr[9], 4, false);
	tm.setDisplayDigit(arr[10], 5, true);
	tm.setDisplayDigit(arr[11], 6, false);
	tm.setDisplayDigit(arr[12], 7, ((runningMilSecs / 1000) % 2) == 1);

	//cursor = 0: do not change value, led remains display
	//cursor = 1-8, select value to change
	if (isButton(btnCursor)) {
		cursor = (++cursor) % 9;
	}
	switch (cursor) {
	case 0:

		break;
	case 1:
		if (isHalfSec()) {
			tm.clearDisplayDigit(0, false);
		}
		else {}
		break;
	case 2:
		if (isHalfSec()) {
			tm.clearDisplayDigit(1, true);
		}
		else {}
		break;
	case 3:
		if (isHalfSec()) {
			tm.clearDisplayDigit(2, false);
		}
		else {}
		break;
	case 4:
		if (isHalfSec()) {
			tm.clearDisplayDigit(3, true);
		}
		else {}
		break;
	case 5:
		if (isHalfSec()) {
			tm.clearDisplayDigit(4, false);
		}
		else {}
		break;
	case 6:
		if (isHalfSec()) {
			tm.clearDisplayDigit(5, true);
		}
		else {}
		break;
	case 7:
		if (isHalfSec()) {
			tm.clearDisplayDigit(6, false);
		}
		else {}
		break;
	case 8:
		if (isHalfSec()) {
			tm.clearDisplayDigit(7, false);
		}
		else {}
		break;
	default:

		break;
	}

	//Inc or Dec value
	if (isButton(btnDigitInc)) {
		switch (cursor) {
		case 0:
			break;
		case 1:
			time.date = time.date + 10 > datesOfMonth[(time.year % 4) == 0][time.month - 1] ?
				datesOfMonth[(time.year % 4) == 0][time.month - 1] : time.date += 10;
			break;
		case 2:
			time.date = time.date + 1 > datesOfMonth[(time.year % 4) == 0][time.month - 1] ? 1 : time.date += 1;
			break;
		case 3:
			time.month = time.month > 2 ? 12 : time.month += 10;
			break;
		case 4:
			time.month = ++time.month > 12 ? 1 : time.month;
			break;
		case 5:
			time.hour = time.hour + 10 > 24 ? 23 : time.hour += 10;
			break;
		case 6:
			time.hour = (++time.hour) % 24;
			break;
		case 7:
			time.minute = time.minute + 10 > 59 ? 59 : time.minute += 10;
			break;
		case 8:
			time.minute = (++time.minute) % 60;
			break;
		}
	}
	if (isButton(btnDigitDec)) {
		switch (cursor) {
		case 0:
			break;
		case 1:
			time.date = time.date <= 10 ? 1 : time.date -= 10;
			break;
		case 2:
			time.date = time.date <= 1 ? datesOfMonth[(time.year % 4) == 0][time.month - 1] : --time.date;
			break;
		case 3:
			time.month = time.month <= 10 ? 1 : time.month -= 10;
			break;
		case 4:
			time.month = time.month <= 1 ? 12 : --time.month;
			break;
		case 5:
			time.hour = time.hour <= 9 ? 0 : time.hour -= 10;
			break;
		case 6:
			time.hour = time.hour <= 0 ? 23 : --time.hour;
			break;
		case 7:
			time.minute = time.minute <= 9 ? 0 : time.minute -= 10;
			break;
		case 8:
			time.minute = time.minute <= 0 ? 59 : --time.minute;
			break;
		}
	}
	
	//btn Back
	cursor =  isButton(btnBack)  ? 0 : cursor;
	//*supplement:we can use btnBack to change the value indirectly
	// or changing directly like above
}

void mode1(uint8_t *brightness) {
	char s[8];
	sprintf(s, "Bright %01d", *brightness);
	tm.setDisplayToString(s);
	if (isButton(btnBrightInc)) {
		*brightness = (++(*brightness))%8;	// brightness: 6 -> 7 -> 0 -> 1
	}
	
	tm.setupDisplay(true, *brightness);
	sprintf(s, "Bright %01d", *brightness);
	tm.setDisplayToString(s);

	if (isButton(btnBrightDec)) {
		*brightness = (*brightness) == 0 ? 7 : --*brightness; // brightness: 2 -> 1 -> 0 -> 7
	}
	tm.setupDisplay(true, *brightness);
	sprintf(s, "Bright %01d", *brightness);
	tm.setDisplayToString(s);
}

void mode2() {
	uint8_t	arrAlarm[4];
	splitTwoDigitsNum(alarmHour, arrAlarm, 0);
	splitTwoDigitsNum(alarmMinute, arrAlarm, 2);
	tm.setDisplayToString("Alr1",0b00010000);
	tm.setDisplayDigit(arrAlarm[0], 4, false);
	tm.setDisplayDigit(arrAlarm[1], 5, true);
	tm.setDisplayDigit(arrAlarm[2], 6, false);
	tm.setDisplayDigit(arrAlarm[3], 7, false);
	if (isButton(btnCursor)) {
		alarmCursor = (++alarmCursor) % 5;
	}
	switch (alarmCursor) {
	case 0:
		break;
	case 1:
		if (isHalfSec()) {
			tm.clearDisplayDigit(4, false);
		}
		else {}
		break;
	case 2:
		if (isHalfSec()) {
			tm.clearDisplayDigit(5, true);
		}
		else {}
		break;
	case 3:
		if (isHalfSec()) {
			tm.clearDisplayDigit(6, false);
		}
		else {}
		break;
	case 4:
		if (isHalfSec()) {
			tm.clearDisplayDigit(7, false);
		}
		else {}
		break;
	}

	if (isButton(btnDigitInc)) {
		switch (alarmCursor) {
		case 0:
			break;
		case 1:
			alarmHour = alarmHour + 10 > 24 ? 23 : alarmHour += 10;
			break;
		case 2:
			alarmHour = (++alarmHour) % 24;
			break;
		case 3:
			alarmMinute = alarmMinute + 10 > 59 ? 59 : alarmMinute += 10;
			break;
		case 4:
			alarmMinute = (++alarmMinute) % 60;
			break;
		}
	}
	if (isButton(btnDigitDec)) {
		switch (alarmCursor) {
		case 0:
			break;
		case 1:
			alarmHour = alarmHour <= 9 ? 0 : alarmHour -= 10;
			break;
		case 2:
			alarmHour = alarmHour <= 0 ? 23 : --alarmHour;
			break;
		case 3:
			alarmMinute = alarmMinute <= 9 ? 0 : alarmMinute -= 10;
			break;
		case 4:
			alarmMinute = alarmMinute <= 0 ? 59 : --alarmMinute;
			break;
		}
	}
	alarmCursor = isButton(btnBack) ? 0 : alarmCursor;
}

void mode3() {

}

void mode4() {

}

void mode5() {

}


void modeImplementation() {

	runningMilSecs = millis() - startingTime;	
	timeCalculator(runningMilSecs);

	//mode 2:
	static uint16_t alarm2Min, alarmLast2Min = 0;
	alarm2Min = alarmHour * 60 + alarmMinute;
	//Serial.print(alarm2Min);	//debug
	//Serial.print("--");
	//Serial.println(alarmLast2Min);
	//Serial.print("--");
	//Serial.println(time.hour*60 +time.minute);
	Serial.print("Alarm:");
	Serial.print(alarmHour);
	Serial.print(":");
	Serial.print(alarmMinute);
	Serial.println("");
	Serial.println("");


	if (!isButton(btnAckAlarm)) {	//if no ack then check alarm
		if (alarmLast2Min < alarm2Min && (time.hour * 60 + time.minute >= alarm2Min)) {
			digitalWrite(relayPin, false);	//relay ON -relay triggered with 0V
			alarmLastHour = time.hour;
			alarmLastMinute = time.minute;
			alarmLast2Min = alarmLastHour * 60 + alarmLastMinute;
			Serial.println("Alarm triggered");//debug
			//delay(300);//debug
		}
	}
	else {	//if ack occurs then turn off alarm
		digitalWrite(relayPin, true); //relay OFF
		Serial.println("Alarm acknowledge");//debug
		//delay(300);//debug
	}

	switch (mode) {		//mode < NumberOfMode
	case 0://mode clock
		tm.setLED(TM1638_COLOR_RED, 0);
		tm.setupDisplay(true, brightness);		//can be deleted? -> nope cause penultimute will turn off
		mode0();
		break;
	case 1://mode brightness
		tm.setLED(TM1638_COLOR_RED, 1);
		mode1(&brightness);
		break;
	case 2://mode alarm
		tm.setLED(TM1638_COLOR_RED, 2);
		mode2();
		break;
	case 3:
		tm.setLED(TM1638_COLOR_RED, 3);
		tm.setDisplayToString("no conf");
		break;

	case NumberOfMode - 1://Turn off
		
		//Blinking turn off notification
		
		////static uint16_t	k = 0;
		//const uint16_t totalPeriod = 100;
		//k = 0;
		//while (1< k <= totalPeriod) {
		//	if (k < totalPeriod / 4) tm.setDisplayToString("turn off");
		//	else if (k < totalPeriod / 2) tm.clearDisplay();
		//	else if (k < totalPeriod * 3 / 4) tm.setDisplayToString("turn off");
		//	else if (k < totalPeriod) tm.clearDisplay();
		//	else k = 0;
		//	k++;
		//	Serial.print("k =");
		//	Serial.println(k);
		//}

		//tm.setDisplayToString("Turning ");
		//tm.setDisplayToString("urning o");
		//tm.setDisplayToString("rning of");
		//tm.setDisplayToString("ning off");
		//tm.setDisplayToString("ing off ");
		//tm.setDisplayToString("ng off  ");
		//tm.setDisplayToString("g off   ");
		//tm.setDisplayToString(" off    ");
		//tm.setDisplayToString("off    T");
		//tm.setDisplayToString("ff    Tu");
		//tm.setDisplayToString("f    Tur");
		//tm.setDisplayToString("    Turn");
		//tm.setDisplayToString("   Turni");
		//tm.setDisplayToString("  Turnin");
		//tm.setDisplayToString(" Turning");
		//tm.setDisplayToString("Turning ");
		//tm.setDisplayToString("urning o");
		//tm.setDisplayToString("rning of");
		//tm.setDisplayToString("ning off");
		//tm.setDisplayToString("ing off ");
		//tm.setDisplayToString("ng off  ");
		//tm.setDisplayToString("g off   ");
		//tm.setDisplayToString(" off    ");
		//tm.setDisplayToString("ff      ");
		//tm.setDisplayToString("f       ");
		//tm.setDisplayToString("        ");


		tm.setupDisplay(false, 0);
		break;

	default:
		tm.setLEDs(0b10101010);
		tm.setDisplayToString(" no func");
		break;
	}
	
}

void clockSimulator() {
	isModeChanged();
	modeImplementation();
}


bool isButton2(uint8_t pos) {	//check if btn at pos (1:8) is pressed
	uint8_t btnCode, btnCodeOld;

	btnCode = tm.getButtons() & (1 << (pos - 1));
	delay(80);		//reducing button bouncing
	btnCodeOld = btnCode;
	btnCode = tm.getButtons() & (1 << (pos - 1));
	return (btnCode != 0) && (btnCodeOld == 0); 	//positive edge on btn 1
}

void loop() {
	buttonsScan();		
	clockSimulator();
}
