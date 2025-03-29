// Implementation of the graphics loader.
// The previous graphics loader was a python script. 
// Author: Matthew Watson

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL.h>

#include "kvm_graphicsloader.h"
#include "leakcheck_util.h"

static int load_kvm_graphics(SDL_Surface* surf, char* bmp_fname, char* bin_fname);
static int load_kvm_palettes(SDL_Surface* surf, char* bmp_fname, char* bin_fname);

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_EVERYTHING);

	FILE *test_file = fopen("graphics/exg.bmp", "r");
	if (!test_file) {
		printf("Error, file could not open.");
		return -1;
	}

	SDL_Surface* test_surf = SDL_LoadBMP("graphics/exp.bmp");
	if (!test_surf) {
		printf("Unsuccessful surface creation.");
		return -1;
	}

	printf("Success!");

	char* newstr = load_graphics("exg", true);
	if (newstr) {
		free(newstr);
	}

	SDL_FreeSurface(test_surf);

	print_allocation_data();
	clean_allocation();
	return 0;


}

bool check_file_exists(char* filename) {
	FILE* f = fopen(filename, "r");
	if (!f) return false;

	fclose(f);
	return true;
}

char* load_graphics(char* filename, bool is_palette) {
	char bmp_fname[FILENAME_LENGTH * 2] = "graphics/";

	char* bin_fname = malloc(FILENAME_LENGTH * 2);
	strcpy(bin_fname, "graphics/out/");

	strncat(bmp_fname, filename, FILENAME_LENGTH);
	strncat(bin_fname, filename, FILENAME_LENGTH);

	// Ensure the strings are null-terminated.
	bmp_fname[FILENAME_LENGTH * 2 - 1] = 0;
	bin_fname[FILENAME_LENGTH * 2 - 1] = 0;

	strncat(bmp_fname, ".bmp", 10);

	if (is_palette) {
		strncat(bin_fname, ".kvmpal", 10);
	}
	else {
		strncat(bin_fname, ".kvmpix", 10);
	}
	

	printf("bmp: %s\nkvmbin: %s\n", bmp_fname, bin_fname);

	bool bin_exists = check_file_exists(bin_fname);
	bool bmp_exists = check_file_exists(bmp_fname);

	// If the bitmap file doesn't exist, we can't continue.
	if (!bmp_exists) {
		printf("Bitmap file '%s' does not exist.\n", bmp_fname);
		free(bin_fname);
		return NULL;
	}

	// Check if palette file already exists. If it does, we don't need to reload it.
	if (bin_exists) {
		// If the binary file exists, we can load it directly into memory, so just return.
		return bin_fname;
	}
	else {
		printf("Binary file does not exist.");

		SDL_Surface* surf = SDL_LoadBMP(bmp_fname);
		if (!surf) {
			printf("Failed to load surface from bmp file.\n");
			free(bin_fname);
			return NULL;
		}

		// If the bin file does not exist, we need to create it.
		int load_result = 0;
		if (is_palette) {
			load_result = load_kvm_palettes(surf, bmp_fname, bin_fname);
		}
		else {
			load_result = load_kvm_graphics(surf, bmp_fname, bin_fname);
		}

		SDL_FreeSurface(surf);
		
		if (load_result == 0) {
			return bin_fname;
		}
		else {
			free(bin_fname);
			return NULL;
		}
	}
}

// Returns -1 on failure, 0 otherwise.
static int load_kvm_palettes(SDL_Surface* surf, char* bmp_fname, char* bin_fname) {
	uint8_t bytes[192];

	// Check for invalid dimensions.
	if (surf->w != 4 || surf->h != 16) {
		printf("Palette load error: Palette image is %d by %d pixels.\nIt must be 4 by 16.\n", surf->w, surf->h);
		return -1;
	}

	if (surf->format->BitsPerPixel != 24) {
		printf("Palette load error: Incorrect image format. BPP: %d\n", surf->format->BitsPerPixel);
		return -1;
	}

	int index = 0;
	uint8_t* pixels = surf->pixels;
	int byte_index = 0;
	for (int i = 0; i < 64; i++) {
		// Must switch the order of the bytes
		for (int j = 2; j >= 0; j--) {
			bytes[byte_index] = pixels[3*i + j];
			byte_index++;
		}
	}

	FILE* output_file = fopen(bin_fname, "wb");
	if (!output_file) {
		printf("Error with file creation.\n");
		return -1;
	}

	if (fwrite(bytes, 1, 192, output_file) != 192) {
		printf("Error with file writing.\n");
		return -1;
	}

	fclose(output_file);

	return 0;
}

static uint8_t process_four_pixels(uint8_t* pix_array, int start_index) {

}

static int load_kvm_graphics(SDL_Surface* surf, char* bmp_fname, char* bin_fname) {
	uint8_t bytes[4096];

	// Check for invalid dimensions.
	if (surf->w != 128 || surf->h != 128) {
		printf("Graphics load error: Graphics image is %d by %d pixels.\nIt must be 128 by 128.\n", surf->w, surf->h);
		return -1;
	}

	if (surf->format->BitsPerPixel != 24) {
		printf("Palette load error: Incorrect image format. BPP: %d\n", surf->format->BitsPerPixel);
		return -1;
	}

	int index = 0;
	uint8_t* pixels = surf->pixels;


}