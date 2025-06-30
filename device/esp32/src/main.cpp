

#ifdef ESPADMIN_DEV



#include <Arduino.h>
#include <WiFi.h>
#include <ESPAdmin.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <TLSClient.h>


TLSClient _tls;
WiFiClientSecure _wcs1;

void setup()
{
	// put your setup code here, to run once:

	pinMode(LED_BUILTIN, OUTPUT);

	Serial.begin(115200);
	delay(3000);

	WiFi.mode(WIFI_STA); // Optional
	WiFi.begin("NET", "senha123");
	Serial.println("\nConnecting");

	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(100);
	}

	Serial.println("\nConnected to the WiFi network");
	Serial.print("Local ESP32 IP: ");
	Serial.println(WiFi.localIP());


	delay(1000);

	_wcs1.setInsecure();
	ESPAdmin::setWSClient(&_wcs1);
	int res = ESPAdmin::begin("192.168.1.10", 4443, "f3061d1dcacc168dfdba47ac10cca9ee"); //5c212979b40067e08efd8bc1d051b9f3
	delay(500);
}



void loop()
{
	// put your main code here, to run repeatedly:
	digitalWrite(LED_BUILTIN, 1);
	delay(100);
	digitalWrite(LED_BUILTIN, 0);
	delay(100);

	static uint32_t last = millis();
	if(millis() - last > 1000l*30)
	{
		if(ESPAdmin::connected())
		{
			ESPAD_LOG_DEBUG("this is an example debug log message %d %d", millis(), millis());
		}
		else
		{
			int res = ESPAdmin::begin("192.168.1.77", 9000, "358ecb06a027f02a73535818596b6f31");
		}

		last = millis();
	}
}







#endif