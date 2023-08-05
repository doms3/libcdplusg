#pragma once

#include <stdio.h> // for FILE *

#define CDPLUSG_SCREEN_HEIGHT 216
#define CDPLUSG_SCREEN_WIDTH  300

#define CDPLUSG_FONT_HEIGHT 12
#define CDPLUSG_FONT_WIDTH  6

#define CDPLUSG_SUBCHANNEL_WIDTH 24
#define CDPLUSG_INSTRUCTION_DATA_WIDTH 16

#define CDPLUSG_COLOR_TABLE_SIZE 16
#define CDPLUSG_LOAD_COLOR_TABLE_SIZE 8

#define CDPLUSG_SCROLL_UP 0
#define CDPLUSG_SCROLL_DOWN 1
#define CDPLUSG_SCROLL_LEFT 2
#define CDPLUSG_SCROLL_RIGHT 3

enum cdplusg_instruction_type
{
  NO_OP = 0,
  MEMORY_PRESET = 1,
  BORDER_PRESET = 2,
  TILE_BLOCK = 6,
  SCROLL_PRESET = 20,
  SCROLL_COPY = 24,
  DEFINE_TRANSPARENT_COLOR = 28,
  LOAD_COLOR_TABLE_LOW = 30,
  LOAD_COLOR_TABLE_HIGH = 31,
  TILE_BLOCK_XOR = 38
};

enum cdplusg_byte_order
{
  CDPLUSG_BYTE_ORDER_RGB,
  CDPLUSG_BYTE_ORDER_BGR
};

struct cdplusg_color_table_entry
{
  unsigned char b;
  unsigned char g;
  unsigned char r;
  unsigned char a;
};

struct cdplusg_instruction
{
  enum cdplusg_instruction_type type;

  unsigned char color0;
  unsigned char color1;

  int repeat;
  int row;
  int column;

  int direction;
  int offset;

  unsigned char tile [CDPLUSG_FONT_HEIGHT];

  struct cdplusg_color_table_entry color_table [CDPLUSG_LOAD_COLOR_TABLE_SIZE];
};

struct cdplusg_graphics_state
{
  unsigned char *pixels;
  struct cdplusg_color_table_entry *color_table;
};

void cdplusg_instruction_initialize_from_subchannel (struct cdplusg_instruction *instruction, const char *subchannel);
int cdplusg_instruction_initialize_from_file (struct cdplusg_instruction *instruction, FILE *file);

void cdplusg_instruction_initialize_no_op (struct cdplusg_instruction *instruction);
void cdplusg_instruction_initialize_border_preset (struct cdplusg_instruction *instruction, unsigned char color);
void cdplusg_instruction_initialize_memory_preset (struct cdplusg_instruction *instruction, unsigned char color, char repeat);
void cdplusg_instruction_initialize_tile_block (struct cdplusg_instruction *instruction, unsigned char color0, unsigned char color1, int row, int column, const unsigned char *tile);
void cdplusg_instruction_initialize_tile_block_xor (struct cdplusg_instruction *instruction, unsigned char color0, unsigned char color1, int row, int column, const unsigned char *tile);
void cdplusg_instruction_initialize_load_color_table_low  (struct cdplusg_instruction *instruction, const struct cdplusg_color_table_entry *color_table);
void cdplusg_instruction_initialize_load_color_table_high (struct cdplusg_instruction *instruction, const struct cdplusg_color_table_entry *color_table);

/** TODO: Implement these other instructions
 * void cdplusg_instruction_initialize_define_transparent_color (struct cdplusg_instruction *instruction, unsigned char color);
 * void cdplusg_instruction_initialize_scroll_preset (struct cdplusg_instruction *instruction, unsigned char color, char direction, char offset);
 * void cdplusg_instruction_initialize_scroll_copy (struct cdplusg_instruction *instruction, char direction, char offset);
 **/

struct cdplusg_graphics_state *cdplusg_graphics_state_new (void);
void cdplusg_graphics_state_free (struct cdplusg_graphics_state *state);
void cdplusg_graphics_state_apply_instruction (struct cdplusg_graphics_state *state, struct cdplusg_instruction *instruction);
void cdplusg_graphics_state_to_pixmap (struct cdplusg_graphics_state *gpx_state, unsigned char *pixmap, unsigned int scale_factor, enum cdplusg_byte_order byte_order);

