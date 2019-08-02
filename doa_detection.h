/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose. You are free to modify it and use it in any way you want,
** but you have to leave this header intact.
**
**
** doa_detection.h
** A Helper class to compute the direction of arrival on the
** ReSpeaker 4mic_hat for the RasPi
**
** Author: Oliver Pahl
** -------------------------------------------------------------------------*/

// Get the direction as a value between 1 and 360 degree
double GetDirection(std::vector<int16_t> &audio_buffer_4_channels);
