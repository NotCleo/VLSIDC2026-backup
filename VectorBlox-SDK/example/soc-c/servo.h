#ifndef SERVO_H
#define SERVO_H

// Initialize the Servo GPIO
// Returns 0 on success, -1 on failure
int servo_init(void);

// Move the servo to a specific angle (0 to 180)
// duration_ms: How long to send the pulse (e.g., 1000 for 1 second)
void servo_set_angle(int angle, int duration_ms);

// Helper for "Apple" (e.g., Angle 0)
void servo_sort_left(void);

// Helper for "Banana" (e.g., Angle 60 or 180)
void servo_sort_right(void);

// Close resources
void servo_close(void);

#endif