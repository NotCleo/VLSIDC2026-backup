#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <pthread.h> 
#include "uart.h"
#include "pwm.h"
#include "ultrasonic.h"
#include "camera.h"
#include "classifier.h"
#include "servo.h"

// CONFIGURATION
#define PWM_CHANNEL 0
#define PWM_PERIOD_NS 20000000    // 20ms = 50Hz
#define PWM_DUTY_NS 1500000       // 1.5ms pulse width
#define DISTANCE_THRESHOLD 8.0    // in cm
#define MODEL_PATH "my_model.vnnx" // compiled model name in same directory
#define IMAGE_PATH "capture.jpg" // the result image will be saved to the working direcotry in this name

// GPIO Configuration for Pin 22 (Line 13)
#define GPIO_BASE 512
#define DEFECT_PIN_OFFSET 13      // Line 13 corresponds to Pin 22
#define GPIO_PATH "/sys/class/gpio/"

// Servo angle for defect rejection
#define SERVO_REJECT_ANGLE 60

// GLOBAL STATE & THREADING
static volatile int running = 1; // Controls the entire application
static int defect_gpio_fd = -1;

//Shared flags for Thread Communication
volatile int shutdown_requested = 0;   // True if 'B' received
volatile int start_command_received = 0; // True if start command received

// Mutex to protect shared UART access
pthread_mutex_t sys_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t input_thread_id; // ID for the input listener thread

// HMI HELPER FUNCTIONS
void hmi_set_var(const char *var_name, int value) {
    char cmd_buffer[32];
    snprintf(cmd_buffer, sizeof(cmd_buffer), "%s.val=%d", var_name, value);
    
    //  Lock mutex because we are writing to UART which is a shared resource
    pthread_mutex_lock(&sys_mutex);
    uart_hmi_send(cmd_buffer);
    pthread_mutex_unlock(&sys_mutex);
}

// SYSTEM HELPER FUNCTIONS
void force_kill_camera(void) {
    // Force kill any process holding /dev/video0
    system("fuser -k -9 /dev/video0 > /dev/null 2>&1");
    usleep(200000); 
}

// THREAD FUNCTION: INPUT MONITOR
// This runs in parallel to the main code. 
// It constantly watches for 'B' or Start commands.
void *input_monitor_thread(void *arg) {
    printf(">> Input Monitor Thread Started\n");
    
    while (running) {
        // Check input non-blocking
        char c = uart_hmi_check_input();

        if (c != 0) {
            // Lock mutex when updating global flags based on input
            pthread_mutex_lock(&sys_mutex);
            
            if (c == 'B' || c == 'b') {
                printf("\n[Thread] Shutdown Command 'B' Received\n");
                shutdown_requested = 1;
                running = 0; // Stop the whole app
            } else {
                // Filters noise. Only accept printable characters as start commands
                if (c >= 33 && c <= 126) { 
                    printf("\n[Thread] Start Command '%c' Received\n", c);
                    start_command_received = 1;
                }
            }
            
            pthread_mutex_unlock(&sys_mutex);
        }
        
        // Sleep to reduce CPU usage
        usleep(50000); // 50ms check cycle
    }
    return NULL;
}

// SIGNAL HANDLER
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    running = 0;
    shutdown_requested = 1;
}

// GPIO HELPER FUNCTIONS
static void setup_defect_gpio(void) {
    char pin_str[16];
    char path[128];
    char check_path[128];
    
    sprintf(pin_str, "%d", GPIO_BASE + DEFECT_PIN_OFFSET);
    snprintf(check_path, sizeof(check_path), "%sgpio%s/direction", GPIO_PATH, pin_str);
    if (access(check_path, F_OK) == -1) {
        snprintf(path, sizeof(path), "%sexport", GPIO_PATH);
        int fd = open(path, O_WRONLY);
        if (fd != -1) {
            write(fd, pin_str, strlen(pin_str));
            close(fd);
            usleep(100000); 
        }
    }
    int fd = open(check_path, O_WRONLY);
    if (fd != -1) {
        write(fd, "out", 3);
        close(fd);
    }
    snprintf(path, sizeof(path), "%sgpio%s/value", GPIO_PATH, pin_str);
    defect_gpio_fd = open(path, O_WRONLY);
    
    if (defect_gpio_fd == -1) {
        perror("Failed to open defect GPIO");
    } else {
        write(defect_gpio_fd, "0", 1);
        printf("Defect GPIO (Pin 22, Line 13) initialized\n");
    }
}

static void set_defect_pin_high(void) {
    if (defect_gpio_fd != -1) {
        write(defect_gpio_fd, "1", 1);
        printf("Defect pin set HIGH\n");
    }
}

static void set_defect_pin_low(void) {
    if (defect_gpio_fd != -1) {
        write(defect_gpio_fd, "0", 1);
        printf("Defect pin set LOW\n");
    }
}

// SYSTEM INITIALIZATION
int initialize_system(void) {
    printf("=== System Initialization ===\n");
    
    if (uart_init() != 0) {
        fprintf(stderr, "ERROR: UART initialization failed\n");
        return -1;
    }
    printf("✓ UART (HMI & Bluetooth) initialized\n");
    
    if (sensor_init() != 0) {
        fprintf(stderr, "ERROR: Ultrasonic sensor initialization failed\n");
        return -1;
    }
    printf("✓ Ultrasonic sensor initialized\n");
    
    force_kill_camera();
    printf("✓ Camera resources cleared\n");
    
    if (classifier_init(MODEL_PATH) != 0) {
        fprintf(stderr, "ERROR: Classifier initialization failed\n");
        return -1;
    }
    printf("✓ Classifier initialized\n");
    
    setup_defect_gpio();
    
    printf("=== System Ready ===\n\n");
    return 0;
}

// SYSTEM CLEANUP
void cleanup_system(void) {
    printf("\n=== System Cleanup ===\n");
    
    hmi_set_var("blinkMode", 0); 
    pwm_disable(PWM_CHANNEL);
    uart_close();
    sensor_cleanup();
    force_kill_camera();
    classifier_cleanup();
    
    if (defect_gpio_fd != -1) {
        set_defect_pin_low();
        close(defect_gpio_fd);
    }
    
    // Destroy Mutex
    pthread_mutex_destroy(&sys_mutex);
    
    printf("✓ System cleaned up\n");
}

// AUTOMATIC INSPECTION LOOP
void run_automatic_mode(int servo_fd) {
    printf("\n--- Entering Automatic Inspection Mode ---\n");
    char bt_buffer[64];

    while (running && !shutdown_requested) {
        // PHASE 1: SCANNING 
        
        hmi_set_var("state", 0);
        hmi_set_var("pf", 0);
        hmi_set_var("prdID", 0); 
        
        printf("Starting PWM (Conveyor)...\n");
        if (pwm_setup(PWM_CHANNEL, PWM_PERIOD_NS, PWM_DUTY_NS) != 0) {
            fprintf(stderr, "ERROR: Failed to start PWM\n");
            break;
        }
        
        printf("Monitoring distance (Waiting for object)...\n");
        double distance;
        int object_detected = 0;
        
        while (running && !object_detected) {
            // Check Global Flag instead of polling UART directly
            if (shutdown_requested) {
                printf("\n>>> Shutdown requested via Thread. Stopping. <<<\n");
                break;
            }

            distance = sensor_get_distance();
            
            if (distance > 0 && distance < DISTANCE_THRESHOLD) {
                printf("Object detected at %.2f cm!\n", distance);
                
                hmi_set_var("state", 1);
                
                printf("Stopping motor immediately...\n");
                pwm_disable(PWM_CHANNEL);
                
                object_detected = 1; 
                
            } else if (distance > 0) {
                printf("Distance: %.2f cm\r", distance);
                fflush(stdout);
            }
            
            usleep(50000); 
        }
        
        if (!running || shutdown_requested) break;
        
        //  PHASE 2: PROCESSING 
        
        usleep(500000); // Mechanical settling

        hmi_set_var("state", 2);
        
        printf("Ensuring camera device is free...\n");
        force_kill_camera();

        printf("Initializing camera...\n");
        if (camera_init() != 0) {
            fprintf(stderr, "ERROR: Camera initialization failed\n");
            break; 
        }

        printf("Capturing image...\n");
        if (camera_capture_to_file(IMAGE_PATH) != 0) {
            fprintf(stderr, "ERROR: Image capture failed\n");
            camera_cleanup();
            force_kill_camera();
            continue;
        }
        printf("✓ Image saved to %s\n", IMAGE_PATH);
        
        // Send via BT
        int unique_id = (rand() % 90000) + 10000;
        snprintf(bt_buffer, sizeof(bt_buffer), "ID:%d\n", unique_id);
        uart_bt_send(bt_buffer);
        printf(">> Bluetooth Sent: %s", bt_buffer);

        printf("Running classifier...\n");
        int class_id = classifier_predict(IMAGE_PATH);
        
        if (class_id < 0) {
            fprintf(stderr, "ERROR: Classification failed\n");
            camera_cleanup();
            force_kill_camera();
            continue;
        } 
        
        hmi_set_var("prdID", class_id);
        
        char result_text[32];
        if (class_id == 1) {
            strcpy(result_text, "DEFECTIVE");
            printf("✓ Result: DEFECTIVE (Class 1)\n");
        } else {
            strcpy(result_text, "NON DEFECTIVE");
            printf("✓ Result: NON DEFECTIVE (Class %d)\n", class_id);
        }

        snprintf(bt_buffer, sizeof(bt_buffer), "RESULT:%s\n", result_text);
        uart_bt_send(bt_buffer);
        printf(">> Bluetooth Sent: %s", bt_buffer);
        
        printf("Waiting 1 second before proceeding...\n");
        sleep(1);

        // PHASE 3: ACTION & RESTART 
        
        if (class_id == 1) {
            printf("\n*** DEFECTIVE ITEM ACTION ***\n");
            hmi_set_var("state", 4);
            hmi_set_var("pf", 2);
            
            set_defect_pin_high();
            sleep(1);
            set_defect_pin_low();
            
            printf("Restarting PWM and activating servo for rejection...\n");
            if (pwm_setup(PWM_CHANNEL, PWM_PERIOD_NS, PWM_DUTY_NS) != 0) {
                 fprintf(stderr, "ERROR: Failed to restart PWM\n");
            }
            
            printf("Activating servo for rejection...\n");
            servo_perform_cycle(servo_fd, SERVO_REJECT_ANGLE);
            printf("✓ Servo cycle completed\n");
            
        } else {
            printf("Item passed inspection.\n");
            hmi_set_var("state", 3);
            hmi_set_var("pf", 1);
            
            printf("\nRestarting PWM (conveyor)...\n");
            if (pwm_setup(PWM_CHANNEL, PWM_PERIOD_NS, PWM_DUTY_NS) != 0) {
                 fprintf(stderr, "ERROR: Failed to restart PWM\n");
            }
            printf("✓ PWM restarted\n");
        }
        
        printf("Cleaning up camera resource...\n");
        camera_cleanup();
        
        printf("\nReady for next item scan in 1 second...\n");
        sleep(1);
        
        hmi_set_var("state", 5);
        printf("--- Item Complete. Looping back to Scan ---\n\n");
    }
}

// MAIN FUNCTION
int main(int argc, char **argv) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║   Automated Inspection System v3.0     ║\n");
    printf("║   PolarFire SoC (Multi-Threaded)       ║\n");
    printf("╚════════════════════════════════════════╝\n");
    printf("\n");
    
    srand(time(NULL));
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (initialize_system() != 0) {
        fprintf(stderr, "System initialization failed, exiting.\n");
        return 1;
    }
    
    printf("Initializing servo...\n");
    int servo_fd = servo_init();
    if (servo_fd == -1) {
        cleanup_system();
        return 1;
    }
    printf("✓ Servo initialized\n\n");
    
    hmi_set_var("blinkMode", 1);
    hmi_set_var("state", 0);

    // Start the Input Monitor Thread
    if (pthread_create(&input_thread_id, NULL, input_monitor_thread, NULL) != 0) {
        perror("Failed to create input thread");
        cleanup_system();
        return 1;
    }

    printf("=== System Active ===\n");
    printf("Waiting for initial Start Command (Any Key)...\n");
    
    // Main Wait Loop
    // Checking flag set by the other thread
    while (running) {
        if (shutdown_requested) {
            printf("Shutdown requested via UART. Exiting.\n");
            break;
        }
        
        if (start_command_received) {
            printf(">>> Start command detected by Monitor Thread <<<\n");
            
            // Enter the automatic loop
            run_automatic_mode(servo_fd);
            
            // This means shutdown was requested
            break;
        }
        usleep(100000); // 100ms idle polling
    }
    
    // Wait for input thread to finish
    pthread_join(input_thread_id, NULL);
    
    // Cleanup
    servo_close(servo_fd);
    cleanup_system();
    
    printf("\nSystem shutdown complete.\n");
    return 0;
}
