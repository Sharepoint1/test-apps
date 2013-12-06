/*
 * videoworker.cpp
 *
 * Author: Johan Hovold <jhovold@gmail.com>
 */

#include <QtCore/QCoreApplication>
#include <QtCore/QThread>

#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "videoworker.h"


#define CAPTURE_BUFFER_COUNT	8
#define OUTPUT_BUFFER_COUNT	4


static void v4l_streamon(int fd, enum v4l2_buf_type type, size_t count)
{
	struct v4l2_buffer buf;
	int arg;
	unsigned i;

	for (i = 0; i < count; ++i) {
		memset(&buf, 0, sizeof(buf));

		buf.type = type;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(fd, VIDIOC_QBUF, &buf) == -1)
			die_errno("VIDIOC_QBUF");
	}

	arg = type;
	if (ioctl(fd, VIDIOC_STREAMON, &arg) == -1)
		die_errno("%s", __func__);
}

static void v4l_streamoff(int fd, enum v4l2_buf_type type)
{
	int arg;

	arg = type;
	if (ioctl(fd, VIDIOC_STREAMOFF, &arg) == -1)
		err_errno("%s", __func__);
}

static struct video_buffer *v4l_buffers_alloc(int fd, enum v4l2_buf_type type,
								size_t *count)
{
	struct video_buffer *buffers;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;
	unsigned i;

	memset(&req, 0, sizeof(req));
	req.count  = *count;
	req.type   = type;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
		die_errno("VIDIOC_REQBUFS");

	if (req.count < *count)
		printf("%s - %u (%u)\n", __func__, req.count, *count);

	if (req.count < 2)
		die("insufficient buffer memory\n");

	*count = req.count;

	buffers = (struct video_buffer *)calloc(req.count, sizeof(*buffers));
	if (!buffers)
		die_errno("calloc");

	for (i = 0; i < req.count; ++i) {
		memset(&buf, 0, sizeof(buf));
		buf.type = type;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(fd, VIDIOC_QUERYBUF, &buf) == -1)
			die_errno("VIDIOC_QUERYBUF");

		buffers[i].length = buf.length;
		buffers[i].start = mmap(NULL,
					buf.length,
					PROT_READ | PROT_WRITE,
					MAP_SHARED,
					fd,
					buf.m.offset);

		if (buffers[i].start == MAP_FAILED)
			die_errno("mmap");
	}

	return buffers;
}

static void v4l_buffers_free(int fd, enum v4l2_buf_type type,
				struct video_buffer *buffers, size_t count)
{
	struct v4l2_requestbuffers req;
	unsigned i;
	int ret;

	for (i = 0; i < count; ++i) {
		ret = munmap(buffers[i].start, buffers[i].length);
		if (ret == -1)
			err_errno("munmap - buf %d", i);
	}

	memset(&req, 0, sizeof(req));
	req.count  = 0;
	req.type   = type;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd, VIDIOC_REQBUFS, &req) == -1)
		err_errno("VIDIOC_REQBUFS(0)");
}

VideoWorker::VideoWorker(const char *device_capture, const char *device_output,
				QSize &videoSize, QObject *parent) :
	QObject(parent),
	videoSize(videoSize)
{
	dev_capture = device_capture;
	dev_output = device_output;
}

VideoWorker::~VideoWorker()
{
}

void VideoWorker::initCapture()
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;

	if (ioctl(fd_capture, VIDIOC_QUERYCAP, &cap) == -1)
		die_errno("VIDIOC_QUERYCAP");

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
		die("%s is not a capture device\n", dev_capture);

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
		die("%s does not support streaming i/o\n", dev_capture);

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = videoSize.width();
	fmt.fmt.pix.height = videoSize.height();
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

	if (ioctl(fd_capture, VIDIOC_S_FMT, &fmt))
		die_errno("VIDIOC_S_FMT");

	buf_capture_count = CAPTURE_BUFFER_COUNT;
	buf_capture = v4l_buffers_alloc(fd_capture,
					V4L2_BUF_TYPE_VIDEO_CAPTURE,
					&buf_capture_count);
	if (!buf_capture)
		die("v4l_buffers_alloc");
}

void VideoWorker::initOutput()
{
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	unsigned int i;

	if (ioctl(fd_output, VIDIOC_QUERYCAP, &cap) == -1)
		die_errno("VIDIOC_QUERYCAP");

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OUTPUT))
		die("%s is not an output device\n", dev_output);

	if (!(cap.capabilities & V4L2_CAP_VIDEO_OVERLAY))
		die("%s does not support video overlay\n", dev_output);

	if (!(cap.capabilities & V4L2_CAP_STREAMING))
		die("%s does not support streaming i/o\n", dev_output);

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	if (ioctl(fd_output, VIDIOC_G_FMT, &fmt) == -1)
		die_errno("VIDEO_OUTPUT: VIDIOC_G_FMT");

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width = videoSize.width();
	fmt.fmt.pix.height = videoSize.height();
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	if (ioctl(fd_output, VIDIOC_S_FMT, &fmt) == -1)
		die_errno("VIDEO_OUTPUT: VIDIOC_S_FMT");

	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	ioctl(fd_output, VIDIOC_G_FMT, &fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.w.left = (SCREEN_WIDTH - videoSize.width()) / 2;
	fmt.fmt.win.w.top = (SCREEN_HEIGHT - videoSize.height()) / 2;
	fmt.fmt.win.w.width = videoSize.width();
	fmt.fmt.win.w.height = videoSize.height();

	if (ioctl(fd_output, VIDIOC_S_FMT, &fmt) == -1)
		die_errno("VIDEO_OVERLAY: VIDIOC_S_FMT");

	buf_output_count = OUTPUT_BUFFER_COUNT;
	buf_output = v4l_buffers_alloc(fd_output,
					V4L2_BUF_TYPE_VIDEO_OUTPUT,
					&buf_output_count);
	if (!buf_output)
		die("v4l_buffers_alloc");

	for (i = 0 ; i < buf_output_count ; ++i)
		memset(buf_output[i].start, 0, buf_output[i].length);
}

void VideoWorker::processFrame(const void *p, size_t size)
{
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_output, VIDIOC_DQBUF, &buf) == -1)
		die_errno("VIDEO_OUPUT: VIDIOC_DQBUF");

	memcpy(buf_output[buf.index].start, p, size);
	buf.bytesused = size;

	if (ioctl(fd_output, VIDIOC_QBUF, &buf) == -1)
		die_errno("VIDEO_OUPUT: VIDIOC_QBUF");
}

int VideoWorker::readFrame()
{
	struct v4l2_buffer buf;

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (ioctl(fd_capture, VIDIOC_DQBUF, &buf) == -1) {
		switch (errno) {
			case EAGAIN:
				return 0;
			case EIO:
			default:
				die_errno("VIDIOC_DQBUF");
			}
	}

	processFrame(buf_capture[buf.index].start, buf.bytesused);

	if (ioctl(fd_capture, VIDIOC_QBUF, &buf) == -1)
		die_errno("VIDIOC_QBUF");

	return 1;
}

void VideoWorker::processStream()
{
	fd_set fds;
	struct timeval tv;
	int r;

	v4l_streamon(fd_capture, V4L2_BUF_TYPE_VIDEO_CAPTURE, buf_capture_count);

	emit started();

	while (!is_paused) {
		FD_ZERO(&fds);
		FD_SET(fd_capture, &fds);

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		r = select(fd_capture + 1, &fds, NULL, NULL, &tv);
		if (r == -1) {
			if (errno == EINTR)
				continue;
			die_errno("select");
		}

		if (r == 0)
			die("select timeout\n");

		readFrame();
	}

	v4l_streamoff(fd_capture, V4L2_BUF_TYPE_VIDEO_CAPTURE);

	emit paused();
}

void VideoWorker::run()
{
	fd_output = open(dev_output, O_RDWR);
	if (fd_output < 0) {
		qCritical("could not open %s", dev_output);
		QCoreApplication::exit(EXIT_FAILURE);
	}

	fd_capture = open(dev_capture, O_RDWR | O_NONBLOCK);
	if (fd_capture < 0) {
		qCritical("could not open %s", dev_capture);
		QCoreApplication::exit(EXIT_FAILURE);
	}

	initOutput();
	initCapture();

	v4l_streamon(fd_output, V4L2_BUF_TYPE_VIDEO_OUTPUT, buf_output_count);

	is_stopped = false;
	is_paused = false;
	while (true) {
		mutex.lock();
		while (is_paused && !is_stopped)
			stateChanged.wait(&mutex);
		mutex.unlock();

		if (is_stopped)
			break;

		processStream();
	}

	v4l_streamoff(fd_output, V4L2_BUF_TYPE_VIDEO_OUTPUT);

	v4l_buffers_free(fd_capture, V4L2_BUF_TYPE_VIDEO_CAPTURE, buf_capture,
							buf_capture_count);
	v4l_buffers_free(fd_output, V4L2_BUF_TYPE_VIDEO_OUTPUT, buf_output,
							buf_output_count);
	close(fd_capture);
	close(fd_output);

	QThread::currentThread()->exit(0);
}

void VideoWorker::pause()
{
	mutex.lock();
	is_paused = !is_paused;
	stateChanged.wakeAll();
	mutex.unlock();
}

void VideoWorker::start()
{
	mutex.lock();
	is_paused = false;
	stateChanged.wakeAll();
	mutex.unlock();
}

void VideoWorker::stop()
{
	mutex.lock();
	is_paused = true;
	is_stopped = true;
	stateChanged.wakeAll();
	mutex.unlock();
}
