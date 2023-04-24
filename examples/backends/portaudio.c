#include <stdio.h>
#include <string.h>

#include <errno.h>

#include <alsa/asoundlib.h>
#include <jack/jack.h>

#include <portaudio.h>

#define MINIMP3_IMPLEMENTATION
#include <minimp3_ex.h>


struct cdplusg_portaudio_context
{
  PaStream *stream;

  short *audio_bytes;
  unsigned long audio_size;
  unsigned long audio_head;
};

static int
cdplusg_portaudio_callback (const void *, void *output, unsigned long frame_count,
                              const PaStreamCallbackTimeInfo *,
                              PaStreamCallbackFlags, void *user_data)
{
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

static void snd_lib_null_error_handler (const char *, int, const char *, int, const char *, ...) {}
static void jack_null_handler (const char *) {}

struct cdplusg_portaudio_context *
cdplusg_portaudio_context_initialize (const char *audio_filename, double scale_factor)
{
  extern char *program_invocation_short_name;
  const char *progname = program_invocation_short_name;

  fprintf (stderr, "%s: debug: attempting to open file '%s'\n", progname, audio_filename);
  mp3dec_t mp3_decoder;
  mp3dec_file_info_t mp3_info;

  int mp3_decoder_retval = mp3dec_load (&mp3_decoder, audio_filename, &mp3_info, NULL, NULL);

  if (mp3_decoder_retval == MP3D_E_IOERROR)
  {
    fprintf (stderr, "%s: debug: could not open file '%s': %s\n", progname,
               audio_filename, strerror (errno));
    return NULL;
  }
  else if (mp3_decoder_retval)
  {
    fprintf (stderr, "%s: debug: something went wrong decoding the audio file '%s'\n",
               progname, audio_filename);
    return NULL;
  }
  
  fprintf (stderr, "%s: debug: successfully opened file '%s'\n", progname, audio_filename);

  struct cdplusg_portaudio_context *context =
    (struct cdplusg_portaudio_context *) calloc (1, sizeof (struct cdplusg_portaudio_context));  

  context->audio_bytes = mp3_info.buffer;
  mp3_info.buffer = NULL;  // tx ownership

  context->audio_size = mp3_info.samples;

  if (context->audio_size == 0)
  {
    fprintf (stderr,
        "%s: debug: audio file has no contents, continuing without audio\n", progname);
    goto error_pre_initialize;
  }

  // set custom error handler for ALSA errors
  snd_lib_error_set_handler (snd_lib_null_error_handler);

  jack_error_callback = jack_null_handler;
  jack_info_callback = jack_null_handler;

  PaError error = Pa_Initialize ();

  if (error != paNoError)
    goto error_post_initialize;

  error = Pa_OpenDefaultStream (&context->stream, 0, 2, paInt16, scale_factor * mp3_info.hz,
            paFramesPerBufferUnspecified, cdplusg_portaudio_callback, context);

  if (error != paNoError)
    goto error_post_initialize;

  error = Pa_StartStream (context->stream);

  if (error == paNoError)
    return context;

  Pa_AbortStream (context->stream);

error_post_initialize:
  fprintf (stderr, "%s: error: error initializing portaudio backend: %s\n",
             progname, Pa_GetErrorText (error));
  fprintf (stderr, "%s: debug: continuing with no audio\n", progname);
  Pa_Terminate ();
error_pre_initialize:
  free (context->audio_bytes);
  context->audio_size = 0;
  free (context);
  return NULL;
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
