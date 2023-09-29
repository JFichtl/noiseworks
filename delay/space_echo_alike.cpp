/**
 * A Delaylaylaylay.
 */

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

#include <jack/jack.h>

const int num_channels = 2;
int delay_samples[] = {0, 0};
int delay_index[] = {0, 0};
int manual;

float **delay_coef;
float **fdbck_coef;
float **head_panning;
float *fdbck_weight;
float input_gain, output_gain;

int lfo_index[] = {0, 0};
int lfo_period;
int lfo_width;

float **delay_buffer;
float **global_buffer;
int global_index[] = {0, 0};
float *LFO;

jack_port_t *input_ports[num_channels];
jack_port_t *output_ports[num_channels];
jack_client_t *client;

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 * 
 * This client is a kickass delay.
 */

float LP_norm(float x, float p){
        float norm = pow(1+pow(x, p), 1/p);
        return norm;
}


float LP_min(float x, float p, float threshold){
        float L_min = 0.5 * (LP_norm(x+threshold, p) - LP_norm(threshold-x, p));
        return L_min;
}

int jack_callback (jack_nframes_t nframes, void *arg){
	jack_default_audio_sample_t *in[num_channels], *out[num_channels];
	int wobble;

	for (int i = 0; i < num_channels; i++){
		// Get channel data.
		in[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (input_ports[i], nframes);
		out[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_ports[i], nframes);

		// circular buffer stage.
		for (int j = 0; j < nframes; j++){
			// calculate index stuff:
			int index0 = (delay_index[i] + 2*delay_samples[i]/4) % delay_samples[i];
			int index1 = (delay_index[i] + 3*delay_samples[i]/4) % delay_samples[i];
			int index2 = (delay_index[i] + 4*delay_samples[i]/4) % delay_samples[i];

			// wobble adds a variable delay to the buffer copy. modulation is not artifact free yet
			// wobble = (int) lfo_width*( 1 - LFO[lfo_index[i]] );
			// out[i][j] = 0*in[i][j] + (delay_buffer[i][delay_index[i]] + 0.8*delay_buffer[i][(delay_index[i] + wobble)%delay_samples[i] ]);
			
			out[i][j] = in[0][j] + delay_coef[i][0]*delay_buffer[i][index0]
					     + delay_coef[i][1]*delay_buffer[i][index1]
				     	     + delay_coef[i][2]*delay_buffer[i][index2];
					    // mod stuff, not ready yet
					    // + 0.5*global_buffer[i][global_index[i]];
			
			delay_buffer[i][delay_index[i]] = (0.6*LP_min(input_gain*in[0][j], 2, 1)
							+ fdbck_coef[i][0]*delay_buffer[i][index0]
							+ fdbck_coef[i][1]*delay_buffer[i][index1]
                                             		+ fdbck_coef[i][2]*delay_buffer[i][index2]);
			
			//global_buffer[i][global_index[i]] = 0.8*(out[i][j]);
			delay_index[i]   = (delay_index[i] + 1 )%delay_samples[i];
			//global_index[i]  = (global_index[i] + 1 )%manual;
			//lfo_index[i] = (lfo_index[i] + 1) % lfo_period;
		}

	}
	
	return 0;
}


/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void jack_shutdown (void *arg){
	exit (1);
}


int main (int argc, char *argv[]) {
	const char *client_name = "space";
	jack_options_t options = JackNoStartServer;
	jack_status_t status;
	
	/* check arguments */
        if (argc < 5) {
                std::cout << "Syntax is:\n";
		std::cout << "\t ./space_echo_alike [MODE (1-8)] [DELAY_TIME (0-2000 ms)] [FEEDBACK (0-0.9) [VOLUME (0-1)]]\n";
                exit(0);
        } 
        
	int mode       = atoi(argv[1]);
        int delay_time = atoi(argv[2]);
        float feedback = atof(argv[3]);
        float volume   = atof(argv[4]);
        
        if (delay_time > 2000){
                std::cout << "max delay time (2s) exceeded.\n";
                exit(0);
        } else if (feedback > 0.9) {
                std::cout << "max feedback (0.9) exceeded.\n";
                exit(0);
        } else if (volume > 1 | volume < 0) {
                std::cout << "max volume (1) exceeded.\n";
                exit(0);
        } else if (mode < 1 | mode > 8) {
                std::cout << "invalid mode.\n";
                exit(0);
        }

        // gain factors:
        input_gain = 1.2;
        output_gain = .7;
	manual = 7; //in ms
	int rate = 1; // in hz
	lfo_width = 1; // in ms

	/* open a client connection to the JACK server */
	client = jack_client_open (client_name, options, &status);
	if (client == NULL){
		/* if connection failed, say why */
		std::cout << "jack_client_open() failed, status = 0x" << status << "\n";
		if (status & JackServerFailed) {
			std::cout << "Unable to connect to JACK server.\n";
		}
		exit (1);
	}
	
	/* if connection was successful, check if the name we proposed is not in use */
	if (status & JackNameNotUnique){
		client_name = jack_get_client_name(client);
		std::cout << "Warning: other agent with our name is running, " << client_name << " has been assigned to us.\n";
	}
	
	/* tell the JACK server to call 'jack_callback()' whenever there is work to be done. */
	jack_set_process_callback (client, jack_callback, 0);
	
	
	/* tell the JACK server to call 'jack_shutdown()' if it ever shuts down,
	   either entirely, or if it just decides to stop calling us. */
	jack_on_shutdown (client, jack_shutdown, 0);
	
	
	/* display info */
	int sample_rate = jack_get_sample_rate(client);
	int nframes = jack_get_buffer_size (client);
	std::cout << "Space, The Final Frontier!\n";
	std::cout << "Engine sample rate: " << sample_rate << "hz\n";
	std::cout << "Buffer size: " << nframes << "\n";
	
	// redefine ms variables in samples:
	manual    = manual*sample_rate/1000;
	lfo_width = lfo_width*sample_rate/1000;
	
	/* Init global buffers, declare delay time in miliseconds */
	for (int i = 0; i < num_channels; i++){
		delay_samples[i] = ((sample_rate*delay_time)/1000);
	}
		
	// initialize delay buffer. we'd have to use malloc instead of new in C.
	delay_buffer  = new float*[num_channels];
	global_buffer = new float*[num_channels];

	lfo_period = sample_rate/rate;
	LFO = new float[lfo_period];
	for (int i = 0; i < lfo_period; i++){
		double x = 2*M_PI*i/(lfo_period-1);
		LFO[i] = sin(x);
	}

	for (int i = 0; i < num_channels; i++){
		delay_buffer[i]  = new float[delay_samples[i]];
		global_buffer[i] = new float[delay_samples[i]];
	}
	
	// fill buffer with zeros to avoid clipping.
	float t, l;
	for (int i = 0; i < num_channels; i++){
		for (int j = 0; j < delay_samples[i]; j++){
			delay_buffer[i][j]  = 0;
			global_buffer[i][j] = 0;
		}
	}

	// choosing a mode
	float set_delays[8][3] = {
                {1, 0, 0},
                {0, 1, 0},
                {0, 0, 1},
                {0, 1, 1},
                {1, 1, 0},
                {0, 1, 1},
                {1, 0, 1},
                {1, 1, 1}
        };

        float set_fdbcks[8][3] = {
                {1, 0, 0},
                {0, 1, 0},
                {0, 0, 1},
                {0, 1, 1},
                {1, 1, 0},
                {0, 1, 1},
                {1, 0, 1},
                {1, 1, 1}
        };

	float head_panning[8][3] = {
		{1, 0, 0},
    	       	{0, 1, 0},
       	       	{0, 0, 1},
       	       	{0, 1, 0},
       	       	{0, 1, 0},
	 	{0, 1, 0},
       	       	{0, 1, 1},
 	       	{0, 1, 1}
	};
	
	delay_coef         = new float*[num_channels];
	fdbck_coef         = new float*[num_channels];
	fdbck_weight       = new float [num_channels];
	float **panng_coef = new float*[num_channels];
	
	float delay_weight[num_channels] = {1};

	// calculate delay parameters.
	for (int i = 0; i < num_channels; i ++){
		delay_coef[i] = new float[3];
		fdbck_coef[i] = new float[3];
		panng_coef[i] = new float[3];
		fdbck_weight[i] = 0;
		// generate panning coefficients
		for (int j = 0; j < 3; j++){
			if (i == 0) {
				// copy panning as is
				panng_coef[i][j] = head_panning[mode-1][j];
			} else if (i == 1){
				// turns 1 to 0, 0 to -1 and takes absolute value to
				// invert the panning
				panng_coef[i][j] = abs(head_panning[mode-1][j] - 1);
			}
		}
		for (int j = 0; j < 3; j++){
        	        delay_coef[i][j] = panng_coef[i][j]*set_delays[mode-1][j];
               		fdbck_coef[i][j] = set_fdbcks[mode-1][j];
               		delay_weight[i] += delay_coef[i][j];
			fdbck_weight[i] += fdbck_coef[i][j];
		}
		// now that we have the weigths, we can recalculate the parameters:
		for (int j = 0; j < 3; j++){
			delay_coef[i][j] = volume*delay_coef[i][j];
			fdbck_coef[i][j] = feedback*(1/fdbck_weight[i])*fdbck_coef[i][j];
		}
	}
	
	// jack port stuff, here ends the processing setup
	std::stringstream ss_input;
	std::stringstream ss_output;
	for (int i = 0; i < num_channels; i++){
		int index = i+1;

		ss_input.str(std::string());
		ss_output.str(std::string());

		ss_input << "input_" << index;
		ss_output << "output_" << index;

		input_ports[i] = jack_port_register (client, ss_input.str().c_str() , JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		output_ports[i] = jack_port_register (client, ss_output.str().c_str(), JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

		/* check that both ports were created succesfully */
		if ( (input_ports[i] == NULL) || (output_ports[i] == NULL) ) {
			std::cout << "Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n";
                	exit (1);
		}
	}

	/* Tell the JACK server that we are ready to roll.
	   Our jack_callback() callback will start running now. */
	if (jack_activate (client)) {
		std::cout << "Cannot activate client.\n";
		exit (1);
	}
	
	std::cout << "Procrastinator activated.\n";
	
	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */
	std::cout << "Connecting ports... ";
	
	 
	/* Assign our input port to a server output port*/
	// Find possible output server port names
	const char **serverports_names;
	serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
	
	if (serverports_names == NULL) {
		std::cout << "No available physical capture (server output) ports.\n";
		exit (1);
	}
	// Connect the first available to our input port
	for (int i = 0; i < num_channels; i++){
		if (jack_connect (client, serverports_names[i], jack_port_name (input_ports[i])) ) {
			std::cout << "Cannot connect input port.\n";
			exit (1);
		}
	}
	// free ports_names variable for reuse in next part of the code
	free (serverports_names);
	
	
	/* Assign our output port to a server input port*/
	// Find possible input server port names
	serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if (serverports_names == NULL) {
		std::cout << "No available physical playback (server input) ports.\n";
		exit (1);
	}
	// Connect the first available to our output port
	for (int i = 0; i < num_channels; i++){
                if (jack_connect (client, jack_port_name(output_ports[i]), serverports_names[i]) ) {
                        std::cout << "Cannot connect input port.\n";
                        exit (1);
                }
        }
	// free serverports_names variable, we're not going to use it again
	free (serverports_names);
	
	
	std::cout << "done.\n";
	std::cout << "v0.c.1\n";
	/* keep running until stopped by the user */
	sleep (-1);
	
	
	/* this is never reached but if the program
	   had some other way to exit besides being killed,
	   they would be important to call.
	*/
	jack_client_close (client);
	exit (0);
}
