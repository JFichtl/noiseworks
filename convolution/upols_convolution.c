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
 * special realtime thread once for each audio cycle.
 */

int jack_callback (jack_nframes_t nframes, void *arg){
        jack_default_audio_sample_t *in, *out;
        int i, j, k;

        in = (jack_default_audio_sample_t *)jack_port_get_buffer (input_port, nframes);
        out = (jack_default_audio_sample_t *)jack_port_get_buffer (output_port, nframes);

        for (i = 0; i < nframes; i++){
        	// nframes come in and are saved in the right half of the input buffer
		buffer[nframes + i] = in[i];
          
        	// the input buffer is then copied into the fftw 3time buffer
        	i_time[i] = buffer[i];
        	i_time[nframes+i] = buffer[nframes+i];
        }
  
	// taking fftw3 to obtain frequency domain coefficients in i_fft array:
  	fftw_execute(i_forward);
	
	// applying a circular shift to the FDL:
	for (i = 0; i < two_nframes; i++){
		for (k = partitions - 1; k > 0; k--){
	        	fdl[k][i] = fdl[k-1][i];
		}
	}

	// write most recent frequency coefficients to the fdl,
  // wipe o_fft clean to discard previous calculations:
	for (i = 0; i < two_nframes; i++){
		fdl[0][i] = i_fft[i];
		o_fft[i] = 0.0 + I*0.0;
	}
  
  // multiply add stage across the fdl and the ir partition
  for (i = 0; i < two_nframes; i++){
		for (k = 0; k < partitions; k++){
			o_fft[i] += fdl[k][i] * fir[k][i];
		}
 	 }
	
	// going back to time domain.
  fftw_execute(o_inverse);
  for (i = 0; i < nframes; i++){
    	// take second half of the ifft as the output, discard the 
    	// time-aliased first half of the ifft.
    
	out[i] = vol*creal(o_time[nframes+i])/two_nframes;
    	// shift input buffer by shifting the second half to take
    	// the first halfs' place.
	buffer[i] = in[i];
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
	const char *client_name = "upols_convolver";
	jack_options_t options = JackNoStartServer;
	jack_status_t status;

	if (argc != 3) {
		printf("Syntax is:\n");
		printf("\t ./rt_convolution.c [IMPULSE_RESPONSE_FILE] [VOLUME]");
		exit(1);
	}
	
	// read file stuff
	char audio_file_path[100];
	strcpy(audio_file_path, argv[1]);
	printf("trying to open impulse response file: %s\n", audio_file_path);
	audio_file = sf_open(audio_file_path, SFM_READ, &audio_info);
	
	// IR volume
	vol = atof(argv[2]);

	if (audio_file == NULL){
		printf("%s\n", sf_strerror(NULL));
		exit(1);
	} else {
		ir_len = audio_info.frames;
		printf("IR file info:\n");
		printf("\tSample Rate: %d\n", audio_info.samplerate);
		printf("\tChannels: %d\n", audio_info.channels);
		printf("\tFrames: %d\n", ir_len);
		sf_command (audio_file, SFC_SET_NORM_FLOAT, NULL, SF_TRUE) ;
	}

  // JACK setup block
	/* open a client connection to the JACK server */
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
	
	
	/* tell the JACK server to call 'jack_shutdown()' if it ever shuts down,
	   either entirely, or if it just decides to stop calling us. */
	jack_on_shutdown (client, jack_shutdown, 0);
	
	
	/* display the window size. */
	//printf ("Sample rate: %d\n", jack_get_sample_rate (client));
	printf ("Window size: %d\n", jack_get_buffer_size (client));
	sample_rate = (float)jack_get_sample_rate(client);
	int nframes = jack_get_buffer_size (client);
	two_nframes = 2*nframes;
	
  // IR init block:
	if (ir_len < nframes){
		printf("Impulse responses with sizes less than %d not supported (ir is %d samples long)\n", nframes, ir_len);
		exit(1);
	}
  
	/* the impulse response file doesn't need to be an exact multiple of two; 
	 * however, the actual number of samples needs to be a multiple of nframes
	 * (which is always a power of two) to use fftw3's acceleration, so we pad
   * the ir with zeros to make it a integer multiple of nframes */
  
	partitions = ceil(ir_len/nframes);
	int ir_aux_len = partitions * nframes;
	printf("padded ir len: %d (%.f ms)\n", ir_aux_len, 1000*ir_len/sample_rate);
	
	// preparing impulse respones for processing:	
	ir = (float*) calloc(ir_aux_len, sizeof(float));
	int read_count = sf_read_float(audio_file, ir, ir_len);
	if (ir_len != read_count){
		printf("An error occurred while writing the IR buffer.\n");
		exit(1);
	}
	sf_close(audio_file);
	ir_len = ir_aux_len;

	// calculating both L2 norm (energy) and supremum norm (wave peak) to
  // normalize our impulse response. This part could use some tweaking
	
	float ir_L2_norm  = 0.0;
	float ir_sup_norm = 0.0;
	for (int i = 0; i < ir_len; i++){
		// L2 norm:
		ir_L2_norm += pow(ir[i], 2);
		
		// supremum norm:
		if (ir[i] > ir_sup_norm){
			ir_sup_norm = ir[i];
		}
	}
	printf("max ir val:    %.6f\n", ir_sup_norm);
	
	// take square root of square sum:
	ir_L2_norm = sqrt(ir_L2_norm);
	printf("max L2 ir val: %.6f\n", ir_L2_norm );
	
	// normalize IR:
	float ir_norm = ir_L2_norm;
	
	for (int i = 0; i < ir_len; i++){
		ir[i] = ir[i]/ir_norm;
	}

	// initialize fdl and filter partitions:
	fdl = malloc(partitions * sizeof(double complex*));
	fir = malloc(partitions * sizeof(double complex*));
	for (int i = 0; i < partitions; i++){
		fdl[i] = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
		fir[i] = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	}

	//preparing FFTW3 buffers
	conv = (float*) calloc(2*nframes, sizeof(float));
	buffer = calloc(2*nframes, sizeof(jack_default_audio_sample_t));

	i_fft   = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	i_time  = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	
	ir_fft  = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	ir_time = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);

	o_fft   = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);
	o_time  = (double complex *) fftw_malloc(sizeof(double complex) * 2*nframes);

	i_forward = fftw_plan_dft_1d(2*nframes, i_time, i_fft ,  FFTW_FORWARD , FFTW_MEASURE);
	o_inverse = fftw_plan_dft_1d(2*nframes, o_fft , o_time,  FFTW_BACKWARD, FFTW_MEASURE);
	ir_forward= fftw_plan_dft_1d(2*nframes, ir_time, ir_fft, FFTW_FORWARD , FFTW_MEASURE);
       	
	// time to write partitions and fdl
	printf("number of partitions: %d\n", partitions);
	int errors = 0;
	for (int k = 0; k < partitions; k++){
		for (int i = 0; i < nframes; i++){
			// create zero padded fft buffers as per Wefers:
			ir_time[i]           = ir[k*nframes + i];
			ir_time[nframes + i] = 0.0;
		}
		
		fftw_execute(ir_forward);
		fir[k][0] = ir_fft[0];
		fir[k][nframes] = ir_fft[nframes];
		fdl[k][0] = 0.0;
		for (int i = 1; i < nframes; i++){
			
			// filter partitions are created here
			int p = 2*nframes - i;
			fir[k][i] = ir_fft[i];
			fir[k][p] = ir_fft[p];
			
			// initialize fdl to zero
			fdl[k][i] = 0.0 + I*0.0;
			fdl[k][p] = 0.0 + I*0.0;
			
			double dc = creal(fir[k][0]);
			double re1 = creal(fir[k][i]);
			double im1 = cimag(fir[k][i]);
			double re2 = creal(fir[k][p]);
			double im2 = cimag(fir[k][p]);
			
			// for some reason, taking the fft of some impulse responses
      // returns absurd values, so we check for that here. Could be removed
      // by implementing some other automated check on the IR first.
			if (re1 > 100 || re2 > 100 || im1 > 100 || im2 > 100){
				printf("\nERROR: fft encountered unreasonably big number.\n");
				printf("index %d: %f + %fi \n", i+1, re1, im1);
				printf("index %d: %f + %fi \n", p, re2, im2);
				errors++;
			}
			if (errors > 0){
				exit(1);
			}
		}
	}

	if (client == NULL){
		/* if connection failed, say why */
		printf ("jack_client_open() failed, status = 0x%2.0x\n", status);
		if (status & JackServerFailed) {
			printf ("Unable to connect to JACK server.\n");
		}
		exit (1);
	}
	/* create the agent input port */
	input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE,JackPortIsInput, 0);
	
	/* create the agent output port */
	output_port = jack_port_register (client, "output1", JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput, 0);
	
	/* check that both ports were created succesfully */
	if ((input_port == NULL) || (output_port == NULL)) {
		printf("Could not create agent ports. Have we reached the maximum amount of JACK agent ports?\n");
		exit (1);
	}
	
	/* Tell the JACK server that we are ready to roll.
	   Our jack_callback() callback will start running now. */
	if (jack_activate (client)) {
		printf ("Cannot activate client.");
		exit (1);
	}
	
	printf ("Agent activated.\n");
	
	/* Connect the ports.  You can't do this before the client is
	 * activated, because we can't make connections to clients
	 * that aren't running.  Note the confusing (but necessary)
	 * orientation of the driver backend ports: playback ports are
	 * "input" to the backend, and capture ports are "output" from
	 * it.
	 */
	printf ("Connecting ports... ");
	 
	/* Assign our input port to a server output port*/
	// Find possible output server port names
	const char **serverports_names;
	serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
	if (serverports_names == NULL) {
		printf("No available physical capture (server output) ports.\n");
		exit (1);
	}
	// Connect the first available to our input port
	if (jack_connect (client, serverports_names[0], jack_port_name (input_port))) {
		printf("Cannot connect input port.\n");
		exit (1);
	}
	// free serverports_names variable for reuse in next part of the code
	free (serverports_names);
	
	
	/* Assign our output port to a server input port*/
	// Find possible input server port names
	serverports_names = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
	if (serverports_names == NULL) {
		printf("No available physical playback (server input) ports.\n");
		exit (1);
	}
	// Connect the first available to our output port
	// free serverports_names variable, we're not going to use it again
	if (jack_connect (client, jack_port_name (output_port), serverports_names[0])) {
		printf ("Cannot connect output ports.\n");
		exit (1);
	}
	free (serverports_names);
	
	
	printf ("done.\n");
	/* keep running until stopped by the user */
	sleep (-1);
	
	
	/* this is never reached but if the program
	   had some other way to exit besides being killed,
	   they would be important to call.
	*/
	jack_client_close (client);
	exit (0);
}
