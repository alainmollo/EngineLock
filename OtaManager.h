/*
OtaManager.h

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

#ifndef _OTAMANAGER_h
#define _OTAMANAGER_h

#define HOSTNAME "ESP8266-OTA-"
#define VERSION "1.00"

#define SSID_PSWD_ADDRESS 0x0024

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include "EepromDs3231.h"
#include <ArduinoOTA.h>

#include "displayManager.h"
#include "Logger.h"

class OtaManagerClass
{
protected:
	bool OtaState;
	bool WebState;

	String ssid = "";
	String password = "";

	// Web returning function in Web mode
	static void handleRoot();

	// Launch Access Point mode
	void AccessPointOTA(void);

	// Launch Appairing mode
	void AppearingOTA(void);

	// Set http registration routes
	void registerRoute(void);
public:
	// Constructor
	OtaManagerClass(void);

	// Refresh OTA downloading process when OTA mode was launched
	void Refresh(void);

	// Set OTA mode with appairing or acces point style
	void Ota(bool);

	// Set Web mode appairing or acces point style
	void Web(bool);

	// Check if OTA mode was launched
	bool CheckOtaState(void);

	// Check if WEB mode was launched
	bool CheckWebState(void);
};

extern OtaManagerClass otaManager;
#endif