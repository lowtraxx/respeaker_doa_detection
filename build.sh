#!/bin/bash
gcc contrib/kiss_fft/kiss_fft.c contrib/kiss_fft/kiss_fftr.c contrib/led_controller/led_controller.cc doa_detection.cc doa_detection_sample.cc -lasound -lm -lstdc++ -Lcontrib/snowboy/lib/ -lsnowboy-detect -L/usr/lib/atlas-base -lf77blas -lcblas -llapack_atlas -latlas -D_GLIBCXX_USE_CXX11_ABI=0 -pg
