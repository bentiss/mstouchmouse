/*
 * Utility to put MS Touch Mouse into raw touch events mode.
 * Copyright (c) 2011 Maurus Cuelenaere <mcuelenaere@gmail.com>
 *
 * Based on samples/hidraw/hid-example.c from Linux kernel tree:
 *   Copyright (c) 2010 Alan Ott <alan@signal11.us>
 *   Copyright (c) 2010 Signal 11 Software
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <linux/hidraw.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef HIDIOCSFEATURE
#warning Please use more recent kernel headers (>=2.6.38)
#endif

static unsigned char scroll, flick, tap, haptic, volume;

static int send_report(int fd, unsigned char *buf, size_t len) {
    int res = ioctl(fd, HIDIOCSFEATURE(len), buf);
    if (res < 0)
        perror("HIDIOCSFEATURE failed");
    return res;
}

static void pr_buffer(unsigned char *buf, int len) {
    int i;
    for (i = 0; i < len; i++)
            printf("%02hhx ", buf[i]);
    puts("\n");
}

static int get_report(int fd, unsigned char id, unsigned char *buf, size_t len) {
    int i, res;

    buf[0] = id;
    res = ioctl(fd, HIDIOCGFEATURE(len), buf);
    if (res < 0) {
        perror("HIDIOCGFEATURE failed");
    } else {
        printf("Report data (%d):\n\t", id);
        pr_buffer(buf, res);
    }
    return res;
}

static int do_voodoo(int fd) {
    int i;
    unsigned char recv_buf[256];

    static struct {
        unsigned char buf[64];
        size_t len;
    } reports[] = {
        {{18, 0x01}, 2},
        {{18, 0x05}, 2},
        {{23, 0x05}, 2},
        {{23, 0x01}, 2},
        {{23, 0x11}, 2},
        {{40, 0x00, 0x02}, 3},
        {{40, 0x10, 0x00}, 3},
        {{0x22, 0x14, 0x01, 0x00, 0x06, 0x00, 0x00, 0x03, 0xe8, 0x00, 0x01, 0x02, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 27},
        {{0x24, 0xfe, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x5e, 0x04, 0x73, 0x07, 0xf0, 0xe0, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, 32},
    };

    for (i=0; i < sizeof(reports)/sizeof(reports[0]); i++) {
        send_report(fd, reports[i].buf, reports[i].len);
        get_report(fd, reports[i].buf[0], recv_buf, reports[i].len);
    }

    return 0;
}

static int do_simple_voodoo(int fd) {
    unsigned char buffer[27];

    get_report(fd, 0x22, buffer, sizeof(buffer));
    if (buffer[0] != 0x22 || buffer[1] != 0x14)
        return 1;

    buffer[2] = 1;
    if (!buffer[3] && buffer[4] != 0x6) {
        buffer[4] = 0x6;
        send_report(fd, buffer, sizeof(buffer));
    }

    return 0;
}

static int arc_touch_get_state(int fd) {
    unsigned char buffer[27];

    get_report(fd, 0x22, buffer, sizeof(buffer));
    if (buffer[0] != 0x22 || buffer[1] != 0x81)
        return 1;

    scroll = !!(buffer[4] & 0x01);
    flick  = !!(buffer[4] & 0x02);
    tap    = !!(buffer[4] & 0x04);
    haptic = !!(buffer[5] & 0x01);
    volume = buffer[6] + 1;

    printf("Current situation: scroll %s; flick %s; tap %s; haptic %s; volume: %d\n",
        scroll ? "on" : "off",
        flick  ? "on" : "off",
        tap    ? "on" : "off",
        haptic ? "on" : "off",
        volume);

    return 0;
}

static void arc_touch_create_report(unsigned char *buffer) {
    buffer[0] = 0x22;
    buffer[1] = 0x81;
    buffer[2] = 0x01;
    buffer[3] = 0x00;
    buffer[4] = 0x18;

    buffer[4] |= (!!scroll << 0);
    buffer[4] |= (!!flick  << 1);
    buffer[4] |= (!!tap    << 2);

//    buffer[4] |= (1        << 3);
    buffer[5] |= (!!haptic << 0);
    buffer[5] |= 0x10;
    buffer[6] = volume - 1;
}

static int arc_touch_send_voodoo(int fd) {
    unsigned char buffer[27];

    memset(buffer, 0, sizeof(buffer));

    arc_touch_create_report(buffer);
    printf("Will write:\n\t"); pr_buffer(buffer, sizeof(buffer));

    send_report(fd, buffer, sizeof(buffer));

    return 0;
}

int main(int argc, char* argv[]) {
    int fd, ret;

    if (argc <= 1) {
        printf("%s /dev/hidrawX \n", argv[0]);
        return -1;
    }

    fd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        perror("Unable to open device");
        return -2;
    }

    //ret = do_voodoo(fd);
    //ret = do_simple_voodoo(fd);
    ret = arc_touch_get_state(fd);
    if (ret)
        goto out;

    haptic = 1;
    volume = 5;

    arc_touch_send_voodoo(fd);

    arc_touch_get_state(fd);

out:
    close(fd);

    return ret;
}
