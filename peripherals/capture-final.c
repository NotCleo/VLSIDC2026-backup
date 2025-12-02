#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <stdint.h>

#define WIDTH 320    // Lower resolution for stability
#define HEIGHT 240
#define QUALITY 90   // JPEG Quality (1-100)
// ---------------------

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static inline uint8_t clamp(int v) {
    return (v < 0) ? 0 : ((v > 255) ? 255 : (uint8_t)v);
}

// Convert YUYV (YUV422) to RGB
// Input: 4 bytes [Y0, U, Y1, V] -> Output: 6 bytes [R,G,B, R,G,B]
void yuyv_to_rgb(uint8_t *yuyv, uint8_t *rgb, int width, int height) {
    int i, j;
    int y0, u, y1, v;
    int r, g, b;
    int pixel_count = width * height;
    
    for (i = 0, j = 0; i < pixel_count * 2; i += 4, j += 6) {
        y0 = yuyv[i];
        u  = yuyv[i + 1] - 128;
        y1 = yuyv[i + 2];
        v  = yuyv[i + 3] - 128;

        // --- Pixel 1 (Y0) ---
        r = y0 + (1.402 * v);
        g = y0 - (0.344136 * u) - (0.714136 * v);
        b = y0 + (1.772 * u);
        rgb[j]     = clamp(r);
        rgb[j + 1] = clamp(g);
        rgb[j + 2] = clamp(b);

        // --- Pixel 2 (Y1) ---
        r = y1 + (1.402 * v);
        g = y1 - (0.344136 * u) - (0.714136 * v);
        b = y1 + (1.772 * u);
        rgb[j + 3] = clamp(r);
        rgb[j + 4] = clamp(g);
        rgb[j + 5] = clamp(b);
    }
}

static int xioctl(int fh, int request, void *arg) {
    int r;
    do { r = ioctl(fh, request, arg); } while (-1 == r && EINTR == errno);
    return r;
}

int main() {
    int fd;
    struct v4l2_format fmt = {0};
    struct v4l2_buffer buf = {0};
    struct v4l2_requestbuffers req = {0};
    enum v4l2_buf_type type;
    fd_set fds;
    struct timeval tv;
    int r;
    void *buffer_start;

    fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    if (fd < 0) { perror("Opening video0"); return 1; }

    // 2. Set Format to YUYV (Raw Uncompressed)
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = WIDTH;
    fmt.fmt.pix.height = HEIGHT;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV; 
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) { perror("Setting Pixel Format"); return 1; }
    printf("Camera configured: %d x %d YUYV\n", WIDTH, HEIGHT);

    // 3. Request Buffer
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0) { perror("Requesting Buffer"); return 1; }

    // 4. Map Memory
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) { perror("Querying Buffer"); return 1; }

    buffer_start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
    if (buffer_start == MAP_FAILED) { perror("Mapping Buffer"); return 1; }

    // 5. Start Stream
    if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) { perror("Queueing Buffer"); return 1; }
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) { perror("Start Capture"); return 1; }

    // 6. Warm Up (Skip 10 frames for auto-exposure)
    printf("Warming up camera...\n");
    for(int i=0; i<10; i++) {
        FD_ZERO(&fds); FD_SET(fd, &fds);
        tv.tv_sec = 2; tv.tv_usec = 0;
        r = select(fd + 1, &fds, NULL, NULL, &tv);
        if (r <= 0) { perror("Timeout waiting for frame"); return 1; }

        xioctl(fd, VIDIOC_DQBUF, &buf);
        if (i < 9) xioctl(fd, VIDIOC_QBUF, &buf);
    }

    // 7. Capture Final Frame
    if (buf.bytesused > 0) {
        printf("Captured Raw Frame: %d bytes. Converting...\n", buf.bytesused);
        
        // Allocate RGB buffer (3 bytes per pixel)
        uint8_t *rgb_data = malloc(WIDTH * HEIGHT * 3);
        if (!rgb_data) { perror("Malloc failed"); return 1; }

        // Convert Raw YUYV -> RGB
        yuyv_to_rgb((uint8_t*)buffer_start, rgb_data, WIDTH, HEIGHT);

        // Write JPEG
        if (stbi_write_jpg("image.jpg", WIDTH, HEIGHT, 3, rgb_data, QUALITY)) {
            printf("Success! Saved as image.jpg\n");
        } else {
            printf("Error: Failed to write JPEG file.\n");
        }

        free(rgb_data);
    } else {
        printf("Error: Captured 0 bytes\n");
    }

    // 8. Cleanup
    xioctl(fd, VIDIOC_STREAMOFF, &type);
    close(fd);
    return 0;
}
