#ifndef W_SAMSUNG_DEVICE_H
#define W_SAMSUNG_DEVICE_H

#include <ArduinoJson.h>
#include "IRremoteESP8266.h"
#include "IRsend.h"
#include "ir_Samsung.h"
#include "WDevice.h"
#include "WSwitch.h"

const uint8_t stateOff[21] = { 0x02, 0xB2, 0x0F, 0x00, 0x00, 0x00, 0xC0, 0x01, 0xD2,
		0x0F, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0xFF, 0x71, 0x80, 0x11, 0xC0 };
const char* MODE_AUTO        = "auto";
const char* MODE_COOL        = "cool";
const char* MODE_FAN         = "fan";
const char* FAN_MODE_AUTO    = "auto";
const char* FAN_MODE_LOW     = "low";
const char* FAN_MODE_MEDIUM  = "medium";
const char* FAN_MODE_HIGH    = "high";
const char* FAN_MODE_TURBO   = "turbo";

class WSamsungDevice: public WDevice {
public:
	WSamsungDevice(WNetwork* network, int irPin, int configButtonPin)
		: WDevice(network, "ac", "Air Condition", DEVICE_TYPE_ON_OFF_SWITCH) {
			//Settings
	  this->showAsWebthingDevice = network->getSettings()->setBoolean("showAsWebthingDevice", true);
	  this->showAsWebthingDevice->setReadOnly(true);
		this->showAsWebthingDevice->setVisibility(NONE);
		this->addProperty(showAsWebthingDevice);
	  this->setVisibility(this->showAsWebthingDevice->getBoolean() ? ALL : MQTT);
		this->sendShort = false;
		this->sendOnSignal = false;
		this->sendOffSignal = false;
		//HtmlPages
    WPage* configPage = new WPage(this->getId(), "Configure AC");
    configPage->setPrintPage(std::bind(&WSamsungDevice::printConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    configPage->setSubmittedPage(std::bind(&WSamsungDevice::saveConfigPage, this, std::placeholders::_1, std::placeholders::_2));
    network->addCustomPage(configPage);
		//IR library
		this->samsungAc = new IRSamsungAc(irPin);
		this->samsungAc->begin();
		//Config button
		configButton = new WSwitch(configButtonPin, MODE_BUTTON);
		this->addPin(configButton);
		this->configButton->setOnPressed([this](){
			this->network->notice(F("configButton pressed..."));
			this->network->startWebServer();
		});
		//Properties
		//On
		this->onProperty = WProperty::createOnOffProperty("on", "Power");
		//this->samsungAc->off();
		this->onProperty->setBoolean(false);
		this->onProperty->setOnChange(std::bind(&WSamsungDevice::onPropertyChanged, this, std::placeholders::_1));
		this->addProperty(onProperty);
		network->getSettings()->add(onProperty);
		//Mode
		this->modeProperty = WProperty::createStringProperty("mode", "Mode", 4);
		this->samsungAc->setMode(kSamsungAcCool);
		this->modeProperty->setString(MODE_COOL);
		this->modeProperty->addEnumString(MODE_AUTO);
		this->modeProperty->addEnumString(MODE_COOL);
		this->modeProperty->addEnumString(MODE_FAN);
		this->modeProperty->setOnChange(std::bind(&WSamsungDevice::modePropertyChanged, this, std::placeholders::_1));
		this->addProperty(modeProperty);
		network->getSettings()->add(modeProperty);
		//Temperature
		this->tempProperty = new WLevelProperty("desiredTemperature", "Set", 18.0, 30.0);
		this->samsungAc->setTemp(28);
		this->tempProperty->setDouble(28);
		this->tempProperty->setMultipleOf(0.5);
		this->tempProperty->setOnChange(std::bind(&WSamsungDevice::tempPropertyChanged, this, std::placeholders::_1));
		this->addProperty(tempProperty);
		network->getSettings()->add(tempProperty);
		//Fan Speed
		this->fanProperty = WProperty::createStringProperty("fanSpeed", "Fan Speed", 6);
		this->samsungAc->setFan(kSamsungAcFanLow);
		this->fanProperty->setString(FAN_MODE_LOW);
		this->fanProperty->addEnumString(FAN_MODE_AUTO);
		this->fanProperty->addEnumString(FAN_MODE_LOW);
		this->fanProperty->addEnumString(FAN_MODE_MEDIUM);
		this->fanProperty->addEnumString(FAN_MODE_HIGH);
		this->fanProperty->addEnumString(FAN_MODE_TURBO);
		this->fanProperty->setOnChange(std::bind(&WSamsungDevice::fanPropertyChanged, this, std::placeholders::_1));
		this->addProperty(fanProperty);
		network->getSettings()->add(fanProperty);
		//Swing
		this->swingProperty = WProperty::createOnOffProperty("swing", "Swing");
		this->samsungAc->setSwing(false);
		this->swingProperty->setBoolean(false);
		this->swingProperty->setOnChange(std::bind(&WSamsungDevice::swingPropertyChanged, this, std::placeholders::_1));
		this->addProperty(swingProperty);
		network->getSettings()->add(swingProperty);
	}

	void loop(unsigned long now) {
		this->WDevice::loop(now);
		if (this->sendShort) {
			this->sendShort = false;
			if (onProperty->getBoolean()) {
				this->samsungAc->send();
				delay(1000);
			}
		}
		if (this->sendOnSignal) {
			this->sendOnSignal = false;
			this->samsungAc->sendExtended();
			delay(1000);
		}
		if (this->sendOffSignal) {
			//getIrSend() must be added to ir_Samsung.h manually to provide access to private _irsend
			/* IRsend* getIrSend() {
	  	  	  	   return &_irsend;
  	  	  	   }
			 */
			this->sendOffSignal = false;
			this->samsungAc->getIrSend()->sendSamsungAC(stateOff, 21);
			delay(1000);
		}
	}

	void printConfigPage(ESP8266WebServer* webServer, WStringStream* page) {
    page->printAndReplace(FPSTR(HTTP_CONFIG_PAGE_BEGIN), getId());
    page->printAndReplace(FPSTR(HTTP_CHECKBOX_OPTION), "sa", "sa", (showAsWebthingDevice->getBoolean() ? HTTP_CHECKED : ""), "", "Show as Mozilla Webthing device");
    page->print(FPSTR(HTTP_CONFIG_SAVE_BUTTON));
	}

	void saveConfigPage(ESP8266WebServer* webServer, WStringStream* page) {
		network->notice(F("Save config page"));
		this->showAsWebthingDevice->setBoolean(webServer->arg("sa") == HTTP_TRUE);
	}

protected:
	void onPropertyChanged(WProperty* property) {
		if (property->getBoolean()) {
			network->debug(F("WSD: switch ac on"));
			this->samsungAc->on();
			this->sendOnSignal = true;
		} else {
			network->debug(F("WSD: switch ac off"));
			this->samsungAc->off();
			this->sendOffSignal = true;
		}
	}

	void modePropertyChanged(WProperty* property) {
		if (property->equalsString(MODE_AUTO)) {
			this->samsungAc->setMode(kSamsungAcAuto);
			this->sendShort = true;
		} else if (property->equalsString(MODE_COOL)) {
			this->samsungAc->setMode(kSamsungAcCool);
			this->sendShort = true;
		} else if (property->equalsString(MODE_FAN)) {
			this->samsungAc->setMode(kSamsungAcFan);
			this->sendShort = true;
		}
	}

	void tempPropertyChanged(WProperty* property) {
		this->samsungAc->setTemp(property->getDouble());
		this->sendShort = true;
	}

	void fanPropertyChanged(WProperty* property) {
		if (property->equalsString(FAN_MODE_AUTO)) {
			this->samsungAc->setFan(kSamsungAcFanAuto);
			this->sendShort = true;
		} else if (property->equalsString(FAN_MODE_LOW)) {
			this->samsungAc->setFan(kSamsungAcFanLow);
			this->sendShort = true;
		} else if (property->equalsString(FAN_MODE_MEDIUM)) {
			this->samsungAc->setFan(kSamsungAcFanMed);
			this->sendShort = true;
		} else if (property->equalsString(FAN_MODE_HIGH)) {
			this->samsungAc->setFan(kSamsungAcFanHigh);
			this->sendShort = true;
		} else if (property->equalsString(FAN_MODE_TURBO)) {
			this->samsungAc->setFan(kSamsungAcFanTurbo);
			this->sendShort = true;
		}
	}

	void swingPropertyChanged(WProperty* property) {
		this->samsungAc->setSwing(property->getBoolean());
		this->sendShort = true;
	}

private:
	WSwitch *configButton;
	WProperty* showAsWebthingDevice;
	WProperty* onProperty;
	WProperty* modeProperty;
	WProperty* tempProperty;
	WProperty* fanProperty;
	WProperty* swingProperty;
	IRSamsungAc* samsungAc;
	bool sendShort, sendOnSignal, sendOffSignal;
};

#endif
