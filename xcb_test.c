#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <jack/jack.h>

#include <portaudio.h>

#define MINIMP3_IMPLEMENTATION
#include <minimp3_ex.h>

#include <sys/time.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <cdplusg.h>

#define FPS 30
#define COMMANDS_PER_FRAME (300 / FPS)
#define SCALE_FACTOR 5

static_assert (300 % FPS == 0, "Frames per second must divide 300.");

static char *progname;

struct cdplusg_portaudio_context
{
  PaStream *stream;

  short *audio_bytes;
  unsigned long audio_size;
  unsigned long audio_head;
};

int
cdplusg_portaudio_callback (const void *, void *output, unsigned long frame_count,
              const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags, void *user_data)
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

void snd_lib_null_error_handler (const char *, int, const char *, int, const char *, ...) {}
void jack_null_handler (const char *) {}

void
cdplusg_portaudio_context_initialize (struct cdplusg_portaudio_context *context, const char *audio_filename)
{
  fprintf (stderr, "%s: debug: attempting to open file '%s'\n", progname, audio_filename);

  memset (context, 0x00, sizeof (struct cdplusg_portaudio_context));

  mp3dec_t mp3_decoder;
  mp3dec_file_info_t mp3_info;

  int mp3_decoder_retval = mp3dec_load (&mp3_decoder, audio_filename, &mp3_info, NULL, NULL);

  if (mp3_decoder_retval == MP3D_E_IOERROR)
  {
    fprintf (stderr, "%s: debug: could not open file '%s': %s\n", progname,
        audio_filename, strerror(errno));
    return;
  }
  else if (mp3_decoder_retval)
  {
    fprintf (stderr,
        "%s: debug: something went wrong decoding the audio file '%s'\n", progname, audio_filename);
    return;
  }
  
  fprintf (stderr, "%s: debug: successfully opened file '%s'\n", progname, audio_filename);

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
  jack_info_callback  = jack_null_handler;

  PaError error = Pa_Initialize ();

  if (error != paNoError)
    goto error_post_initialize;

  error = Pa_OpenDefaultStream (&context->stream, 0, 2, paInt16, mp3_info.hz,
            paFramesPerBufferUnspecified, cdplusg_portaudio_callback, context);

  if (error != paNoError)
    goto error_post_initialize;

  error = Pa_StartStream (context->stream);

  if (error == paNoError)
    return;

  Pa_AbortStream (context->stream);

error_post_initialize:
  fprintf (stderr, "%s: error initializing portaudio backend: %s\n", progname, Pa_GetErrorText (error));
  fprintf (stderr, "%s: debug: continuing with no audio\n", progname);
  Pa_Terminate ();
error_pre_initialize:
  free (context->audio_bytes);
  context->audio_size = 0;
  return;
}

void
cdplusg_portaudio_context_destroy (struct cdplusg_portaudio_context *context)
{
  if (context->audio_size)
  {
      Pa_AbortStream (context->stream);
      Pa_Terminate ();
      free (context->audio_bytes);
  }
}

struct cdplusg_xcb_context
{
  xcb_connection_t *connection;

  xcb_image_t      *xcb_image;
  size_t            image_data_size;
  unsigned char    *image_data;

  xcb_window_t      window;
  xcb_pixmap_t      pixmap;
  xcb_gcontext_t    gcontext;
};

void
cdplusg_xcb_context_initialize (struct cdplusg_xcb_context *context)
{
  context->connection = xcb_connect (NULL, NULL);

  const xcb_setup_t *setup = xcb_get_setup (context->connection);
  xcb_screen_iterator_t iterator = xcb_setup_roots_iterator (setup);
  xcb_screen_t *screen = iterator.data;

  unsigned int mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  unsigned int value_mask = XCB_EVENT_MASK_EXPOSURE;
  unsigned int values [] = { screen->black_pixel, value_mask };

  context->window = xcb_generate_id (context->connection);
  xcb_create_window (context->connection, XCB_COPY_FROM_PARENT, context->window, screen->root,
        0, 0, SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT,
		    0, XCB_WINDOW_CLASS_INPUT_OUTPUT,	screen->root_visual, mask, values);

  context->image_data_size =
        SCALE_FACTOR * SCALE_FACTOR * 4 * CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT;

  context->image_data = (unsigned char *) malloc (context->image_data_size);

  context->pixmap = xcb_generate_id (context->connection);
  xcb_create_pixmap (context->connection, 24, context->pixmap, context->window,
        SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT);

  context->gcontext = xcb_generate_id (context->connection);
  xcb_create_gc (context->connection, context->gcontext, context->pixmap, 0, NULL);

  context->xcb_image = xcb_image_create_native (context->connection,
	      SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT,
	      XCB_IMAGE_FORMAT_Z_PIXMAP,
	      24, NULL, context->image_data_size, context->image_data);

  xcb_map_window (context->connection, context->window);
}

void
cdplusg_xcb_context_update_from_gpx_state (struct cdplusg_xcb_context *context,
              struct cdplusg_graphics_state *gpx_state)
{
  cdplusg_write_graphics_state_to_pixmap
    (gpx_state, context->image_data, CDPLUSG_Z_FORMAT, SCALE_FACTOR);

  xcb_image_put
    (context->connection, context->pixmap, context->gcontext, context->xcb_image, 0, 0, 0);

  xcb_copy_area (context->connection, context->pixmap, context->window, context->gcontext,
      0, 0, 0, 0, SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT);
  xcb_flush (context->connection);
}

void
cdplusg_xcb_context_destroy (struct cdplusg_xcb_context *context)
{
  xcb_image_destroy (context->xcb_image);
  xcb_free_pixmap (context->connection, context->pixmap);
  xcb_disconnect (context->connection);
  free (context->image_data);
}

int
main (int argc, char **argv)
{
  progname = argv[0];

  char *filename = argv[1];

  if (argc != 2)
  {
    fprintf (stderr, "usage: %s [filename]\n", progname);
    return 1;
  }

  FILE *file = fopen (filename, "r");

  if (file == NULL)
  {
    fprintf (stderr, "%s: error opening file '%s': %s\n", progname, filename, strerror(errno));
    return 1;
  }

  char audio_filename [256];
  const char *audio_file_extensions [] = { ".mp3" };
  const char *audio_file_extension = audio_file_extensions[0];

  struct cdplusg_portaudio_context audio_context;

  char *last_dot = strrchr (filename, '.');

  if (last_dot != NULL && strchr (last_dot, '/') == NULL)
  {
    *last_dot = '\0';
  }

  if (strlen (filename) + strlen (audio_file_extension) >= sizeof (audio_filename))
  {
    fprintf (stderr,
        "%s: debug: could not deduce audio file name to look for, continuing without audio\n",
        progname);
  }
  else
  {
    strcpy (audio_filename, filename);
    strcat (audio_filename, audio_file_extension);

    cdplusg_portaudio_context_initialize (&audio_context, audio_filename);
  }

  struct cdplusg_xcb_context xcb_context;

  cdplusg_xcb_context_initialize (&xcb_context);

  struct cdplusg_instruction instruction;
  struct cdplusg_graphics_state *gpx_state = cdplusg_create_graphics_state ();

  struct timeval previous_time;
  struct timeval time_stride;

  gettimeofday (&previous_time, NULL);

  timerclear (&time_stride);

  time_stride.tv_usec = 10000 * COMMANDS_PER_FRAME / 3;
  static_assert (10000 * COMMANDS_PER_FRAME / 3 <= 999999, "too many commands per frame");

  unsigned int counter = 0;

  while (cdplusg_get_next_instruction_from_file (&instruction, file) == 1)
  {
    counter++;
    cdplusg_update_graphics_state (gpx_state, &instruction);

    if (counter % COMMANDS_PER_FRAME == 0)
    {
      cdplusg_xcb_context_update_from_gpx_state (&xcb_context, gpx_state);

      struct timeval next_time;
      struct timeval time_difference;

      gettimeofday (&next_time, NULL);
      timersub (&next_time, &previous_time, &time_difference);

      while (timercmp (&time_difference, &time_stride, <))
      {
        struct timeval remaining_time;

        timersub (&time_stride, &time_difference, &remaining_time);

        usleep (3 * remaining_time.tv_usec / 4);

        gettimeofday (&next_time, NULL);
	      timersub (&next_time, &previous_time, &time_difference);
      }

      timeradd (&previous_time, &time_stride, &previous_time);
    }
  }

  cdplusg_free_graphics_state (gpx_state);
  cdplusg_xcb_context_destroy (&xcb_context);
  cdplusg_portaudio_context_destroy (&audio_context);

  return 0;
}
