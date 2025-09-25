
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#define MULTIBOOT2_HEADER_MAGIC         0xe85250d6

const unsigned int multiboot_header[]  __attribute__((section(".multiboot"))) = {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16+MULTIBOOT2_HEADER_MAGIC), 0, 12};

uint8_t inb (uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

void main() {
    unsigned short *vram = (unsigned short*)0xb8000; // Base address of video mem
    const unsigned char color = 7; // gray text on black background

    while(1) {
        uint8_t status = inb(0x64);
        if(status & 1) {
            uint8_t scancode = inb(0x60);
        }
    }
}

unsigned int screen_offset = 0;

typedef struct {
	char character;
	char color;
}
char_entry;

volatile char_entry *video_memory = (char_entry *)0xB8000;

void putc(int data) {
	if (screen_offset >= 4000) {
		return;
	}

	char_entry entry = {
		.character = (char)data,
		.color = 0x0F
	};

	video_memory[screen_offset] = entry;

	screen_offset++;
}


#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
char screen_buffer[SCREEN_HEIGHT][SCREEN_WIDTH];

void scroll_screen_up(void) {
	int row, col;
	for (row = 1; row < SCREEN_HEIGHT; row++) {
		for (col = 0; col < SCREEN_WIDTH; col++) {
			screen_buffer[row - 1][col] = screen_buffer[row][col];
			}
		}
	for (col = 0; col < SCREEN_WIDTH; col++) {
		screen_buffer[SCREEN_HEIGHT - 1][col] = ' ';
	}
}
	
