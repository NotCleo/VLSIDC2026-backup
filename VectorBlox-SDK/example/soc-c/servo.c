#include "servo.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>

// ==========================================
// CONFIGURATION
// ==========================================
#define GPIO_BASE 512       
#define SERVO_PIN_OFFSET 12 // Pin 18 (Line 12)
#define GPIO_PATH "/sys/class/gpio/"

// MG995 Timing Constants
#define PWM_PERIOD 20000     // 20ms period (50Hz)
#define PULSE_MIN 450        // 0 degrees
#define PULSE_MAX 2500       // 180 degrees

// Internal State
static int servo_fd = -1;
static char servo_pin_str[16];

// --- Internal Helper: Setup GPIO ---
static int setup_gpio_internal(void) {
    char path[128];
    char check_path[128];
    
    // Calculate global pin number
    sprintf(servo_pin_str, "%d", GPIO_BASE + SERVO_PIN_OFFSET);

    // 1. Export
    snprintf(check_path, sizeof(check_path), "%sgpio%s/direction", GPIO_PATH, servo_pin_str);
    if (access(check_path, F_OK) == -1) {
        snprintf(path, sizeof(path), "%sexport", GPIO_PATH);
        int fd = open(path, O_WRONLY);
        if (fd == -1) { 
            perror("Servo: Error exporting GPIO"); 
            return -1; 
        }
        write(fd, servo_pin_str, strlen(servo_pin_str));
        close(fd);
        usleep(100000); // Wait for sysfs to create files
    }

    // 2. Set Direction to OUT
    int dir_fd = open(check_path, O_WRONLY);
    if (dir_fd == -1) { 
        perror("Servo: Error setting direction"); 
        return -1; 
    }
    write(dir_fd, "out", 3);
    close(dir_fd);

    return 0;
}

// --- Public Functions ---

int servo_init(void) {
    if (setup_gpio_internal() != 0) return -1;

    // 3. Open Value File (Keep open for speed)
    char val_path[128];
    snprintf(val_path, sizeof(val_path), "%sgpio%s/value", GPIO_PATH, servo_pin_str);
    
    servo_fd = open(val_path, O_WRONLY);
    if (servo_fd == -1) {
        perror("Servo: Error opening value file");
        return -1;
    }

    printf("Servo Initialized on GPIO %s\n", servo_pin_str);
    // Reset to 0 position on init
    servo_set_angle(0, 500); 
    return 0;
}

void servo_set_angle(int angle, int duration_ms) {
    if (servo_fd == -1) return;

    if (angle < 0) angle = 0;
    if (angle > 180) angle = 180;

    // 1. Calculate pulse width
    // Map angle (0-180) to pulse (450-2500)
    int pulse_width = PULSE_MIN + ((angle * (PULSE_MAX - PULSE_MIN)) / 180);
    int sleep_time = PWM_PERIOD - pulse_width;

    // 2. Calculate cycles
    // duration_ms / 20ms = count
    int cycles = duration_ms / 20;

    // 3. Generate Software PWM
    // WARNING: This blocks the CPU. The conveyor/camera cannot run during this loop.
    for (int i = 0; i < cycles; i++) {
        write(servo_fd, "1", 1);
        usleep(pulse_width); 

        write(servo_fd, "0", 1);
        usleep(sleep_time);
    }
}

// Wrapper for sorting Logic
void servo_sort_left(void) {
    // Move to 0 degrees (e.g. Apple)
    printf("[Servo] Sorting LEFT (0 deg)\n");
    servo_set_angle(0, 1000); // Hold for 1 second to ensure mechanism moves
}

void servo_sort_right(void) {
    // Move to 60 degrees (e.g. Banana) - Using 60 based on your example
    printf("[Servo] Sorting RIGHT (60 deg)\n");
    servo_set_angle(60, 1000); 
    
    // Optional: Return to 0 after sorting?
    // usleep(500000);
    // servo_set_angle(0, 500);
}

void servo_close(void) {
    if (servo_fd != -1) {
        close(servo_fd);
        servo_fd = -1;
    }
}