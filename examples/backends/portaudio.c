#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <portaudio.h>

#define MINIMP3_IMPLEMENTATION
#include <minimp3_ex.h>

#ifdef __GLIBC__
extern char *program_invocation_short_name;
#define PROGNAME() program_invocation_short_name
#else
#define PROGNAME() getprogname ()
#endif

struct cdplusg_portaudio_context
{
  PaStream *stream;

  short *audio_bytes;
  unsigned long audio_size;
  unsigned long audio_head;

  double scale_factor;
  int samples_per_second;

  int is_playing;
};

static int
cdplusg_portaudio_callback (const void *data, void *output, unsigned long frame_count,
                              const PaStreamCallbackTimeInfo *time_info,
                              PaStreamCallbackFlags callback_flags, void *user_data)
{
  (void) time_info;
  (void) callback_flags;
  (void) data;

  struct cdplusg_portaudio_context *info = (struct cdplusg_portaudio_context *) user_data;

  unsigned long samples_to_tx = frame_count * 2;
  unsigned long bytes_to_tx = samples_to_tx * 2;

  if (samples_to_tx + info->audio_head <= info->audio_size)
  {
    memcpy (output, &info->audio_bytes[info->audio_head], bytes_to_tx);
    info->audio_head += samples_to_tx;
    return 0;
  }

  memset (output, 0x00, bytes_to_tx);
  memcpy (output, &info->audio_bytes[info->audio_head], 2 * (info->audio_size - info->audio_head));
  info->audio_head = info->audio_size;
  return paComplete;
}

struct cdplusg_portaudio_context *
cdplusg_portaudio_context_initialize (const char *audio_filename, double scale_factor)
{
  fprintf (stderr, "%s: debug: attempting to open file '%s'\n", PROGNAME (), audio_filename);
  mp3dec_t mp3_decoder;
  mp3dec_file_info_t mp3_info;

  int mp3_decoder_retval = mp3dec_load (&mp3_decoder, audio_filename, &mp3_info, NULL, NULL);

  if (mp3_decoder_retval == MP3D_E_IOERROR)
  {
    fprintf (stderr, "%s: debug: could not open file '%s': %s\n", PROGNAME (),
               audio_filename, strerror (errno));
    return NULL;
  }
  else if (mp3_decoder_retval)
  {
    fprintf (stderr, "%s: debug: something went wrong decoding the audio file '%s'\n",
               PROGNAME (), audio_filename);
    return NULL;
  }
  
  fprintf (stderr, "%s: debug: successfully opened file '%s'\n", PROGNAME (), audio_filename);

  struct cdplusg_portaudio_context *context =
    (struct cdplusg_portaudio_context *) calloc (1, sizeof (struct cdplusg_portaudio_context));  

  context->is_playing = 0;
  context->audio_bytes = mp3_info.buffer;
  mp3_info.buffer = NULL;  // tx ownership

  context->audio_size = mp3_info.samples;
  context->scale_factor = scale_factor;

  if (context->audio_size == 0)
  {
    fprintf (stderr,
        "%s: debug: audio file has no contents, continuing without audio\n", PROGNAME ());
    goto error_pre_initialize;
  }

  // workaround to temporarily disable output to stderr
  fflush (stderr);
  int stderr_backup = dup (fileno (stderr));
  int stdnull = open ("/dev/null", O_WRONLY);
  dup2 (stdnull, fileno (stderr));
  close (stdnull);

  PaError error = Pa_Initialize ();

  // restore stderr
  fflush (stderr);
  dup2 (stderr_backup, fileno (stderr));
  close (stderr_backup);

  if (error != paNoError)
    goto error_post_initialize;

  error = Pa_OpenDefaultStream (&context->stream, 0, 2, paInt16, scale_factor * mp3_info.hz,
            paFramesPerBufferUnspecified, cdplusg_portaudio_callback, context);

  context->samples_per_second = 2 * mp3_info.hz;

  if (error != paNoError)
    goto error_post_initialize;

  error = Pa_StartStream (context->stream);

  if (error == paNoError)
  {
    context->is_playing = 1;
    return context;
  }

  Pa_AbortStream (context->stream);

error_post_initialize:
  fprintf (stderr, "%s: error: error initializing portaudio backend: %s\n",
             PROGNAME (), Pa_GetErrorText (error));
  fprintf (stderr, "%s: debug: continuing with no audio\n", PROGNAME ());
  Pa_Terminate ();
error_pre_initialize:
  free (context->audio_bytes);
  context->audio_size = 0;
  free (context);
  return NULL;
}

unsigned int
cdplusg_portaudio_context_get_elapsed_time_ms (struct cdplusg_portaudio_context *context)
{
  return (unsigned int) (context->audio_head * 1000 / (context->scale_factor * context->samples_per_second));
}

void
cdplusg_portaudio_context_restart (struct cdplusg_portaudio_context *context)
{
  context->audio_head = 0;
}

void
cdplusg_portaudio_context_pause (struct cdplusg_portaudio_context *context)
{
  if (context->is_playing)
  {
    Pa_StopStream (context->stream);
    context->is_playing = 0;
  }
}

void
cdplusg_portaudio_context_resume (struct cdplusg_portaudio_context *context)
{
  if (!context->is_playing)
  {
    Pa_StartStream (context->stream);
    context->is_playing = 1;
  }
}

void
cdplusg_portaudio_context_toggle_playback (struct cdplusg_portaudio_context *context)
{
  if (context->is_playing)
    cdplusg_portaudio_context_pause (context);
  else
    cdplusg_portaudio_context_resume (context);
}

void
cdplusg_portaudio_context_destroy (struct cdplusg_portaudio_context *context)
{
  if (context)
  {
    Pa_AbortStream (context->stream);
    Pa_Terminate ();
    free (context->audio_bytes);
    free (context);
  }
}
