#include "autojackio.h"
#include <math.h>

double input_gain, output_gain;

int jack_callback (jack_nframes_t nframes, void *arg){
	int j;
	for (j = 0; j < nframes; j++){
		in[0] =  (jack_default_audio_sample_t*) jack_port_get_buffer ( input_port[0], nframes);
		out[0] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port[0], nframes);
    
    // 0.2 = 1/5 is approximately half the amplitude of a typical JACK signal.
		out[0][j] = output_gain * 0.2* tanh(5*input_gain*in[0][j]);
	}
	return 0;
}

 
int main (int argc, char *argv[]) {
	const char *client_name = "soft_clipper";
	num_channels = 2;
	working      = 1;
	
	if (argc != 3) {
		printf("Syntax is:\n");
		printf("\t ./soft_clipper.c [INPUT GAIN (-12 / 12 dB)] [OUTPUT GAIN (-6 / 6 dB)] \n");
		exit(1);
	}
	
	input_gain    = atof(argv[1]);
	output_gain   = atof(argv[2]);

	if ((input_gain < -24) | (input_gain > 24)){
		printf("Input gain out of range.\n");
		exit(1);
	}
	
	if ((output_gain < -12) | (output_gain > 12)){
		printf("Output gain out of range.\n");
		exit(1);
	}


	printf("Input  gain in dB: %f \n", input_gain);
	printf("Output gain in dB: %f \n", output_gain);
	
	input_gain  = pow(10, input_gain/20);
	output_gain = pow(10, output_gain/20);
	
	printf("Relative input  gain: %f\n",  input_gain);
	printf("Relative output gain: %f\n", output_gain);
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
