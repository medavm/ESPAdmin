

#ifdef ESPADMIN_DEV



#include <Arduino.h>
#include <WiFi.h>
#include <ESPAdmin.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

// TCPClient _tcp;
// WSClient _ws;
// WiFiClient _wc;

WiFiClientSecure _wcs1;
WiFiClientSecure _wcs2;


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

	// const char* j = "{\"cmd\": 0, \"response\":{ \"option1\":\"this is a test\", \"option2\": 4783}}";

	// StaticJsonDocument<200> doc;

	// if(deserializeJson(doc, j, strlen(j)) == DeserializationError::Ok)
	// {
	// 	int cmd = doc["cmd"];
	// 	JsonObject resp = doc["response"];

	// 	// Serial.printf("cmd %d", cmd);

	// 	log_d("cmd %d", cmd);
	// 	if(resp != NULL)
	// 	{
	// 		const char* op1 = resp["option1"];
	// 		log_d("option1 %s", op1);

	// 		int op2 = resp["option2"];
	// 		log_d("option2 %d", op2);

	// 		log_d("option2 %d", resp["option2"].as<int>());
	// 	}
	// 	else
	// 	{
	// 		log_d("resp is null");
	// 	}
		
	// }
	// else
	// {
	// 	log_d("Failed to des json");
	// }

	// IoTAdmin::loop();

	// ESPAdmin::begin(&_wc, "authtoken", 0);

	// int res = ESPAdmin::connect("example.com", 80);

	_wcs1.setInsecure();
	_wcs2.setInsecure();

	ESPAdmin::setWSClient(&_wcs1);
	ESPAdmin::setUpdateClient(&_wcs2);
	
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
	if(millis() - last > 1000l*60)
	{
		if(ESPAdmin::connected())
		{
			
		
			// ESPAD_LOG_CRITICAL("test test test! %d", millis());
			ESPAD_LOG_DEBUG("this is a test DEBUG %d %d", millis(), millis());
			//ESPAD_LOG_INFO("this is a test %s=%d", "millis()", millis());
			ESPAD_LOG_WARN("this is a test WARN %d %d", millis(), millis());
			// ESPAD_LOG_ERROR("this is a test ERROR");

		}
		else
		{
			int res = ESPAdmin::connect("espadmin.alagoa.top", 443, "358ecb06a027f02a73535818596b6f31");
			// int res = ESPAdmin::connect("192.168.1.145", 9000, "358ecb06a027f02a73535818596b6f31");
		}

		last = millis();
	}
	
	
	ESPAdmin::loop();



	// static uint32_t last2 = millis();
	// if(millis() - last2 > 1000l*10)
	// {
	// 	if(_ws.connected())
	// 	{
	// 		const char text[] = "this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! \
	// 		this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! this is a test! ";
	// 		_ws.write((uint8_t*)text, sizeof(text));

	// 	}
	// 	else
	// 	{
	// 		_ws.setPath("/device/e92211cbce531e417fadca9c381ffeaa/ws");
	// 		_ws.connect("192.168.1.145", 9000);
	// 	}


	// 	last2 = millis();
	// }

	// int res = _ws.read();
	// if(res > -1)
	// 	Serial.print(res);


	
}









#endif