#ifndef USB_CONTROLLER_H_
#define USB_CONTROLLER_H_

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>

class USBController
{
public:
  // Strukturen für die Datenpakete (Müssen exakt zum USB-Deskriptor passen)
  struct __attribute__((packed)) ReportAxes
  {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t rx;
    int16_t ry;
    int16_t rz;
  };

  struct __attribute__((packed)) ReportButtons
  {
    uint16_t bits;
  };

  // NEU: Struktur für das kombinierte Batterie-Byte (1 Byte Gesamtgröße)
  struct __attribute__((packed)) ReportBattery
  {
    uint8_t percentage : 7; // Bit 0-6: Akkustand (0 bis 100)
    uint8_t isCharging : 1; // Bit 7: Ladestatus (0 = Batterie, 1 = lädt)
  };

  // Funktionen, die von aussen (z.B. in der main.cpp) aufgerufen werden
  void begin();
  void task();
  bool sendReports(const float motion[6], uint16_t buttonBits);

private:
  // Interne Hilfsfunktionen
  ReportAxes makeAxesReport(const float motion[6]);
  bool axesReportChanged(const ReportAxes &axes) const;

  // NEU: Funktion misst die Hardware-Spannung des XIAO BLE
  uint8_t readBatteryPercentage();

  // Interne Variablen zur Speicherung des letzten Status
  Adafruit_USBD_HID usbHid_;
  ReportAxes lastSentAxes_{};
  uint16_t buttonBitsSent_ = 0xFFFF;

  // NEU: Speicher für den letzten Sendezeitpunkt des Akkus
  uint32_t lastBatterySend_ = 0;
};

#endif // USB_CONTROLLER_H_
