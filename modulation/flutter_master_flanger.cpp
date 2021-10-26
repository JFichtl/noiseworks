/**
 * Flangetastic Flanger
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
float delay_time;
int delay_samples[] = {0, 0};
int delay_index[]   = {0, 0};
int read_pointer[]  = {0, 0};
int write_pointer[] = {0, 0};

float feedback;

int lfo_index[] = {0, 0};
int lfo_period;
float lfo_depth;

float **delay_buffer;
float *LFO;

jack_port_t *input_ports[num_channels];
jack_port_t *output_ports[num_channels];
jack_client_t *client;

/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 * 
 */
int jack_callback (jack_nframes_t nframes, void *arg){
	jack_default_audio_sample_t *in[num_channels], *out[num_channels];
	int id, jd, stwobble;
	float a, wobble, delay_interpolation;

	for (int i = 0; i < num_channels; i++){
		// Get channel data.
		in[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (input_ports[i], nframes);
		out[i] = (jack_default_audio_sample_t*) jack_port_get_buffer (output_ports[i], nframes);
		
		// circular buffer delay modulation stage.
		for (int j = 0; j < nframes; j++){
			
			// wobble outputs float values but for now we only use the rounded version
			// this works by separating the read and write pointers and running them 
			// at different speeds.
			stwobble = (int) floor(LFO[lfo_index[i]]);
			out[i][j] = 0.5*(in[0][j] + lfo_depth*delay_buffer[i][read_pointer[i]]);
			delay_buffer[i][write_pointer[i]] = 0.5*(in[0][j] + feedback*out[i][j]);
      
      // we create pitch changes by 'wobbling' the read pointer.
			read_pointer[i]  = (write_pointer[i] + 1 + stwobble) % (delay_samples[i]);
			write_pointer[i] = (write_pointer[i] + 1) % delay_samples[i];
			lfo_index[i]   = (  lfo_index[i] + 1) % lfo_period;
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
	const char *client_name = "flanger";
	jack_options_t options = JackNoStartServer;
	jack_status_t status;
	
	if (argc != 6){
		std::cout << "\nSyntax:\n";
		std::cout << "\tflanger [DELAY TIME] [RATE] [DEPTH] [SWEEP] [FEEDBACK]\n\n";
		exit(0);
	}

	delay_time = atof(argv[1]);
	float rate = atof(argv[2]); // in hz
	lfo_depth = atof(argv[3]); // in ms
	float sweep = atof(argv[4]);
	feedback = atof(argv[5]);

	if ( (delay_time > 500) or (delay_time <= 0)) {
		std::cout << "Delay time of " << delay_time << " not supported (max 500ms)\n";
		exit(0);
	} else if ( (rate > 100) or (rate <= 0) ){
		std::cout << "Rate of " << rate << " not supported (max 100hz)\n";
		exit(0);
	} else if ( (lfo_depth > 1) or (lfo_depth < -1) ) {
		std::cout << "LFO depth of " << lfo_depth << " not supported (must be between -1 and 1)\n";
		exit(0);	
	} else if ( (sweep > 1) or (sweep < 0) ){
		std::cout << "Sweep value of " << sweep << " not supported (must be between 0 and 1)\n";
		exit(0);
	} else if ( (feedback > 1) or (feedback < 0)){
		std::cout << "Feedback gain of " << feedback << " not supported (must be between 0 and 1)\n";
		exit(0);
	}


	std::cout << "Flutter Master Flanger!\n";
	std::cout << "Delay Time : " << delay_time << "ms\n";
	std::cout << "Rate       : " << rate       << "hz\n";
	std::cout << "Depth      : " << lfo_depth  << "\n";
	std::cout << "Sweep      : " << sweep      << "\n";
	std::cout << "Feedback   : " << feedback   << "\n\n";
	
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
	
	
	/* display the current sample rate. */
	int sample_rate = jack_get_sample_rate(client);
	int nframes = jack_get_buffer_size (client);
	std::cout << "Engine sample rate:" << sample_rate << "\n";

	/* Init global buffers, declare delay time in miliseconds */
	for (int i = 0; i < num_channels; i++){
		delay_samples[i] = (int) (sample_rate*delay_time)/1000;
	}
	
	// initialize delay buffer. we'd have to use malloc instead of new in C.
	delay_buffer = new float*[num_channels];
	lfo_period = (int) sample_rate/rate;
	LFO = new float[lfo_period];
	
	// a delay value of exactly delay_samples[0] creates artifacts by
	// inducing skips when sweep is equal to 1. Artifacts can be heard
	// approaching sweep = 1 and could maybe be solved by using an
	// antialiasing filter.
	
	for (int i = 0; i < lfo_period; i++){
		double x = 2*M_PI*i/lfo_period;
		// solution 1:
		// take one sample out of the LFO amplitude
		LFO[i] = 0.5*(delay_samples[0] - 1)*(1 - sweep*cos(x));
		
		//solution 2:
		// limit the sweep range by making its value strictly less than 1
		// LFO[i] = 0.5*delay_samples[0]*(1 - 0.99*sweep*cos(x));
	}

	// set a different starting point for right channel LFO to get a nice stereo effect.
	lfo_index[1] = lfo_period/3;
	
	// delay buffer initialization
	for (int i = 0; i < num_channels; i++){
		delay_buffer[i] = new float[delay_samples[i] + 1];
		for (int j = 0; j < delay_samples[i] + 1; j++){
			delay_buffer[i][j] = 0;
		}
	}

	/* create the agent input port tags automatically */
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
	
	std::cout << "Agent activated.\n";
	
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
	/* keep running until stopped by the user */
	sleep (-1);
	
	
	/* this is never reached but if the program
	   had some other way to exit besides being killed,
	   they would be important to call.
	*/
	jack_client_close (client);
	exit (0);
}
