#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/time.h> // Needed for gettimeofday()

// ============================================================
// CONFIGURATION AREA
// ============================================================
// Run: ls /sys/class/gpio/gpiochip* // The number in the name is your base.
#define GPIO_BASE 512

#define TRIG_OFFSET 5    // Pin 11
#define ECHO_OFFSET 15   // Pin 13

#define GPIO_PATH "/sys/class/gpio/"

char TRIG_PIN[16];
char ECHO_PIN[16];

// Helper: Export and Configure Direction
void setup_gpio(const char *pin, const char *direction) {
    char path[128];
    char check_path[128];
    
    // 1. Export if needed
    snprintf(check_path, sizeof(check_path), "%sgpio%s/direction", GPIO_PATH, pin);
    if (access(check_path, F_OK) == -1) {
        snprintf(path, sizeof(path), "%sexport", GPIO_PATH);
        int fd = open(path, O_WRONLY);
        if (fd == -1) { perror("Error exporting"); exit(1); }
        write(fd, pin, strlen(pin));
        close(fd);
        usleep(100000); // Wait for sysfs
    }

    // 2. Set Direction
    int fd = open(check_path, O_WRONLY);
    if (fd == -1) { perror("Error setting direction"); exit(1); }
    write(fd, direction, strlen(direction));
    close(fd);
}

// Helper: Open the value file and return file descriptor
int open_gpio_value(const char *pin) {
    char path[128];
    snprintf(path, sizeof(path), "%sgpio%s/value", GPIO_PATH, pin);
    int fd = open(path, O_RDWR);
    if (fd == -1) { perror("Error opening value file"); exit(1); }
    return fd;
}

int main() {
    // Calculate global GPIO numbers
    sprintf(TRIG_PIN, "%d", GPIO_BASE + TRIG_OFFSET);
    sprintf(ECHO_PIN, "%d", GPIO_BASE + ECHO_OFFSET);

    printf("=== Ultrasonic Distance Measurer ===\n");
    printf("Trig: %s | Echo: %s\n", TRIG_PIN, ECHO_PIN);

    // Setup Pins
    setup_gpio(TRIG_PIN, "out");
    setup_gpio(ECHO_PIN, "in");

    // Open File Descriptors (Keep them open for speed!)
    int trig_fd = open_gpio_value(TRIG_PIN);
    int echo_fd = open_gpio_value(ECHO_PIN);

    char buffer[2];
    struct timeval start, end;

    while (1) {
        printf("Distance: ");
        fflush(stdout);

        // 1. TRIGGER PULSE (10us)
        write(trig_fd, "1", 1);
        usleep(10);
        write(trig_fd, "0", 1);

        // 2. WAIT FOR ECHO START (0 -> 1)
        // We use a timeout loop to prevent hanging forever
        long timeout = 0;
        int signal_started = 0;
        
        while (timeout < 50000) { // Timeout safety
            lseek(echo_fd, 0, SEEK_SET); // Rewind file
            read(echo_fd, buffer, 1);
            if (buffer[0] == '1') {
                gettimeofday(&start, NULL); // Record Start Time
                signal_started = 1;
                break;
            }
            timeout++;
        }

        if (!signal_started) {
            printf("Sensor Timed out (Start)\n");
            usleep(500000);
            continue;
        }

        // 3. WAIT FOR ECHO END (1 -> 0)
        timeout = 0;
        int signal_ended = 0;
        while (timeout < 50000) {
            lseek(echo_fd, 0, SEEK_SET);
            read(echo_fd, buffer, 1);
            if (buffer[0] == '0') {
                gettimeofday(&end, NULL); // Record End Time
                signal_ended = 1;
                break;
            }
            timeout++;
        }

        if (!signal_ended) {
            printf("Sensor Timed out (End)\n");
        } else {
            // 4. CALCULATE DISTANCE
            // Time in microseconds
            long seconds = end.tv_sec - start.tv_sec;
            long micros = ((seconds * 1000000) + end.tv_usec) - start.tv_usec;
            
            // Speed of sound is 343 m/s = 0.0343 cm/us
            // Distance = (Time * Speed) / 2
            double distance = (micros * 0.0343) / 2.0;

            if (distance > 400 || distance < 2) {
                printf("Out of range (%.2f cm)\n", distance);
            } else {
                printf("%.2f cm\n", distance);
            }
        }

        // Wait before next measurement
        usleep(1000000); // 1 second
    }

    close(trig_fd);
    close(echo_fd);
    return 0;
}