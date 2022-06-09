#include "autojackio.h"
#include <math.h>

double quant;
int sampling_factor;
int k = 0;

int jack_callback (jack_nframes_t nframes, void *arg){
	int j;
	for (j = 0; j < nframes; j++){
		in[0] =  (jack_default_audio_sample_t*) jack_port_get_buffer ( input_port[0], nframes);
		out[0] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port[0], nframes);
		
		// 1: sample rate reduction
		in[0][j] = in[0][j - k];
    k = (k+1)%sampling_factor;
		
		// 2: quantize	
		out[0][j] = round(quant*in[0][j]) / quant;
	}
	return 0;
}

 
int main (int argc, char *argv[]) {
	const char *client_name = "bit_crusher";
	num_channels = 2;
	working      = 1;
	
	if (argc != 3) {
		printf("Syntax is:\n");
		printf("\t ./simple_delay.c [BITS] [SAMPLING REDUCTION] \n");
		exit(1);
	}
	
	double bits     = atof(argv[1]);
	sampling_factor = atoi(argv[2]);
	
	if ((bits < 0) | (bits > 24)){
		printf("Bit %f out of range 0-16\n.\n", bits);
		exit(1);
	}

	if ((sampling_factor < 1) | (sampling_factor > 16)){
		printf("Sampling rate out of range (0-8)");
		exit(1);
	}

	quant = pow(2, bits);

	initialize_client(client_name, num_channels);
	
	/* automated client setup
	 * signal waits for CTRL+C to fire up a 
	 * shutdown script that can free a number
	 * of variables for extreme optimization
	*/	
	connect_ports();
	signal(SIGINT, INThandler);
	
	/* keep running until stopped by the user
	 * usleep works in microseconds. 10 seems
	 * to be a good choice.
	 */
	do{usleep (10);} while (working == 1);
	
	jack_client_close (client);
	printf("\nGoodbye!\n");
	exit (0);
}
