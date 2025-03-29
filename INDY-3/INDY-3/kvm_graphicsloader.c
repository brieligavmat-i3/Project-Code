#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <SDL.h>

#include "kvm_graphicsloader.h"

#define FILENAME_LENGTH 256


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

	load_palettes("graphics/exp");

	SDL_FreeSurface(test_surf);
	return 0;


}

bool check_file_exists(char* filename) {
	FILE* f = fopen(filename, "r");
	if (!f) return false;

	fclose(f);
	return true;
}

int load_palettes(char* filename) {
	char bmp_fname[FILENAME_LENGTH];
	char bin_fname[FILENAME_LENGTH];

	strncpy(bmp_fname, filename, FILENAME_LENGTH);
	strncpy(bin_fname, filename, FILENAME_LENGTH);

	bmp_fname[FILENAME_LENGTH - 1] = 0;
	bin_fname[FILENAME_LENGTH - 1] = 0;

	strncat(bmp_fname, ".bmp", 10);
	strncat(bin_fname, ".kvmpal", 10);

	printf("bmp: %s\nkvmpal: %s\n", bmp_fname, bin_fname);

	// Check if palette file already exists.
	if (check_file_exists(bin_fname)) {
		printf("Binary file exists.");
	}
	else {
		printf("Binary file does not exist.");
	}

	if (check_file_exists(bmp_fname)) {
		printf("Palette file exists.");
	}
	else {
		printf("Palette file does not exist.");
	}

}

int load_graphics(char* filename) {

}