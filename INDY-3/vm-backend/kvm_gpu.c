/*	Implementation of the KSU Micro graphics routines
	Author: Matthew Watson
*/

#include <SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "kvm_gpu.h"
#include "kvm_mem_map_constants.h"

SDL_Window* main_window = NULL;
SDL_Renderer* main_renderer = NULL;

SDL_Surface* target_surface = NULL;

int kvm_gpu_init(kvm_memory* mem) {
	main_window = SDL_CreateWindow(
		NULL,
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		OUTER_WINDOW_SIZE,
		OUTER_WINDOW_SIZE,
		SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP
	);

	if (!main_window) {
		printf("Error creating SDL window.\n");
		return -1;
	}

	SDL_WarpMouseInWindow(main_window, 128, 128);

	main_renderer = SDL_CreateRenderer(
		main_window,
		-1,
		SDL_RENDERER_ACCELERATED
	);

	if (!main_renderer) {
		printf("Error creating SDL renderer.\n");
		return -1;
	}

	SDL_Surface* window_surface = SDL_GetWindowSurface(main_window);
	target_surface = SDL_CreateRGBSurfaceWithFormat(0, 256, 256, window_surface->format->BitsPerPixel, window_surface->format->format);

	SDL_ShowCursor(SDL_DISABLE);

	for (int i = 0; i < 1024; i++) {
		mem->data[VRAM_TILE_MAP_TABLE + i] = 0xFF;
	}

	return 0;
}

void kvm_gpu_quit(void) {
	SDL_DestroyRenderer(main_renderer);
	SDL_DestroyWindow(main_window);
}

#pragma region Drawing Functions
uint8_t extract_bits(uint8_t b, uint8_t mask, uint8_t shift) {
	return (b & mask) >> shift;
}

void set_pixel(SDL_Surface* surface, int x, int y, Uint32 color)
{
	int px = x % WINDOW_SIZE; //potential optimization here if it's a problem later.
	int py = y % WINDOW_SIZE;

	Uint32* const target_pixel = (Uint32*)((Uint8*)surface->pixels
		+ py * surface->pitch
		+ px * surface->format->BytesPerPixel);
	*target_pixel = color;
}

// This is called if bit 2 of screen flags is set when a graphics update is triggered.
void tile_lock_update(kvm_memory* mem) {
	uint8_t* lock_table = mem->data + VRAM_TILE_LINE_LOCK_TABLE;
	uint8_t* pix_offsets = mem->data + VRAM_PIX_OFFSET_MAP;

	// Update the table

	uint8_t* screen_flags = mem->data + VRAM_SCREEN_FLAGS;
	*screen_flags &= 0b11111101; // Clear the lock update flag
}

// Tile rendering algorithm. Will always render the entire field of tiles, which is 32x32.
int render_tiles(SDL_Surface* surf, kvm_memory *mem) {
	if (!surf) {
		printf("Could not get window surface.");
		return -1;
	}

	uint8_t* mem_data = mem->data;
	uint8_t* tile_map = mem_data + VRAM_TILE_MAP_TABLE;
	uint8_t* tile_attributes = mem_data + VRAM_TILE_ATTRIBUTE_TABLE;
	uint8_t* tile_rom = mem_data + GRAPHICS_ROM_MEM_LOC;
	uint8_t* palettes = mem_data + VRAM_COLOR_PALETTES;

	uint8_t screen_flags = mem_data[VRAM_SCREEN_FLAGS]; // Currently, screen flags will only test bit 1 for x/y scroll mode.
	uint8_t* scroll_table = mem_data + VRAM_TILE_LINE_SHIFT_TABLE;
	uint8_t* lock_table = mem_data + VRAM_TILE_LINE_LOCK_TABLE;

	uint8_t perpendicular_scroll = mem_data[VRAM_PERPENDICULAR_SCROLL];

	// Get the background color
	uint8_t* bg_color_ptr = mem_data + VRAM_BGCOLOR;
	uint32_t bg_color = (bg_color_ptr[0] << 16) | (bg_color_ptr[1] << 8) | (bg_color_ptr[2]);

	uint8_t scroll_mode = extract_bits(screen_flags, 0b1, 0); // zero represents x-scroll, 1 is y-scroll

	uint8_t lock_update = extract_bits(screen_flags, 0b10, 1);
	if (lock_update) {
		tile_lock_update(mem);
	}

	SDL_LockSurface(surf);
	for (int tile_i = 0; tile_i < 1024; tile_i++) {
		int t_x = (tile_i * 8) % 256;
		int t_y = (tile_i * 8) / 256 * 8;

		uint8_t t_xcoord = t_x >> 3;
		uint8_t t_ycoord = t_y >> 3;

		// Set up scroll values for the tile.
		uint8_t x_scroll = 0;
		uint8_t y_scroll = 0;
		if (scroll_mode == 1) {
			x_scroll = scroll_table[t_ycoord];

			if(!lock_table[t_ycoord]) y_scroll = perpendicular_scroll;
		}
		else {
			y_scroll = scroll_table[t_xcoord];
			
			if(!lock_table[t_xcoord]) x_scroll = perpendicular_scroll;
		}

		uint8_t tile_id = tile_map[tile_i];

		uint8_t attributes = tile_attributes[tile_i];

		uint8_t fliph = extract_bits(attributes, 0b10000000, 7); // horizontal flip
		uint8_t flipv = extract_bits(attributes, 0b01000000, 6); // vertical flip
		uint8_t mirror = extract_bits(attributes, 0b00100000, 5); // reverse x and y
		uint8_t zeroc = extract_bits(attributes, 0b00010000, 4); // Whether or not to use the zero color (if no, bg color is used)

		uint8_t palette = extract_bits(attributes, 0b00001111, 0); // The color palette to use
		int palette_ptr = palette * 12;

		int tile_rom_ptr = tile_id * 16; // 16 bytes per tile in ROM

		//printf("x: %d, y: %d, i: (%d), id: %x, trp: %d, p: %d, p_ptr: %d, fh: %d, fv: %d, mir: %d, zc: %d, atts: %x\n", t_x, t_y, tile_i, tile_id, tile_rom_ptr, palette, palette_ptr, fliph, flipv, mirror, zeroc, attributes);

		// Set up initial drawing values for the tile
		int x = 0, x_init = 0;
		int y = 0, y_init = 0;
		int x_increment = 1;
		int y_increment = 1;

		if (fliph) {
			x += 7;
			x_init = 7;
			x_increment = -1;
		}

		if (flipv) {
			y += 7;
			y_init = 7;
			y_increment = -1;
		}

		int shift = 0;
		int byte_offset = 0;
		int bit_pos = 0;

		// Counts to 8 and then resets
		int count = 0;

		for (int pix_i = 0; pix_i < 64; pix_i++) {
			// Get the color for the pixel.
			uint8_t color_index = extract_bits(tile_rom[tile_rom_ptr + byte_offset], 0b11 << shift, shift);

			uint32_t color = bg_color;
			if ((!zeroc || color_index != 0) && tile_id != 0xff) {
				int i0 = palette_ptr + (3 * color_index);
				color = (palettes[i0] << 16) | (palettes[i0 + 1] << 8) | (palettes[i0 + 2]);
			}
			
			if (mirror) {
				// Flip the pixel x and y within the tile if it's mirrored.
				set_pixel(surf, y + t_x + x_scroll, x + t_y + y_scroll, color);
			}
			else {
				set_pixel(surf, x + t_x + x_scroll, y + t_y + y_scroll, color);
			}

			// Increment pixel drawing values
			count++;
			x += x_increment;
			if (count > 7) {
				x = x_init;
				y += y_increment;
				count = 0;
			}

			bit_pos++;
			if (bit_pos > 3) {
				bit_pos = 0;
				byte_offset++;
			}

			shift += 2;
			if (shift > 6) {
				shift = 0;
			}
		}

	}
	SDL_UnlockSurface(surf);

	return 0;
}

// Sprite rendering routine.
// Very similar to tiles, except they aren't locked to a grid, and you can render between 0 and 256 of them.
int render_sprites(SDL_Surface* surf, kvm_memory* mem) {
	if (!surf) {
		printf("Could not get window surface.");
		return -1;
	}

	uint8_t* mem_data = mem->data;
	uint8_t* sprite_x = mem_data + VRAM_SPRITE_X_TABLE;
	uint8_t* sprite_y = mem_data + VRAM_SPRITE_Y_TABLE;
	uint8_t* sprite_tiles = mem_data + VRAM_SPRITE_TILE_TABLE;
	uint8_t* sprite_attributes = mem_data + VRAM_SPRITE_ATTRIBUTE_TABLE;

	uint8_t* tile_rom = mem_data + GRAPHICS_ROM_MEM_LOC;
	uint8_t* palettes = mem_data + VRAM_COLOR_PALETTES;

	SDL_LockSurface(surf);
	for (int sprite_i = 0; sprite_i < 256; sprite_i++) {
		uint8_t t_x = sprite_x[sprite_i];
		uint8_t t_y = sprite_y[sprite_i];
		if ((uint8_t)(t_x - 1) > 248 || (uint8_t)(t_y - 1) > 248) {
			// Illegal sprite position, do not render.
			continue;
		}

		// Get info about the sprite.
		uint8_t tile_id = sprite_tiles[sprite_i];

		uint8_t attributes = sprite_attributes[sprite_i];

		uint8_t fliph = extract_bits(attributes, 0b10000000, 7); // horizontal flip
		uint8_t flipv = extract_bits(attributes, 0b01000000, 6); // vertical flip
		uint8_t mirror = extract_bits(attributes, 0b00100000, 5); // reverse x and y
		uint8_t zeroc = extract_bits(attributes, 0b00010000, 4); // Whether or not to use the zero color (if no, color is not drawn)

		uint8_t palette = extract_bits(attributes, 0b00001111, 0); // The color palette to use
		int palette_ptr = palette * 12;

		int tile_rom_ptr = tile_id * 16; // 16 bytes per tile in ROM

		//printf("x: %d, y: %d, i: (%d), id: %x, trp: %d, p: %d, p_ptr: %d, fh: %d, fv: %d, mir: %d, zc: %d, atts: %x\n", t_x, t_y, tile_i, tile_id, tile_rom_ptr, palette, palette_ptr, fliph, flipv, mirror, zeroc, attributes);

		int x = 0, x_init = 0;
		int y = 0, y_init = 0;
		int x_increment = 1;
		int y_increment = 1;

		if (fliph) {
			x += 7;
			x_init = 7;
			x_increment = -1;
		}

		if (flipv) {
			y += 7;
			y_init = 7;
			y_increment = -1;
		}

		int shift = 0;
		int byte_offset = 0;
		int bit_pos = 0;

		// Counts to 8 and then resets
		int count = 0;

		for (int pix_i = 0; pix_i < 64; pix_i++) {
			uint8_t color_index = extract_bits(tile_rom[tile_rom_ptr + byte_offset], 0b11 << shift, shift);
			//printf("TRptr: %d, offset: %d, shift: %d, ci: %d\n", tile_rom_ptr, byte_offset, shift, color_index);

			if (!zeroc || color_index != 0) {
				int i0 = palette_ptr + (3 * color_index);
				uint32_t color = (palettes[i0] << 16) | (palettes[i0 + 1] << 8) | (palettes[i0 + 2]);

				//printf("x: %d, y:%d, c:%x, ci:%d", x, y, color, color_index);
				if (mirror) {
					set_pixel(surf, y + t_x, x + t_y, color);
					//printf("x: %d, y:%d, c:%x\n", y, x, color);
				}
				else {
					set_pixel(surf, x + t_x, y + t_y, color);
				}
			}

			count++;
			x += x_increment;
			if (count > 7) {
				x = x_init;
				y += y_increment;
				count = 0;
			}

			bit_pos++;
			if (bit_pos > 3) {
				bit_pos = 0;
				byte_offset++;
			}

			shift += 2;
			if (shift > 6) {
				shift = 0;
			}
		}
	}
	SDL_UnlockSurface(surf);
	return 0;
}



#pragma endregion

int kvm_gpu_refresh_graphics(kvm_memory* mem) {


	if (render_tiles(target_surface, mem) != 0)
	{
		printf("Error rendering tiles.\n");
		return -1;
	}

	if (render_sprites(target_surface, mem) != 0)
	{
		printf("Error rendering sprites.\n");
		return -1;
	}



	//renderTarget = SDL_CreateTextureFromSurface(main_renderer, surf);
	SDL_Rect inner_resolution = {
		0,
		0,
		256,
		256
	};

	SDL_Rect outer_resolution = {
		0,
		0,
		OUTER_WINDOW_SIZE,
		OUTER_WINDOW_SIZE
	};

	SDL_BlitScaled(target_surface, &inner_resolution, SDL_GetWindowSurface(main_window), &outer_resolution);

	SDL_UpdateWindowSurface(main_window);
	return 0;
}
