/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose. You are free to modify it and use it in any way you want,
** but you have to leave this header intact.
**
**
** led_controller.cc
** A Helper class to control the LEDs of the ReSpeaker 4mic_hat for the RasPi
**
** Author: Oliver Pahl
** -------------------------------------------------------------------------*/
#include "led_controller.h"

#include <linux/gpio.h>
#include <linux/spi/spidev.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

bool LedController::PowerUp(int number_of_leds) {
  if(!powered_up_) {
    // Power on the LED via GPIO
    led_gpio_file_descriptor_ = -1;
    if (!SetGpioPower(true)) return false;

    // Init the SPI device
    led_spi_file_descriptor_ = -1;
    if (!InitSpiDevice()) return false;

    // Allocate our pixel map
    pixel_map_ = new uint8_t[number_of_leds * 4];

    // Set the now known number of leds
    number_of_leds_ = number_of_leds;

    // We are initialized
    powered_up_ = true;

    return true;
  } else {
    std::cout << "Already powered up." << std::endl;
    return false;
  }
}

bool LedController::InitSpiDevice() {
  // Try to open the SPI device
  led_spi_file_descriptor_ = open("/dev/spidev0.1", O_RDWR, 0);
  if (led_spi_file_descriptor_ < 0) {
    std::cout << "Failed to open LED SPI device" << std::endl;
    return false;
  }

  // Try to get the SPI to write mode
  int ioctl_ret_val = 0;
  if ((ioctl_ret_val =
           ioctl(led_spi_file_descriptor_, SPI_IOC_RD_MODE, &spi_mode_)) < 0)
    std::cout << "Failed to get write mode on the LED SPI device" << std::endl;

  // Get bits per word
  if ((ioctl_ret_val = ioctl(led_spi_file_descriptor_, SPI_IOC_RD_BITS_PER_WORD,
                             &bits_per_word_)) < 0)
    std::cout << "Failed to get bits per word on the LED SPI device"
              << std::endl;

  // Get the speed
  if ((ioctl_ret_val = ioctl(led_spi_file_descriptor_, SPI_IOC_RD_MAX_SPEED_HZ,
                             &speed_in_hz_)) < 0)
    std::cout << "Failed to get desired speed on the LED SPI device"
              << std::endl;

  // Try to set it to fast, so we can update the LEDs faster
  speed_in_hz_ = 8000000;
  if ((ioctl_ret_val = ioctl(led_spi_file_descriptor_, SPI_IOC_WR_MAX_SPEED_HZ,
                             &speed_in_hz_)) < 0)
    std::cout << "Failed to set desired speed on the LED SPI device"
              << std::endl;

  return true;
}

bool LedController::SetGpioPower(bool power) {
  // Set the power state (1 for on 0 for off)
  struct gpiohandle_data data;
  data.values[0] = power ? 1 : 0;

  // Open the GPIO device
  struct gpiohandle_request led_gpio_request;

  // Check if the file descriptor is already open
  if(led_gpio_file_descriptor_ < 0) {
    led_gpio_file_descriptor_ = open("/dev/gpiochip0", 0);
    if (led_gpio_file_descriptor_ < 0) {
      std::cout << "Failed to open LED GPIO device" << std::endl;
      return false;
    }
  }

  // Set the flags needed in the request
  led_gpio_request.flags = GPIOHANDLE_REQUEST_OUTPUT;
  strcpy(led_gpio_request.consumer_label, "LED Controller");
  led_gpio_request.lineoffsets[0] = 5;  // 5 Controls the LED power
  led_gpio_request.lines = 1;           // We need one request line
  memcpy(led_gpio_request.default_values, &data,
         sizeof(led_gpio_request.default_values));

  // Try to send the power command
  int ioctl_ret_val = 0;
  if ((ioctl_ret_val = ioctl(led_gpio_file_descriptor_,
                             GPIO_GET_LINEHANDLE_IOCTL, 
                             &led_gpio_request)) < 0) {
    std::cout << "Failed to send power" << (power ? " on " : " off ")
              << "signal to the GPIO" << std::endl;
    return false;
  }

  return true;
}

void LedController::SetPixelColor(int pixel, uint8_t r, uint8_t g, uint8_t b,
                                  uint8_t brightness) {
  if (!powered_up_) {
    std::cout << "Not powered up. Please call PowerUp first." << std::endl;
    return;
  }

  if (pixel < 0 || pixel >= number_of_leds_) {
    std::cout << "The addressed pixel does not exist." << std::endl;
    return;
  }

  // Make sure we do not set the brightness above maximum
  if (brightness > 31 || brightness < 0) brightness = 31;

  // Set the Pixel
  int start_index = pixel * 4;
  pixel_map_[start_index] = (brightness & 0b00011111) | 0b11100000;
  pixel_map_[start_index + 1] = b;
  pixel_map_[start_index + 2] = g;
  pixel_map_[start_index + 3] = r;
}

void LedController::Clear() {
  if (!powered_up_) {
    std::cout << "Not powered up. Please call PowerUp first." << std::endl;
    return;
  }

  int brightness = 31;
  for (int i = 0; i < number_of_leds_; i++) {
    int start_index = i * 4;
    pixel_map_[start_index] = (brightness & 0b00011111) | 0b11100000;
    pixel_map_[start_index + 1] = 0;
    pixel_map_[start_index + 2] = 0;
    pixel_map_[start_index + 3] = 0;
  }

  Show();
}

void LedController::Show() {
  if (!powered_up_) {
    std::cout << "Not powered up. Please call PowerUp first." << std::endl;
    return;
  }

  WriteStart();
  MakeTransfer(pixel_map_, number_of_leds_ * 4, speed_in_hz_, bits_per_word_);
  WriteEnd();
}

void LedController::WriteStart() {
  // Write Begin Segment
  uint8_t start_segment_buffer[4];
  memset(start_segment_buffer, 0, 4);
  MakeTransfer(start_segment_buffer, 4, speed_in_hz_, bits_per_word_);
}

void LedController::WriteEnd() {
  // Write End Segment
  uint8_t end_segment_buffer[1];
  end_segment_buffer[0] = 0x00;
  MakeTransfer(end_segment_buffer, 1, speed_in_hz_, bits_per_word_);
}

void LedController::MakeTransfer(uint8_t *data, int len, int speed_in_hz,
                                 int bits_per_word) {
  // Initialize receive data. This should not be necessary anymore
  uint8_t *receive_data = new uint8_t[len];

  // Set the transfer components
  struct spi_ioc_transfer spi_transfer;
  memset(&spi_transfer, 0, sizeof(spi_transfer));
  spi_transfer.tx_buf = (unsigned long)data;
  spi_transfer.rx_buf = (unsigned long)receive_data;
  spi_transfer.len = len;
  spi_transfer.delay_usecs = 0;
  spi_transfer.speed_hz = speed_in_hz;
  spi_transfer.bits_per_word = bits_per_word;

  // Transfer the message
  int ioctl_ret_val =
      ioctl(led_spi_file_descriptor_, SPI_IOC_MESSAGE(1), &spi_transfer);
  if (ioctl_ret_val < 0)
    std::cout << "Failed to make data transfer to the SPI LED device"
              << std::endl;

  // UNUSED: Read the received data back
  if (spi_mode_ & SPI_CS_HIGH)
    ssize_t ret = read(led_spi_file_descriptor_, &receive_data[0], 0);

  delete receive_data;
}

void LedController::PowerDown() {
  if(powered_up_) {
    // Clear the LEDs
    Clear();

    // Close the SPI connection
    if (led_spi_file_descriptor_ >= 0) {
      close(led_spi_file_descriptor_);
      led_spi_file_descriptor_ = -1;
    }

    // Power off GPIO and close the connection
    if (led_gpio_file_descriptor_ >= 0) {

      // Setting the Gpio to power down does not work
      // currently, as it does not accept the power state off.
      // Closing the fd seems to accomplish the same thing,
      // so I am commenting this for now.
      // SetGpioPower(false);

      close(led_gpio_file_descriptor_);
      led_gpio_file_descriptor_ = -1;
    }

    // Cleanup the pixel map
    if (pixel_map_) {
      delete pixel_map_;
      pixel_map_ = nullptr;
    }

    // Reset the members
    spi_mode_ = 0;
    bits_per_word_ = 0;
    speed_in_hz_ = 0;
    number_of_leds_ = 0;
    powered_up_ = false;
  }
}
