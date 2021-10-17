/**
 * Uniformly Partitioned Overlap Save Convolution
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <jack/jack.h>

// Include FFTW header
#include <complex.h> //needs to be included before fftw3.h for compatibility
#include <fftw3.h>

// to read audio files
#include <sndfile.h>
SNDFILE * audio_file;
SF_INFO audio_info;
unsigned int channels = 1;

double complex *i_fft , *i_time,
      	       *ir_fft, *ir_time,
      	       *o_fft , *o_time;

double complex **fdl, **fir;

// input buffer for upols
jack_default_audio_sample_t *buffer;

float *ir, *conv;
double vol;
int ir_len, two_nframes;
fftw_plan i_forward, ir_forward, o_inverse;

int partitions;

jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;

float sample_rate;

/**
 * The process callback for this JACK application is called in a
