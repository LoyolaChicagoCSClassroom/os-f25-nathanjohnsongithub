
#include <stdint.h>
#include "rprintf.h"
#include "page.h"

#define MULTIBOOT2_HEADER_MAGIC         0xe85250d6

const unsigned int multiboot_header[]  __attribute__((section(".multiboot"))) = {MULTIBOOT2_HEADER_MAGIC, 0, 16, -(16+MULTIBOOT2_HEADER_MAGIC), 0, 12};

uint8_t inb (uint16_t _port) {
    uint8_t rv;
    __asm__ __volatile__ ("inb %1, %0" : "=a" (rv) : "dN" (_port));
    return rv;
}

struct termbuf {
	char ascii;
	char color;
};

unsigned char keyboard_map[128] =
{
   0,  27, '1', '2', '3', '4', '5', '6', '7', '8',     /* 9 */
 '9', '0', '-', '=', '\b',     /* Backspace */
 '\t',                 /* Tab */
 'q', 'w', 'e', 'r',   /* 19 */
 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', /* Enter key */
   0,                  /* 29   - Control */
 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',     /* 39 */
'\'', '`',   0,                /* Left shift */
'\\', 'z', 'x', 'c', 'v', 'b', 'n',                    /* 49 */
 'm', ',', '.', '/',   0,                              /* Right shift */
 '*',
   0,  /* Alt */
 ' ',  /* Space bar */
   0,  /* Caps lock */
   0,  /* 59 - F1 key ... > */
   0,   0,   0,   0,   0,   0,   0,   0,
   0,  /* < ... F10 */
   0,  /* 69 - Num lock*/
   0,  /* Scroll Lock */
   0,  /* Home key */
   0,  /* Up Arrow */
   0,  /* Page Up */
 '-',
   0,  /* Left Arrow */
   0,
   0,  /* Right Arrow */
 '+',
   0,  /* 79 - End key*/
   0,  /* Down Arrow */
   0,  /* Page Down */
   0,  /* Insert Key */
   0,  /* Delete Key */
   0,   0,   0,
   0,  /* F11 Key */
   0,  /* F12 Key */
   0,  /* All other keys are undefined */
};

int x = 0;
int y = 0;
struct termbuf *vram = (struct termbuf*)0xB8000;

void helper() {
	int arr[] = {65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89};
	for(int i = 0; i < 25; i++) {	
		for (int j = 0; j < 80; j++) {
			vram[i*80 + j].ascii = arr[i];
		}
	} 
}

/* Helper function for putc, which scrolls the screen up one line.
   Return the new value for y, which is the position for the new line */
int scroll() {
	// Move every line up one
	for(int i = 0; i < 24; i++) {
		for (int j = 0; j < 80; j++) {
			vram[i*80 + j].ascii = vram[(i+1)*80 + j].ascii;
			vram[i*80 + j].color = vram[(i+1)*80 + j].color;
		}
	} 
	// Make the last line empty
	for (int i = 24; i < 25; i++) {
		for (int j = 0; j < 80; j++) {	
			vram[i*80 + j].ascii = ' ';
		}
	}
	return 24;
}

int putc(int data) {
	// We've hit the end of the screen so we need to scroll up
	if (y >= 25) {
		y = scroll();	
	}

	// Tab
	if (data == 9) {
		x += 4;
		if (x >= 80) {
			y += x / 80;
			x %= 80;
			// If the new line causes a scroll, scroll
			if (y >= 25) {
				y = scroll();	
			}
		}
		return 0;
	}

	// New line
	if (data == 10) {
		//Change line
		y += 1;
		// Reset line pointer to start
		x = 0;
		// If the new line causes a scroll, scroll
		if (y >= 25) {
			y = scroll();	
		}
		return 0;
	}

	// carriage return
	if (data == 13) {
		x = 0;
		return 0;
	}

	vram[x + y * 80].ascii = data;
	vram[x + y * 80].color = 7;
	x++;
	y += x / 80;
	x %= 80;
	return 0;
}

/*
void main () {

	//putc(65);
	//putc(66);
	helper();
	y = 80 * 25;
	esp_printf(putc, "Hello World!AHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\tCHARGLE\ncharles");	
	
	while(1);
}
*/

void main() {
    // Example putc use
    putc('X');
    putc('\n');

    // Example esp_printf use 
    esp_printf(putc, "Hello World!AHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\tCHARGLE\ncharles\n");
    esp_printf(putc, "%d\n", 1738); 
    
    // Example showing the scrolling
    int line = 1;
    while (line < 21) {     
        esp_printf(putc, "Nathan %d\n", line); 
        line++;
    }

    // Print out the execution level
    unsigned short cs;
    __asm__("movw %%cs, %0" : "=r" (cs));
    int execution_level = cs & 0x3;
    esp_printf(putc, "Current execution level: %d", execution_level);  
    
    esp_printf(putc, "\n\n\n"); 
   
  

    while(1) {
        // Get the status from PS/2 register
        uint8_t status = inb(0x64);
        // If the LSB is 1 print out the scancode and the keyboard equivalent value 
        if(status & 1) {
            uint8_t scancode = inb(0x60);
            if (scancode > 128) {
                continue;
            }
            esp_printf(putc, "%x=%c | ",scancode, keyboard_map[scancode]);
        }
    }
}

