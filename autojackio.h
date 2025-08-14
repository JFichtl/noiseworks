#ifndef AUTOJACKIO_H_INCLUDED
#define AUTOJACKIO_H_INCLUDED

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <jack/jack.h>

jack_default_audio_sample_t **in, **out;
jack_port_t                 **input_port, **output_port;
jack_client_t *client;

int num_channels, working;
jack_nframes_t SAMPLE_RATE, BUFFER_SIZE;

void initialize_client(const char *client_name, const int channels);
void connect_ports();
void jack_shutdown (void *arg);
int jack_callback (jack_nframes_t nframes, void *arg);

void initialize_client(const char *client_name, const int channels){
	jack_options_t options = JackNoStartServer;
	jack_status_t status;
	client = jack_client_open (client_name, options, &status);
	
	if (client == NULL){
		/* if connection failed, say why */
		printf ("jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			printf ("Unable to connect to JACK server.\n");
		}
		exit (1);
	}
	
	/* if connection was successful, check if the name we proposed is not in use */
	if (status & JackNameNotUnique){
		client_name = jack_get_client_name(client);
		printf ("Warning: other agent with our name is running, `%s' has been assigned to us.\n", client_name);
	}
	
	/* tell the JACK server to call 'jack_callback()' whenever there is work to be done. */
	jack_set_process_callback (client, jack_callback, 0);
	in  = malloc(num_channels * sizeof(jack_default_audio_sample_t*));
	out = malloc(num_channels * sizeof(jack_default_audio_sample_t*));
	
	/* tell the JACK server to call 'jack_shutdown()' if it ever shuts down,
	   either entirely, or if it just decides to stop calling us. */
	jack_on_shutdown (client, jack_shutdown, 0);
	
	/* display the current sample rate. */
	SAMPLE_RATE = jack_get_sample_rate(client);
	BUFFER_SIZE = jack_get_buffer_size(client);
	printf ("Engine sample rate: %d hz\n", SAMPLE_RATE);
	
	printf("Initializing I/O channels... ");
	
	input_port  = malloc(num_channels * sizeof(jack_port_t*));
	output_port = malloc(num_channels * sizeof(jack_port_t*));
	for (int i = 0; i < channels; i++){
		char  input_name[20];
		char output_name[20];
		sprintf( input_name,  "input_%d", i+1);
		sprintf(output_name, "output_%d", i+1);
		
		input_port[i]  = jack_port_register (client,  input_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput,  0);
		output_port[i] = jack_port_register (client, output_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

		/* check that both ports were created succesfully */
		if ( (input_port[i] == NULL) || (output_port[i] == NULL) ) {
			printf( "\nCould not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
                	exit (1);
		}
	}
	printf("Done.\n");

}

void connect_ports(){
	
	/* Tell the JACK server that we are ready to roll.
	   Our jack_callback() callback will start running now. */
	if (jack_activate (client)) {
		printf ("Cannot activate client.");
		exit (1);
	}
	
	printf ("Agent activated.\n");
	printf ("Connecting ports... ");

	/* Assign our input port to a server output port*/
	const char **serverports_names;
	serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);

	if (serverports_names == NULL) {
		printf("No available physical capture (server output) ports.\n");
		exit (1);
	}
	// Connect the first available to our input port
	for (int i = 0; i < num_channels; i++){
		if (jack_connect (client, serverports_names[i], jack_port_name (input_port[i]) )) {
			printf("Cannot connect input port %d.\n", i+1);
			exit (1);
		}
	}
	// free ports_names variable for reuse in next part of the code
	free (serverports_names);
	serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if (serverports_names == NULL) {
		printf("No available physical playback (server input) ports.\n");
		exit (1);
	}
	// Connect the first available to our output port
	for (int i = 0; i < num_channels; i++){
		if (jack_connect (client, jack_port_name (output_port[i]), serverports_names[i] )) {
			printf("Cannot connect output port %d.\n", i+1);
			exit (1);
		}
	}
	// free serverports_names variable, we're not going to use it again
	free (serverports_names);
	
	
	printf ("done.\n");
	/* keep running until stopped by the user */
}

void  INThandler(int sig)
{
     signal(sig, SIG_IGN);
     working = 0;
}

void jack_shutdown (void *arg){
	exit (1);
}

#endif  /* AUTOJACKIO_H_INCLUDED */
