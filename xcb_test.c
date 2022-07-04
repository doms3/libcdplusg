#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <alsa/asoundlib.h>
#include <jack/jack.h>

#include <portaudio.h>

#include <sys/time.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <cdplusg.h>

#define FPS 30
#define COMMANDS_PER_FRAME (300 / FPS)
#define SCALE_FACTOR 4

static_assert (300 % FPS == 0, "Frames per second must divide 300.");

struct audio_info
{
  short *audio_bytes;
  unsigned long audio_size;
  unsigned long audio_head;
};

int
portaudio_callback (const void *, void *output, unsigned long frame_count,
              const PaStreamCallbackTimeInfo *, PaStreamCallbackFlags, void *user_data)
{
  struct audio_info *info = (struct audio_info *) user_data;

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

void
snd_lib_null_error_handler (const char *, int, const char *, int, const char *, ...) {}

void
libjack_null_handler (const char *) {}

int
main (int argc, char **argv)
{
  char *progname = argv[0];
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
  const char *audio_file_extensions [] = { ".raw", ".ogg" };
  const char *audio_file_extension = audio_file_extensions[0];

  struct audio_info info;
  memset (&info, 0x00, sizeof (info));

  char *last_dot = strrchr (filename, '.');

  if (last_dot != NULL && strchr (last_dot, '/') == NULL)
  {
    *last_dot = '\0';
  }

  if (strlen (filename) + strlen (audio_file_extension) >= sizeof (audio_filename))
  {
    fprintf (stderr, "%s: debug: could not deduce audio file name to look for, continuing without audio\n", progname);
    goto continue_with_no_audio;
  }

  strcpy (audio_filename, filename);
  strcat (audio_filename, audio_file_extension);

  PaStream *stream;

  fprintf (stderr, "%s: debug: attempting to open file '%s'\n", progname, audio_filename);

  FILE *audio_file = fopen (audio_filename, "r");

  if (audio_file == NULL)
  {
    fprintf (stderr, "%s: debug: could not open file '%s': %s\n", progname, audio_filename, strerror(errno));
    goto continue_with_no_audio;
  }

  fprintf (stderr, "%s: debug: successfully opened file '%s'\n", progname, audio_filename);
  fseek (audio_file, 0L, SEEK_END);
  info.audio_size = ftell (audio_file);

  if (info.audio_size == 0)
  {
    fclose (audio_file);
    fprintf (stderr, "%s: debug: audio file has no contents, continuing without audio\n", progname);
    goto continue_with_no_audio;
  }

  info.audio_bytes = (short *) malloc (info.audio_size);
  rewind (audio_file);

  if (fread (info.audio_bytes, info.audio_size, 1, audio_file) != 1)
  {
    fclose (audio_file);
    fprintf (stderr, "%s: debug: could not read from audio file, continuing without audio\n", progname);
    free (info.audio_bytes);
    info.audio_size = 0;
    goto continue_with_no_audio;
  }

  fclose (audio_file);
  info.audio_size = info.audio_size / sizeof (short);

  // set custom error handler for ALSA errors
  snd_lib_error_set_handler (snd_lib_null_error_handler);

  jack_error_callback = &libjack_null_handler;
  jack_info_callback  = &libjack_null_handler;

  PaError error = Pa_Initialize ();

  if (error != paNoError)
  {
    fprintf (stderr, "%s: error initializing portaudio backend: %s\n", progname,
              Pa_GetErrorText (error));
    fprintf (stderr, "%s: debug: continuing with no audio\n", progname);
    free (info.audio_bytes);
    info.audio_size = 0;
    goto continue_with_no_audio;
  }

  error = Pa_OpenDefaultStream (&stream, 0, 2, paInt16, 44100, paFramesPerBufferUnspecified,
            portaudio_callback, &info);

  if (error != paNoError)
  {
    fprintf (stderr, "%s: error initializing portaudio backend: %s\n", progname,
              Pa_GetErrorText (error));
    fprintf (stderr, "%s: debug: continuing with no audio\n", progname);

    free (info.audio_bytes);
    Pa_Terminate ();
    info.audio_size = 0;
    goto continue_with_no_audio;
  }

  error = Pa_StartStream (stream);

  if (error != paNoError)
  {
    fprintf (stderr, "%s: error initializing portaudio backend: %s\n", progname,
              Pa_GetErrorText (error));
    fprintf (stderr, "%s: debug: continuing with no audio\n", progname);

    free (info.audio_bytes);

    error = Pa_AbortStream (stream);

    if (error != paNoError)
    {
      fprintf (stderr, "%s: error aborting portaudio stream: %s\n", progname,
              Pa_GetErrorText (error));
    }

    Pa_Terminate ();
    info.audio_size = 0;
    goto continue_with_no_audio;
  }

continue_with_no_audio:
  /* Open the connection to the X server */
  xcb_connection_t *connection = xcb_connect (NULL, NULL);

  /* Get the first screen */
  const xcb_setup_t *setup = xcb_get_setup (connection);
  xcb_screen_iterator_t iterator = xcb_setup_roots_iterator (setup);
  xcb_screen_t *screen = iterator.data;

  unsigned int mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  unsigned int value_mask = XCB_EVENT_MASK_EXPOSURE;
  unsigned int values [] = { screen->black_pixel, value_mask };

  /* Create the window */
  xcb_window_t window = xcb_generate_id (connection);
  xcb_create_window (connection, XCB_COPY_FROM_PARENT, window, screen->root, 0, 0,
		    SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT,
		    0, XCB_WINDOW_CLASS_INPUT_OUTPUT,	screen->root_visual, mask, values);

  const size_t image_data_size =
        SCALE_FACTOR * SCALE_FACTOR * 4 * CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT;

  unsigned char *image_data =
        (unsigned char *) malloc (image_data_size);

  xcb_pixmap_t pixmap = xcb_generate_id (connection);
  xcb_create_pixmap (connection, 24, pixmap, window,
        SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT);

  xcb_gcontext_t gcontext = xcb_generate_id (connection);
  xcb_create_gc (connection, gcontext, pixmap, 0, NULL);

  xcb_image_t *xcb_image = xcb_image_create_native (connection,
	      SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT,
	      XCB_IMAGE_FORMAT_Z_PIXMAP,
	      24, NULL, image_data_size, image_data);

  xcb_image_put (connection, pixmap, gcontext, xcb_image, 0, 0, 0);

  /* Map the window on the screen */
  xcb_map_window (connection, window);
  /* Make sure commands are sent before we pause so that the window gets shown */
  xcb_flush (connection);

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
      cdplusg_write_graphics_state_to_pixmap (gpx_state, image_data, CDPLUSG_Z_FORMAT, SCALE_FACTOR);

      xcb_image_put (connection, pixmap, gcontext, xcb_image, 0, 0, 0);
      xcb_copy_area (connection, pixmap, window, gcontext,
		     0, 0, 0, 0, SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH, SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT);
      xcb_flush (connection);

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
  xcb_image_destroy (xcb_image);
  xcb_free_pixmap (connection, pixmap);
  xcb_disconnect (connection);
  free (image_data);

  if (info.audio_size)
  {
      Pa_AbortStream (stream);
      Pa_Terminate ();
      free (info.audio_bytes);
  }

  return 0;
}
