/* Define several constants for immutable memory locations
*  As well as one for window size.
*  Author: Matthew Watson
*/

#pragma once

// This is not a memory constant but I need it in enough spots for it to go here.
#define OUTER_WINDOW_SIZE 512
#define WINDOW_SIZE 256


// In the memory map, 0xE000 is the start point for program ROM information.
// This value serves as the program counter entry point
#define INSTRUCTION_ROM_MEM_LOC 0xE000

// VROM
#define GRAPHICS_ROM_MEM_LOC 0x9000

// I/O Memory
#define IO_MEM_KEYBOARD_LOC 0x7C00
#define IO_MEM_MOUSE_LOC 0x7D00

// Vram
#define VRAM_BGCOLOR 0x8000
#define VRAM_COLOR_PALETTES 0x8003
#define VRAM_SCREEN_FLAGS 0x80FF
#define VRAM_TILE_LINE_SHIFT_TABLE 0x8100
#define VRAM_TILE_LINE_LOCK_TABLE 0x8120
#define VRAM_PERPENDICULAR_SCROLL 0x8140
#define	VRAM_PIX_OFFSET_MAP 0x8300

#define VRAM_TILE_MAP_TABLE 0x8400
#define VRAM_TILE_ATTRIBUTE_TABLE 0x8800

#define VRAM_SPRITE_X_TABLE 0x8C00
#define VRAM_SPRITE_Y_TABLE 0x8D00
#define VRAM_SPRITE_TILE_TABLE 0x8E00
#define VRAM_SPRITE_ATTRIBUTE_TABLE 0x8F00