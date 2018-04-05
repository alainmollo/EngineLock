#include "CommandManager.h"

// Default constructor
CommandManagerClass::CommandManagerClass(Sim800L * _Sim800, RtcDS3231<TwoWire> * _Rtc, RfidManagerClass * _RfidManager, bool & readFull)
{
	Sim800 = _Sim800;
	Rtc = _Rtc;
	RfidManager = _RfidManager;
	readyFull = &readFull;
}

// Launch command treatment if callnumber is include in 3 authorized numbers
bool CommandManagerClass::TreatCommand(String * Message, String * Who, uint8_t From)
{
	Logger.Log(F("Command was launched by "), false);
	Logger.Log(*Who);
	Logger.Log(*Message);

	String retour;
	if (From == COMMAND_FROM_SIM800)
	{
		int adress = CALLBACK_ADRESS;
		for (int i = 0; i < NUMBER_SIZE; i++)
		{
			char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
			retour.concat(read);
			delay(10);
		}
		if (Who->indexOf(retour) == -1)
		{
			retour = "";
			adress = NUMBER_ADRESS;
			for (int i = 0; i < NUMBER_SIZE; i++)
			{
				char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
				retour.concat(read);
				delay(10);
			}
			if (Who->indexOf(retour) == -1)
			{
				retour = "";
				adress = MANAGER_ADRESS;
				for (int i = 0; i < NUMBER_SIZE; i++)
				{
					char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
					retour.concat(read);
					delay(10);
				}
				if (Who->indexOf(retour) == -1)
				{
					Logger.Log(F("Non authorized number."));
					return false;
				}
			}
		}
	}

	bool result = LaunchCommand(Message, Who, From);
	if (!result)
		ReplyToSender(F("CMD ERROR"), Who, From);
	return result;
}

bool CommandManagerClass::TreatCommand(char * Message, String * Who, uint8_t From)
{
	String * tmpstr = new String(Message);
	bool result = TreatCommand(tmpstr, Who, From);
	delete tmpstr;
	return result;
}

bool CommandManagerClass::TreatCommand(String * Message, char * Who, uint8_t From)
{
	String * tmpstr = new String(Who);
	bool result = TreatCommand(Message, tmpstr, From);
	delete tmpstr;
	return result;
}

bool CommandManagerClass::TreatCommand(char * Message, char * Who, uint8_t From)
{
	String * tmpstr1 = new String(Message);
	String * tmpstr2 = new String(Who);
	bool result = TreatCommand(tmpstr1, tmpstr2, From);
	delete tmpstr1;
	delete tmpstr2;
	return result;
}

bool CommandManagerClass::LaunchCommand(String * Message, String * Who, uint8_t From)
{
	// Reset the system
	// RSETEND#533
	if (AnalyseSms(Message, F("RSET")))
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Restart");
		display.display();
		delay(1000);
		ESP.restart();

		return true;
	}

	// Reset the system
	// ASKEND#
	if (AnalyseSms(Message, F("ASK")))
	{
		this->askPlanning();
		return true;
	}

	// Reset the system
	// CLREND#440
	if (AnalyseSms(Message, F("CLR")))
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Clear Memory");
		display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 + 20, "0 %");
		display.display();

		int adress = 0x0000;
		for (int i = 0; i < 32767; i++)
		{
			uint8_t read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
			if (read != 0xFF)
			{
				delay(5);
				EepromDs3231Class::i2c_eeprom_write_byte(0x57, adress + i, 0xFF);
			}
			delay(5);
			if (i % 100 == 99)
			{
				if (i % 1000 == 999)
					Logger.Log(F("."), false);
				display.clear();
				display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Clear Memory");
				display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 + 20, String((i / 327), DEC) + " %");
				display.display();
			}
		}
		Logger.Log(F(""));
		Logger.Log(F("Clear ended"));

		return true;
	}

	// Launch OTA Mode in access point
	// OTAAPEND#
	if (AnalyseSms(Message, F("OTAAP")))
	{
		otaManager.Ota(false);

		return ReplyToSender(F("OTA AP was launched..."), Who, From);
	}

	// Launch OTA Mode in station
	// OTASTAEND#
	if (AnalyseSms(Message, F("OTASTA")))
	{
		otaManager.Ota(true);

		return ReplyToSender(F("OTA STA was launched..."), Who, From);
	}

	// Launch Web Mode
	// WEBAPEND#
	if (AnalyseSms(Message, F("WEBAP")))
	{
		otaManager.Web(false);

		return ReplyToSender(F("WEB was launched..."), Who, From);
	}

	// Launch Web Mode
	// WEBSTAEND#
	if (AnalyseSms(Message, F("WEBSTA")))
	{
		otaManager.Web(true);

		return ReplyToSender(F("WEB was launched..."), Who, From);
	}

	// Unlock door
	// 0 -> 3 ex: UNLK1END#100
	if (AnalyseSms(Message, F("UNLK")))
	{
		double  x = Message->toFloat();
		uint8_t toLock = pow(2, x);
		// Unlock engine
		digitalWrite(D7, LOW);
		delay(50);

		return ReplyToSender(F("OK"), Who, From);
	}

	// Store data's informations in flash or eeprom
	// DATA@0010$4A325E66@0020$55666A23END#1983
	if (AnalyseSms(Message, F("DATA")))
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Datas Setting ..."));
		display.display();

		int ix = 0;
		while (ix < Message->length())
		{
			if (Message->charAt(ix) == '@')
			{
				ix++;
				String * addr = new String();
				while (ix < Message->length() && Message->charAt(ix) != '$')
				{
					*addr += Message->charAt(ix);
					ix++;
				}
				ix++;
				String * data = new String();
				while (ix < Message->length() && Message->charAt(ix) != '@')
				{
					*data += Message->charAt(ix);
					ix++;
				}

				int deciAdress = strtol(addr->c_str(), 0, 16);

				Logger.Log(F("Split:"), false);
				for (int j = 0; j < data->length(); j = j + 2)
				{
					Logger.Log((*data).substring(j, j + 2), false);
					Logger.Log(F(","), false);
					uint8_t deciData = strtol((*data).substring(j, j + 2).c_str(), nullptr, 16);
					EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, deciData);
					delay(10);
				}
				Logger.Log(F(""));
				delete addr;
				delete data;
			}
		}
		return ReplyToSender(F("OK"), Who, From);
	}

	// Set planning cards authorized
	// PLAN$4A325E6625AA11E0END#1983
	if (AnalyseSms(Message, F("PLAN")))
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Plan Setting ..."));
		display.display();

		int ix = 0;
		while (ix < Message->length())
		{
			if (Message->charAt(ix) == '$')
			{
				ix++;
				String * addr = new String();
				while (ix < Message->length() && Message->charAt(ix) != '$')
				{
					*addr += Message->charAt(ix);
					ix++;
				}
				
				// Fill the end of sequence with 0xFF
				for (int j = 0; j < RFID_MAX_LEN; j++)
				{
					*addr += "FF";
				}

				int deciAdress = PLANNING_ADRESS;
				Logger.Log(F("Split:"), false);
				for (int j = 0; j < addr->length(); j = j + 2)
				{
					Logger.Log((*addr).substring(j, j + 2), false);
					Logger.Log(F(","), false);
					uint8_t deciData = strtol((*addr).substring(j, j + 2).c_str(), nullptr, 16);
					EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, deciData);
					delay(10);
				}
				Logger.Log(F(""));
				delete addr;
			}
		}

		*readyFull = true;
		return ReplyToSender(F("OK"), Who, From);
	}

	// Send AT Command to Sim800
	// SIM800@AT+CSQ?END#xxx
	if (AnalyseSms(Message, F("SIM800")))
		return SENDATCOMMAND(Message, Who, From);

	// Dump data's informations from flash or eeprom
	// DUMP@0010END#782
	if (AnalyseSms(Message, F("DUMP")))
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Dump Asking ... "));
		display.display();

		int ix = 0;
		if (Message->charAt(ix) == '@')
		{
			ix++;
			String * addr = new String();
			while (ix < Message->length())
			{
				*addr += Message->charAt(ix);
				ix++;
			}

			int deciAdress = strtol(addr->c_str(), 0, 16);

			String * output = new String();
			for (int j = 0; j < 48; j++)
			{
				uint8_t result;
				result = EepromDs3231Class::i2c_eeprom_read_byte(0x57, deciAdress + j); // dump less than 0x2000
				delay(10);
				if (result < 16)
					*output += F("0");
				*output += String(result, HEX);;
			}
			output->toUpperCase();

			int crc = 0;
			for (int i = 0; i < output->length(); i++)
			{
				crc += output->charAt(i);
			}
			*output = "@" + *addr + ":" + *output + "#" + String(crc);

			Logger.Log(F("Send Dump :"), false);
			Logger.Log(*output);

			bool result = ReplyToSender(*output, Who, From);

			delete addr;
			delete output;
			return result;
		}
		else
			return false;
	}

	// Set phone number for Callback (SERVER)
	// CLBK+33614490515END#1111
	if (AnalyseSms(Message, F("CLBK")))
	{
		if (Message->length() == NUMBER_SIZE)
		{
			// Set callback number eeprom storage adress
			int adress = CALLBACK_ADRESS;
			for (int i = 0; i < NUMBER_SIZE; i++)
			{
				EepromDs3231Class::i2c_eeprom_write_byte(0x57, adress + i, Message->charAt(i));
				delay(10);
			}

			display.clear();
			display.drawString(DISPLAY_WIDTH / 2, 20, F("Set CallBack Num"));
			display.display();
			display.lockDisplay();

			return ReplyToSender(F("OK"), Who, From);
		}
		else
			return false;
	}

	// Set phone number of sim card to not reply to itself
	// CNUM+33614490515END#1111
	if (AnalyseSms(Message, F("CNUM")))
	{
		if (Message->length() == NUMBER_SIZE)
		{
			// Set callback number eeprom storage adress
			int adress = NUMBER_ADRESS;
			for (int i = 0; i < NUMBER_SIZE; i++)
			{
				EepromDs3231Class::i2c_eeprom_write_byte(0x57, adress + i, Message->charAt(i));
				delay(10);
			}

			display.clear();
			display.drawString(DISPLAY_WIDTH / 2, 20, F("Set Number Num"));
			display.display();
			display.lockDisplay();

			return ReplyToSender(F("OK"), Who, From);
		}
		else
			return false;
	}

	// Set phone number of manager (ME)
	// CNUM+33614490515END#1111
	if (AnalyseSms(Message, F("CMNG")))
	{
		if (Message->length() == NUMBER_SIZE)
		{
			// Set callback number eeprom storage adress
			int adress = MANAGER_ADRESS;
			for (int i = 0; i < NUMBER_SIZE; i++)
			{
				EepromDs3231Class::i2c_eeprom_write_byte(0x57, adress + i, Message->charAt(i));
				delay(10);
			}

			display.clear();
			display.drawString(DISPLAY_WIDTH / 2, 20, F("Set MAnager Num"));
			display.display();
			display.lockDisplay();

			return ReplyToSender(F("OK"), Who, From);
		}
		else
			return false;
	}

	// Read phone number for Callback (SERVER)
	// RCBKEND#1111
	if (AnalyseSms(Message, F("RCBK")))
	{
		String retour;
		// Set callback number eeprom storage adress
		int adress = CALLBACK_ADRESS;
		for (int i = 0; i < NUMBER_SIZE; i++)
		{
			char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
			retour.concat(read);
			delay(10);
		}

		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Read CallBack Num"));
		display.display();
		display.lockDisplay();

		return ReplyToSender(retour, Who, From);
	}

	// Read phone number of sim card to not reply to itself
	// RNUMEND#1111
	if (AnalyseSms(Message, F("RNUM")))
	{
		String retour;
		// Set callback number eeprom storage adress
		int adress = NUMBER_ADRESS;
		for (int i = 0; i < NUMBER_SIZE; i++)
		{
			char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
			retour.concat(read);
			delay(10);
		}

		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Read Number Num"));
		display.display();
		display.lockDisplay();

		return ReplyToSender(retour, Who, From);
	}

	// Read phone number of manager (ME)
	// RNUMEND#1111
	if (AnalyseSms(Message, F("RMNG")))
	{
		String retour;
		// Set callback number eeprom storage adress
		int adress = MANAGER_ADRESS;
		for (int i = 0; i < NUMBER_SIZE; i++)
		{
			char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
			retour.concat(read);
			delay(10);
		}

		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Read Manager Num"));
		display.display();
		display.lockDisplay();

		return ReplyToSender(retour, Who, From);
	}

	// emulate rfid scan for card 45 5A 32 AA ...
	// RFID@455A32AA...END#111
	if (AnalyseSms(Message, F("RFID")))
	{
		int ix = 0;
		if (Message->charAt(ix) == '@')
		{
			ix++;
			String * datas = new String();
			while (ix < Message->length())
			{
				*datas += Message->charAt(ix);
				ix++;
			}
			Logger.Log("Emulate Rfid : ",false);

			uint8 * tag = new uint8[RFID_MAX_LEN];
			byte pos = 0x00;
			for (int j = 0; j < RFID_MAX_LEN * 2; j = j + 2)
			{
				if (j >= (datas->length() - 1))
					break;
				uint8_t deciData = strtol((*datas).substring(j, j + 2).c_str(), nullptr, 16);
				*(tag + pos++) = deciData;
				Logger.Log(String(deciData, HEX), false);
				Logger.Log(F(","), false);
			}
			Logger.Log(F(""));

			rfidTreatCommand(tag, Who, From);
			delete tag;

			return ReplyToSender(F("OK"), Who, From);
		}
		else
			return false;
	}

	// Store data's informations in flash or eeprom
	// WIFI@SSID~PASSWORDEND#...
	if (AnalyseSms(Message, F("WIFI")))
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Wifi Setting ..."));
		display.display();

		int ix = 0;
		while (ix < Message->length())
		{
			if (Message->charAt(ix) == '@')
			{
				ix++;
				String * addr = new String();
				while (ix < Message->length() && Message->charAt(ix) != '~')
				{
					*addr += Message->charAt(ix);
					ix++;
				}
				ix++;
				String * data = new String();
				while (ix < Message->length() && Message->charAt(ix) != '@')
				{
					*data += Message->charAt(ix);
					ix++;
				}

				Logger.Log(F("Set Wifi : "),false);
				Logger.Log(*addr, false);
				Logger.Log(F("/"), false);
				Logger.Log(*data);

				int deciAdress = SSID_PSWD_ADDRESS;

				for (int j = 0; j < data->length(); j++)
				{
					EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, addr->charAt(j));
					delay(10);
				}
				EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, 0xFF);
				delay(10);
				for (int j = 0; j < data->length(); j++)
				{
					EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, data->charAt(j));
					delay(10);
				}
				EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, 0xFF);
				delay(10);
				delete addr;
				delete data;
			}
		}
		return ReplyToSender(F("OK"), Who, From);
	}

	return false;
}

// Decode SMS and check validity, return valid string or null
// La forme correcte est par exemple :
// RSETEND#533
bool CommandManagerClass::AnalyseSms(String * message, String code)
{
	if (message->startsWith(code))
	{
		int diezeindex = message->indexOf(F("#"));
		String dieze = message->substring(diezeindex + 1);
		int crcmessage = dieze.toInt();
		int crc = 0;
		for (int i = 0; i < diezeindex; i++)
		{
			crc += message->charAt(i);
		}

		Logger.Log(F("Analyze Sms:"), false);
		Logger.Log(String(crc));

		if (crc == crcmessage || Logger.IsLogging())
		{
			int endindex = message->indexOf(F("END"));
			if (endindex != -1)
			{
				int atindex = message->indexOf(F("@"));
				if (atindex != -1)
					*message = message->substring(atindex, endindex);
				else
					*message = message->substring(code.length(), endindex);

				Logger.Log(F("Message = "), false);
				Logger.Log(*message);
				return true;
			}
		}
		return false;
	}
	return false;
}

// Command treatment function when Tag was detected
void CommandManagerClass::rfidTreatCommand(uint8 * tagid)
{
	rfidTreatCommand(tagid, nullptr, COMMAND_FROM_INTERNAL);
}

// Command treatment function when Tag was detected
void CommandManagerClass::rfidTreatCommand(uint8 * tagid, String * Who, uint8_t From)
{
	Logger.Log(F("rfidTreatCommand"));

	RtcDateTime now = Rtc->GetDateTime();
	if (checkAuthorization(now, tagid))
	{
		// Unlock engine
		digitalWrite(D7, LOW);
		delay(50);

		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Porte debloquee."));
		display.display();
		display.lockDisplay();

		String * toSend = new String(F("TRACE:"));
		for (int j = 0; j < RFID_MAX_LEN; j++)
		{
			toSend->concat(String(*(tagid+j), HEX));
		}
		toSend->toUpperCase();

		ReplyToSender(*toSend, Who, From);

		delete toSend;
		Logger.Log(F("Porte debloquee."));
	}
	else
	{
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Carte rejetee..."));
		display.display();
		display.lockDisplay();

		Logger.Log(F("Carte rejetee..."));
	}
}

// Answer to incoming request
bool CommandManagerClass::ReplyToSender(String reply, String * Who, uint8_t From)
{
	switch (From)
	{
	case COMMAND_FROM_SERIAL:
		Serial.println(reply);
		delay(10);
		return true;
	case COMMAND_FROM_SIM800:
		return sendSms(Who, &reply);
	default:
		return true;
	}
}

// Send message to callback number
bool CommandManagerClass::sendSms(String * callbackNumber, String * message)
{
	String retour;
	// Set callback number eeprom storage adress
	int adress = NUMBER_ADRESS;
	for (int i = 0; i < NUMBER_SIZE; i++)
	{
		char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
		retour.concat(read);
		delay(10);
	}

	if (callbackNumber->indexOf(retour) != -1)
	{
		Logger.Log(F("Change callback number!"));
		retour = "";
		int adress = CALLBACK_ADRESS;
		for (int i = 0; i < NUMBER_SIZE; i++)
		{
			char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
			retour.concat(read);
			delay(10);
		}

		return Sim800->sendSms(retour, *message);
	}
	else
	{
		Logger.Log(F("sending Sms to "), false);
		Logger.Log(*callbackNumber, false);
		Logger.Log(F(" : "), false);
		Logger.Log(*message);

		return Sim800->sendSms(*callbackNumber, *message);
	}
}

// Check if a card is authorized to unlock engine
bool CommandManagerClass::checkAuthorization(RtcDateTime now, uint8 * tagid)
{
	Logger.Log(F("Check Authorization from : "), false);
	for (int j = 0; j < RFID_MAX_LEN; j++)
	{
		Logger.Log(String(*(tagid + j), HEX), false);
	}
	Logger.Log(F(""));
	bool result = false;

	byte code[RFID_MAX_LEN];
	byte count = 0;
	int deciAdress = LAST_CARD_ADRESS;

	// we are checking less or equal than 4 cards on planning buffer
	while (++count < 4)
	{
		for (int i = 0; i < RFID_MAX_LEN; i++)
		{
			code[i] = EepromDs3231Class::i2c_eeprom_read_byte(0x57, deciAdress++);
			delay(10);
		}

		byte flag = 0;
		for (int i = 0; i < RFID_MAX_LEN; i++)
		{
			if (code[i] == 0xFF)
				flag++;
		}
		if (flag == RFID_MAX_LEN && count > 1)
			break;

		Logger.Log(F("Memory chek : "), false);
		Logger.Log(String(count, DEC), false);
		Logger.Log(F(" , "), false);
		for (int i = 0; i < RFID_MAX_LEN; i++)
		{
			Logger.Log(String(code[i], HEX), false);
		}
		Logger.Log(F(""));

		flag = 0;
		for (int i = 0; i < RFID_MAX_LEN; i++)
		{
			if (code[i] == *(tagid + i))
				flag++;
		}
		if (flag == RFID_MAX_LEN)
		{
			result = true;
			break;
		}
	}

	// if check is ok, write on eeprom the card number for secure unlock
	if (result)
	{
		deciAdress = LAST_CARD_ADRESS;

		for (int i = 0; i < RFID_MAX_LEN; i++)
		{
			EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, *(tagid + i));
			delay(10);
		}

		// Make a log entry with date/time to send to server if tag is authorized
		logOneEntry(now, tagid);
	}
	return result;
}

// Send SMS Planning request to callback user (manager)
bool CommandManagerClass::askPlanning()
{
	Logger.Log("Ask Planning to : ", false);
	String * callNumber = new String();
	int adress = CALLBACK_ADRESS;
	for (int i = 0; i < NUMBER_SIZE; i++)
	{
		char read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress + i);
		(*callNumber).concat(read);
		delay(10);
	}
	Logger.Log(*callNumber);
	String * logdisp = new String(F("ASKPLAN@"));

	RtcDateTime dt = Rtc->GetDateTime();

	char datestring[11];
	char timestring[9];

	snprintf_P(datestring,
		countof(datestring),
		PSTR("%02u/%02u/%04u"),
		dt.Day(),
		dt.Month(),
		dt.Year());
	snprintf_P(timestring,
		countof(timestring),
		PSTR("%02u:%02u:%02u"),
		dt.Hour(),
		dt.Minute(),
		dt.Second());

	*logdisp += datestring;
	*logdisp += F(";");
	*logdisp += timestring;
	*logdisp += F("#");

	int diezeindex = logdisp->indexOf(F("#"));
	String dieze = logdisp->substring(diezeindex + 1);
	int crcmessage = dieze.toInt();
	int crc = 0;
	for (int i = 0; i < diezeindex; i++)
	{
		crc += logdisp->charAt(i);
	}

	*logdisp += String(crc);
	bool result = sendSms(callNumber, logdisp);
	delete callNumber;
	delete logdisp;

	return result;
}

// Make an entry log for authorized tag
void CommandManagerClass::logOneEntry(RtcDateTime now, uint8 * tagid)
{
	//TODO
}