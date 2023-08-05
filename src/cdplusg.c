#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cdplusg.h"

static_assert (sizeof (struct cdplusg_color_table_entry) == 4, "struct padding error, contact the maintainer");

#ifdef __GLIBC__
extern char *program_invocation_short_name;
#define PROGNAME() program_invocation_short_name
#else
#define PROGNAME() getprogname ()
#endif

static unsigned char *
cdplusg_get_pixels_at (unsigned char *pixels, int row, int col)
{
  return &pixels[row * CDPLUSG_SCREEN_WIDTH + col];
}

static void
cdplusg_decode_color_to_struct (const unsigned char *color_data, struct cdplusg_color_table_entry *color_struct)
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


void
cdplusg_instruction_initialize_no_op (struct cdplusg_instruction *instruction)
{
  instruction->type = NO_OP;
}

static void
cdplusg_instruction_execute_memory_preset (const struct cdplusg_instruction *this, unsigned char *pixels)
{
  if (this->repeat != 0)
    return;

  for (int i = 0; i < CDPLUSG_SCREEN_HEIGHT; i++)
  {
    // set each row to the foreground color using memset
    unsigned char *row_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    memset (row_pixels, this->color0, CDPLUSG_SCREEN_WIDTH);
  }
}

void
cdplusg_instruction_initialize_memory_preset (struct cdplusg_instruction *instruction, unsigned char color, char repeat)
{
  instruction->type = MEMORY_PRESET;
  instruction->color0 = color;
  instruction->repeat = repeat;
}

static void
cdplusg_instruction_execute_border_preset (const struct cdplusg_instruction *this, unsigned char *pixels)
{
  unsigned char color = this->color0;

  // set top rows to the foreground color
  for (int i = 0; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    unsigned char *row_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    memset (row_pixels, color, CDPLUSG_SCREEN_WIDTH);
  }

  // set left and right columns to the foreground color
  for (int i = CDPLUSG_FONT_HEIGHT; i < CDPLUSG_SCREEN_HEIGHT - CDPLUSG_FONT_HEIGHT; i++)
  {
    unsigned char *left_side_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    unsigned char *right_side_pixels =
      cdplusg_get_pixels_at (pixels, i, CDPLUSG_SCREEN_WIDTH - CDPLUSG_FONT_WIDTH - 1);

    memset (left_side_pixels, color, CDPLUSG_FONT_WIDTH);
    memset (right_side_pixels, color, CDPLUSG_FONT_WIDTH);
  }

  // set bottom rows to the foreground color
  for (int i = CDPLUSG_SCREEN_HEIGHT - CDPLUSG_FONT_HEIGHT; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    unsigned char *row_pixels = cdplusg_get_pixels_at (pixels, i, 0);
    memset (row_pixels, color, CDPLUSG_SCREEN_WIDTH);
  }
}

void
cdplusg_instruction_initialize_border_preset (struct cdplusg_instruction *instruction, unsigned char color)
{
  instruction->type = BORDER_PRESET;
  instruction->color0 = color;
}


void
cdplusg_instruction_initialize_tile_block (struct cdplusg_instruction *instruction, unsigned char color0, unsigned char color1, int row, int column, const unsigned char *tile)
{
  instruction->type = TILE_BLOCK;
  instruction->color0 = color0;
  instruction->color1 = color1;
  instruction->row = row;
  instruction->column = column;

  memcpy (instruction->tile, tile, CDPLUSG_FONT_HEIGHT);
}

static void
cdplusg_instruction_execute_tile_block (const struct cdplusg_instruction *this, unsigned char *pixels)
{
  for (int i = 0; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    for (int j = 0; j < CDPLUSG_FONT_WIDTH; j++)
    {
      int tile_selector = this->tile[i] & (0x20 >> j);
      *cdplusg_get_pixels_at (pixels, this->row + i, this->column + j) = (tile_selector ? this->color1 : this->color0);
    }
  }
}


void
cdplusg_instruction_initialize_tile_block_xor (struct cdplusg_instruction *instruction, unsigned char color0, unsigned char color1, int row, int column, const unsigned char *tile)
{
  cdplusg_instruction_initialize_tile_block (instruction, color0, color1, row, column, tile);
  instruction->type = TILE_BLOCK_XOR;
}

static void
cdplusg_instruction_execute_tile_block_xor (const struct cdplusg_instruction *this, unsigned char *pixels)
{
  for (int i = 0; i < CDPLUSG_FONT_HEIGHT; i++)
  {
    for (int j = 0; j < CDPLUSG_FONT_WIDTH; j++)
    {
      int tile_selector = this->tile[i] & (0x20 >> j);
      *cdplusg_get_pixels_at (pixels, this->row + i, this->column + j) ^= tile_selector ? this->color1 : this->color0;
    }
  }
}

void
cdplusg_instruction_initialize_load_color_table_low  (struct cdplusg_instruction *instruction, const struct cdplusg_color_table_entry *color_table)
{
  instruction->type = LOAD_COLOR_TABLE_LOW;
  memcpy (instruction->color_table, color_table, sizeof (struct cdplusg_color_table_entry) * CDPLUSG_LOAD_COLOR_TABLE_SIZE);
}

static void
cdplusg_instruction_execute_load_color_table_low (const struct cdplusg_instruction *this, struct cdplusg_color_table_entry *colors)
{
  memcpy (&colors[0], this->color_table, sizeof (struct cdplusg_color_table_entry) * CDPLUSG_LOAD_COLOR_TABLE_SIZE);
}

void
cdplusg_instruction_initialize_load_color_table_high  (struct cdplusg_instruction *instruction, const struct cdplusg_color_table_entry *color_table)
{
  instruction->type = LOAD_COLOR_TABLE_HIGH;
  memcpy (instruction->color_table, color_table, sizeof (struct cdplusg_color_table_entry) * CDPLUSG_LOAD_COLOR_TABLE_SIZE);
}

static void
cdplusg_instruction_execute_load_color_table_high (const struct cdplusg_instruction *this, struct cdplusg_color_table_entry *colors)
{
  memcpy (&colors[8], this->color_table, sizeof (struct cdplusg_color_table_entry) * CDPLUSG_LOAD_COLOR_TABLE_SIZE);
}

void
cdplusg_instruction_initialize_from_subchannel (struct cdplusg_instruction *this, const char *subchannel)
{
  char command = subchannel[0] & 0x3F;
  char instruction = subchannel[1] & 0x3F;
  const unsigned char *data = (unsigned char *) &subchannel[4];

  if (command != 0x09 || instruction == NO_OP)
  {
    cdplusg_instruction_initialize_no_op (this);
    return;
  }

  switch (instruction)
  {
    case MEMORY_PRESET:
    {
      unsigned char color = data[0] & 0x0F;
      int repeat = data[1] & 0x0F;
      cdplusg_instruction_initialize_memory_preset (this, color, repeat);
      break;
    }
    case BORDER_PRESET:
    {
      unsigned char color = data[0] & 0x0F;
      cdplusg_instruction_initialize_border_preset (this, color);
      break;
    }
    case TILE_BLOCK:
    case TILE_BLOCK_XOR:
    {
      unsigned char color0 = data[0] & 0x0F;
      unsigned char color1 = data[1] & 0x0F;

      int row = (data[2] & 0x1F) * CDPLUSG_FONT_HEIGHT;
      int column = (data[3] & 0x3F) * CDPLUSG_FONT_WIDTH;

      const unsigned char *tile = &data[4];

      if (instruction == TILE_BLOCK)
        cdplusg_instruction_initialize_tile_block (this, color0, color1, row, column, tile);
      else
        cdplusg_instruction_initialize_tile_block_xor (this, color0, color1, row, column, tile);

      break;
    }
    case LOAD_COLOR_TABLE_LOW:
    case LOAD_COLOR_TABLE_HIGH:
    {
      struct cdplusg_color_table_entry colors [CDPLUSG_LOAD_COLOR_TABLE_SIZE] = { 0 };

      for (int i = 0; i < CDPLUSG_LOAD_COLOR_TABLE_SIZE; i++)
      {
        cdplusg_decode_color_to_struct (&data[2 * i], &colors[i]);
      }

      if (instruction == LOAD_COLOR_TABLE_LOW)
        cdplusg_instruction_initialize_load_color_table_low (this, colors);
      else
        cdplusg_instruction_initialize_load_color_table_high (this, colors);

      break;
    }
    default:
    {
      cdplusg_instruction_initialize_no_op (this);
      fprintf (stderr, "%s: warning: invalid instruction %2d found.\n", PROGNAME(), instruction);
      break;
    }
  }
}

struct cdplusg_graphics_state *
cdplusg_graphics_state_new ()
{
  struct cdplusg_graphics_state *gpx_state =
    (struct cdplusg_graphics_state *) malloc (sizeof (struct cdplusg_graphics_state));

  gpx_state->pixels = (unsigned char *) calloc (CDPLUSG_SCREEN_WIDTH * CDPLUSG_SCREEN_HEIGHT, 1);
  gpx_state->color_table =
    (struct cdplusg_color_table_entry *) calloc (CDPLUSG_COLOR_TABLE_SIZE,
        sizeof (struct cdplusg_color_table_entry));

  return gpx_state;
}

void
cdplusg_graphics_state_free (struct cdplusg_graphics_state *gpx_state)
{
  if (gpx_state)
  {
    free (gpx_state->pixels);
    free (gpx_state->color_table);
  }

  free (gpx_state);
}

void
cdplusg_graphics_state_apply_instruction (struct cdplusg_graphics_state *gpx_state, struct cdplusg_instruction *instruction)
{
  switch (instruction->type)
  {
    case NO_OP:
      break;
    case MEMORY_PRESET:
      cdplusg_instruction_execute_memory_preset (instruction, gpx_state->pixels);
      break;
    case BORDER_PRESET:
      cdplusg_instruction_execute_border_preset (instruction, gpx_state->pixels);
      break;
    case TILE_BLOCK:
      cdplusg_instruction_execute_tile_block (instruction, gpx_state->pixels);
      break;
    case TILE_BLOCK_XOR:
      cdplusg_instruction_execute_tile_block_xor (instruction, gpx_state->pixels);
      break;
    case LOAD_COLOR_TABLE_LOW:
      cdplusg_instruction_execute_load_color_table_low (instruction, gpx_state->color_table);
      break;
    case LOAD_COLOR_TABLE_HIGH:
      cdplusg_instruction_execute_load_color_table_high (instruction, gpx_state->color_table);
      break;
    default:
      fprintf (stderr, "%s: warning: invalid instruction %2d found.\n", PROGNAME(), instruction->type);
      break;
  }
}

void
cdplusg_graphics_state_to_pixmap (struct cdplusg_graphics_state *gpx_state, unsigned char *pixmap, unsigned int scale_factor, enum cdplusg_byte_order byte_order)
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

      if (byte_order == CDPLUSG_BYTE_ORDER_RGB)
      {
        pixmap[target_index++] = color->r;
        pixmap[target_index++] = color->g;
        pixmap[target_index++] = color->b;
        pixmap[target_index++] = 0xFF;
      }
      else
      {
        pixmap[target_index++] = color->b;
        pixmap[target_index++] = color->g;
        pixmap[target_index++] = color->r;
        pixmap[target_index++] = 0xFF;
      }
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
cdplusg_instruction_initialize_from_file (struct cdplusg_instruction *instruction, FILE *file)
{
  char subchannel_data [CDPLUSG_SUBCHANNEL_WIDTH];

  if (file == NULL)
    return 0;

  if (fread (subchannel_data, CDPLUSG_SUBCHANNEL_WIDTH, 1, file) == 0)
    return 0;

  cdplusg_instruction_initialize_from_subchannel (instruction, subchannel_data);

  return 1;
}

const char *cdplusg_instruction_type_to_string (enum cdplusg_instruction_type type)
{
  switch (type)
  {
    case NO_OP:
      return "NO_OP";
    case MEMORY_PRESET:
      return "MEMORY_PRESET";
    case BORDER_PRESET:
      return "BORDER_PRESET";
    case TILE_BLOCK:
      return "TILE_BLOCK";
    case SCROLL_PRESET:
      return "SCROLL_PRESET";
    case SCROLL_COPY:
      return "SCROLL_COPY";
    case DEFINE_TRANSPARENT_COLOR:
      return "DEFINE_TRANSPARENT_COLOR";
    case LOAD_COLOR_TABLE_LOW:
      return "LOAD_COLOR_TABLE_LOW";
    case LOAD_COLOR_TABLE_HIGH:
      return "LOAD_COLOR_TABLE_HIGH";
    case TILE_BLOCK_XOR:
      return "TILE_BLOCK_XOR";
    default:
      return "UNKNOWN";
  }
}
