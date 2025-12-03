#ifndef PWM_H
#define PWM_H

// Configures and Enables PWM on a specific channel
// channel: The PWM channel (e.g., 0, 1, 2, 3) on the chip
// period_ns: The total period of the signal in nanoseconds (e.g., 20000000 for 50Hz)
// duty_ns: The active duration of the signal in nanoseconds (e.g., 1500000 for 1.5ms)
// Returns: 0 on success, -1 on error
int pwm_setup(int channel, int period_ns, int duty_ns);

// Disables the PWM output for a specific channel
// Returns: 0 on success, -1 on error
int pwm_disable(int channel);

#endif