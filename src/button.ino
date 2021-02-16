boolean brightDirection;
static bool startButtonHolding = false; 

void buttonTick() {
  touch.tick();
  uint8_t clickCount = touch.hasClicks() ? touch.getClicks() : 0U;

  // if (touch.isSingle()) {
  if (clickCount == 1U) {
    if (dawnFlag) {
      manualOff = true;
      dawnFlag = false;
      loadingFlag = true;
      FastLED.setBrightness(modes[currentMode].brightness);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
      changePower();
    } else {
      if (ONflag) {
        Serial.println("ONflag OFF");
        // changePower();
        eepromTick();
        esp_deep_sleep_start();
        digitalWrite(LED_ENABLE_PIN, LOW);
      } else {
        digitalWrite(LED_ENABLE_PIN, HIGH);
        Serial.println("ONflag ON");
        changePower();
      }
      ONflag = !ONflag;
    }
  }

  // if (ONflag && touch.isDouble()) {
  if (ONflag && clickCount == 2U) {
    if (currentMode == 3) ++currentMode;
    if (++currentMode >= MODE_AMOUNT) currentMode = 0;
    FastLED.setBrightness(modes[currentMode].brightness);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
    loadingFlag = true;
    settChanged = true;
    eepromTimer = millis();
    FastLED.clear();
    delay(1);
  }
  // if (ONflag && touch.isTriple()) {
  if (ONflag && clickCount == 3U) {
    if (currentMode == 5) --currentMode;
    if (--currentMode < 0) currentMode = MODE_AMOUNT - 1;
    FastLED.setBrightness(modes[currentMode].brightness);
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
    loadingFlag = true;
    settChanged = true;
    eepromTimer = millis();
    FastLED.clear();
    delay(1);
  }

  if (ONflag && touch.isHolded()) {
    brightDirection = !brightDirection;
    startButtonHolding = true;
  }
  // if (ONflag && touch.isStep()) {
  //   if (brightDirection) {
  //     if (modes[currentMode].brightness < 10) modes[currentMode].brightness += 1;
  //     else if (modes[currentMode].brightness < 250) modes[currentMode].brightness += 5;
  //     else modes[currentMode].brightness = 255;
  //   } else {
  //     if (modes[currentMode].brightness > 15) modes[currentMode].brightness -= 5;
  //     else if (modes[currentMode].brightness > 1) modes[currentMode].brightness -= 1;
  //     else modes[currentMode].brightness = 1;
  //   }
  //   FastLED.setBrightness(modes[currentMode].brightness);
  // if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);
  //   settChanged = true;
  //   eepromTimer = millis();
  // }
  if (ONflag && touch.isStep())
  {
    switch (touch.getHoldClicks())
    {
      case 0U:                                              // просто удержание (до удержания кнопки кликов не было) - изменение яркости
      {
        uint8_t delta = modes[currentMode].brightness < 10U // определение шага изменения яркости: при яркости [1..10] шаг = 1, при [11..16] шаг = 3, при [17..255] шаг = 15
          ? 1U
          : 5U;
        modes[currentMode].brightness =
          constrain(brightDirection
            ? modes[currentMode].brightness + delta
            : modes[currentMode].brightness - delta,
          1, 255);
        FastLED.setBrightness(modes[currentMode].brightness);

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("New brightness: %d\n"), modes[currentMode].brightness);
        #endif

        break;
      }

      case 1U:                                              // удержание после одного клика - изменение скорости
      {
        modes[currentMode].speed = constrain(brightDirection ? modes[currentMode].speed + 1 : modes[currentMode].speed - 1, 1, 255);

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("New speed: %d\n"), modes[currentMode].speed);
        #endif

        break;
      }

      case 2U:                                              // удержание после двух кликов - изменение масштаба
      {
        modes[currentMode].scale = constrain(brightDirection ? modes[currentMode].scale + 1 : modes[currentMode].scale - 1, 1, 100);

        #ifdef GENERAL_DEBUG
        LOG.printf_P(PSTR("New scale: %d\n"), modes[currentMode].scale);
        #endif

        break;
      }

      default:
        break;
    }

    settChanged = true;
    eepromTimer = millis();
  }
  
  if (ONflag && !touch.isHold() && startButtonHolding)
  {
    startButtonHolding = false;
    loadingFlag = true;
  }
}
