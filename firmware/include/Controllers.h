#pragma once

#include "controllers/USBController.h"
#include "controllers/InputController.h"
#include "controllers/LEDController.h"
#include "controllers/MotionController.h"
#include "controllers/SensorController.h"
#include "controllers/TelemetryController.h"

extern USBController usbController;
extern InputController inputController;
extern LEDController ledController;
extern SensorController sensorController;
extern MotionController motionController;
extern TelemetryController telemetryController;
