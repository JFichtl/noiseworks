/* Use the generic makefile to compile. Just type in "make TARGET=schroeder_logan_reverb" and go nuts.
*  Usually called the Schroeder reverb, This was actually developed by TWO people,
*  Manfred Schroeder and Ben Logan, thus, Schroeder-Logan (SL) reverberator. The 'all-pass'
*  filter they used isn't actually a garden variety all-pass filter; it's a mix between the
*  dry and delayed signal that achieves unity gain across the frequency spectrum. The five stages
*  are the same as in the original paper, giving the user onyl the option to stretch the time array
*  for some pretty interesting effects. The SL reverb doesn't actually eliminate flutter echoes, 
*  as can be heard in the reverb tail. It is, however, the grand-daddy of almost all algorithmic
*  reverberators on the market. Feel free to improve upon it.
*/

#include "autojackio.h"
#include <math.h>
double ***i_buffer;
double ***o_buffer;
int     **read_index, **write_index;
int      *delay_samples;
double    g, g2;

int stages = 5;
float delay_times[] = {0.100, 0.068, 0.060, 0.0197, 0.00585};
float loop_gains[]  = {0.7  , -0.7, 0.7 , 0.7   , 0.7   };


int jack_callback (jack_nframes_t nframes, void *arg){
	int i, j, k;
	for (i = 0; i < num_channels; i++){
		in[i] =  (jack_default_audio_sample_t*) jack_port_get_buffer ( input_port[i], nframes);
		out[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_port[i], nframes);
		
		// All pass stages:
		for (k = 0; k < stages; k++){
			for (j = 0; j < nframes; j++){
				g  = loop_gains[k];
				g2 = 1-g*g;

				o_buffer[k][i][read_index[k][i]] = i_buffer[k][i][read_index[k][i]];
        i_buffer[k][i][read_index[k][i]] = in[i][j] + g*o_buffer[k][i][read_index[k][i]];
				
        out[i][j] = -g*in[i][j] + g2*o_buffer[k][i][read_index[k][i]];
        // make the output of this stage the input of the next
				in[i][j] = out[i][j];
	
        read_index[k][i]  = (1  + read_index[k][i])%delay_samples[k];
			}
		}
	}
	return 0;
}

 
int main (int argc, char *argv[]) {
	const char *client_name = "schroeder_logan_reverb";
	num_channels = 2;
	working      = 1;
	if (argc != 2) {
		printf("Syntax is:\n");
		printf("\t ./schroeder_logan_reverb.c [TIME_MULTIPLIER]\n");
		exit(1);
	}
	
	
	float time_multiplier = atof(argv[1]);
	

	if (time_multiplier < 0){
		printf("C'mon, this isn't a time travel machine!\n.\n");
		exit(1);
	} else if (time_multiplier > 10){
		printf("C'mon, I don't have all day!.\n");
		exit(1);
	}

	initialize_client(client_name, num_channels);
	
	// dynamic buffer allocation, setting indexes to zero:
	int time_to_samples = (time_multiplier * 1000*SAMPLE_RATE)/1000;
	read_index  = (int**) malloc(stages*sizeof(int*));
	delay_samples = (int*) malloc(stages * sizeof(int));
	i_buffer      = (double***)malloc(stages * sizeof(double**));
	o_buffer      = (double***)malloc(stages * sizeof(double**));
	for (int i= 0; i < stages; i++){
		read_index[i] = (int*) calloc(num_channels, sizeof(int));
		delay_samples[i] = time_to_samples * delay_times[i];
		i_buffer[i] = (double**) malloc(num_channels*sizeof(double*));
		o_buffer[i] = (double**) malloc(num_channels*sizeof(double*));
		for (int j = 0; j < num_channels; j++){
			i_buffer[i][j] = (double*) calloc(delay_samples[i], sizeof(double));
			o_buffer[i][j] = (double*) calloc(delay_samples[i], sizeof(double));
		}
	}
	
	
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
