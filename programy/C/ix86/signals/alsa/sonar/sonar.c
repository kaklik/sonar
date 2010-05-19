///////////////////////////////////////////////////////////////////////////////////
//                        A small demo of sonar.
// Program allow distance measuring.
// Uses cross-correlation algorithm to find echos
//
// Author: kaklik  (kaklik@mlab.cz)
//$Id:$
///////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include <fftw3.h>

#define SOUND_SPEED	340.0	// sound speed in air in metrs per second
#define MAX_RANGE	5.0	// maximal working radius in meters

static char *device = "plughw:0,0";			/* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S16;	/* sample format */
static unsigned int rate = 96000;			/* stream rate */
static unsigned int buffer_time = 2 * (MAX_RANGE / SOUND_SPEED * 1e6);		/* ring buffer length in us */
static unsigned int period_time = MAX_RANGE / SOUND_SPEED * 1e6;		/* period time in us */
static int resample = 1;				/* enable alsa-lib resampling */

unsigned int chirp_size;

static snd_pcm_sframes_t buffer_size;	// size of buffer at sound card
static snd_pcm_sframes_t period_size;	//samples per frame
static snd_output_t *output = NULL;

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, unsigned int channels)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;

    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0)
    {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0)
    {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0)
    {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0)
    {
        printf("Channels count (%i) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0)
    {
        printf("Rate %iHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate)
    {
        printf("Rate doesn't match (requested %iHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    else printf("Rate set to %i Hz\n", rate, err);
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    if (err < 0)
    {
        printf("Unable to set buffer time %i for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0)
    {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    printf("Bufffer size set to:  %d  Requested buffer time: %ld \n", (int) buffer_size, (long) buffer_time);


    // set the period time
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    if (err < 0)
    {
        printf("Unable to set period time %i for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }

    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0)
    {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;
    printf("Period size set to:  %d Requested period time: %ld \n", (int) period_size, (long) period_time);

    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0)
    {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
    int err;

    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0)
    {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    // start the transfer when the buffer is almost full: never fou our case
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, 2 * buffer_size);
    if (err < 0)
    {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }

    err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
    if (err < 0)
    {
        printf("Unable to set period event: %s\n", snd_strerror(err));
        return err;
    }

    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0)
    {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}

////// SIGNAL GENERATION STUFF
unsigned int linear_windowed_chirp(short *pole)  // generate the ping signal
{
    unsigned int maxval = (1 << (snd_pcm_format_width(format) - 1)) - 1;

    static const float f0 = 5000;		//starting frequency
    static const float fmax = 10000;		//ending frequency
    static const float Tw = 0.0015;	// time width of ping in seconds 
    static float k;

    unsigned int n=0;
    double t;
    unsigned int chirp_samples;		// number of samples per period

    k=2*(fmax-f0)/Tw;
    chirp_samples = ceil(rate*Tw);	// compute size of ping sinal in samples

    for (n=0;n<=chirp_samples;n++)
    {
        t = (double) n / (double)rate;
        pole[n] = (short) floor( (0.35875 - 0.48829*cos(2*M_PI*t*1/Tw) + 0.14128*cos(2*M_PI*2*t*1/Tw) - 0.01168*cos(2*M_PI*3*t*1/Tw))*maxval*sin(2*M_PI*(t)*(f0+(k/2)*(t))) ); // ping signal generation formula
    }
    return (chirp_samples);	// return count of samples in ping
}

int main(int argc, char *argv[])
{
    snd_pcm_t *playback_handle, *capture_handle;		//variables for driver handlers
    int err;
    snd_pcm_hw_params_t *hwparams;		// hardware and software parameters arrays
    snd_pcm_sw_params_t *swparams;

    long int *correlationl, *correlationr;	// pointers to arrays where correlation will be stored
    float k;
    int *L_signal, *R_signal;	// array of captured data from left and right channel
    short *chirp, *signal;	// chirp and soundcard buffer output data
    unsigned int i,j,m,n;
    unsigned int map_size; //number of points in echo map.
    long int l,r;  // store correlation at strict time

    FILE *out;		// dummy variable for file data output

    snd_pcm_hw_params_alloca(&hwparams);	// allocation of soundcard parameters registers
    snd_pcm_sw_params_alloca(&swparams);

    printf("Simple PC sonar $Rev:$ starting work.. \n");

//open and set playback device
    if ((err = snd_pcm_open(&playback_handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }

    if ((err = set_hwparams(playback_handle, hwparams, 1)) < 0)
    {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = set_swparams(playback_handle, swparams)) < 0)
    {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

//open and set capture device
    if ((err = snd_pcm_open(&capture_handle, device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }

    if ((err = set_hwparams(capture_handle, hwparams, 2)) < 0)
    {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = set_swparams(capture_handle, swparams)) < 0)
    {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    /*    err = snd_pcm_link( capture_handle, playback_handle); //link capture and playback together seems doesn't work 
        if (err < 0)
        {
            printf("Device linking error: %s\n", snd_strerror(err));
            exit(EXIT_FAILURE);
        }*/

    k = SOUND_SPEED/rate; // normalising constant - normalise sample number to distance

    correlationl = malloc(period_size * sizeof(long int)); //array to store correlation curve
    correlationr = malloc(period_size * sizeof(long int)); //array to store correlation curve
    L_signal = malloc(period_size * sizeof(int));
    R_signal = malloc(period_size * sizeof(int));
    chirp = calloc(2*period_size, sizeof(short));
    signal = malloc(2*period_size * sizeof(short));

// generate ping pattern
    chirp_size = linear_windowed_chirp(chirp);

// write generated chirp data to souncard buffer
    err = snd_pcm_writei(playback_handle, chirp, period_size);
    if (err < 0)
    {
        printf("Initial write error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

//start sream
    err = snd_pcm_start(playback_handle);
    if (err < 0)
    {
        printf("Start error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    err = snd_pcm_start(capture_handle);
    if (err < 0)
    {
        printf("Start error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    else printf("Transmitting all samples of chirp\n");
//--------------

    while ( snd_pcm_avail_update(capture_handle) < period_size)			// wait until one period of data is transmitted
    {
        usleep(1000);
        printf(".");
    }

    err = snd_pcm_drop(playback_handle);		// stop audio stream
    err = snd_pcm_drain(capture_handle);
    if (err < 0)
    {
        printf("Stop error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    err = snd_pcm_readi(capture_handle, signal, period_size);		//read whole period from audio buffer
    if (err < 0)
    {
        printf("Read error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    j=0;
    for (i=0;i < period_size;i++)		// separe inretleaved samples to two arrays
    {
        L_signal[i]=signal[j];
        R_signal[i]=signal[j+1];
        j+=2;
    }

    printf("\nChirp transmitted \ncorrelating\n");
    for (n=0; n < (period_size - chirp_size - 1); n++)
    {
        l=0;
        r=0;
        for ( m = 0; m < chirp_size;m++)
        {
            l += chirp[m]*L_signal[m+n];	// correlate with left channel
            r += chirp[m]*R_signal[m+n];	// correlate with right channel
        }
        correlationl[n]=abs(l);
        correlationr[n]=abs(r);
    }

    printf("Writing output files\n");
    out=fopen("/tmp/sonar.txt","w");		// save captured and computed correlation data for both channels
    for (i=0; i <= (period_size - 1); i++)
    {
        fprintf(out,"%2.3f %6d %6d %9ld %9ld\n",i*k, L_signal[i], R_signal[i], correlationl[i], correlationr[i]);
    }
    fclose(out);

    out=fopen("/tmp/chirp.txt","w");		// save chirp data to someone who want it
    for (i=0; i <= (chirp_size - 1); i++)
    {
        fprintf(out,"%6d %6d\n", i, chirp[i]);
    }
    fclose(out);

    printf("Job done.\n");

				//free all arrays 
    free(correlationl);
    free(correlationr);
    free(L_signal);
    free(R_signal);
    free(chirp);
    free(signal);

    snd_pcm_close(playback_handle);	// free driver handlers 
    snd_pcm_close(capture_handle);
    return 0;
}

