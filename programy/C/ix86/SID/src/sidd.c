//
// sidd.c:  A VLF signal monitor.
//
// authors: Paul Nicholson (paul@abelian.demon.co.uk), Jakub kakona (kaklik@mlab.cz)
//

#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <sched.h>
#include <linux/soundcard.h>

#include <fftw3.h>

///////////////////////////////////////////////////////////////////////////////
//  Tuneable Settings                                                        //
///////////////////////////////////////////////////////////////////////////////

#define VERSION "0.94"

//
//  Number of soundcard bytes to read at a time.
#define NREAD 2048

//
//  Max number of bands which can be read from the config file.
#define MAXBANDS 20

//
//  Name of the configuration file.
#define CONFIG_FILE "sidd.conf"

///////////////////////////////////////////////////////////////////////////////
//  Globals and fixed definitions                                            // 
///////////////////////////////////////////////////////////////////////////////
//
//  Default values here are over-ridden by the config file.

int mode = 1;                                          //  1 = mono, 2 = stereo
int bits = 16;                                    // Sample width, 8 or 16 bits
int BINS = 2048;                                    // Number of frequency bins
#define FFTWID (2 * BINS)                    // Number of samples in FFT period

int background = 1;                        // Set zero if running in foreground
int fdi;                                                   // Input file handle
int fdm;                                                   // Mixer file handle
int VFLAG = 0;                                    //  Set non-zero by -v option
int MFLAG = 0;                                    //  Set non-zero by -m option

int spec_max = 100;       // Issue a spectrum for every spec_max output records
int spec_cnt = 0;
int sample_rate = 100000;                                 // Samples per second

int chans = 1;
int alert_on = 0;

int priority = 0;                       // Set to 1 if high scheduling priority
struct sigaction sa;
char mailaddr[100];

double los_thresh = 0;                    // Threshold for loss of signal, 0..1
int los_timeout = 0;        // Number of seconds before loss of signal declared

double DF;                                   // Frequency resolution of the FFT
int bailout_flag = 0;                           // To prevent bailout() looping
int grab_cnt = 0;                       // Count of samples into the FFT buffer

// Mixer gain settings requested by config file.
int req_lgain = -1;              // Line gain
int req_mgain = -1;              // Microphone gain
int req_igain = -1;              // Input gain 
int req_rgain = -1;              // Record level


// Actual mixer values, read by open_mixer()
int mixer_recmask;      // Recording device mask
int mixer_stereo;       // Stereo device mask
int mixer_line;         // Line input gain setting
int mixer_microphone;	// Microphone input gain
int mixer_igain;        // Overall input gain setting
int mixer_reclev;       // Recording level setting
int mixer_recsrc;       // Mask indicating which inputs are set to record

//
// Various filenames, contents set by config file.
//
char logfile[100] = "";
char device[100] = "/dev/dsp";
char mixer[100] = "/dev/mixer";
char spectrum_file[100] = "/tmp/sidspec"; 
char datadir[100] = ".";

//
// Table of frequency bands to monitor
//

struct BAND
{
   char ident[50];

   int start;
   int end;
}
 bands[MAXBANDS];    // Table of bands to be monitored

int nbands = 0;

//
//  Independent state variables and buffers for left and right channels
//
struct CHAN
{
   char *name;
   double *signal_avg;
   double *powspec;
   double *fft_inbuf;
   fftw_complex *fft_data;
   fftw_plan ffp;
   double peak;
   double sum_sq;
   int los_state;
   time_t los_time;
   FILE *fo;
   char fname[100];
}
 left = { "left" }, right = { "right" };

///////////////////////////////////////////////////////////////////////////////
//  Various Utility Functions                                                //
///////////////////////////////////////////////////////////////////////////////

//
//  Issue a message to the log file, if the verbosity level is high enough...
//

void report( int level, char *format, ...)
{
   va_list ap;
   void bailout( char *format, ...);
   char temp[ 200];

   if( VFLAG < level) return;

   va_start( ap, format);
   vsprintf( temp, format, ap);
   va_end( ap);

   if( !logfile[0] || !background)
      if( background != 2) fprintf( stderr, "%s\n", temp);

   if( logfile[0])
   {
      time_t now = time( NULL);
      struct tm *tm = gmtime( &now);
      FILE *flog = NULL;

      if( (flog = fopen( logfile, "a+")) == NULL)
         bailout( "cannot open logfile [%s]: %s", logfile, strerror( errno));

      fprintf( flog, "%04d/%02d/%02d %02d:%02d:%02d %s\n", 
                tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
                tm->tm_hour, tm->tm_min, tm->tm_sec, temp);
      fclose( flog);
   }
}

void alert( char *format, ...)
{
   FILE *f;
   va_list( ap);
   char cmd[100], temp[100];

   va_start( ap, format);
   vsprintf( temp, format, ap);
   va_end( ap);
 
   report( -1, "%s", temp);

   if( !alert_on || !mailaddr[0]) return;

   sprintf( cmd, "mail -s 'sidd alert' '%s'", mailaddr);
   if( (f=popen( cmd, "w")) == NULL)
   {
      report( 0, "cannot exec [%s]: %s", cmd, strerror( errno));
      return;
   }

   fprintf( f, "sidd: %s\n", temp);
   fclose( f);
}

//
//  We try to exit the program through here, if possible.
//  

void bailout( char *format, ...)
{
   va_list ap;
   char temp[ 200];

   if( bailout_flag) exit( 1);
   bailout_flag = 1;
   va_start( ap, format);
   vsprintf( temp, format, ap);
   va_end( ap);

   alert( "terminating: %s", temp);
   exit( 1);
}

//
//  Exit with a message if we get any signals.
//  

void handle_sigs( int signum)
{
   bailout( "got signal %d", signum);
}

///////////////////////////////////////////////////////////////////////////////
//  Soundcard Setup                                                          //
///////////////////////////////////////////////////////////////////////////////

//
//  Prepare the input stream, setting up the soundcard if the input
//  is a character device.
//

void setup_input_stream( void)
{
   struct stat st;

   report( 1, "taking data from [%s]", device);

   if( (fdi = open( device, O_RDONLY)) < 0)
      bailout( "cannot open [%s]: %s", strerror( errno));

   if( fstat( fdi, &st) < 0)
      bailout( "cannot stat input stream: %s", strerror( errno));

   if( S_ISCHR( st.st_mode)) 
   {
      int blksize;
      int fragreq = 0x7fff000a;
      unsigned int format;
      unsigned int req_format = AFMT_S16_LE;
      if( bits == 8) req_format = AFMT_U8;

      if (ioctl( fdi, SNDCTL_DSP_SETFRAGMENT, &fragreq))
         report( 01, "cannot set fragment size");

      if( ioctl( fdi, SNDCTL_DSP_RESET, NULL) < 0)
         bailout( "cannot reset input device");

      chans = mode;
      if( ioctl( fdi, SNDCTL_DSP_CHANNELS, &chans) < 0)
         bailout( "cannot set channels on input device");

      if( ioctl( fdi, SNDCTL_DSP_GETFMTS, &format) < 0)
         bailout( "cannot get formats from input device");

      report( 2, "formats available: %08X", format);
      if( (format & req_format) == 0)
      {
         report( 0, "available dsp modes are %08X", format);
         bailout( "unable to set %d bit dsp mode", bits);
      }

      format = req_format;
      if( ioctl( fdi, SNDCTL_DSP_SETFMT, &format) < 0)
         bailout( "cannot set dsp format on input device");

      if( ioctl( fdi, SNDCTL_DSP_GETBLKSIZE, &blksize) < 0)
         bailout( "cannot get block size from input device");

      report( 2, "dsp block size: %d", blksize);
      if( ioctl( fdi, SNDCTL_DSP_CHANNELS, &chans) < 0)
         bailout( "cannot get channels from input device");

      report( 1, "requesting rate %d", sample_rate);
      if( ioctl( fdi, SNDCTL_DSP_SPEED, &sample_rate) < 0)
         bailout( "cannot set sample rate of input device");

      report( 1, "actual rate set: %d samples/sec", sample_rate);
      report( 1, "soundcard channels: %d  bits: %d", chans, bits);
   }
}

///////////////////////////////////////////////////////////////////////////////
//  Output Functions                                                         //
///////////////////////////////////////////////////////////////////////////////

void maybe_output_spectrum( void)
{
   FILE *f;
   int i;

   if( ++spec_cnt < spec_max) return;  // Wait for spec_max records
   spec_cnt = 0;

   if( !spectrum_file[0]) return;     // Spectrum file not wanted.

   if( (f=fopen( spectrum_file, "w+")) == NULL)
      bailout( "cannot open spectrum file %s, %s", strerror( errno));

   if( mode == 1){
      fprintf( f, "Frequency PowerL \n");
      for( i=0; i<BINS; i++) fprintf( f, "%.5e %.5e\n", 
             (i+0.5) * DF, left.signal_avg[i]/spec_max);}
   else{
      fprintf( f, "Frequncy PowerL PowerR \n");
      for( i=0; i<BINS; i++) fprintf( f, "%.5e %.5e %.5e\n", 
             (i+0.5) * DF, left.signal_avg[i]/spec_max,
                          right.signal_avg[i]/spec_max);}
   fclose( f);

   for( i=0; i<BINS; i++) left.signal_avg[i] = 0;
   if( mode == 2) for( i=0; i<BINS; i++) right.signal_avg[i] = 0;
}

void output_record( struct CHAN *c, char *prefix, double fsecs)
{
   int i, j;
   char test[100];

   if( mode == 1)
      sprintf( test, "%s.dat", prefix);
   else
      sprintf( test, "%s.%s.dat", prefix, c->name);

   if( !c->fo || strcmp( test, c->fname))
   {
      if( c->fo) fclose( c->fo);
      strcpy( c->fname, test);
      report( 0, "using output file [%s]", c->fname);
      if( (c->fo=fopen( c->fname, "a+")) == NULL)
         bailout( "cannot open [%s], %s", c->fname, strerror( errno));
   }

   fprintf( c->fo, "%.3f %.3f %.3f", fsecs, c->peak, sqrt( c->sum_sq/FFTWID));

   for( i=0; i<nbands; i++)
   {
      double e = 0;
      int n1 = bands[i].start/DF;
      int n2 = bands[i].end/DF;
      for( j=n1; j<= n2; j++) e += c->powspec[j];
      e /= n2 - n1 + 1;
      fprintf( c->fo, " %.2e", e);
   }
   fprintf( c->fo, "\n");
   fflush( c->fo);

   c->peak = c->sum_sq = 0;
}

void output_records( void)
{
   struct timeval tv;
   struct tm *tm;
   double fsecs;
   time_t ud;
   char prefix[100];

   gettimeofday( &tv, NULL);
   fsecs = tv.tv_sec + 1e-6 * tv.tv_usec;
   ud = tv.tv_sec - tv.tv_sec % 86400;
   tm = gmtime( &ud);
   sprintf( prefix, "%s/%02d%02d%02d", datadir,
                  tm->tm_year - 100, tm->tm_mon+1, tm->tm_mday);

   output_record( &left, prefix, fsecs);
   if( mode == 2) output_record( &right, prefix, fsecs);
}

void check_los( struct CHAN *c)
{
   if( !c->los_state)
   {
      if( !c->los_time && c->peak < los_thresh) time( &c->los_time);
      if( c->los_time && c->peak > los_thresh) c->los_time = 0;
      if( c->los_time && c->los_time + los_timeout < time( NULL))
      {
         c->los_state = 1;
         c->los_time = 0;
         if( mode == 1) alert( "loss of signal");
         else alert( "loss of signal on %s", c->name);
      }
   }
   else
   {
      if( !c->los_time && c->peak > los_thresh) time( &c->los_time);
      if( c->los_time && c->peak < los_thresh) c->los_time = 0;
      if( c->los_time && c->los_time + los_timeout < time( NULL))
      {
         c->los_state = 0;
         c->los_time = 0;
         if( mode == 1) alert( "signal restored");
         else alert( "signal restored on %s", c->name);
      }
   } 
}

///////////////////////////////////////////////////////////////////////////////
//  Signal Processing                                                        //
///////////////////////////////////////////////////////////////////////////////

void process_fft( struct CHAN *c)
{
   int i;

   //
   //  Do the FFT.  First time through, initialise the fft plan.
   //

   if( !c->ffp)
      c->ffp = fftw_plan_dft_r2c_1d( FFTWID, c->fft_inbuf, c->fft_data,
                           FFTW_ESTIMATE | FFTW_DESTROY_INPUT);

   fftw_execute( c->ffp);

   //
   //  Obtain squared amplitude of each bin.
   //

   c->powspec[ 0] = 0.0;  // Zero the DC component
   for( i=1; i<BINS; i++)
   {
      double t1 = c->fft_data[ i][0];  
      double t2 = c->fft_data[ i][1]; 
      c->powspec[ i] = t1*t1 + t2*t2;
   }

   //
   //  Accumulate average signal levels in each bin.  signal_avg is used
   //  only for the spectrum file output.
   //

   for( i=0; i<BINS; i++) c->signal_avg[ i] += c->powspec[i];
   check_los( c);
}

void insert_sample( struct CHAN *c, double f)
{
   c->sum_sq += f * f;
   if( f > c->peak) c->peak = f;
   if( f < -c->peak) c->peak = -f;

   c->fft_inbuf[ grab_cnt] = f * sin( grab_cnt/(double) FFTWID * M_PI);
}

void maybe_do_fft( void)
{
   if( ++grab_cnt < FFTWID) return;
   grab_cnt = 0;

   process_fft( &left);
   if( mode == 2) process_fft( &right);

   output_records();
   maybe_output_spectrum(); 
}

//
// Main signal processing loop.  Never returns.
//

void process_signal( void)
{
   unsigned char buff[ NREAD];

   while( 1) 
   {
      int i, q;

      if( (q=read( fdi, buff, NREAD)) <= 0) 
      {
         if( !q || errno == ENOENT || errno == 0) 
         {  
            sched_yield();
            usleep( 50000); 
            continue;
         }

         report( 0, "input file: read error, count=%d errno=%d", q, errno);
         exit( 1);
      }

      //  Unpack the input buffer into signed 16 bit words.
      //  then scale to -1..+1 for further processing.
      //  We use 'chans' to decide if the soundcard is giving stereo or
      //  mono samples, rather than 'mode', because some cards will refuse
      //  to do mono.  
      if( bits == 16)
      {
         if( chans == 1)
         {
            for( i=0; i<q; i += 2)
            {
               int fh = *(short *)(buff + i);
   
               insert_sample( &left, fh/32768.0);
               maybe_do_fft();
            }
         }
         else  // chans must be 2
         {
            if( mode == 1)
               for( i=0; i<q; i += 4)
               {
                  int fh = *(short *)(buff + i);
                  insert_sample( &left, fh/32768.0);
                  maybe_do_fft();
               }
            else  // mode == 2
               for( i=0; i<q; i += 4)
               {
                  int fh = *(short *)(buff + i);
                  insert_sample( &left, fh/32768.0);
   
                  fh = *(short *)(buff + i + 2);
                  insert_sample( &right, fh/32768.0);
                  maybe_do_fft();
               }
         }
      }
      else   // bits must be 8
      {
         if( chans == 1)
         {
            for( i=0; i<q; i++)
            {
               int fh = ((short)buff[i] - 128)*256;
               insert_sample( &left, fh/32768.0);
               maybe_do_fft();
            }
         }
         else  // chans must be 2
         {
            if( mode == 1)
               for( i=0; i<q; i += 2)
               {
                  int fh = ((short)buff[i] - 128)*256;
                  insert_sample( &left, fh/32768.0);
                  maybe_do_fft();
               }
            else  // mode == 2
               for( i=0; i<q; i += 2)
               {
                  int fh = ((short)buff[i] - 128)*256;
                  insert_sample( &left, fh/32768.0);
   
                  fh = ((short)buff[i+1] - 128)*256;
                  insert_sample( &right, fh/32768.0);
                  maybe_do_fft();
               }
         }
      }
   }
}

///////////////////////////////////////////////////////////////////////////////
//  Configuration File Stuff                                                 //
///////////////////////////////////////////////////////////////////////////////

void config_band( char *ident, char *start, char *end)
{
   struct BAND *b = bands + nbands++;

   if( nbands == MAXBANDS) bailout( "too many bands specified in config file");

   strcpy( b->ident, ident);
   b->start = atoi( start);
   b->end = atoi( end);

   report( 1, "band %s %d %d", b->ident, b->start, b->end);
}

void load_config( void)
{
   int lino = 0, nf;
   FILE *f;
   char buff[100], *p, *fields[20];

   if( (f=fopen( CONFIG_FILE, "r")) == NULL)
      bailout( "no config file found");

   while( fgets( buff, 99, f))
   {
      lino++;

      if( (p=strchr( buff, '\r')) != NULL) *p = 0;
      if( (p=strchr( buff, '\n')) != NULL) *p = 0;
      if( (p=strchr( buff, ';')) != NULL) *p = 0;

      p = buff;  nf = 0;
      while( 1)
      {
         while( *p && isspace( *p)) p++;
         if( !*p) break;
         fields[nf++] = p;
         while( *p && !isspace( *p)) p++;
         if( *p) *p++ = 0;
      }
      if( !nf) continue;

      if( nf == 4 && !strcasecmp( fields[0], "band")) 
         config_band( fields[1], fields[2], fields[3]);
      else
      if( nf == 2 && !strcasecmp( fields[0], "logfile"))
      {
         strcpy( logfile, fields[1]);
         report( 1, "logfile %s", logfile);
      }
      else
      if( nf == 3 && !strcasecmp( fields[0], "los"))
      {
         los_thresh = atof( fields[1]);
         los_timeout = atoi( fields[2]);
         report( 1, "los threshold %.3f, timeout %d seconds", 
                    los_thresh, los_timeout);
      }
      else
      if( nf == 2 && !strcasecmp( fields[0], "device"))
         strcpy( device, fields[1]);
      else
      if( nf == 2 && !strcasecmp( fields[0], "mixer"))
         strcpy( mixer, fields[1]);
      else
      if( nf == 2 && !strcasecmp( fields[0], "mode"))
      {
         if( !strcasecmp( fields[1], "mono")) mode = 1;
         else
         if( !strcasecmp( fields[1], "stereo")) mode = 2;
         else
            bailout( "error in config file, line %d", lino);
      }
      else
      if( nf == 2 && !strcasecmp( fields[0], "bits"))
      {
         bits = atoi( fields[1]);
         if( bits != 8 && bits != 16)
            bailout( "can only do 8 or 16 bits, config file line %d", lino);
      }
      else
      if( nf == 3 && !strcasecmp( fields[0], "spectrum"))
      {
         strcpy( spectrum_file, fields[1]);
         spec_max = atoi( fields[2]);
      }
      else
      if( nf == 2 && !strcasecmp( fields[0], "sched")
                  && !strcasecmp( fields[1], "high"))
      {
         priority = 1;
      }
      else
      if( nf == 4 && !strcasecmp( fields[0], "gain"))
      {
         int left = atoi( fields[2]);
         int right = atoi( fields[3]);
         int gain = (right << 8) | left;

         if( !strcasecmp( fields[1], "line"))
         {
           req_lgain = gain;
           mixer_recsrc = SOUND_MASK_LINE;
         }
         else
         if( !strcasecmp( fields[1], "mic"))
         { 
           req_mgain = gain;
           mixer_recsrc = SOUND_MASK_MIC;
         }
         else
         if( !strcasecmp( fields[1], "overall")) req_igain = gain;
         else
         if( !strcasecmp( fields[1], "record")) req_rgain = gain;
         else
            bailout( "unknown gain control [%s]", fields[1]);
      }
      else
      if( nf == 2 && !strcasecmp( fields[0], "rate"))
         sample_rate = atoi( fields[1]);
      else
      if( nf == 2 && !strcasecmp( fields[0], "bins"))
         BINS = atoi( fields[1]);
      else
      if( nf == 2 && !strcasecmp( fields[0], "datadir"))
      {
         struct stat st;
         strcpy( datadir, fields[1]);
         if( stat( datadir, &st) < 0 || !S_ISDIR( st.st_mode))
            bailout( "no data directory, %s", datadir);
      }
      else
         bailout( "error in config file, line %d", lino);
   }

   fclose( f);
}

///////////////////////////////////////////////////////////////////////////////
//  Mixer Stuff                                                              //
///////////////////////////////////////////////////////////////////////////////

void open_mixer( void)
{
   if( (fdm = open( mixer, O_RDWR)) < 0)
      bailout( "cannot open [%s]: %s", mixer, strerror( errno));

   // Determine the available mixer recording gain controls.
   // We must at least have a line input.

   if( ioctl( fdm, SOUND_MIXER_READ_RECMASK, &mixer_recmask) < 0)
      bailout( "cannot read mixer devmask");

   if( (mixer_recmask & SOUND_MASK_LINE) == 0)
      bailout( "mixer has no line device");

   if( ioctl( fdm, SOUND_MIXER_READ_STEREODEVS, &mixer_stereo) < 0)
      bailout( "cannot read mixer stereodevs");

   if( ioctl( fdm, SOUND_MIXER_READ_RECSRC, &mixer_recsrc) < 0)
      bailout( "cannot read mixer recsrc");

   // Read the line input gain.  
   if( ioctl( fdm, SOUND_MIXER_READ_LINE, &mixer_line) < 0)
      bailout( "cannot read mixer line");

   // Read overall input gain?  Optional.
   if( (mixer_recmask & SOUND_MASK_IGAIN) &&
        ioctl( fdm, SOUND_MIXER_READ_IGAIN, &mixer_igain) < 0)
           bailout( "cannot read mixer igain");

   // Read overall recording level?  Optional.
   if( (mixer_recmask & SOUND_MASK_RECLEV) &&
        ioctl( fdm, SOUND_MIXER_READ_RECLEV, &mixer_reclev) < 0)
           bailout( "cannot read mixer reclev");
}

void report_mixer_settings( void)
{
   report( 1, "mixer: line input is %s", 
     mixer_stereo & SOUND_MASK_LINE ? "stereo" : "mono");

   report( 1, "mixer: line input is %s", 
     mixer_recsrc & SOUND_MASK_LINE ? "on" : "off");

   report( 1, "mixer: line input gain: left=%d right=%d", 
             mixer_line & 0xff, (mixer_line >> 8) & 0xff);

   // Overall input gain?  Optional.
   if( mixer_recmask & SOUND_MASK_IGAIN)
   {
      report( 1, "mixer: igain: left=%d right=%d", 
                 mixer_igain & 0xff, (mixer_igain >> 8) & 0xff);
   }
   else report( 1, "mixer: igain: n/a");

   // Overall recording level?  Optional.
   if( mixer_recmask & SOUND_MASK_RECLEV)
   {
      report( 1, "mixer: reclev: left=%d right=%d", 
                mixer_reclev & 0xff, (mixer_reclev >> 8) & 0xff);
   }
   else report( 1, "mixer: reclev: n/a");

}

void setup_mixer( void)
{
   if( req_lgain >= 0)
   {
      report( 1, "requesting line input gains left=%d right=%d",
             req_lgain & 0xff, (req_lgain >> 8) & 0xff);

      if( ioctl( fdm, SOUND_MIXER_WRITE_LINE, &req_lgain) < 0 ||
          ioctl( fdm, SOUND_MIXER_READ_LINE, &mixer_line) < 0)
         bailout( "error setting mixer line gain");

      report( 1, "line input gains set to: left=%d right=%d", 
             mixer_line & 0xff, (mixer_line >> 8) & 0xff);
   }

   if( req_mgain >= 0)
   {
      report( 1, "requesting microphone input gains left=%d right=%d",
             req_mgain & 0xff, (req_mgain >> 8) & 0xff);

      if( ioctl( fdm, SOUND_MIXER_WRITE_MIC, &req_mgain) < 0 ||
          ioctl( fdm, SOUND_MIXER_READ_MIC, &mixer_microphone) < 0)
         bailout( "error setting mixer microphone gain");

      report( 1, "Microphone input gains set to: left=%d right=%d", 
             mixer_microphone & 0xff, (mixer_microphone >> 8) & 0xff);
   }

   if( req_igain >= 0 &&
       (mixer_recmask & SOUND_MASK_IGAIN))
   {
      report( 1, "requesting overall input gains left=%d right=%d",
             req_igain & 0xff, (req_igain >> 8) & 0xff);

      if( ioctl( fdm, SOUND_MIXER_WRITE_IGAIN, &req_igain) < 0 ||
          ioctl( fdm, SOUND_MIXER_READ_IGAIN, &mixer_igain) < 0)
         bailout( "error setting mixer overall input gain");

      report( 1, "overall input gains set to: left=%d right=%d", 
                 mixer_igain & 0xff, (mixer_igain >> 8) & 0xff);
   }

   if( req_rgain >= 0 &&
       (mixer_recmask & SOUND_MASK_RECLEV))
   {
      report( 1, "requesting overall record levels left=%d right=%d",
             req_rgain & 0xff, (req_rgain >> 8) & 0xff);

      if( ioctl( fdm, SOUND_MIXER_WRITE_RECLEV, &req_rgain) < 0 ||
          ioctl( fdm, SOUND_MIXER_READ_RECLEV, &mixer_reclev) < 0)
         bailout( "error setting mixer record level");

      report( 1, "mixer record levels set to: left=%d right=%d", 
                 mixer_reclev & 0xff, (mixer_reclev >> 8) & 0xff);
   }

//mixer_recsrc= SOUND_MASK_LINE;
mixer_recsrc= SOUND_MASK_MIC;

   switch (mixer_recsrc)
   {
     case SOUND_MASK_MIC:
     
       if( ioctl( fdm, SOUND_MIXER_WRITE_RECSRC, &mixer_recsrc) < 0)
        bailout( "cannot set mixer recsrc to microphone");
       else report(1, "Input device set to microphone");
     break;

     case SOUND_MASK_LINE:
       if( ioctl( fdm, SOUND_MIXER_WRITE_RECSRC, &mixer_recsrc) < 0)
        bailout( "cannot set mixer recsrc to line");
       else report(1, "Input device set to line");
     break;
   }
}

///////////////////////////////////////////////////////////////////////////////
//  Main                                                                     //
///////////////////////////////////////////////////////////////////////////////

void make_daemon( void)
{
   int childpid, fd;

   if( (childpid = fork()) < 0)
      bailout( "cannot fork: %s", strerror( errno));
   else if( childpid > 0) exit( 0);

   if( setpgrp() == -1) bailout( "cannot setpgrp");

   if( (childpid = fork()) < 0)
      bailout( "cannot fork: %s", strerror( errno));
   else if( childpid > 0) exit( 0);

   for( fd = 0; fd <NOFILE; fd++) if( fd != fdi) close( fd);
   errno = 0;
   background = 2;
}

void initialise_channel( struct CHAN *c)
{
   int i;

   c->fft_inbuf = (double *) malloc( BINS * 2 * sizeof( double));
   c->fft_data = fftw_malloc( sizeof( fftw_complex) * FFTWID);
   c->powspec = (double *) malloc( BINS * sizeof( double));
   c->signal_avg = (double *) malloc( BINS * sizeof( double));
   for( i=0; i<BINS; i++) c->signal_avg[i] = 0;
}

void setup_signal_handling( void)
{
   sa.sa_handler = handle_sigs;
   sigemptyset( &sa.sa_mask);
   sa.sa_flags = 0;
   sigaction( SIGINT, &sa, NULL);
   sigaction( SIGTERM, &sa, NULL);
   sigaction( SIGHUP, &sa, NULL);
   sigaction( SIGQUIT, &sa, NULL);
   sigaction( SIGFPE, &sa, NULL);
   sigaction( SIGBUS, &sa, NULL);
   sigaction( SIGSEGV, &sa, NULL);
}

// Set scheduling priority to the minimum SCHED_FIFO value.
void set_scheduling( void)
{
   struct sched_param pa;
   int min = sched_get_priority_min( SCHED_FIFO);

   pa.sched_priority = min;
   if( sched_setscheduler( 0, SCHED_FIFO, &pa) < 0)
      report( -1, "cannot set scheduling priority: %s", strerror( errno));
   else
      report( 0, "using SCHED_FIFO priority %d", min);
}

int main( int argc, char *argv[])
{
   while( 1)
   {
      int c = getopt( argc, argv, "vfm");

      if( c == 'v') VFLAG++;
      else
      if( c == 'm') MFLAG++;
      else
      if( c == 'f') background = 0;
      else if( c == -1) break;
      else bailout( "unknown option [%c]", c);
   }

   setup_signal_handling();
   load_config();
   open_mixer();

   if( MFLAG)
   {
      VFLAG = 1;
      background = 0;
      report_mixer_settings();
      exit( 0);
   }

   setup_mixer();
 
   if( background && !logfile[0]) 
      report( -1, "warning: no logfile specified for daemon");

   setup_input_stream();
   DF = (double) sample_rate/(double) FFTWID;

   report( 1, "resolution: bins=%d fftwid=%d df=%f", BINS, FFTWID, DF);
   report( 1, "spectrum file: %s", spectrum_file); 

   initialise_channel( &left);
   if( mode == 2) initialise_channel( &right);

   if( background) make_daemon();
   if( priority) set_scheduling();

   report( 0, "sidd version %s: starting work", VERSION);
   alert_on = 1;
   process_signal();
   return 0;
}

