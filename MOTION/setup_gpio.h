#pragma once

#include <ESPAsyncWebServer.h>
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <StreamString.h>
#include <Updater.h>
#include <vector>
#include "esphome/core/helpers.h"


class MySetupGPIO : public Component {
public:

	void setup() override{
		pinMode(6, OUTPUT);
		digitalWrite(6, HIGH);
	}
	void loop() override {
	}

protected:
};


