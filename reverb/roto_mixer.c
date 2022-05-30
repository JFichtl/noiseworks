/**
 * A simple 1-input to 1-output JACK client.
 */
#include "autojackio.h"
#include <math.h>
double **mixing_matrix; 
double theta, step, c, s;

int jack_callback (jack_nframes_t nframes, void *arg){
	for (int i = 0; i < num_channels; i++){
		in[i] =  (jack_default_audio_sample_t*) jack_port_get_buffer ( input_port[i], nframes);
		out[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port[i], nframes);
		}
	for (int j = 0; j < nframes; j++){
		c = cos(theta);
		s = sin(theta); 
		out[0][j] =  in[0][j]*c + in[1][j]*s;
		out[1][j] = -in[0][j]*s + in[1][j]*c;
		
		theta += step/nframes;
		if (theta > 2*M_PI){
			theta -= 2*M_PI;
		}
	}
	return 0;
}

 
int main (int argc, char *argv[]) {
	const char *client_name = "stereo_mixer";
	num_channels = 2;
	working = 1;
	theta = 0;
	//step  = 0.1;
	
	if (argc != 2) {
		printf("Syntax is:\n");
		printf("\t ./simple_mixer.c [ANGLE]\n");
		exit(1);
	}
	
	step = atof(argv[1]);
	if (step < -0.5 || step > 0.5){
		printf("Step too large!.\n");
		exit(1);
	}
	/*
	theta = (2*M_PI/360) * theta;
	printf("Angle: %.3f\n", theta);
	
	mixing_matrix = (double**) malloc(num_channels * sizeof(double*));
	for (int i = 0; i < num_channels; i++){
		mixing_matrix[i] = (double*) malloc(num_channels * sizeof(double));
	}
	mixing_matrix[0][0] =  cos(theta); mixing_matrix[0][1] = sin(theta);
	mixing_matrix[1][0] = -sin(theta); mixing_matrix[1][1] = cos(theta);
	*/

	/* automated client setup
	 * signal waits for CTRL+C to fire up a 
	 * shutdown script that can free a number
	 * of variables for extreme optimization
	*/	
	initialize_client(client_name, num_channels);
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
