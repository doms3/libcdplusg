#define _DEFAULT_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <cdplusg.h>
#include <cdplusg/portaudio.h>

#define FPS 30
#define COMMANDS_PER_FRAME (300 / FPS)
#define DEFAULT_SCALE_FACTOR 3

#define XCB_SCREEN_WIDTH (DEFAULT_SCALE_FACTOR * CDPLUSG_SCREEN_WIDTH)
#define XCB_SCREEN_HEIGHT (DEFAULT_SCALE_FACTOR * CDPLUSG_SCREEN_HEIGHT)

static_assert (300 % FPS == 0, "Frames per second must divide 300.");

static char *progname;

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
  int xcb_connection_error = xcb_connection_has_error (context->connection);

  if (xcb_connection_error != 0)
  {
    fprintf (stderr, "%s: xcb_connection_has_error: %d\n", progname, xcb_connection_error);
    exit (1);
  }

  const xcb_setup_t *setup = xcb_get_setup (context->connection);
  xcb_screen_iterator_t iterator = xcb_setup_roots_iterator (setup);
  xcb_screen_t *screen = iterator.data;

  unsigned int mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  unsigned int value_mask = XCB_EVENT_MASK_EXPOSURE;
  unsigned int values [] = { screen->black_pixel, value_mask };

  context->window = xcb_generate_id (context->connection);
  xcb_create_window (context->connection, XCB_COPY_FROM_PARENT, context->window, screen->root,
        0, 0, XCB_SCREEN_WIDTH, XCB_SCREEN_HEIGHT,
		    0, XCB_WINDOW_CLASS_INPUT_OUTPUT,	screen->root_visual, mask, values);

  context->image_data_size =
        DEFAULT_SCALE_FACTOR * DEFAULT_SCALE_FACTOR * 4 * CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT;

  context->image_data = (unsigned char *) malloc (context->image_data_size);

  context->pixmap = xcb_generate_id (context->connection);
  xcb_create_pixmap (context->connection, 24, context->pixmap, context->window,
        XCB_SCREEN_WIDTH, XCB_SCREEN_HEIGHT);

  context->gcontext = xcb_generate_id (context->connection);
  xcb_create_gc (context->connection, context->gcontext, context->pixmap, 0, NULL);

  context->xcb_image = xcb_image_create_native (context->connection,
	      XCB_SCREEN_WIDTH, XCB_SCREEN_HEIGHT,
	      XCB_IMAGE_FORMAT_Z_PIXMAP,
	      24, NULL, context->image_data_size, context->image_data);

  xcb_map_window (context->connection, context->window);
}

void
cdplusg_xcb_context_update_from_gpx_state (struct cdplusg_xcb_context *context,
              struct cdplusg_graphics_state *gpx_state)
{
  cdplusg_graphics_state_to_pixmap
    (gpx_state, context->image_data, DEFAULT_SCALE_FACTOR);

  xcb_image_put
    (context->connection, context->pixmap, context->gcontext, context->xcb_image, 0, 0, 0);

  xcb_get_geometry_cookie_t cookie = xcb_get_geometry (context->connection, context->window);
  xcb_get_geometry_reply_t *reply = xcb_get_geometry_reply (context->connection, cookie, NULL);

  unsigned int x = 0;
  unsigned int y = 0;

  if (reply->width > XCB_SCREEN_WIDTH)
  {
    x = (reply->width - XCB_SCREEN_WIDTH) / 2;
  }

  if (reply->height > XCB_SCREEN_HEIGHT)
  {
    y = (reply->height - XCB_SCREEN_HEIGHT) / 2;
  }

  xcb_copy_area (context->connection, context->pixmap, context->window, context->gcontext,
      0, 0, x, y, XCB_SCREEN_WIDTH, XCB_SCREEN_HEIGHT);
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

  struct cdplusg_portaudio_context *audio_context = NULL;

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

    audio_context = cdplusg_portaudio_context_initialize (audio_filename, 1);
  }

  struct cdplusg_xcb_context xcb_context;

  cdplusg_xcb_context_initialize (&xcb_context);

  struct cdplusg_instruction instruction;
  struct cdplusg_graphics_state *gpx_state = cdplusg_graphics_state_new ();

  struct timeval previous_time;
  gettimeofday (&previous_time, NULL);
  
  struct timeval time_stride;
  timerclear (&time_stride);

  time_stride.tv_usec = 10000 * COMMANDS_PER_FRAME / 3;
  static_assert (10000 * COMMANDS_PER_FRAME / 3 <= 999999, "too many commands per frame");

  unsigned int counter = 0;

  while (cdplusg_instruction_initialize_from_file (&instruction, file) == 1)
  {
    counter++;
    cdplusg_graphics_state_apply_instruction (gpx_state, &instruction);

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

  cdplusg_graphics_state_free (gpx_state);
  cdplusg_xcb_context_destroy (&xcb_context);
  cdplusg_portaudio_context_destroy (audio_context);

  return 0;
}
