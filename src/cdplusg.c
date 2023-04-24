#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdplusg.h"

static_assert (sizeof (struct cdplusg_color_table_entry) == 4,
	       "struct padding error, contact the maintainer");

extern char *program_invocation_short_name;

static void
cdplusg_debug_print (const char *message)
{
  fprintf (stderr, "%s: debug: %s\n", program_invocation_short_name, message);
  return;
}

static void
cdplusg_warning_print (const char *message)
{
  fprintf (stderr, "%s: warning: %s\n", program_invocation_short_name, message);
  return;
}

static void
cdplusg_no_op_action (const struct cdplusg_instruction *this, unsigned char *,
		      struct cdplusg_color_table_entry *)
{
  if (this->type != NO_OP)
    cdplusg_warning_print ("instruction type does not match action");

  return;
}

void
cdplusg_init_no_op_instruction (struct cdplusg_instruction *instruction)
{
  instruction->type = NO_OP;
  instruction->action = &cdplusg_no_op_action;
}

static unsigned char *
cdplusg_get_pixels_at (unsigned char *pixels, int row, int col)
{
  return &pixels[row * CDPLUSG_SCREEN_WIDTH + col];
}

static void
cdplusg_memory_preset_action (const struct cdplusg_instruction *this,
			      unsigned char *pixels, struct cdplusg_color_table_entry *)
{
  if (this->type != MEMORY_PRESET)
    cdplusg_warning_print ("instruction type does not match action");

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
    cdplusg_warning_print ("instruction type does not match action");

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
    cdplusg_warning_print ("instruction type does not match action");

  unsigned char color_zero = this->data[0] & 0x0F;
  unsigned char color_one = this->data[1] & 0x0F;

  int row = (this->data[2] & 0x1F) * CDPLUSG_FONT_HEIGHT;
  int col = (this->data[3] & 0x3F) * CDPLUSG_FONT_WIDTH;

  const unsigned char *tile_data = &this->data[4];

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
    cdplusg_warning_print ("instruction type does not match action");

  unsigned char color_zero = this->data[0] & 0x0F;
  unsigned char color_one  = this->data[1] & 0x0F;

  int row = (this->data[2] & 0x1F) * CDPLUSG_FONT_HEIGHT;
  int col = (this->data[3] & 0x3F) * CDPLUSG_FONT_WIDTH;

  const unsigned char *tile_data = &this->data[4];

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
cdplusg_decode_color_to_struct (const unsigned char *color_data,
              struct cdplusg_color_table_entry *color_struct)
{
  //  -byte 0-  -byte 1-
  // xxrr rrgg xxgg bbbb

  color_struct->r  = (color_data[0] & 0x3C) >> 2;
  color_struct->b  = (color_data[1] & 0x0F) >> 0;
  color_struct->g  = (color_data[0] & 0x03) << 2;
  color_struct->g |= (color_data[1] & 0x30) >> 4;

  color_struct->r = 255 * color_struct->r / 15;
  color_struct->g = 255 * color_struct->g / 15;
  color_struct->b = 255 * color_struct->b / 15;
}

static void
cdplusg_load_color_table_low_action (const struct cdplusg_instruction *this,
				     unsigned char *, struct cdplusg_color_table_entry *color_table)
{
  if (this->type != LOAD_COLOR_TABLE_LOW)
    cdplusg_warning_print ("instruction type does not match action");

  for (int i = 0; i < 8; i++)
  {
    cdplusg_decode_color_to_struct (&this->data[2 * i], &color_table[i]);
  }
}

static void
cdplusg_load_color_table_high_action (const struct cdplusg_instruction *this,
              unsigned char *, struct cdplusg_color_table_entry *color_table)
{
  if (this->type != LOAD_COLOR_TABLE_HIGH)
    cdplusg_warning_print ("instruction type does not match action");

  for (int i = 0; i < 8; i++)
  {
    cdplusg_decode_color_to_struct (&this->data[2 * i], &color_table[i + 8]);
  }
}

void
cdplusg_init_border_preset_instruction (struct cdplusg_instruction *instruction,
              unsigned char color)
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

  if (command != 0x09 || instruction == DEFINE_TRANSPARENT_COLOR || instruction == NO_OP)
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
    fprintf (stderr, "%s: warning: invalid instruction %2d found.\n", program_invocation_short_name, instruction);
    return;
  }

  memcpy (this->data, data, sizeof (this->data));
}

struct cdplusg_graphics_state *
cdplusg_create_graphics_state ()
{
  struct cdplusg_graphics_state *gpx_state =
    (struct cdplusg_graphics_state *) malloc (sizeof (struct cdplusg_graphics_state));

  gpx_state->pixels = (unsigned char *) calloc (CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT, 1);
  gpx_state->color_table =
    (struct cdplusg_color_table_entry *) calloc (CDPLUS_COLOR_TABLE_SIZE,
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
cdplusg_write_graphics_state_to_pixmap (struct cdplusg_graphics_state *gpx_state,
              unsigned char *pixmap, enum cdplusg_pixmap_format, unsigned int scale_factor)
{
  unsigned int source_index = 0;
  unsigned int target_index = 0;
  unsigned int saved_target_index = 0;
  unsigned int scale_factor_sq = scale_factor * scale_factor;

  while (source_index < CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT)
  {
    unsigned int color_index = gpx_state->pixels[source_index];
    struct cdplusg_color_table_entry *color = &gpx_state->color_table[color_index];

    for (unsigned int i = 0; i < scale_factor; i++)
    {
      assert (target_index < scale_factor_sq * 4 * CDPLUSG_SCREEN_HEIGHT * CDPLUSG_SCREEN_WIDTH);

      pixmap[target_index + 0] = color->b;
      pixmap[target_index + 1] = color->g;
      pixmap[target_index + 2] = color->r;
      pixmap[target_index + 3] = color->a;

      target_index += 4;
    }

    source_index += 1;

    if (source_index % CDPLUSG_SCREEN_WIDTH == 0)
    {
      for (unsigned int i = 1; i < scale_factor; i++)
      {
        assert (target_index < scale_factor_sq * 4 * CDPLUSG_SCREEN_HEIGHT * CDPLUSG_SCREEN_WIDTH);

        memcpy
          (&pixmap[target_index], &pixmap[saved_target_index], scale_factor * 4 * CDPLUSG_SCREEN_WIDTH);
        
        target_index += scale_factor * 4 * CDPLUSG_SCREEN_WIDTH;
      }

      saved_target_index = target_index;
    }
  }
}

int
cdplusg_get_next_instruction_from_file (struct cdplusg_instruction *instruction, FILE *file)
{
  char subchannel_data[CDPLUSG_SUBCHANNEL_WIDTH];

  if (file == NULL)
    return 0;

  if (fread (subchannel_data, CDPLUSG_SUBCHANNEL_WIDTH, 1, file) == 0)
    return 0;

  cdplusg_init_instruction_from_subchannel (instruction, subchannel_data);

  return 1;
}
