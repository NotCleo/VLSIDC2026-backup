#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

// Paths based on your ls output
#define PWM_CHIP_PATH "/sys/class/pwm/pwmchip0"

int pwm_setup(int channel, int period_ns, int duty_ns) {
    char path[256];
    char buffer[50];
    int fd;

    // 1. Export the channel
    // We try to open the 'period' file first. If it exists, the channel is 
    // already exported. If not, we export it.
    snprintf(path, sizeof(path), "%s/pwm%d/period", PWM_CHIP_PATH, channel);
    if (access(path, F_OK) != 0) {
        snprintf(path, sizeof(path), "%s/export", PWM_CHIP_PATH);
        fd = open(path, O_WRONLY);
        if (fd < 0) { perror("Failed to open export"); return -1; }
        snprintf(buffer, sizeof(buffer), "%d", channel);
        write(fd, buffer, strlen(buffer));
        close(fd);
    }

    // 2. Set Period (Must be set BEFORE duty cycle if current duty > new period)
    // Note: In your specific driver, Period is global, so be careful changing this 
    // if other channels are active.
    snprintf(path, sizeof(path), "%s/pwm%d/period", PWM_CHIP_PATH, channel);
    fd = open(path, O_WRONLY);
    if (fd < 0) { perror("Failed to open period"); return -1; }
    snprintf(buffer, sizeof(buffer), "%d", period_ns);
    write(fd, buffer, strlen(buffer));
    close(fd);

    // 3. Set Duty Cycle
    snprintf(path, sizeof(path), "%s/pwm%d/duty_cycle", PWM_CHIP_PATH, channel);
    fd = open(path, O_WRONLY);
    if (fd < 0) { perror("Failed to open duty_cycle"); return -1; }
    snprintf(buffer, sizeof(buffer), "%d", duty_ns);
    write(fd, buffer, strlen(buffer));
    close(fd);

    // 4. Enable
    snprintf(path, sizeof(path), "%s/pwm%d/enable", PWM_CHIP_PATH, channel);
    fd = open(path, O_WRONLY);
    if (fd < 0) { perror("Failed to open enable"); return -1; }
    write(fd, "1", 1);
    close(fd);

    return 0;
}

int main() {
    // Example: Configure Channel 0 to 1kHz period, 25% duty cycle
    printf("Starting PWM 0...\n");
    if (pwm_setup(0, 1000000, 250000) == 0) {
        printf("PWM configured successfully.\n");
    }
    return 0;
}