#include "OtaManager.h"

const char* mySsid = "ESP8266";

OtaManagerClass::otaTreatfunction * _theFunction;

const char INDEX_HTML[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>DoorLock Command Line</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body>"
"<h1 style=\"color:blue;\">DoorLock Command Line</h1>"
"<FORM action=\"/\" method=\"post\">"
"<P>"
"Command<br>"
"<INPUT type=\"text\" name=\"COMMAND\" size=\"80\"><BR>"
"<INPUT type=\"submit\" value=\"Send\"> <INPUT type=\"reset\">"
"</P>"
"</FORM>"
"</body>"
"</html>";

const char ANSWER_1_HTML[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>DoorLock Command Line</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body>"
"<h1 style=\"color:blue;\">DoorLock Command Line</h1>"
"<FORM action=\"/\" method=\"post\">"
"<P>"
"Command<br>"
"<INPUT type=\"text\" name=\"COMMAND\" size=\"80\"><BR>"
"<INPUT type=\"submit\" value=\"Send\"> <INPUT type=\"reset\">"
"</P>"
"</FORM>";

const char ANSWER_2_HTML[] =
"</body>"
"</html>";

IPAddress ip(192, 168, 11, 4);
IPAddress gateway(192, 168, 11, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

// Init state
OtaManagerClass::OtaManagerClass()
{
	OtaState = false;
	WebState = false;
	_theFunction = NULL;
}

// Return Ota State
bool OtaManagerClass::CheckOtaState()
{
	return OtaState;
}

// Return Web State
bool OtaManagerClass::CheckWebState()
{
	return WebState;
}

// Refresh OTA actions
void OtaManagerClass::Refresh()
{
	if (WebState)
	{
		server.handleClient();
	}
	if (OtaState)
	{
		ArduinoOTA.handle();
		server.handleClient();
	}
}

// Response for root web site of server
void OtaManagerClass::handleRoot() {
	String CommandLine;

	Logger.Log(F("Command from Web"));
	CommandLine = server.arg("COMMAND");
	if (CommandLine.length() == 0)
	{
		server.send(200, "text/html", INDEX_HTML);
	}
	else
	{
		String result;
		Logger.Log(CommandLine.c_str());
		
		if (_theFunction != NULL)
			result = (*_theFunction)(&CommandLine);

		Logger.Log(F("Answer to Web:"), false);
		Logger.Log(result.c_str());

		String response = ANSWER_1_HTML + String(F("<h2>")) + result + String(F("</h2>")) + ANSWER_2_HTML;

		server.send(200, "text/html", response);
	}
}

// Prepare OTA in Access Point Mode
void OtaManagerClass::AccessPointOTA()
{
	Logger.Log(F("Ota Access Point"));
	WiFi.mode(WIFI_AP);

	WiFi.softAPConfig(ip, gateway, subnet);
	boolean apsuccess = WiFi.softAP(mySsid, password.c_str());

	if (apsuccess)
	{
		// Wait for connection
		String st;
		st = "";
		while (WiFi.softAPgetStationNum() == 0) {
			delay(500);
			st += ".";
			display.clear();
			display.drawString(64, 10, "Access Point");
			display.drawString(64, 30, WiFi.softAPIP().toString());
			display.drawString(64, 40, st);
			display.display();
			if (st.length() > 30)
			{
				st = ".";
				display.clear();
				display.drawString(64, 10, "Access Point");
				display.drawString(64, 30, WiFi.softAPIP().toString());
				display.drawString(64, 40, st);
				display.display();
				delay(10);
			}
			delay(10);
		}

		Logger.Log(F("Ota AP Ok..."));
		OtaState = true;
	}
	else
	{
		Logger.Log(F("Ota cannot be AP"));

		// Access point was unavaible => Restart...
		display.clear();
		display.drawString(64, 10, "Error");
		display.drawString(64, 30, "occured.");
		display.drawString(64, 50, "Restarting...");
		display.display();
		delay(1000);
		ESP.restart();
	}
}

// OTA in Appearing Mode
void OtaManagerClass::AppearingOTA()
{
	Logger.Log(F("Ota Appearing"));

	display.clear();
	display.drawString(64, 10, "Connecting to");
	display.drawString(64, 30, ssid);
	display.display();
	delay(10);

	WiFi.mode(WIFI_STA);

	WiFi.begin(ssid.c_str(), password.c_str());

	// Wait for connection
	String st;
	st = "";
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		st += ".";
		display.drawString(64, 40, st);
		display.display();
		if (st.length() > 30)
		{
			st = ".";
			display.clear();
			display.drawString(64, 10, "Connecting to");
			display.drawString(64, 30, ssid);
			display.drawString(64, 40, st);
			display.display();
			delay(10);
		}
		delay(10);
	}

	Logger.Log(F("Ota Appearing Ok..."));
	OtaState = true;
}

// Ota mechanism
void OtaManagerClass::Ota(bool fromCommand) {
	if (WiFi.isConnected())
	{
		Logger.Log(F("Always connected..."));
		Logger.Log(F("Reset connection !"));
		WiFi.disconnect(true);
		delay(1000);
		WiFi.begin();
	}

	// Read configuration
	int adress = SSID_PSWD_ADDRESS;
	char read;
	while ((read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress++)) != 0xFF && adress < 0x00FA)
	{
		ssid.concat(read);
		delay(10);
	}
	while ((read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress++)) != 0xFF && adress < 0x00FA)
	{
		password.concat(read);
		delay(10);
	}

	String hostname(HOSTNAME);
	hostname += String(ESP.getChipId(), HEX);
	WiFi.hostname(hostname);

	delay(1000);
	// if we stay pressed D3 => Accesspoint OTA also Appearing OTA
	if (digitalRead(D3) == 1 && !fromCommand)
		AccessPointOTA();
	else
	{
		Logger.Log(F("SSID/PSWD:"), false);
		Logger.Log(ssid.c_str(), false);
		Logger.Log(F("/"), false);
		Logger.Log(password.c_str());

		AppearingOTA();
	}

	MDNS.begin(HOSTNAME);
	registerRoute();
	server.begin();

	ArduinoOTA.setHostname((const char *)hostname.c_str());
	ArduinoOTA.begin();
	ArduinoOTA.onStart([]() {
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2 - 10, "OTA Update");
		display.display();
	});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		display.drawProgressBar(4, 32, 120, 8, progress / (total / 100));
		display.display();
	});

	ArduinoOTA.onEnd([]() {
		display.clear();
		display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Restart");
		display.display();
	});

	// Align text vertical/horizontal center
	display.clear();
	display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Ready for OTA\n" + (WiFi.localIP().toString() == "0.0.0.0" ? WiFi.softAPIP().toString() : WiFi.localIP().toString()));
	Logger.Log(F("Ready for OTA"));
	Logger.Log(WiFi.localIP().toString() == "0.0.0.0" ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
	display.display();
}

// Set Web Mode
void OtaManagerClass::Web(bool fromCommand)
{
	if (WiFi.isConnected())
	{
		Logger.Log(F("Always connected..."));
		Logger.Log(F("Reset connection !"));
		WiFi.disconnect(true);
		delay(1000);
		WiFi.begin();
	}

	// Read configuration
	int adress = SSID_PSWD_ADDRESS;
	char read;
	while ((read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress++)) != 0xFF && adress < 0x00FA)
	{
		ssid.concat(read);
		delay(10);
	}
	while ((read = EepromDs3231Class::i2c_eeprom_read_byte(0x57, adress++)) != 0xFF && adress < 0x00FA)
	{
		password.concat(read);
		delay(10);
	}

	if (fromCommand)
	{
		Logger.Log(F("Web Station"));
		WiFi.mode(WIFI_STA);

		Logger.Log(F("SSID/PSWD:"), false);
		Logger.Log(ssid.c_str(), false);
		Logger.Log(F("/"), false);
		Logger.Log(password.c_str());

		String hostname(HOSTNAME);
		hostname += String(ESP.getChipId(), HEX);
		WiFi.hostname(hostname);

		Logger.Log(F("Web Appearing"));

		display.clear();
		display.drawString(64, 10, "Connecting to");
		display.drawString(64, 30, ssid);
		display.display();
		delay(10);

		WiFi.begin(ssid.c_str(), password.c_str());
		// Wait for connection
		String st;
		st = "";
		while (WiFi.status() != WL_CONNECTED) {
			delay(500);
			st += ".";
			display.drawString(64, 40, st);
			display.display();
			if (st.length() > 30)
			{
				st = ".";
				display.clear();
				display.drawString(64, 10, "Connecting to");
				display.drawString(64, 30, ssid);
				display.drawString(64, 40, st);
				display.display();
				delay(10);
			}
			delay(10);
		}
	}
	else
	{
		Logger.Log(F("Web Access Point"));
		WiFi.mode(WIFI_AP);

		String hostname(HOSTNAME);
		hostname += String(ESP.getChipId(), HEX);
		WiFi.hostname(hostname);

		WiFi.softAPConfig(ip, gateway, subnet);
		boolean apsuccess = WiFi.softAP(mySsid, password.c_str());

		if (apsuccess)
		{
			// Wait for connection
			String st;
			st = "";
			while (WiFi.softAPgetStationNum() == 0) {
				delay(500);
				st += ".";
				display.clear();
				display.drawString(64, 10, "Access Point");
				display.drawString(64, 30, WiFi.softAPIP().toString());
				display.drawString(64, 40, st);
				display.display();
				if (st.length() > 30)
				{
					st = ".";
					display.clear();
					display.drawString(64, 10, "Access Point");
					display.drawString(64, 30, WiFi.softAPIP().toString());
					display.drawString(64, 40, st);
					display.display();
					delay(10);
				}
				delay(10);
			}

			Logger.Log(F("Web AP Ok..."));
			WebState = true;
		}
		else
		{
			Logger.Log(F("Web cannot be AP"));

			// Access point was unavaible => Restart...
			display.clear();
			display.drawString(64, 10, "Error");
			display.drawString(64, 30, "occured.");
			display.drawString(64, 50, "Restarting...");
			display.display();
			delay(1000);
			ESP.restart();
		}
	}

	MDNS.begin(HOSTNAME);
	registerRoute();
	server.begin();

	display.clear();
	display.drawString(DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, "Ready for WEB\n" + (WiFi.localIP().toString() == "0.0.0.0" ? WiFi.softAPIP().toString() : WiFi.localIP().toString()));
	Logger.Log(F("Ready for WEB"));
	Logger.Log(WiFi.localIP().toString() == "0.0.0.0" ? WiFi.softAPIP().toString() : WiFi.localIP().toString());
	display.display();

	WebState = true;
}

void OtaManagerClass::registerRoute()
{
	server.on("/", handleRoot);
	server.on("/version", []() {
		server.send(200, "text/plain", "DoorLock version 1.0");
	});
}

void OtaManagerClass::registerTreatFunction(otaTreatfunction & theFunction)
{
	_theFunction = &theFunction;
}