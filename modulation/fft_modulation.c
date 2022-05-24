/**
 * A simple example of how to do FFT with FFTW3 and JACK.
 */


#include <math.h>
#include <time.h>
#include "autojackio.h"

// Include FFTW header
#include <complex.h> //needs to be included before fftw3.h for compatibility
#include <fftw3.h>

double complex *i_fft, *i_time, *o_fft, *o_time;
float *b[2], *delay_buffer,
       *hann_window, *freqs;

int buffer_index = 0;

double t, delta_t, two_nframes, theta, 
       freq, width, depth, wow;

int freq_index;
fftw_plan i_forward, o_inverse;

double sample_rate;

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 */



int jack_callback (jack_nframes_t nframes, void *arg){
	int i;
	double t = width*0.5*(1+sin(theta));
	double wobble;
       	
	for (int i = 0; i < num_channels; i++){
		in[i] =  (jack_default_audio_sample_t*) jack_port_get_buffer ( input_port[i], nframes);
                out[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port[i], nframes);
                }
	
	for (i = 0; i < nframes; i++){
		b[1][2*nframes + i] = in[0][i];
	
		i_time[i] = hann_window[i]*b[1][nframes+i];
		i_time[nframes+i] = hann_window[nframes + i]*b[1][2*nframes+i];
	}

	fftw_execute(i_forward);
	for (i = 0; i < 2*nframes; i++){
		o_fft[i] = i_fft[i]*cexp(-2*I*M_PI*freqs[i]*t);
	}
	
	fftw_execute(o_inverse);
	for (i = 0; i < nframes; i++){
		b[1][nframes + i] = hann_window[i]*(creal(o_time[i])/(2*nframes));
		b[1][2*nframes + i] = hann_window[nframes + i]*(creal(o_time[nframes+i])/(2*nframes));
		
		out[0][i] = 0.5*(delay_buffer[buffer_index] + (b[0][nframes + i] + b[1][nframes + i]));
		out[1][i] = 0.5*(delay_buffer[buffer_index] - (b[0][nframes + i] + b[1][nframes + i]));

		delay_buffer[buffer_index] = in[0][i];
		++buffer_index;
		buffer_index = buffer_index % (nframes);

		
		b[0][i] = b[1][nframes + i];
		b[0][nframes + i] = b[1][2*nframes+i];
		
		b[1][nframes+i] = in[0][i];
	}
	
	srand((unsigned int)time(NULL));
	wobble = ((double) rand()/(double)(RAND_MAX)) * (wow * delta_t);
	theta += 2*M_PI*freq*(delta_t + wobble);
	if (theta > 2*M_PI){
                        theta -= 2*M_PI;
                }
	
	return 0;
}


int main (int argc, char *argv[]) {
	const char *client_name = "fft modulation";
	num_channels = 2;
	working = 1;
	theta = 0;

	if (argc != 5) {
                printf("Syntax is:\n");
                printf("\t ./fft_modulation.c [RATE (Hz)] [DELAY TIME (ms)] [DEPTH (0-1)] [WOW & FLUTTER (0 - 10)]\n");
                exit(1);
        }

	freq  = atof(argv[1]); // frequency in Hz
	width = atof(argv[2]); // width in samples
	depth = atof(argv[3]); // depth range of 0 to 1
	wow   = atof(argv[4]); // wow & flutter range of 0 to 10

	if ( (freq <= 0) | (freq > 10)){
		printf("Rate out of range (0, 10] (hz)\n");
		exit(1);
	}
	if ( (width <= 0) | (width > 100)){
		printf("Width out of range (0, 100] (ms)\n");
		exit(1);
	}
	if ( (depth < 0) | (depth > 1)){
		printf("Depth out of range (0, 1] (mix) \n");
		exit(1);
	}
	if ( (wow < 0) | (wow > 10)){
		printf("Wow & FLutter out of range (0, 10] (mix) \n");
		exit(1);
	}


	initialize_client(client_name, num_channels);
        sample_rate = (float)jack_get_sample_rate(client);
        int nframes = jack_get_buffer_size (client);
        two_nframes = 2*nframes;
	delta_t = 1/sample_rate;


	//preparing FFTW3 buffers
	for (int i = 0; i < 2; i++){
		b[i] = calloc(3*nframes, sizeof(jack_default_audio_sample_t));
		delay_buffer = calloc(nframes, sizeof(jack_default_audio_sample_t));
	}
	hann_window = (float*) malloc(2*nframes*sizeof(float));
	freqs       = (float*) malloc(2*nframes*sizeof(float));
	for (int i = 0; i < 2*nframes; i++){
		hann_window[i] = sin( M_PI*i/(2*nframes-1) );
	}
	double max_freq = sample_rate/2;
	for (int i = 0; i < nframes; i++){
		freqs[i] = i*max_freq/two_nframes;
		freqs[2*nframes-i-1] = -i*max_freq/two_nframes;
	}
	freqs[nframes] = max_freq;
	printf("freq %d: %0.2f ", nframes, freqs[nframes]);

	i_fft = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	i_time = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	o_fft = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	o_time = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);

	if (b[0] == NULL ) {
		printf("Could not create fftw buffers.\n");
		exit(1);
	}
	
	i_forward = fftw_plan_dft_1d(2*nframes, i_time, i_fft , FFTW_FORWARD, FFTW_MEASURE);
	o_inverse = fftw_plan_dft_1d(2*nframes, o_fft , o_time, FFTW_BACKWARD, FFTW_MEASURE);
	
	connect_ports();

        signal(SIGINT, INThandler);

        /* keep running until stopped by the user */
        do{usleep (10);} while (working == 1);
	
	jack_client_close (client);
	printf("\nGoodbye!\n");

	exit (0);
}
