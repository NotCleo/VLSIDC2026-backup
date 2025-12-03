#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>
#include <ctype.h>

static volatile int keep_running = 1;

void int_handler(int signum) {
    (void)signum;
    keep_running = 0;
}

int configure_serial(int fd, int baud)
{
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        perror("tcgetattr");
        return -1;
    }


    tty.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | ISTRIP | IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    tty.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    tty.c_cflag |= CS8 | CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_cc[VMIN] = 1;    
    tty.c_cc[VTIME] = 0;   

    speed_t speed;
    switch (baud) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 115200: speed = B115200; break;
        default:
            fprintf(stderr, "Unsupported baud %d, using 9600\n", baud);
            speed = B9600;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("tcsetattr");
        return -1;
    }

    
    tcflush(fd, TCIOFLUSH);
    return 0;
}

int main(int argc, char **argv)
{
    const char *dev = "/dev/ttyS0;
    int baud = 9600;

    if (argc >= 2) dev = argv[1];
    if (argc >= 3) baud = atoi(argv[2]);

    printf("Opening serial device: %s at %d baud\n", dev, baud);

    int fd = open(dev, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "ERROR: cannot open %s: %s\n", dev, strerror(errno));
        return 2;
    }

    if (configure_serial(fd, baud) != 0) {
        fprintf(stderr, "ERROR: failed to configure serial port\n");
        close(fd);
        return 3;
    }

    // handle ctrl-c
    signal(SIGINT, int_handler);
    signal(SIGTERM, int_handler);

    unsigned char buf[256];
    ssize_t n;

    printf("Listening...\n");

    while (keep_running) {
        n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read");
            break;
        } else if (n == 0) {
            // no data
            continue;
        }

        for (ssize_t i = 0; i < n; ++i) {
            unsigned char c = buf[i];
            if (isprint(c) || c == '\n' || c == '\r' || c == '\t') {
                putchar(c);
                fflush(stdout);
            } else {
                // print hex for non-printables
                printf("[0x%02X]", c);
                fflush(stdout);
            }
        }
    }

    printf("\nExiting %s\n", dev);
    close(fd);
    return 0;
}
