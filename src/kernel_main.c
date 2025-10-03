
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

// --- Constants for Screen Dimensions ---
#define VIDEO_WIDTH 80  // Characters per line
#define VIDEO_HEIGHT 25 // Lines on the screen
#define BYTES_PER_CHAR 2 // Character and attribute byte

// --- Function to write a single character ---
void putc(int data) {
    char character = (char)data; // Cast the input to a char
   char *vram = (char*)0xb8000; // Base address of video mem
 int current_offset; // Tracks the current position in video memory
    // Write the character to the video memory
    vram[current_offset] = character;
    // Write a default attribute byte (e.g., white on black)
    vram[current_offset + 1] = 0x0F;

    // Increment the offset to the next character position
    current_offset += BYTES_PER_CHAR;

    // Handle newline character
    if (character == '\n') {
        // Move to the beginning of the next line
        current_offset = (current_offset / (VIDEO_WIDTH * BYTES_PER_CHAR)) * (VIDEO_WIDTH * BYTES_PER_CHAR);
    }

    // Handle screen wrap-around (optional, but good practice)
    if (current_offset >= VIDEO_WIDTH * VIDEO_HEIGHT * BYTES_PER_CHAR) {
        // If at the end of the screen, reset to the beginning of video memory
        current_offset = 0;
        // You might also want to scroll the entire screen content up here
    }
}


void main() {
    char *vram = (char*)0xb8000; // Base address of video mem
    const char color = 7; // gray text on black background
    int current_offset = 0;

    putc('I');
    putc(' ');
    putc('h');
    putc('a');
    putc('v');
    putc('e');
    putc(' ');
    putc('f');
    putc('r');
    putc('i');
    putc('e');
    putc('n');
    putc('d');
    putc('s');
    putc(' ');
    putc('e');
    putc('v');
    putc('e');
    putc('r');
    putc('y');
    putc('w');
    putc('h');
    putc('e');
    putc('r');
    putc('e');
    putc('.');


    putc('\n'); 






    while(1) {
        uint8_t status = inb(0x64);
        if(status & 1) {
            uint8_t scancode = inb(0x60);
        }
    }
}	
