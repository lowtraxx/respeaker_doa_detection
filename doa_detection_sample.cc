/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose. You are free to modify it and use it in any way you want,
** but you have to leave this header intact.
**
**
** doa_detection_sample.cc
** A sample to demonstrate the usage of the doa_detection using snowboy
** hotword detection and listening to 'jarvis'.
**
** Author: Oliver Pahl
** -------------------------------------------------------------------------*/
#include <signal.h>
#include <fstream>
#include <iostream>
#include <vector>

// snowboy
#include "contrib/snowboy/include/snowboy-detect.h"

// LED controller
#include "contrib/led_controller/led_controller.h"

// ALSA lib
#include <alsa/asoundlib.h>

// DoA detection
#include "doa_detection.h"

// This returns a default string currently, because the
// seeed ALSA driver has an issue where it does not report
// the name of the PCM device
const char *Get4MicHatPcmDevice() {
  register int error;
  int card_num = -1;
  char card_id_string[64];
  int pcm_device_id = -1;

  // ALSA parameters
  snd_pcm_t *pcm_handle;
  snd_pcm_info_t *pcm_info;
  snd_pcm_info_alloca(&pcm_info);
  snd_ctl_t *card_handle;
  snd_ctl_card_info_t *card_info;
  snd_ctl_card_info_alloca(&card_info);

  // The PCM device name we want to get
  const char *pcm_device_name;

  while (snd_card_next(&card_num) == 0 && card_num >= 0) {
    // Get the card id
    sprintf(card_id_string, "hw:%d", card_num);
    if ((error = snd_ctl_open(&card_handle, card_id_string, 0)) < 0) {
      std::cout << "Can't open card " << card_num << " : "
                << snd_strerror(error) << std::endl;
      continue;
    }

    // Get the Soundcard Info
    if ((error = snd_ctl_card_info(card_handle, card_info)) < 0) {
      snd_ctl_close(card_handle);
      continue;
    }

    // Iterate over the PCM devices
    while (snd_ctl_pcm_next_device(card_handle, &pcm_device_id) == 0 &&
           pcm_device_id >= 0) {
      // Get device info
      snd_pcm_info_set_device(pcm_info, pcm_device_id);
      snd_pcm_info_set_subdevice(pcm_info, 0);
      snd_pcm_info_set_stream(pcm_info, SND_PCM_STREAM_CAPTURE);

      // If we cannot use it for recording, discard
      if (snd_ctl_pcm_info(card_handle, pcm_info) < 0)
        continue;
      else
        pcm_device_name = snd_pcm_info_get_name(pcm_info);
    }

    // Close the card's control interface after we're done with it
    snd_ctl_close(card_handle);
  }

  // Free the memory we used
  snd_config_update_free_global();

  // Due to a bug
  // (https://forum.seeedstudio.com/viewtopic.php?f=87&t=32458&sid=555a99ad0a9374ef0332af489a5eb9db)
  // in the driver, there is no pcm device name to get, which leads to the
  // problem, that we cannot open the device by name. For the time being, we use
  // the default capture device, but this may not be sufficent in the future.
  // return pcm_device_name;
  return "default";
}

snd_pcm_t *InitializeAlsaDevice(const char *pcm_device_name) {
  int err;
  snd_pcm_t *capture_handle = nullptr;
  snd_pcm_hw_params_t *hw_params;

  if ((err = snd_pcm_open(&capture_handle, pcm_device_name,
                          SND_PCM_STREAM_CAPTURE, 0)) < 0) {
    std::cerr << "cannot open audio device " << pcm_device_name << "("
              << snd_strerror(err) << ")" << std::endl;
    return nullptr;
  }

  if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
    std::cerr << "cannot allocate hardware parameter structure ("
              << snd_strerror(err) << ")" << std::endl;
    return nullptr;
  }

  if ((err = snd_pcm_hw_params_any(capture_handle, hw_params)) < 0) {
    std::cerr << "cannot initialize hardware parameter structure ("
              << snd_strerror(err) << ")" << std::endl;
    return nullptr;
  }

  if ((err = snd_pcm_hw_params_set_access(capture_handle, hw_params,
                                          SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    std::cerr << "cannot set access type (" << snd_strerror(err) << ")"
              << std::endl;
    return nullptr;
  }

  if ((err = snd_pcm_hw_params_set_format(capture_handle, hw_params,
                                          SND_PCM_FORMAT_S16_LE)) < 0) {
    std::cerr << "cannot set sample format (" << snd_strerror(err) << ")"
              << std::endl;
    return nullptr;
  }

  unsigned int rate = 16000;
  if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate,
                                             0)) < 0) {
    std::cerr << "cannot set sample rate (" << snd_strerror(err) << ")"
              << std::endl;
    return nullptr;
  }

  if ((err = snd_pcm_hw_params_set_channels(capture_handle, hw_params, 4)) <
      0) {
    std::cerr << "cannot set channel count (" << snd_strerror(err) << ")"
              << std::endl;
    return nullptr;
  }

  if ((err = snd_pcm_hw_params(capture_handle, hw_params)) < 0) {
    std::cerr << "cannot set parameters (" << snd_strerror(err) << ")"
              << std::endl;
    return nullptr;
  }

  snd_pcm_hw_params_free(hw_params);

  if ((err = snd_pcm_prepare(capture_handle)) < 0) {
    std::cerr << "cannot prepare audio interface for use (" << snd_strerror(err)
              << ")" << std::endl;
    return nullptr;
  }

  return capture_handle;
}

// Interruption Signal Handler, so we clean up after Ctrl+C
void IntSignalHandler(int sig) {
  LedController *led_controller = &LedController::GetInstance();
  led_controller->PowerDown();
  exit(0);
}

// Test with a file as input
int main() {
  // Install the signal handler
  signal(SIGINT, IntSignalHandler);

  // Get the LED Controller and power it up
  LedController *led_control = &LedController::GetInstance();
  led_control->PowerUp(12);

  const char *pcm_device = Get4MicHatPcmDevice();
  snd_pcm_t *capture_handle = InitializeAlsaDevice(pcm_device);

  if (capture_handle) {
    // Make snowboy ready using jarvis as hotword
    std::string resource_filename = "contrib/snowboy/resources/common.res";
    std::string model_filename = "contrib/snowboy/resources/models/jarvis.umdl";
    std::string sensitivity_str = "0.8,0.80";
    float audio_gain = 1;
    bool apply_frontend = true;

    // Initializes Snowboy detector.
    snowboy::SnowboyDetect detector(resource_filename, model_filename);
    detector.SetSensitivity(sensitivity_str);
    detector.SetAudioGain(audio_gain);
    detector.ApplyFrontend(apply_frontend);

    int size_of_sample = 4096;
    std::vector<int16_t> buffer(size_of_sample * 4 * sizeof(short) /
                                sizeof(int16_t));
    bool capture_running = true;
    while (capture_running) {
      int err;
      if ((err = snd_pcm_readi(capture_handle, (char *)buffer.data(),
                               size_of_sample)) != size_of_sample) {
        fprintf(stderr, "read from audio interface failed (%s)\n",
                snd_strerror(err));
        capture_running = false;
        continue;
      } else {
        // Create the channels for each mic and fill them with data
        std::vector<int16_t> channel_1(buffer.size() / 4);
        for (int i = 0, j = 0; j < buffer.size() / 4; i += 4, j++) {
          channel_1[j] = buffer[i];
        }
        int result = detector.RunDetection(channel_1.data(), channel_1.size());
        if (result > 0) {
          double best_guess = GetDirection(buffer);

          // If we have an LED controller, Paint the pixels accordingly
          int best_guess_pixel = (int)(best_guess / 30.0);
          led_control->Clear();

          // Set all Pixel to green
          for (int i = 0; i < 12; i++) {
            led_control->SetPixelColor(i, 0, 24, 0, 1);
          }

          // Set the Pixel before and after the computed one to a lower
          // brightness
          led_control->SetPixelColor(
              best_guess_pixel == 0 ? 11 : (best_guess_pixel - 1) % 12, 0, 0,
              48, 1);
          led_control->SetPixelColor(best_guess_pixel, 0, 0, 48, 31);
          led_control->SetPixelColor((best_guess_pixel + 1) % 12, 0, 0, 48, 1);
          led_control->Show();

          std::cout << "Hotword " << result << " detected!" << std::endl;
          std::cout << "direction estimate is: " << best_guess << std::endl;
        }
      }
    }

    // Close the soundcard handle
    snd_pcm_close(capture_handle);

    // Power Down the LED ring
    led_control->PowerDown();
  }
}
