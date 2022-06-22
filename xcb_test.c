#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>

#include <xcb/xcb.h>
#include <xcb/xcb_image.h>

#include <cdplusg.h>

#define FPS 30
#define COMMANDS_PER_FRAME (300 / FPS)

static_assert (300 % FPS == 0, "Frames per second must divide 300.");

int
main (int argc, char **argv)
{
  if (argc != 2)
  {
    fprintf (stderr, "usage: %s [filename]\n", argv[0]);
    return 1;
  }

  FILE *file = fopen (argv[1], "r");

  if (file == NULL)
  {
    fprintf (stderr, "error opening file %s: %s\n", argv[1], strerror(errno));
    return 1;
  }

  /* Open the connection to the X server */
  xcb_connection_t *connection = xcb_connect (NULL, NULL);

  /* Get the first screen */
  const xcb_setup_t *setup = xcb_get_setup (connection);
  xcb_screen_iterator_t iterator = xcb_setup_roots_iterator (setup);
  xcb_screen_t *screen = iterator.data;

  unsigned int mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  unsigned int value_mask = XCB_EVENT_MASK_EXPOSURE;
  unsigned int values[] = { screen->black_pixel, value_mask };

  /* Create the window */
  xcb_window_t window = xcb_generate_id (connection);
  xcb_create_window (connection,	// connection          
		     XCB_COPY_FROM_PARENT,	// depth (same as root)
		     window,	// window id           
		     screen->root,	// parent window       
		     0, 0,	// x, y                
		     CDPLUSG_SCREEN_WIDTH, CDPLUSG_SCREEN_HEIGHT,	// width, height       
		     0,		// border_width        
		     XCB_WINDOW_CLASS_INPUT_OUTPUT,	// class               
		     screen->root_visual,	// visual              
		     mask, values);	// masks, not used yet 

  char image_data[4 * CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT];

  xcb_pixmap_t pixmap = xcb_generate_id (connection);
  xcb_create_pixmap (connection, 24, pixmap, window, CDPLUSG_SCREEN_WIDTH, CDPLUSG_SCREEN_HEIGHT);

  xcb_gcontext_t gcontext = xcb_generate_id (connection);
  xcb_create_gc (connection, gcontext, pixmap, 0, NULL);

  xcb_image_t *xcb_image = xcb_image_create_native (connection,
						    CDPLUSG_SCREEN_WIDTH, CDPLUSG_SCREEN_HEIGHT,
						    XCB_IMAGE_FORMAT_Z_PIXMAP,
						    24, NULL,
						    CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT * 4,
						    (unsigned char *) image_data);

  xcb_image_put (connection, pixmap, gcontext, xcb_image, 0, 0, 0);

  /* Map the window on the screen */
  xcb_map_window (connection, window);
  /* Make sure commands are sent before we pause so that the window gets shown */
  xcb_flush (connection);

  struct cdplusg_instruction instruction;
  struct cdplusg_graphics_state *gpx_state = cdplusg_create_graphics_state ();

  struct timeval previous_time;
  gettimeofday (&previous_time, NULL);

  unsigned int counter = 0;
  while (cdplusg_get_next_instruction_from_file (&instruction, file) == 1)
  {
    counter++;
    cdplusg_update_graphics_state (gpx_state, &instruction);

    if (counter % COMMANDS_PER_FRAME == 0)
    {
      cdplusg_write_graphics_state_to_pixmap (gpx_state, image_data, CDPLUSG_Z_FORMAT);

      xcb_image_put (connection, pixmap, gcontext, xcb_image, 0, 0, 0);
      xcb_copy_area (connection, pixmap, window, gcontext,
		     0, 0, 0, 0, CDPLUSG_SCREEN_WIDTH, CDPLUSG_SCREEN_HEIGHT);

      xcb_flush (connection);

      struct timeval next_time;
      struct timeval time_difference;

      do
      {
	      gettimeofday (&next_time, NULL);
	      timersub (&next_time, &previous_time, &time_difference);
      }
      while (time_difference.tv_usec < 10000 * COMMANDS_PER_FRAME / 3);

      previous_time = next_time;
    }
  }

  cdplusg_free_graphics_state (gpx_state);
  xcb_image_destroy (xcb_image);
  xcb_free_pixmap (connection, pixmap);
  xcb_disconnect (connection);

  return 0;
}
