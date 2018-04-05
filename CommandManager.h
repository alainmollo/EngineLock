/*
CommandManager.h

Copyright (c) 2015, Arduino LLC
Original code (pre-library): Copyright (c) 2016, Alain Mollo

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef _COMMANDMANAGER_h
#define _COMMANDMANAGER_h

// I2C adress for PCF8574
#define PCF8574_ADDRESS 0x38

// Command sender declaration
#define COMMAND_FROM_SERIAL   0
#define COMMAND_FROM_SIM800   1
#define COMMAND_FROM_INTERNAL 2
#define COMMAND_FROM_WIFI 3

// Memory map
#define PLANNING_ADRESS 0x010C
#define LAST_CARD_ADRESS 0x0100
#define CALLBACK_ADRESS 0x0000
#define NUMBER_ADRESS 0x000C
#define MANAGER_ADRESS 0x0018
#define SSID_PSWD_ADDRESS 0x0024
#define USE_A0_ADDRESS 0x00FA

#define NUMBER_SIZE 0x0C

#include "Logger.h"
#include <SoftwareSerial.h>
#include <Wire.h>
#include "RtcDS3231.h"
#include "EepromDs3231.h"
#include "SSD1306.h"
#include "Sim800L.h"
#include "RfidManager.h"
#include "displayManager.h"
#include "OtaManager.h"

class CommandManagerClass
{
protected:
	// Init Sim800l library
	Sim800L * Sim800;

	// Setup DS3231 library
	RtcDS3231<TwoWire> * Rtc;

	// Setup Rfid manager class
	RfidManagerClass * RfidManager;
	
	// Property flag if remote data's was loaded
	bool * readyFull;

	// Real treatment
	bool LaunchCommand(String *, String *, uint8_t);

	// Send SMS
	bool sendSms(String *, String *);

	// Read SMS and treat commant if number is allowed
	bool AnalyseSms(String *, String);

	// Reply after a command was treated
	bool ReplyToSender(String, String *, uint8_t);

	// Send AT command to Sim800L
	bool SENDATCOMMAND(String *, String *, uint8_t);

	// Check if a tag is authorized
	bool checkAuthorization(RtcDateTime, uint8 *);

	// Log entry for unlock operation
	void logOneEntry(RtcDateTime now, uint8 * tagid);
public:
	// Default constructor
	CommandManagerClass(Sim800L *, RtcDS3231<TwoWire> *, RfidManagerClass *, bool &);

	// Launch command treatment
	bool TreatCommand(String *, String *, uint8_t);

	// Launch command treatment
	bool TreatCommand(char *, String *, uint8_t);

	// Launch command treatment
	bool TreatCommand(String *, char *, uint8_t);

	// Launch command treatment
	bool TreatCommand(char *, char *, uint8_t);

	// Launch rfid treatment
	void rfidTreatCommand(uint8 *);

	// Launch rfid treatment
	void rfidTreatCommand(uint8 *, String *, uint8_t);

	// Send SMS Planning request to callback user (manager)
	bool askPlanning();
};
#endif