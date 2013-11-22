/*
 * vo_atmel-test.c - Testing the LCD HEO of Atmel SoCs
 *
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 * Copyright (C) 2009-2012 Tomi Valkeinen
 * Author: Nicolas Ferre <nicolas.ferre@atmel.com>
 * Copyright (C) 2012 Atmel, Nicolas Ferre

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <sys/types.h>
#include <linux/videodev2.h>

typedef unsigned long uint32_t;

static int v4l2_overlay_ioctl(int fd, int req, void *arg, const char* msg)
{
	int ret;
	ret = ioctl(fd, req, arg);
	if (ret < 0) {
		printf("Error %s\n", msg);
		return -1;
	}
	return 0;
}

/* ready for multi-buffering but not used */
#define BUF_NBR 1

struct atmel_priv_t {
	int v4l2_fd;
	int src_width;
	int src_height;
	int dst_width;
	int dst_height;
	int len[BUF_NBR];
	void *buf[BUF_NBR];
	struct v4l2_format format;
	struct v4l2_pix_format pixformat;
	struct v4l2_requestbuffers reqbuf;
};

static struct atmel_priv_t atmel_priv; 

/*****************************************************************************
 * preinit
 *
 * Preinitializes driver
 *   returns: zero on successful initialization, non-zero on error.
 *
 ****************************************************************************/
static int preinit(void)
{
	struct atmel_priv_t *priv = &atmel_priv;

	printf("vo_atmel: preinit() was called\n");

	priv->v4l2_fd = open("/dev/video0", O_RDWR);
	/* TODO: choose proper /dev/videoX device */

	if(priv->v4l2_fd < 0) {
		printf("vo_atmel: Could not open base video4linux device\n");
		return -1;
	}

	return 0;
}

/*****************************************************************************
 * config
 *
 * Config the display driver.
 * params:
 *   src_width,srcheight: image source size
 *   dst_width,dst_height: size of the requested window size, just a hint
 *   fullscreen: flag, 0=windowd 1=fullscreen, just a hint
 *   title: window title, if available
 *   format: fourcc of pixel format
 * returns : zero on successful initialization, non-zero on error.
 *
 ****************************************************************************/
static int config(uint32_t src_width, uint32_t src_height,
		uint32_t dst_width, uint32_t dst_height, uint32_t flags,
		char *title, uint32_t fmt)
{
	int ret;
	int i;
	int fd = atmel_priv.v4l2_fd;
	struct v4l2_format format;
	struct v4l2_requestbuffers reqbuf;
	struct v4l2_buffer buf;
	void *start;
	uint32_t type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	if(fmt != V4L2_PIX_FMT_YUV420) {
		printf("vo_atmel: unsupported fourcc for this driver\n");
		return -1;
	}

	atmel_priv.src_width = src_width;
	atmel_priv.src_height = src_height;
	atmel_priv.dst_width = dst_width;
	atmel_priv.dst_height = dst_height;

	memset(&format, 0x00, sizeof (struct v4l2_format));
	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get format");
	if (ret)
		return ret;
	printf("vo_atmel: config() G_FMT (init) w=%d h=%d\n", format.fmt.pix.width, format.fmt.pix.height);

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	format.fmt.pix.pixelformat = fmt;
	format.fmt.pix.width = src_width;
	format.fmt.pix.height = src_height;
	printf("vo_atmel: config() S_FMT w=%d h=%d\n", format.fmt.pix.width, format.fmt.pix.height);
	ret = v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &format, "set output format");
	if (ret)
		return ret;

	format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get output format");
	printf("vo_atmel: config() G_FMT w=%d h=%d\n", format.fmt.pix.width, format.fmt.pix.height);
	if (ret)
		return ret;

	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret = v4l2_overlay_ioctl(fd, VIDIOC_G_FMT, &format, "get v4l2_overlay format");
	printf("v4l2_overlay_get_position:: w=%d h=%d\n", format.fmt.win.w.width, format.fmt.win.w.height);
	if (ret)
		return ret;

	format.fmt.win.w.left = 200;
	format.fmt.win.w.top = 10;
	format.fmt.win.w.width = dst_width;
	format.fmt.win.w.height = dst_height;
	format.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ret = v4l2_overlay_ioctl(fd, VIDIOC_S_FMT, &format, "set v4l2_overlay format");
	printf("v4l2_overlay_set_position:: w=%d h=%d\n", format.fmt.win.w.width, format.fmt.win.w.height);
	if (ret)
		return ret;

	reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	reqbuf.memory = V4L2_MEMORY_MMAP;
	reqbuf.count = BUF_NBR;
	printf("v4l2_overlay_request_buffer, requested=%u\n", reqbuf.count);
	ret = v4l2_overlay_ioctl(fd, VIDIOC_REQBUFS, &reqbuf, "requets v4l2 buffers");
	printf("v4l2_overlay_request_buffer, result: requested=%u return=%u\n", reqbuf.count, ret);
	if (ret)
		return ret;

	for (i = 0 ; i < BUF_NBR ; i++) {
		memset(&buf, '\0', sizeof(buf));
		buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;
		ret = v4l2_overlay_ioctl(fd, VIDIOC_QUERYBUF, &buf, "querybuf ioctl");
		printf("v4l2_overlay_query_buffer, length=%u offset=%u\n", buf.length, buf.m.offset);
		if (ret)
			return ret;

		if (buf.flags == V4L2_BUF_FLAG_MAPPED) {	
			printf("Trying to mmap buffers that are already mapped!\n");
			return -EINVAL;
		}

		printf("mmap, length=%u offset=%u\n", buf.length, buf.m.offset);
		start = (void*) mmap(NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
		if (start == 0) {
			printf("map failed, length=%u offset=%u\n", buf.length, buf.m.offset);
			return -EINVAL;
		}
		atmel_priv.len[i] = buf.length;
		atmel_priv.buf[i] = start;

		/* Temporary fill the buffer with nothing */
		memset(atmel_priv.buf[i], '\0', atmel_priv.len[i]);

		ret = v4l2_overlay_ioctl(fd, VIDIOC_QBUF, &buf, "qbuf");
		if (ret)
			return ret;

		printf("buffer %d queued\n", i);
	}

	ret = v4l2_overlay_ioctl(fd, VIDIOC_STREAMON, &type, "stream on");
	if(ret) {
		printf("Stream on failed\n");
		return ret;
	}

	return 0;	
}

/*****************************************************************************
 * uninit
 ****************************************************************************/
static void uninit(void)
{
	struct atmel_priv_t *priv = &atmel_priv;
	uint32_t type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	int i;

	printf("vo_atmel: uninit() was called\n");
	v4l2_overlay_ioctl(priv->v4l2_fd, VIDIOC_STREAMOFF, &type, "stream off");
	for (i = 0 ; i < BUF_NBR ; i++) {
		if (atmel_priv.len[i] > 0)
			munmap(atmel_priv.buf[i], atmel_priv.len[i]);
	}
	close(priv->v4l2_fd);
	priv->v4l2_fd = -1;
}

/*****************************************************************************
 * main
 ****************************************************************************/
#define BUFF_SIZE 1024

int main(int argc, char **argv)
{
	int	ret = 0;
	int	i;
	char	chunk[BUFF_SIZE];
	int	lenna_fd;
	int	total;
	char    *ptr;
	struct v4l2_buffer buf;
	uint32_t type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	printf("vo_atmel %d.%d.%d (%s)\n", VERSION, PATCHLEVEL, SUBLEVEL,
		VERSION_NAME);

	ret = preinit();
	if (ret)
		return -1;

	/* with scaling */
	ret = config(512, 512, 400, 400,
		     0, "lenna",
		     V4L2_PIX_FMT_YUV420);
	if (ret)
		goto err;

	printf("vo_atmel: open file\n");
	lenna_fd = open("./lenna.yuv", O_RDWR);
	if (lenna_fd < 0) {
		printf("vo_atmel: Could not open image file\n");
		goto err;
	}

	memset(&buf, '\0', sizeof(buf));
	buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;

	// Test with solid colors:
	//memset(atmel_priv.buf[buf.index], '\0', atmel_priv.len[buf.index]);
	//memset(atmel_priv.buf[buf.index], 0xff, atmel_priv.len[buf.index]);

	// Test with YUV data
	total = atmel_priv.len[buf.index];
	ptr = (char *)atmel_priv.buf[buf.index];
	ret = read(lenna_fd, chunk, BUFF_SIZE);
	while (ret > 0) {
		total -= ret;
		if (total <= 0) {
			printf("ret remains: %d\n", ret);
			break;
		}
		memcpy(ptr, chunk, ret);
		ptr += ret;
		ret = read(lenna_fd, chunk, BUFF_SIZE);
	}

	ret = 0;
	printf("vo_atmel: sleeping a little bit\n");
	sleep(10);

err:
	uninit();
	return ret;
}
