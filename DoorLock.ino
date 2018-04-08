/*
DoorLock.ino

Copyright (c) 2017, Alain Mollo
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

// Embeded libraries
#include <RtcUtility.h>
#include <RtcTemperature.h>
#include <RtcDS3231.h>
#include <RtcDS1307.h>
#include <RtcDateTime.h>
#include <YunServer.h>
#include <YunClient.h>
#include <Process.h>
#include <Mailbox.h>
#include <HttpClient.h>
#include <FileIO.h>
#include <Console.h>
#include <BridgeUdp.h>
#include <BridgeSSLClient.h>
#include <BridgeServer.h>
#include <BridgeClient.h>
#include <Bridge.h>
#include "Tools.h"
#include "displayManager.h"

// Start up method entry
void setup() {
	// Setup Serial communication to 115200 bauds for debugging over serial/usb line
	Serial.begin(115200);

	// Setup Rfid serial module
	RfidManager.init();

	// Set D3 as input pull up for detecting key press action
	// at start up to select normal mode or OTA
	pinMode(D3, INPUT);
	bootState = digitalRead(D3);

	// Setup wire library = I2C
	Wire.begin(D1, D2);

	//Setup Rtc library
	Rtc.Begin();

	display.init();
	display.setContrast(255);
	display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
	display.setFont(ArialMT_Plain_16);
	display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Booting ...\n");
	display.display();
	delay(10);

	// Check wich kind of booting we want during startUp
	int count = 0;
	while (count < BOOT_DURATION && bootState != 0)
	{
		bootState = digitalRead(D3);
		delay(10);
		count++;
	}

	// Output for engine locker
	pinMode(D7, OUTPUT);
	digitalWrite(D7, HIGH);

	// Turn light off
	pinMode(D4, OUTPUT);
	digitalWrite(D4, HIGH);

	// Ota Booting
	if (bootState == 0)
		otaManager.Ota(false);
	else
	{
		// Cut wifi for reduce energy consumption
		WiFi.mode(WIFI_OFF);

		display.init();
		display.setContrast(255);
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Loading ...\n"));
		display.drawString(DISPLAY_WIDTH / 2, 40, F("Please wait !\n"));
		display.display();

		SetUpNormalMode();
	}
}

// Function launch while sim800 process is waiting
void sim800WaitFunction(void)
{
	flashingLed();

	// Check if Rfid Tag was detected
	uint8 rfidCard[RFID_MAX_LEN];
	if (RfidManager.CheckRfid(rfidCard))
		CommandManager.rfidTreatCommand(&rfidCard[0]);
}

// Booting entry in normal mode
void SetUpNormalMode()
{
	Logger.Log(F("Check Sim800 Init State"));

	// Register wait function to sim800 lib
	Sim800.RegisterWaitOptionalFunction(&sim800WaitFunction);

	// Initialization of Sim800L module
	if (!Sim800.Init() && ESP_RESTART_AFTER_SIM800_DEFAULT)
	{
		// Signal an error
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Sim800 fault"));
		display.display();

		Logger.Log(F("Sim800 fault"));

		delay(10000);
		ESP.restart();
	}
	else
	{
		Logger.Log(F("Sim800 was init."));

		// Show GSM Level signal
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, 20, F("Signal  level:"));
		uint8_t levelSignal = Sim800.getLevelSignal(false);
		display.drawString(DISPLAY_WIDTH / 2, 40, String(levelSignal));
		display.display();

		String * dtetme = new String();

		// Get network date and time
		Sim800L::dateTime * dte = new Sim800L::dateTime();
		Sim800.RTCtime(dtetme, dte);

		Logger.Log(F("Obtain DateTime from network:"), false);
		Logger.Log(*dtetme);

		if (!(dte->year < 15))
		{
			// Set DS3231 date and time
			RtcDateTime dt(dte->year, dte->month, dte->day, dte->hour, dte->minute, dte->second);

			Rtc.SetDateTime(dt);
			delay(10);

			Logger.Log(F("Set DateTime from network"));
		}

		// Led indicator turn on
		digitalWrite(D4, LOW);

		delete dtetme;
		delete dte;
	}

	dutyCycle = MAX_DUTY_CYCLE - 2;
	Sim800.AssignInterruptLater(D3);

	CommandManager.registerOtaTreatCommand();
}

// Redirect SMS Message to Command Treatment
void smsTreatCommand(Sim800L::smsReader * smsCommand)
{
	if (!CommandManager.TreatCommand(&smsCommand->Message, &smsCommand->WhoSend, COMMAND_FROM_SIM800))
	{
		Logger.Log(F("Command error :"), false);
		Logger.Log(smsCommand->Message);
	}
}

// Loop process to survey every tasks
void loop()
{
	if (otaManager.CheckOtaState())
		bootState = 0;

	// If bootState == 0, we choose OTA firmware upgrading
	if (bootState == 0)
	{
		otaManager.Refresh();

		// Check if serial data arrived and send to TreatCommand
		if (Serial.available())
		{
			String * cmdstr = new String();
			while (Serial.available())
			{
				cmdstr->concat((char)Serial.read());
				delay(10);
			}
			if (cmdstr->length() > 0)
				if (!CommandManager.TreatCommand(cmdstr, "SERIAL", COMMAND_FROM_SERIAL))
				{
					Logger.Log(F("Command error :"), false);
					Logger.Log(cmdstr->c_str());
				}
			delete cmdstr;
		}
		yield();
	}
	// Else Normal Mode
	else
	{
		otaManager.Refresh();

		if (bootState < FAULT_CYCLE)
		{
			// Check if serial data arrived and send to TreatCommand
			if (Serial.available())
			{
				String * cmdstr = new String();
				while (Serial.available())
				{
					cmdstr->concat((char)Serial.read());
					delay(10);
				}
				if (cmdstr->length() > 0)
					if (!CommandManager.TreatCommand(cmdstr, "SERIAL", COMMAND_FROM_SERIAL))
					{
						Logger.Log(F("Command error :"), false);
						Logger.Log(cmdstr->c_str());
					}
				delete cmdstr;
			}

			// Check if Rfid Tag was detected
			uint8 rfidCard[RFID_MAX_LEN];
			if (RfidManager.CheckRfid(rfidCard))
			{
				dutyCycle = 0;
				if (Logger.IsLogging())
				{
					String rfidStr;
					display.clear();
					display.drawString(DISPLAY_WIDTH / 2, 10, "Normal Mode");
					for (int i = 0; i < RFID_MAX_LEN / 2; i++)
					{
						rfidStr += String(rfidCard[i], HEX);
					}
					display.drawString(DISPLAY_WIDTH / 2, 30, rfidStr);
					rfidStr = "";
					for (int i = RFID_MAX_LEN / 2; i < RFID_MAX_LEN; i++)
					{
						rfidStr += String(rfidCard[i], HEX);
					}
					display.drawString(DISPLAY_WIDTH / 2, 45, rfidStr);
					display.display();
					delay(10);
				}
				else
				{
					display.clear();
					display.drawString(DISPLAY_WIDTH / 2, 10, "Controle carte");
					display.display();
					delay(10);
				}
				CommandManager.rfidTreatCommand(&rfidCard[0]);
			}

			if (readyFull && (bootState == (MAX_CYCLE / 3) || bootState == (2 * MAX_CYCLE / 3)))
			{
				flashingLed();
			}

			// After a long cycle time (approximatively 1.5 second), we check date/time validity
			if (bootState != FAULT_CYCLE && bootState++ > MAX_CYCLE)
			{
				if (!Rtc.IsDateTimeValid())
					Logger.Error(F("RTC lost confidence in the DateTime"));

				// Log and display day/time memory
				display.printDateTime(Rtc);

				flashingLed();

				while (Sim800.checkBootOk() && Sim800.checkSMS())
				{
					// Launch SMS treatment if present
					if (!Sim800.ReadSMSTreatment(smsTreatCommand))
					{
						if (errorCounter++ > MAX_SMS_READING_ERROR)
							bootState = FAULT_CYCLE;
					}
					else
						errorCounter = 0;
				}

				// Reset the bootState value to 1
				if (bootState != FAULT_CYCLE)
					bootState = 1;

				dutyCycle++;
			}

			// We can tag again after 10 seconds
			if ((dutyCycle % 11) == 10 && bootState == 1)
			{
				RfidManager.clearBuffer();
			}

			if (dutyCycle > MAX_DUTY_CYCLE)
			{
				if (!Sim800.checkConnectionOk())
				{
					// Signal an error
					display.clear();
					display.drawString(DISPLAY_WIDTH / 2, 20, F("Sim800 lost\n"));
					display.drawString(DISPLAY_WIDTH / 2, 40, F("Restarting...\n"));
					display.display();

					Logger.Log(F("Sim800 lost"));

					// Try to reconnect
					Sim800.reset();
				}
				else
				{
					if (!Sim800.ReadSMSTreatment(smsTreatCommand))
					{
						if (errorCounter++ > MAX_SMS_READING_ERROR)
							bootState = FAULT_CYCLE;
					}
				}
			}

			if (dutyCycle > MAX_DUTY_CYCLE)
			{
				uint8_t hour = Rtc.GetDateTime().Hour();
				if (hour != lastRefreshAuthCard)
				{
					if (Sim800.checkConnectionOk())
					{
						Logger.Log(F("Asking authorized cards..."));
						if (CommandManager.askPlanning())
							lastRefreshAuthCard = hour;
					}
				}
			}

			// Reset ducyCycle property when Cycle overflow
			if (dutyCycle > MAX_DUTY_CYCLE)
			{
				dutyCycle = 0;
				Logger.Log(F("MAX DUTY CYCLE DETECTED"));
			}
		}
		else
			if (bootState == FAULT_CYCLE && ESP_RESTART_AFTER_SIM800_DEFAULT)
			{
				// Signal an error
				display.clear();
				display.drawString(DISPLAY_WIDTH / 2, 20, F("System fault\n"));
				display.drawString(DISPLAY_WIDTH / 2, 40, F("Restarting...\n"));
				display.display();

				Logger.Log(F("System fault"));

				delay(100);
				ESP.restart();
			}
			else
				if (!Sim800.checkBootOk() && ESP_RESTART_AFTER_SIM800_DEFAULT)
				{
					// Signal an error
					display.clear();
					display.drawString(DISPLAY_WIDTH / 2, 20, F("Sim800 fault\n"));
					display.drawString(DISPLAY_WIDTH / 2, 40, F("Restarting...\n"));
					display.display();

					Logger.Log(F("Sim800 fault restart"));

					delay(100);
					ESP.restart();
				}
				else
				{
					// Signal an error
					display.clear();
					display.drawString(DISPLAY_WIDTH / 2, 20, F("Sim800 fault\n"));
					display.drawString(DISPLAY_WIDTH / 2, 40, F("Reset...\n"));
					display.display();

					Logger.Log(F("Sim800 fault reset"));

					delay(100);
					Sim800.reset();
					bootState = 1;
				}
	}
}

void flashingLed()
{
	bool unlockEngine = false;
	if (digitalRead(D7) == LOW)
		unlockEngine = true;
	// Flashing led if is not fully ready
	if (!unlockEngine)
	{
		bool led = digitalRead(D4);
		digitalWrite(D4, !led);
	}
	else
		digitalWrite(D4, LOW);
}