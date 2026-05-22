#include <Arduino.h>

#include "Config.h"
#include "Controllers.h"
#include "StateMachine.h"

InputController inputController;
LEDController ledController;
SensorController sensorController;
MotionController motionController;
USBController usbController;
TelemetryController telemetryController;
BTController btController;

void setup()
{
  // WICHTIG: USB und Bluetooth MÜSSEN beim Booten immer initialisiert werden,
  // da der nRF52840 seine Hardware-Schnittstellen (Transceiver) vorbereiten muss.

  // Start des USB-HID Subsystems (inkl. TinyUSBDevice.begin() intern)
  usbController.begin();

  // Start des Bluetooth-Subsystems (inkl. Bluefruit.begin() und Batterie-Dienst)
  btController.begin();

  if (Config::ENABLE_TELEMETRY)
  {
    Serial.begin(115200);
    // Wir warten nicht starr, falls kein USB-Datenport offen ist
    uint32_t startTime = millis();
    while (!Serial && (millis() - startTime < 500))
    {
      delay(10);
    }
  }

  // Initialisierung der restlichen Peripherie und Sensoren
  inputController.begin();
  ledController.begin();
  sensorController.begin();
  motionController.reset();
  telemetryController.begin();

  // Zustandsmaschine starten
  stateMachine.changeState(&StateMachine::calibratingState);
}

void loop()
{
  // 1. Hardware-Prüfung: Liegt überhaupt Strom am USB-C Anschluss an?
  // (HEX-Registerauswertung des nRF52840: HEX: 0x40000438 & HEX: 0x00000001)
  bool vbusVorhanden = (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk);

  // 2. Treiber-Prüfung: Ist das Gerät erfolgreich am PC als SpaceMouse angemeldet (mounted)
  // und bereit, Daten zu senden (ready)?
  bool usbAktiv = (vbusVorhanden && TinyUSBDevice.mounted() && TinyUSBDevice.ready());

  // 3. Dynamische Weichenstellung für die Tasks
  if (usbAktiv)
  {
    // FALL A: Gerät ist aktiv am PC eingesteckt
    // Führt den USB-Datenversand (Achsen, Buttons, USB-Batteriebericht) aus
    usbController.task();
  }
  else
  {
    // FALL B & C: Reines USB-Netzteil (Laden) ODER reiner Akkubetrieb
    // Verarbeitet die Bluetooth-Ereignisse und funkt die Daten via BLE-PnP raus
    btController.update();
  }

  // Die Zustandsmaschine läuft unabhängig vom Übertragungsweg immer weiter
  stateMachine.update();
}
