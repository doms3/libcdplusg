#pragma once

#define CDPLUSG_SCREEN_HEIGHT 216
#define CDPLUSG_SCREEN_WIDTH  300

#define CDPLUSG_FONT_HEIGHT 12
#define CDPLUSG_FONT_WIDTH  6

#define CDPLUSG_SUBCHANNEL_WIDTH 24
#define CDPLUSG_INSTRUCTION_DATA_WIDTH 16

#define CDPLUS_COLOR_TABLE_SIZE 16

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

enum cdplusg_pixmap_format
{ CDPLUSG_Z_FORMAT };

struct cdplusg_color_table_entry
{
  char b;
  char g;
  char r;
  char a;
};

struct cdplusg_instruction
{
  enum cdplusg_instruction_type type;
  void (*action) (const struct cdplusg_instruction *, char *, struct cdplusg_color_table_entry *);
  char data[CDPLUSG_INSTRUCTION_DATA_WIDTH];
};

struct cdplusg_graphics_state
{
  char *pixels;
  struct cdplusg_color_table_entry *color_table;
};

void cdplusg_init_instruction_from_subchannel (struct cdplusg_instruction *, char *subchannel_data);
void cdplusg_init_border_preset_instruction (struct cdplusg_instruction *, char color);
void cdplusg_init_memory_preset_instruction (struct cdplusg_instruction *, char color, char repeat);
void cdplusg_init_no_op_instruction (struct cdplusg_instruction *);

struct cdplusg_graphics_state *cdplusg_create_graphics_state (void);
void cdplusg_free_graphics_state (struct cdplusg_graphics_state *);
void cdplusg_update_graphics_state (struct cdplusg_graphics_state *, struct cdplusg_instruction *);

void cdplusg_write_graphics_state_to_pixmap (struct cdplusg_graphics_state *, char *pixmap,
					     enum cdplusg_pixmap_format);

int cdplusg_get_next_instruction_from_file (struct cdplusg_instruction *, FILE *);
