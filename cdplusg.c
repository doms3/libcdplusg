#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdplusg.h"

static_assert (sizeof (struct cdplusg_color_table_entry) == 4,
	       "struct padding error, contact the maintainer");

static void
cdplusg_debug_print (const char *message)
{
  fputs (message, stderr);
  return;
}

static void
cdplusg_no_op_action (const struct cdplusg_instruction *this, unsigned char *,
		      struct cdplusg_color_table_entry *)
{
  if (this->type != NO_OP)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  return;
}

void
cdplusg_init_no_op_instruction (struct cdplusg_instruction *instruction)
{
  instruction->type = NO_OP;
  instruction->action = &cdplusg_no_op_action;
}

static char *
cdplusg_get_pixels_at (unsigned char *pixels, int row, int col)
{
  return &pixels[row * CDPLUSG_SCREEN_WIDTH + col];
}

static void
cdplusg_memory_preset_action (const struct cdplusg_instruction *this,
			      unsigned char *pixels, struct cdplusg_color_table_entry *)
{
  if (this->type != MEMORY_PRESET)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  unsigned char color = this->data[0] & 0x0F;
  int repeat = this->data[1] & 0x0F;

  if (repeat != 0)
    return;

  // we assume each pointer in `pixels` points to a contiguous array of 
  // bytes which is a row of the image 
  for (int i = 0; i < CDPLUSG_SCREEN_HEIGHT; i++)
  {
    unsigned char *row_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    memset (row_pixels, color, CDPLUSG_SCREEN_WIDTH);
  }
}

void
cdplusg_init_memory_preset_instruction (struct cdplusg_instruction *instruction,
					unsigned char color, char repeat)
{
  instruction->type = MEMORY_PRESET;
  instruction->data[0] = color;
  instruction->data[1] = repeat;

  instruction->action = &cdplusg_memory_preset_action;
}

static void
cdplusg_border_preset_action (const struct cdplusg_instruction *this,
			      unsigned char *pixels, struct cdplusg_color_table_entry *)
{
  if (this->type != BORDER_PRESET)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  unsigned char color = this->data[0] & 0x0F;

  for (int i = 0; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    unsigned char *row_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    memset (row_pixels, color, CDPLUSG_SCREEN_WIDTH);
  }

  for (int i = CDPLUSG_FONT_HEIGHT; i < CDPLUSG_SCREEN_HEIGHT - CDPLUSG_FONT_HEIGHT; i++)
  {
    unsigned char *left_side_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    unsigned char *right_side_pixels =
      cdplusg_get_pixels_at (pixels, i, CDPLUSG_SCREEN_WIDTH - CDPLUSG_FONT_WIDTH - 1);

    memset (left_side_pixels, color, CDPLUSG_FONT_WIDTH);
    memset (right_side_pixels, color, CDPLUSG_FONT_WIDTH);
  }

  for (int i = CDPLUSG_SCREEN_HEIGHT - CDPLUSG_FONT_HEIGHT; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    unsigned char *row_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    memset (row_pixels, color, CDPLUSG_SCREEN_WIDTH);
  }
}

static void
cdplusg_tile_block_action (const struct cdplusg_instruction *this,
			   unsigned char *pixels, struct cdplusg_color_table_entry *)
{
  if (this->type != TILE_BLOCK)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  unsigned char color_zero = this->data[0] & 0x0F;
  unsigned char color_one = this->data[1] & 0x0F;

  int row = (this->data[2] & 0x1F) * CDPLUSG_FONT_HEIGHT;
  int col = (this->data[3] & 0x3F) * CDPLUSG_FONT_WIDTH;

  const char *tile_data = &this->data[4];

  for (int i = 0; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    for (int j = 0; j < CDPLUSG_FONT_WIDTH; j++)
    {
      if (tile_data[i] & (0x20 >> j))
	*cdplusg_get_pixels_at (pixels, row + i, col + j) = color_one;
      else
	*cdplusg_get_pixels_at (pixels, row + i, col + j) = color_zero;
    }
  }
}

static void
cdplusg_tile_block_xor_action (const struct cdplusg_instruction *this,
			       unsigned char *pixels, struct cdplusg_color_table_entry *)
{
  if (this->type != TILE_BLOCK_XOR)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  unsigned char color_zero = this->data[0] & 0x0F;
  unsigned char color_one = this->data[1] & 0x0F;

  int row = (this->data[2] & 0x1F) * CDPLUSG_FONT_HEIGHT;
  int col = (this->data[3] & 0x3F) * CDPLUSG_FONT_WIDTH;

  const char *tile_data = &this->data[4];

  for (int i = 0; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    for (int j = 0; j < CDPLUSG_FONT_WIDTH; j++)
    {
      if (tile_data[i] & (0x20 >> j))
	*cdplusg_get_pixels_at (pixels, row + i, col + j) ^= color_one;
      else
	*cdplusg_get_pixels_at (pixels, row + i, col + j) ^= color_zero;
    }
  }
}

static void
cdplusg_load_color_table_low_action (const struct cdplusg_instruction *this,
				     unsigned char *, struct cdplusg_color_table_entry *color_table)
{
  if (this->type != LOAD_COLOR_TABLE_LOW)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  short *colors = (short *) this->data;

  for (int i = 0; i < 8; i++)
  {
    color_table[i].b = (colors[i] & 0x000F) << 4;
    color_table[i].r = (colors[i] & 0x3C00) >> 6;
    color_table[i].g = (colors[i] & 0x0030) | ((colors[i] & 0x0300) >> 2);
  }
}

static void
cdplusg_load_color_table_high_action (const struct cdplusg_instruction *this,
				      unsigned char *, struct cdplusg_color_table_entry *color_table)
{
  if (this->type != LOAD_COLOR_TABLE_HIGH)
    cdplusg_debug_print ("warning: instruction type does not match action.");

  short *colors = (short *) this->data;

  for (int i = 0; i < 8; i++)
  {
    color_table[i + 8].b = (colors[i] & 0x000F) << 4;
    color_table[i + 8].r = (colors[i] & 0x3C00) >> 6;
    color_table[i + 8].g = (colors[i] & 0x0030) | ((colors[i] & 0x0300) >> 2);
  }
}

void
cdplusg_init_border_preset_instruction (struct cdplusg_instruction *instruction, unsigned char color)
{
  instruction->type = BORDER_PRESET;
  instruction->data[0] = color;

  instruction->action = &cdplusg_border_preset_action;
}

void
cdplusg_init_instruction_from_subchannel (struct cdplusg_instruction *this, char *subchannel_data)
{
  char command = subchannel_data[0] & 0x3F;
  char instruction = subchannel_data[1] & 0x3F;
  char *data = &subchannel_data[4];

  // TODO: Do something with these parity values.
  // uint16_t q_channel_parity = *((uint16_t *) & subchannel_data[2]);
  // uint32_t p_channel_parity = *((uint32_t *) & subchannel_data[4 + CDPLUSG_INSTRUCTION_DATA_WIDTH]);

  if (command != 0x09 || instruction == DEFINE_TRANSPARENT_COLOR)
  {
    cdplusg_init_no_op_instruction (this);
    return;
  }

  switch (instruction)
  {
  case MEMORY_PRESET:
    this->type = MEMORY_PRESET;
    this->action = &cdplusg_memory_preset_action;
    break;
  case BORDER_PRESET:
    this->type = BORDER_PRESET;
    this->action = &cdplusg_border_preset_action;
    break;
  case TILE_BLOCK:
    this->type = TILE_BLOCK;
    this->action = &cdplusg_tile_block_action;
    break;
  case TILE_BLOCK_XOR:
    this->type = TILE_BLOCK_XOR;
    this->action = &cdplusg_tile_block_xor_action;
    break;
  case LOAD_COLOR_TABLE_LOW:
    this->type = LOAD_COLOR_TABLE_LOW;
    this->action = &cdplusg_load_color_table_low_action;
    break;
  case LOAD_COLOR_TABLE_HIGH:
    this->type = LOAD_COLOR_TABLE_HIGH;
    this->action = &cdplusg_load_color_table_high_action;
    break;
  default:
    cdplusg_init_no_op_instruction (this);
    fprintf (stderr, "warning: invalid instruction %2d found.\n", instruction);
    return;
  }

  memcpy (this->data, data, sizeof (this->data));
}

struct cdplusg_graphics_state *
cdplusg_create_graphics_state ()
{
  struct cdplusg_graphics_state *gpx_state =
    (struct cdplusg_graphics_state *) malloc (sizeof (struct cdplusg_graphics_state));

  gpx_state->pixels = (unsigned char *) malloc (CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT);
  gpx_state->color_table =
    (struct cdplusg_color_table_entry *) malloc (CDPLUS_COLOR_TABLE_SIZE *
						 sizeof (struct cdplusg_color_table_entry));

  return gpx_state;
}

void
cdplusg_free_graphics_state (struct cdplusg_graphics_state *gpx_state)
{
  if (gpx_state)
  {
    free (gpx_state->pixels);
    free (gpx_state->color_table);
  }

  free (gpx_state);
}

void
cdplusg_update_graphics_state (struct cdplusg_graphics_state *gpx_state,
			       struct cdplusg_instruction *instruction)
{
  instruction->action (instruction, gpx_state->pixels, gpx_state->color_table);
}

void
cdplusg_write_graphics_state_to_pixmap (struct cdplusg_graphics_state *gpx_state, unsigned char *pixmap,
					enum cdplusg_pixmap_format)
{
  for (int i = 0; i < CDPLUSG_SCREEN_HEIGHT * CDPLUSG_SCREEN_WIDTH; i++)
  {
    unsigned int index = gpx_state->pixels[i];
    struct cdplusg_color_table_entry *color = &gpx_state->color_table[index];

    pixmap[4 * i] = color->b;
    pixmap[4 * i + 1] = color->g;
    pixmap[4 * i + 2] = color->r;
    pixmap[4 * i + 3] = color->a;
  }
}

int
cdplusg_get_next_instruction_from_file (struct cdplusg_instruction *instruction, FILE * file)
{
  char subchannel_data[CDPLUSG_SUBCHANNEL_WIDTH];

  if (fread (subchannel_data, CDPLUSG_SUBCHANNEL_WIDTH, 1, file) == 0)
    return 0;

  cdplusg_init_instruction_from_subchannel (instruction, subchannel_data);

  return 1;
}
