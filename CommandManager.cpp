#include "CommandManager.h"

// Default constructor
CommandManagerClass::CommandManagerClass(Sim800L * _Sim800, RtcDS3231<TwoWire> * _Rtc, RfidManagerClass * _RfidManager, bool & readFull)
{
	Sim800 = _Sim800;
	Rtc = _Rtc;
	RfidManager = _RfidManager;
	readyFull = &readFull;
}

bool CommandManagerClass::TreatCommand(String * Message, String * Who, uint8_t From)
{
	Logger.Log(F("Command was launched by "), false);
	Logger.Log(*Who);
	Logger.Log(*Message);
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

	// Reset the system
	// OTAEND#
	if (AnalyseSms(Message, F("OTA")))
	{
		otaManager.Ota(true);

		return ReplyToSender(F("OTA was launched..."), Who, From);
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

				for (int j = 0; j < data->length(); j = j + 2)
				{
					uint8_t deciData = strtol((*data).substring(j, j + 2).c_str(), nullptr, 16);
					EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, deciData);
					delay(10);
				}
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
				*addr += "FFFFFFFF";
				int deciAdress = PLANNING_ADRESS;

				for (int j = 0; j < addr->length(); j = j + 2)
				{
					uint8_t deciData = strtol((*addr).substring(j, j + 2).c_str(), nullptr, 16);
					EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, deciData);
					delay(10);
				}
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

	// Set phone number for Callback
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

	// Set phone number for Callback
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

	// Read phone number for Callback
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

	// Read phone number for Callback
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

	// emulate rfid scan for card 45 5A 32 AA
	// RFID@455A32AAEND#111
	if (AnalyseSms(Message, F("RFID")))
	{
		int ix = 0;
		if (Message->charAt(ix) == '@')
		{
			ix++;
			String * s5 = new String();
			while (ix < Message->length())
			{
				*s5 += Message->charAt(ix);
				ix++;
			}

			String * x = new String();
			char * Qstr = new char[3];
			*(Qstr + 2) = 0;
			*x = s5->substring(0, 2);
			strncpy(Qstr, x->c_str(), 2);
			byte Q1 = strtol(Qstr, 0, 16);
			*x = s5->substring(2, 4);
			strncpy(Qstr, x->c_str(), 2);
			byte Q2 = strtol(Qstr, 0, 16);
			*x = s5->substring(4, 6);
			strncpy(Qstr, x->c_str(), 2);
			byte Q3 = strtol(Qstr, 0, 16);
			*x = s5->substring(6);
			strncpy(Qstr, x->c_str(), 2);
			byte Q4 = strtol(Qstr, 0, 16);
			delete s5;
			delete Qstr;
			delete x;

			Logger.Log(String(Q1, HEX), false);
			Logger.Log(F(","), false);
			Logger.Log(String(Q2, HEX), false);
			Logger.Log(F(","), false);
			Logger.Log(String(Q3, HEX), false);
			Logger.Log(F(","), false);
			Logger.Log(String(Q4, HEX));

			uint8 * tag = new uint8[CARD_SIZE];
			tag[0] = Q1;
			tag[1] = Q2;
			tag[2] = Q3;
			tag[3] = Q4;
			rfidTreatCommand(tag, Who, From);
			delete tag;

			return ReplyToSender(F("OK"), Who, From);
		}
		else
			return false;
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
	if (checkAuthorization(now, tagid[0], tagid[1], tagid[2], tagid[3]))
	{
		// Unlock engine
		digitalWrite(D7, LOW);
		delay(50);

		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Porte debloquee."));
		display.display();
		display.lockDisplay();

		String * toSend = new String(F("TRACE:"));
		toSend->concat(String(tagid[0], HEX));
		toSend->concat(String(tagid[1], HEX));
		toSend->concat(String(tagid[2], HEX));
		toSend->concat(String(tagid[3], HEX));
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
bool CommandManagerClass::checkAuthorization(RtcDateTime now, byte Q1, byte Q2, byte Q3, byte Q4)
{
	Logger.Log(F("Check Authorization from : "), false);
	Logger.Log(String(Q1, HEX), false);
	Logger.Log(String(Q2, HEX), false);
	Logger.Log(String(Q3, HEX), false);
	Logger.Log(String(Q4, HEX));
	bool result = false;

	byte code[CARD_SIZE];
	byte count = 0;
	int deciAdress = LAST_CARD_ADRESS;
	while (++count < CARD_SIZE)
	{
		code[0] = EepromDs3231Class::i2c_eeprom_read_byte(0x57, deciAdress++);
		delay(10);
		code[1] = EepromDs3231Class::i2c_eeprom_read_byte(0x57, deciAdress++);
		delay(10);
		code[2] = EepromDs3231Class::i2c_eeprom_read_byte(0x57, deciAdress++);
		delay(10);
		code[3] = EepromDs3231Class::i2c_eeprom_read_byte(0x57, deciAdress++);
		delay(10);

		if ((code[0] == 0xFF && code[1] == 0xFF && code[2] == 0xFF && code[3] == 0xFF) && count > 1)
			break;

		Logger.Log(F("Memory chek : "), false);
		Logger.Log(String(count, DEC), false);
		Logger.Log(F(" , "), false);
		Logger.Log(String(code[0], HEX), false);
		Logger.Log(String(code[1], HEX), false);
		Logger.Log(String(code[2], HEX), false);
		Logger.Log(String(code[3], HEX));

		if (code[0] == Q1 && code[1] == Q2 && code[2] == Q3 && code[3] == Q4)
		{
			result = true;
			break;
		}
	}

	// if check is ok, write on eeprom the card number for secure unlock
	if (result)
	{
		deciAdress = LAST_CARD_ADRESS;

		EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, Q1);
		delay(10);
		EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, Q2);
		delay(10);
		EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, Q3);
		delay(10);
		EepromDs3231Class::i2c_eeprom_write_byte(0x57, deciAdress++, Q4);
		delay(10);

		// Make a log entry with date/time to send to server

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
	return sendSms(callNumber, logdisp);
}