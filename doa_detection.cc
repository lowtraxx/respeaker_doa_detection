/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose. You are free to modify it and use it in any way you want,
** but you have to leave this header intact.
**
**
** doa_detection.cc
** A Helper class to compute the direction of arrival on the
** ReSpeaker 4mic_hat for the RasPi
**
** Author: Oliver Pahl
** -------------------------------------------------------------------------*/
#include <complex>
#include <vector>

// Simple rfft
#include "contrib/kiss_fft/kiss_fftr.h"

// The defines we need
static const double SOUND_SPEED = 340.0;
static const double MIC_DISTANCE_4 = 0.081;
static const double MAX_TDOA_4 = MIC_DISTANCE_4 / SOUND_SPEED;
static const double PI = 3.14159265358979323846;

// Direct port of the doa_respeaker_4mic_arry.py with
// hardcoded values for unchanging parts
double GccPhat(double sig[], double refsig[], int len) {
  // Prepare the cfg for fftr
  kiss_fftr_cfg rfft_cfg = kiss_fftr_alloc(len, 0, 0, 0);

  // Get space for the results of the transformation
  kiss_fft_cpx sig_out[len / 2 + 1];
  kiss_fft_cpx refsig_out[len / 2 + 1];

  // Do the transformation
  kiss_fftr(rfft_cfg, sig, sig_out);
  kiss_fftr(rfft_cfg, refsig, refsig_out);

  // Cross-Correlation table
  kiss_fft_cpx cc[len / 2 + 1];

  // Compute values for the Cross-Correlation table
  std::complex<double> r[len];
  std::complex<double> tmp;
  for (int i = 0; i < len; i++) {
    reinterpret_cast<double *>(r)[2 * i] =
        (sig_out[i].r * refsig_out[i].r - sig_out[i].i * -refsig_out[i].i);
    reinterpret_cast<double *>(r)[2 * i + 1] =
        (sig_out[i].r * -refsig_out[i].i + sig_out[i].i * refsig_out[i].r);
    tmp = r[i] / std::abs(r[i]);
    cc[i].r = tmp.real();
    cc[i].i = tmp.imag();
  }

  // Clear the config
  free(rfft_cfg);

  // Prepare a config for ifttr
  kiss_fftr_cfg irfft_cfg = kiss_fftr_alloc(len, 1, 0, 0);

  // Compute irfft
  double cc_irfft_res[len];
  kiss_fftri(irfft_cfg, cc, cc_irfft_res);

  // Clear the config
  free(irfft_cfg);

  // Build the Cross-Correlation result array
  double cc_result[7];
  cc_result[0] = std::abs(cc_irfft_res[len - 3]);
  cc_result[1] = std::abs(cc_irfft_res[len - 2]);
  cc_result[2] = std::abs(cc_irfft_res[len - 1]);
  cc_result[3] = std::abs(cc_irfft_res[0]);
  cc_result[4] = std::abs(cc_irfft_res[1]);
  cc_result[5] = std::abs(cc_irfft_res[2]);
  cc_result[6] = std::abs(cc_irfft_res[3]);

  // Find the maxximum in the result array and
  // note its position in the array
  int pos = -1;
  double curr = cc_result[0];
  for (int i = 1; i < 7; i++) {
    if (curr < cc_result[i]) {
      curr = cc_result[i];
      pos = i;
    }
  }

  // compute tau and return it
  return (pos - 3) / 16000.0;
}

// Compute the modulo, but wrap-around at 360 degree
double FmodWrap(double x, double y) {
  if (x < 0) x += 360;

  return std::fmod(x, y);
}

// Get the direction as a value between 1 and 360 degree
double GetDirection(std::vector<int16_t> &audio_buffer_4_channels) {
  // Get the item count
  int buffer_items_count = audio_buffer_4_channels.size();

  // Get the buffer size per channel (we are using 4 from the 4mics_hat)
  int channel_buffer_size = (buffer_items_count - buffer_items_count % 4) / 4;

  // Create the channels for each mic and fill them with data
  double channel_1[channel_buffer_size], channel_2[channel_buffer_size],
      channel_3[channel_buffer_size], channel_4[channel_buffer_size];
  for (int i = 0, j = 0; j < channel_buffer_size; i += 4, j++) {
    channel_1[j] = audio_buffer_4_channels[i];
    channel_2[j] = audio_buffer_4_channels[i + 1];
    channel_3[j] = audio_buffer_4_channels[i + 2];
    channel_4[j] = audio_buffer_4_channels[i + 3];
  }

  // Get tau and theta for the two channel combinations
  double tau1 = GccPhat(channel_1, channel_3, channel_buffer_size);
  double theta1 = asin(tau1 / MAX_TDOA_4) * 180.0 / PI;

  double tau2 = GccPhat(channel_2, channel_4, channel_buffer_size);
  double theta2 = asin(tau2 / MAX_TDOA_4) * 180.0 / PI;

  // Use the results for best effort computation of the DoA
  double best_guess = 0.0;
  if (std::abs(theta1) < std::abs(theta2)) {
    if (theta2 > 0)
      best_guess = FmodWrap((theta1 + 360.0), 360.0);
    else
      best_guess = (180.0 - theta1);
  } else {
    if (theta1 < 0)
      best_guess = FmodWrap((theta2 + 360.0), 360.0);
    else
      best_guess = (180.0 - theta2);
    best_guess = FmodWrap((best_guess + 270.0), 360.0);
  }
  best_guess = FmodWrap((-best_guess + 120.0), 360.0);

  return best_guess;
}
