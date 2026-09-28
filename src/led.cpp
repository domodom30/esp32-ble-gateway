#include <Arduino.h>
#include "led.h"
#include "gw_settings.h"
#include "noble_api.h"

bool LedIndicator::lastState = false;

void LedIndicator::init()
{
  pinMode(ESP_GW_LED2_PIN, OUTPUT);
  applyState(false);
}

void LedIndicator::applyState(bool on)
{
  if (on == lastState)
  {
    return;
  }
  lastState = on;
  digitalWrite(ESP_GW_LED2_PIN, on ? HIGH : LOW);
}

void LedIndicator::loop()
{
  if (!GwSettings::getLed2Enabled())
  {
    applyState(false);
    return;
  }

  uint32_t now = millis();

  if (NobleApi::isHaConnected())
  {
    // Bref flash toutes les 10 s (ON pendant ~80 ms, OFF le reste du temps).
    static uint32_t cycleStart = 0;
    uint32_t phase = now - cycleStart;
    if (phase >= 10000)
    {
      cycleStart = now;
      phase = 0;
    }
    applyState(phase < 80);
  }
  else
  {
    // Clignotement rapide : toggle toutes les 120 ms.
    static uint32_t lastToggle = 0;
    static bool blinkState = false;
    if (now - lastToggle >= 120)
    {
      lastToggle = now;
      blinkState = !blinkState;
      applyState(blinkState);
    }
  }
}
