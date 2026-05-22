#include "controllers/USBController.h"
#include <math.h>
#include "Config.h"

namespace
{
  // ============================================================================
  // ERWEITERTER USB-HID REPORT DESKRIPTOR (SpaceMouse + USB Battery Info)
  // ============================================================================
  const uint8_t kHidReportDescriptor[] = {
      // ------------------------------------------------------------------------
      // ORIGINAL-BAUSTEIN: 3Dconnexion SpaceMouse (6 Achsen)
      // ------------------------------------------------------------------------
      0x05, 0x01,       // USAGE_PAGE (Generic Desktop)
      0x09, 0x08,       // USAGE (Multi-axis Controller)
      0xA1, 0x01,       // COLLECTION (Application)
      0xA1, 0x00,       // COLLECTION (Physical)
      0x85, 0x01,       // REPORT_ID (1)
      0x16, 0xA2, 0xFE, // LOGICAL_MINIMUM  (-350)
      0x26, 0x5E, 0x01, // LOGICAL_MAXIMUM  (350)
      0x09, 0x30,       // USAGE (X)
      0x09, 0x31,       // USAGE (Y)
      0x09, 0x32,       // USAGE (Z)
      0x09, 0x33,       // USAGE (Rx)
      0x09, 0x34,       // USAGE (Ry)
      0x09, 0x35,       // USAGE (Rz)
      0x75, 0x10,       // REPORT_SIZE (16)
      0x95, 0x06,       // REPORT_COUNT (6)
      0x81, 0x02,       // INPUT (Data,Var,Abs)
      0xC0,             // END_COLLECTION

      // ------------------------------------------------------------------------
      // ORIGINAL-BAUSTEIN: SpaceMouse Tasten (2 Buttons)
      // ------------------------------------------------------------------------
      0xA1, 0x00, // COLLECTION (Physical)
      0x85, 0x03, // REPORT_ID (3)
      0x05, 0x09, // USAGE_PAGE (Button)
      0x19, 0x01, // USAGE_MINIMUM (Button 1)
      0x29, 0x02, // USAGE_MAXIMUM (Button 2)
      0x15, 0x00, // LOGICAL_MINIMUM (0)
      0x25, 0x01, // LOGICAL_MAXIMUM (1)
      0x75, 0x01, // REPORT_SIZE (1)
      0x95, 0x02, // REPORT_COUNT (2)
      0x81, 0x02, // INPUT (Data,Var,Abs)
      0x95, 0x0E, // REPORT_COUNT (14) padding
      0x81, 0x01, // INPUT (Const,Array,Abs)
      0xC0,       // END_COLLECTION

      // ------------------------------------------------------------------------
      // ERWEITERUNG: USB Battery System (Akkustand + Ladestatus)
      // ------------------------------------------------------------------------
      0x05, 0x06,       // HEX: USAGE_PAGE (Generic Device Controls)
      0x09, 0x20,       // HEX: USAGE (Battery Strength) -> S. 77 der hut1_2.pdf
      0xA1, 0x02,       // HEX: COLLECTION (Logical)
      0x85, 0x04,       // HEX: REPORT_ID (4)
      0x15, 0x00,       // HEX: LOGICAL_MINIMUM (0)
      0x26, 0xFF, 0x00, // HEX: LOGICAL_MAXIMUM (255) -> Erlaubt volles Byte inkl. Status-Bit (0xFF)
      0x75, 0x08,       // HEX: REPORT_SIZE (8)       -> 8 Bit (1 Byte) Datenbreite
      0x95, 0x01,       // HEX: REPORT_COUNT (1)      -> 1 kombiniertes Statusbyte
      0x81, 0x02,       // HEX: INPUT (Data,Var,Abs)
      0xC0,             // END_COLLECTION

      0xC0 // END_COLLECTION (Abschluss des Gesamt-Reports)
  };
} // namespace -> WICHTIG: Namespace schliesst hier, damit der Controller global sichtbar ist!

void USBController::begin()
{
  if (!TinyUSBDevice.ready())
  {
    TinyUSBDevice.begin(0);
  }

  // Sichere Variableninitialisierung für den nRF52-Compiler
  lastSentAxes_ = {0, 0, 0, 0, 0, 0};
  buttonBitsSent_ = 0xFFFF;
  lastBatterySend_ = 0;

  usbHid_.setReportDescriptor(kHidReportDescriptor, sizeof(kHidReportDescriptor));
  usbHid_.setPollInterval(1);
  usbHid_.begin();
}

void USBController::task() { TinyUSBDevice.task(); }

USBController::ReportAxes USBController::makeAxesReport(const float motion[6])
{
  ReportAxes axes{};
  axes.x = static_cast<int16_t>(motion[0]);
  axes.y = static_cast<int16_t>(motion[1]);
  axes.z = static_cast<int16_t>(motion[2]);
  axes.rx = static_cast<int16_t>(motion[3]);
  axes.ry = static_cast<int16_t>(motion[4]);
  axes.rz = static_cast<int16_t>(motion[5]);
  return axes;
}

bool USBController::axesReportChanged(const ReportAxes &axes) const
{
  return axes.x != lastSentAxes_.x ||
         axes.y != lastSentAxes_.y ||
         axes.z != lastSentAxes_.z ||
         axes.rx != lastSentAxes_.rx ||
         axes.ry != lastSentAxes_.ry ||
         axes.rz != lastSentAxes_.rz;
}

// Hardware-Akkumessung über den internen ADC des XIAO BLE
uint8_t USBController::readBatteryPercentage()
{
  analogReference(AR_INTERNAL_3_6);
  analogReadResolution(12);

  uint32_t raw_adc = analogRead(PIN_VBAT);

  float voltage = (raw_adc * 3600.0) / 4096.0 * ((1510.0 + 510.0) / 510.0);

  int percentage = (voltage - 3500) * 100 / (4200 - 3500);
  if (percentage > 100)
    percentage = 100;
  if (percentage < 0)
    percentage = 0;

  return static_cast<uint8_t>(percentage);
}

bool USBController::sendReports(const float motion[6], uint16_t buttonBits)
{
  const ReportAxes axes = makeAxesReport(motion);
  const bool sendAxes = axesReportChanged(axes);
  const bool sendButtons = (buttonBits != buttonBitsSent_);

  if (!usbHid_.ready())
  {
    return false;
  }

  // 1. Sende Achsen (Report ID 0x01)
  if (sendAxes)
  {
    usbHid_.sendReport(0x01, &axes, sizeof(axes));
    lastSentAxes_ = axes;
  }

  // 2. Sende Buttons (Report ID 0x03)
  if (sendButtons)
  {
    ReportButtons btn{};
    btn.bits = buttonBits & 0x0003;
    usbHid_.sendReport(0x03, &btn, sizeof(btn));
    buttonBitsSent_ = buttonBits;
  }

  // 3. Sende zyklisch den Batteriebericht (Report ID 0x04)
  if (millis() - lastBatterySend_ > 10000)
  {
    lastBatterySend_ = millis();

    ReportBattery bat{};
    bat.percentage = readBatteryPercentage();

    // Wenn Strom am USB anliegt, lädt das Board automatisch
    bool vbusVorhanden = (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk);
    bat.isCharging = vbusVorhanden ? 1 : 0;

    usbHid_.sendReport(0x04, &bat, sizeof(bat));
  }

  return sendAxes || sendButtons || (millis() - lastBatterySend_ < 20);
}
