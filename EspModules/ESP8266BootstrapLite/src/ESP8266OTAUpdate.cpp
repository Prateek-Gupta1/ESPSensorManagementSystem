
#include "ESP8266OTAUpdate.h"

#define GET_DEVICE_INFO_API_ENDPOINT "api/device/mac"
#define UPDATE_API_ENDPOINT "api/firmware/ota/update/device/"
#define DEFAULT_URL "http://localhost:3000/"
#define FILE_DEVICE_IDENTITY "/device.csv"
#define DEVICE_IDENTITY "device"

/*
*  User should provide all the parameters. "apihost" must be the domain name only. If "port" is not provided, then the "apihost" must 
*  be complete url of the service. If "userToken" is not passed then the library functions will not work.
*/
ESP8266OTAUpdate::ESP8266OTAUpdate(String apihost, String port, String userToken){
	if(apihost != "\0" && apihost.length() != 0){
		url = apihost;
		if(port != "\0" && port.length() != 0){
			url = "http://" + url + ":" + port + "/";
		}
	}else{
		DEBUG_PRINTLN("API host and port not provided. Using default 'http://localhost:3000/' as api host.\n");
		url = DEFAULT_URL;
	}
	if(userToken == "\0" || userToken.length() <= 6){
		zombie = true;
	}else{
		user_token = userToken;
	}
}


ESP8266OTAUpdate::ESP8266OTAUpdate(String apihost, String userToken){
	ESP8266OTAUpdate(apihost, "\0", userToken);
}


/*
*  Helper function that retreives info from the device's permanent memory i.e. SPIFFS.
*/
String ESP8266OTAUpdate::getInfoFromSPIFFS(String key){
	if(zombie){
		DEBUG_PRINTLN("Object creation failed as user token was not provded.\n");
		return "\0";
	}

	if(SPIFFS.begin() && SPIFFS.exists(FILE_DEVICE_IDENTITY)){
	  File f = SPIFFS.open(FILE_DEVICE_IDENTITY, "r");
		if(f){
			String key = f.readStringUntil('\n');
			if(key && key.length() > 0){
		  		String deviceId = f.readStringUntil('\n');
				DEBUG_PRINTLN("Device Id stored = %s.\n", &deviceId[0]);
		  		deviceId = deviceId.substring(0, deviceId.length()-1);
		  		if(deviceId != "\0" && deviceId.length() > 6){
					f.close();
					return deviceId;
		  		}
			}
	  	}
	}
	return "\0";
}

/*
*  Helper function that stores info on the device's permanent memory i.e. SPIFFS.
*/
void ESP8266OTAUpdate::storeInfoInSPIFFS(String key, String value){
	if(zombie){
		DEBUG_PRINTLN("Object creation failed as user token was not provded.\n");
	}else{
		File f = SPIFFS.open(FILE_DEVICE_IDENTITY, "w+");
		if(f){
		  f.println(key);
		  f.println(value);
		  f.close();
		}
	}
}


/*
*  This is a helper function that gets the device identity from the server. The device identity is a unique token that is assigned 
*  to each device when the user registers the device on backend service. 
*  If "resourceUri" is not provided then it tries to access a default uri.
*/
String ESP8266OTAUpdate::getDeviceIndentityFromServer(String macAddress, String resourceUri){
	if(zombie) {
		DEBUG_PRINTLN("Object creation failed as user token was not provded.\n");
		return "\0";
	}

	HTTPClient http;
	StaticJsonBuffer<1500> JSONBuffer;

	if(WiFi.status() == WL_CONNECTED){
		if(resourceUri != NULL && resourceUri.length() != 0){
			http.begin(url + resourceUri + "?macaddress=" + macAddress);
		}else{
			http.begin(url + GET_DEVICE_INFO_API_ENDPOINT + "?macaddress=" + macAddress);
		}

		http.addHeader("Authentication", user_token);
		int httpCode = http.GET();

		if(httpCode > 0){
			String payload = http.getString();
			JsonObject& parsed = JSONBuffer.parseObject(payload);

			if(!parsed["message"]){
				http.end();
				String result;
				result = String(parsed["_id"].as<const char*>());
				return result;
			}

			http.end();
		}else{
			DEBUG_PRINTLN("Get response with server code : %d.\n", httpCode);
		}
	}
	return "\0";
}


/*
*  Performs update on the device by communicating with the server. It is a helper function called by the public method "performUpdate"
*/
OTAError ESP8266OTAUpdate::update(String deviceId){
	HTTPClient http;
	http.begin(url + UPDATE_API_ENDPOINT + deviceId);
	http.addHeader("Authentication", user_token);
	HTTPUpdateResult ret = handleUpdate(http, "\0");

	switch(ret){
      case HTTP_UPDATE_FAILED:
          DEBUG_PRINTLN("HTTP_UPDATE_FAILD Error (%d): %s.\n", ESPhttpUpdate.getLastError(), 
          						&((ESPhttpUpdate.getLastErrorString().c_str())[0]));
          break;
      case HTTP_UPDATE_NO_UPDATES:
          DEBUG_PRINTLN("HTTP_UPDATE_NO_UPDATES.\n");
          break;
      case HTTP_UPDATE_OK:
          DEBUG_PRINTLN("HTTP_UPDATE_OK.\n");
          break;
    }
    return OTA_UPDATE_FAILED;
}

/*
*  User should call this method to download new firmware.
*/
OTAError ESP8266OTAUpdate::performUpdate(String macAddress){
	if(zombie){
		DEBUG_PRINTLN("Object creation failed as user token was not provided.\n");
		return OTA_UNINDENTIFIED_USER;
	}

	String deviceId = getInfoFromSPIFFS(DEVICE_IDENTITY);

	if(deviceId == NULL){
		if(WiFi.status() != WL_CONNECTED) {
			DEBUG_PRINTLN("No network connection.\n");
			return OTA_NO_NETWORK_CONNECTION;
		}

		deviceId = getDeviceIndentityFromServer(macAddress, GET_DEVICE_INFO_API_ENDPOINT);

		if(deviceId == NULL){
			DEBUG_PRINTLN("Device could not be identified.\n");
			return OTA_UNINDENTIFIED_DEVICE;
		}else{
			storeInfoInSPIFFS(DEVICE_IDENTITY, deviceId);
		}
	}
	return update(deviceId);
}

