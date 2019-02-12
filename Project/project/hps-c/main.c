#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "hps_0.h"

#define HW_REGS_BASE ( ALT_STM_OFST )
#define HW_REGS_SPAN ( 0x04000000 )
#define HW_REGS_MASK ( HW_REGS_SPAN - 1 )

void *my_keyboard(int *speed);

int main(int argc, char** argv) {
	
	// Mouse Variables
    int mouse_fd;
	int bytes;
	int mode;
    unsigned char data[3];
    const char *mouse_path = "/dev/input/mice";
	int left, middle, right;
	signed char x;
	
	// LED Variables
	const char *mem_path = "/dev/mem";
	void *virtual_base;
	int mem_fd;
	int led_direction;
	int led_mask;
	int speed;
	void *h2p_lw_led_addr;
	
	// Thread variable, needed for keyboard input
	pthread_t keyboard_thread;
	
	// Open files
	mouse_fd = open(mouse_path, (O_RDWR | O_NONBLOCK)); // ***Set mouse_fd to NON-BLOCKING 
	mem_fd = open(mem_path, (O_RDWR | O_SYNC));
    if(mouse_fd == -1 || mem_fd == -1) {
        printf("ERROR: mouse_fd=%d mem_fd=%d\n", mouse_fd, mem_fd);
        return -1;
    }
	
	// Map physical address of LEDs to virtual address in user memory
	
	// The Linux built-in driver „/dev/mem‟ and mmap system-call are used to map 
	// the physical base address of pie_led component to a virtual address which 
	// can be directly accessed by Linux application software
	
	virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, mem_fd, HW_REGS_BASE);
	if(virtual_base == MAP_FAILED) {
		printf( "ERROR: mmap() failed...\n" );
		close(mem_fd);
		return -1;
	}
	
	// the virtual address of pio_led can be calculated by adding the below two 
	// offset addresses to virtual_base.
		// 1. Offset address of Lightweight HPS-to-FPGA AXI bus relative to HPS base address
		// 2. Offset address of Pio_led relative to Lightweight HPS-to-FPGA AXI bus
	// The first offset address is 0xff200000 which is defined as a constant 
	// ALT_LWFPGASLVS_OFST in the header hps.h. The hps.h is a header of Altera 
	// SoC EDS. It is located in the folder:
		// Quartus Installed Folder\embedded\ip\altera\hps\altera_hps\hwlib\include\ soc_cv_av\socal
	// The second offset address is 0x00000000 which is defined as PIO_LED_BASE 
	// in the hps_0.h header file which is generated in above section.
	
	h2p_lw_led_addr=virtual_base + ((unsigned long)(ALT_LWFPGASLVS_OFST + PIO_LED_BASE) & (unsigned long)(HW_REGS_MASK));
	
	// Set initial conditions
	led_direction = 0; // DEFAULT : Right to Left
	led_mask = 0x01;   // DEFAULT : Position LEDR0
	bytes = 0;         // DEFAULT : Nothing read from mouse
	speed = 100;       // DEFAULT : 100ms LED movements
	mode = 0;          // DEFAULT : Program controlled by mouse buttons
	
	printf("Adjust direction of LEDs with mouse\n");
	printf("Adjust speed of LEDs with keyboard\n");
	
	// Create new thread for user keyboard input
	if(pthread_create(&keyboard_thread, NULL, my_keyboard, &speed) == -1) {
		printf("Failed to create keyboard thread\n");
		return -1;
	}
	
	// Loop, check for user mouse input
	while(1) {
		
		// Map LEDs
		*(uint32_t *)h2p_lw_led_addr = ~led_mask; 
		
		// Wait for value of speed (base 100)
		usleep(speed * 1000);
		
		// Update mouse
		bytes = read(mouse_fd, data, sizeof(data));
		
		if(bytes > 0) { // Mouse input received, update LEDs
			
            left = data[0] & 0x1; // 0001
            right = data[0] & 0x2; // 0010
			middle = data[0] & 0x4; // 0100
			
			x = data[1];
			
				// Switch mode of operation if middle button pressed
				if(middle > 0 && mode == 0) {
					mode = 1; // Movement-controlled
				}
				else if(middle > 0 && mode == 1) {
					mode = 0; // Button-controlled
				}
				
				// Update LEDs
				
				if(mode == 0) { // Button-controlled
				
					// If direction = left and mouse = right
					if(led_direction == 0 && right > 0) { // Change direction
					
						// Update direction
						led_direction = 1;

						// Check for roll over
						if(led_mask == 0x01) { // Last position
							led_mask = (0x01 << (PIO_LED_DATA_WIDTH-1)); // First position
						}
						else { // increment
							led_mask >>= 1;
						}
					}
					// If direction = right and mouse = left
					else if(led_direction == 1 && left > 0) { // Change direction
					
						// Update direction
						led_direction = 0;

						// Check for roll over
						if(led_mask == (0x01 << (PIO_LED_DATA_WIDTH-1))) { // Last position
							led_mask = 0x01; // First position
						}
						else { // increment
							led_mask <<= 1;
						}
					}
					// Input, but doesn't affect anything
					else if(led_direction == 0) {

						// Check for roll over
						if(led_mask == (0x01 << (PIO_LED_DATA_WIDTH-1))) { // Last position
							led_mask = 0x01; // First position
						}
						else { // increment
							led_mask <<= 1;
						}
					}
					// Input, but doesn't affect anything
					else if(led_direction == 1) {

						// Check for roll over
						if(led_mask == 0x01) { // Last position
							led_mask = (0x01 << (PIO_LED_DATA_WIDTH-1)); // First position
						}
						else { // increment
							led_mask >>= 1;
						}
					}
				}
				else { // mode = 1 // Movement-controlled
				
					// If direction = left and mouse = right
					if(led_direction == 0 && x > 0) { // Change direction
					
						// Update direction
						led_direction = 1;

						// Check for roll over
						if(led_mask == 0x01) { // Last position
							led_mask = (0x01 << (PIO_LED_DATA_WIDTH-1)); // First position
						}
						else { // increment
							led_mask >>= 1;
						}
					}
					// If direction = right and mouse = left
					else if(led_direction == 1 && x < 0) { // Change direction
					
						// Update direction
						led_direction = 0;

						// Check for roll over
						if(led_mask == (0x01 << (PIO_LED_DATA_WIDTH-1))) { // Last position
							led_mask = 0x01; // First position
						}
						else { // increment
							led_mask <<= 1;
						}
					}
					// Input, but doesn't affect anything
					else if(led_direction == 0) {

						// Check for roll over
						if(led_mask == (0x01 << (PIO_LED_DATA_WIDTH-1))) { // Last position
							led_mask = 0x01; // First position
						}
						else { // increment
							led_mask <<= 1;
						}
					}
					// Input, but doesn't affect anything
					else if(led_direction == 1) {

						// Check for roll over
						if(led_mask == 0x01) { // Last position
							led_mask = (0x01 << (PIO_LED_DATA_WIDTH-1)); // First position
						}
						else { // increment
							led_mask >>= 1;
						}
					}
				}
		}
		else { // No mouse inputs, update LEDs
		
			if(led_direction == 0) {

				// Check for roll over
				if(led_mask == (0x01 << (PIO_LED_DATA_WIDTH-1))) { // Last position
					led_mask = 0x01; // First position
				}
				else { // increment
					led_mask <<= 1;
				}
			}
			else if(led_direction == 1) {

				// Check for roll over
				if(led_mask == 0x01) { // Last position
					led_mask = (0x01 << (PIO_LED_DATA_WIDTH-1)); // First position
				}
				else { // increment
					led_mask >>= 1;
				}
			}
		}
	}
	
	(void) pthread_join(keyboard_thread, NULL);
	return 0;
}

// Function in separate thread that handles keyboard input
void *my_keyboard(int *speed) {
	while(1) {
		scanf("%d", speed); // scanf() blocks this loop, not loop in other thread
	}
}