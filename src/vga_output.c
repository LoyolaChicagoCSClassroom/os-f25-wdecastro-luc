// vga_output.c - Simple VGA text mode output
#include <stdint.h>
#include <stdarg.h>

// VGA text mode buffer
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

// VGA color codes
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GREY    7
#define VGA_COLOR_DARK_GREY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW        14
#define VGA_COLOR_WHITE         15

// Global state
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static int cursor_x = 0;
static int cursor_y = 0;
static uint8_t current_color = (VGA_COLOR_LIGHT_GREY << 4) | VGA_COLOR_BLACK;

// Helper: Create VGA entry
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

// Scroll screen up by one line
static void vga_scroll(void) {
    // Move all lines up
    for (int y = 0; y < VGA_HEIGHT - 1; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_buffer[(y + 1) * VGA_WIDTH + x];
        }
    }
    
    // Clear bottom line
    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', current_color);
    }
    
    cursor_y = VGA_HEIGHT - 1;
}

// Clear the screen
void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', current_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

// Set text color
void vga_set_color(uint8_t fg, uint8_t bg) {
    current_color = (bg << 4) | fg;
}

// Put a character at specific position
void vga_putchar_at(char c, uint8_t color, int x, int y) {
    vga_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

// Put a single character (with cursor advancement)
void kputchar(char c) {
    // Handle special characters
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 4) & ~3;  // Align to multiple of 4
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_putchar_at(' ', current_color, cursor_x, cursor_y);
        }
    } else {
        // Regular character
        vga_putchar_at(c, current_color, cursor_x, cursor_y);
        cursor_x++;
    }
    
    // Handle line wrap
    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    
    // Handle scrolling
    if (cursor_y >= VGA_HEIGHT) {
        vga_scroll();
    }
}

// Print a string
void kputs(const char* str) {
    while (*str) {
        kputchar(*str++);
    }
}

// Print a string (alias for compatibility)
void print_string(const char* str) {
    kputs(str);
}

// Print an unsigned integer in decimal
void print_dec(uint32_t num) {
    if (num == 0) {
        kputchar('0');
        return;
    }
    
    char buffer[32];
    int i = 0;
    
    while (num > 0) {
        buffer[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    // Print in reverse
    while (i > 0) {
        kputchar(buffer[--i]);
    }
}

// Print a signed integer in decimal
void print_int(int32_t num) {
    if (num < 0) {
        kputchar('-');
        num = -num;
    }
    print_dec((uint32_t)num);
}

// Print in hexadecimal
void print_hex(uint32_t num) {
    const char hex[] = "0123456789ABCDEF";
    kputchar('0');
    kputchar('x');
    
    for (int i = 28; i >= 0; i -= 4) {
        kputchar(hex[(num >> i) & 0xF]);
    }
}

// Print in hexadecimal (8-bit)
void print_hex8(uint8_t num) {
    const char hex[] = "0123456789ABCDEF";
    kputchar(hex[(num >> 4) & 0xF]);
    kputchar(hex[num & 0xF]);
}

// Simple printf implementation
void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 'd':  // Decimal integer
                case 'i': {
                    int val = va_arg(args, int);
                    print_int(val);
                    break;
                }
                case 'u': {  // Unsigned integer
                    uint32_t val = va_arg(args, uint32_t);
                    print_dec(val);
                    break;
                }
                case 'x':  // Hexadecimal
                case 'X': {
                    uint32_t val = va_arg(args, uint32_t);
                    print_hex(val);
                    break;
                }
                case 'c': {  // Character
                    char val = (char)va_arg(args, int);
                    kputchar(val);
                    break;
                }
                case 's': {  // String
                    const char* val = va_arg(args, const char*);
                    kputs(val);
                    break;
                }
                case 'p': {  // Pointer
                    void* val = va_arg(args, void*);
                    print_hex((uint32_t)val);
                    break;
                }
                case '%': {  // Literal %
                    kputchar('%');
                    break;
                }
                default:
                    kputchar('%');
                    kputchar(*fmt);
                    break;
            }
        } else {
            kputchar(*fmt);
        }
        fmt++;
    }
    
    va_end(args);
}

// Initialize VGA (call this at kernel start)
void vga_init(void) {
    vga_clear();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
