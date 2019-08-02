/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose. You are free to modify it and use it in any way you want,
** but you have to leave this header intact.
**
**
** led_controller.h
** A Helper class to control the LEDs of the ReSpeaker 4mic_hat for the RasPi
**
** Author: Oliver Pahl
** -------------------------------------------------------------------------*/
#include <stdint.h>

class LedController {
 public:
  // Singleton because we only ever have one
  // LED array to control in this usecase
  static LedController &GetInstance() {
    static LedController instance;
    return instance;
  }

  // Powers up the GPIO and SPI connections
  bool PowerUp(int number_of_leds);

  // Powers down and cleans up
  void PowerDown();

  // Clears all LEDs and turns them off
  void Clear();

  // Sets the color of the specified pixel and its brightness
  void SetPixelColor(int pixel, uint8_t r, uint8_t g, uint8_t b,
                     uint8_t brightness = 31);

  // Display the pixels set with SetColor
  void Show();

  // Copy constructor and operator removed for Singleton
  LedController(LedController const &) = delete;
  void operator=(LedController const &) = delete;

 private:
  LedController(){};
  bool SetGpioPower(bool power);
  bool InitSpiDevice();
  void WriteStart();
  void WriteEnd();
  void MakeTransfer(uint8_t *data, int len, int speed_in_hz, int bits_per_word);

 private:
  // LED SPI Control
  int led_spi_file_descriptor_;
  uint8_t *pixel_map_;
  int speed_in_hz_;
  uint8_t bits_per_word_;
  uint8_t spi_mode_;

  // LED GPIO Control
  int led_gpio_file_descriptor_;

  // Other
  bool powered_up_;
  int number_of_leds_;
};
