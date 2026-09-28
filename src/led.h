#ifndef ESP_GW_LED_H
#define ESP_GW_LED_H

#ifndef ESP_GW_LED2_PIN
#define ESP_GW_LED2_PIN 2
#endif

class LedIndicator
{
public:
  static void init();
  static void loop();

private:
  static void applyState(bool on);
  static bool lastState;
};

#endif
