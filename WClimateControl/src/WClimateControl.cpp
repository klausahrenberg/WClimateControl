#include "Arduino.h"
#include "WNetwork.h"
#include "WSamsungDevice.h"

#define APPLICATION "Climate Control"
#define VERSION "1.17"
#define FLAG_SETTINGS 0x17
#define DEBUG false

#define PIN_LED LED_BUILTIN
#define PIN_IR_SEND 4 //D2
#define PIN_BUTTON 5

WNetwork *network;
WSamsungDevice *samsungDevice;

void setup() {
	if (DEBUG) {
		Serial.begin(9600);
	}
	network = new WNetwork(DEBUG, APPLICATION, VERSION, true, LED_BUILTIN, FLAG_SETTINGS);
	//Samsung Ac
	samsungDevice = new WSamsungDevice(network, PIN_IR_SEND, PIN_BUTTON);
	network->addDevice(samsungDevice);
}

void loop() {
  network->loop(millis());
	delay(100);
}
