/* 
 * ATMEL - SAMA5D3 
 * Capture example with video display using overlay capabilities
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <signal.h>
#include <getopt.h>  

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), '\0', sizeof(x))
#define VIDEO_BUF_NBR 4
#define CAPTURE_BUF_NBR 2

struct buffer
{
	void   *start;
	size_t	length;
};

static char *capture_dev_name;
static int fd_capture = -1;
static char *video_dev_name;
static int fd_video =  -1;
static unsigned int n_buffers;
struct buffer *buffers;
struct buffer *video_buffers;
static int count = 1000;

static void errno_exit(const char *s)
{
	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
	exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
	int r;
	
	do {
		r = ioctl(fh, request, arg);
	} while (-1 == r && EINTR == errno);

	return r;
}

static void usage(FILE *fp, int argc, char **argv)
{
	fprintf(fp,
		"Usage: %s [options]\n\n"
		"Version 0.1\n"
		"Options:\n"
		"-d | --device <name>   Video capture device name  [%s]\n"
		"-v | --video  <name>   Video output devive name   [%s]\n" 
		"-c | --count  <value>  Number of frame to capture [%d]\n"
		"-h | --help	        Print this message\n"
		"",
		argv[0], capture_dev_name, video_dev_name, count);
}

static const char short_options[] = "dvc:h:";

static const struct option
long_options[] =
{
	{ "device", required_argument, NULL, 'd' },
	{ "videoe", required_argument, NULL, 'v' },
	{ "count", required_argument,  NULL, 'c' },
	{ "help",   no_argument,       NULL, 'h' },
	{ 0, 0, 0, 0 }
};

static void open_video_device(void)
{
	fd_video = open(video_dev_name, O_RDWR);

	if (-1 == fd_video ) 	{
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			video_dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void close_video_device(void)
{
	
	if (-1 == close(fd_video))
		errno_exit("close");

	fd_video = -1;
}

static void init_video_device(void)
{
	unsigned int i,j;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	struct v4l2_buffer buf;

	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	xioctl(fd_video, VIDIOC_G_FMT, &fmt);

	fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	fmt.fmt.pix.width	= 640;
	fmt.fmt.pix.height	= 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;

	if (-1 == xioctl(fd_video, VIDIOC_S_FMT, &fmt))
	{
		if (EINVAL == errno) 
		{
			fprintf(stderr, "%s is no V4L2 device\n",
				capture_dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			errno_exit("VIDEO_OUTPUT: VIDIOC_S_FMT");
		}
	}

	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	xioctl(fd_video, VIDIOC_G_FMT, &fmt);

	printf("v4l2_overlay_get_position:: w=%d h=%d\n", fmt.fmt.win.w.width, fmt.fmt.win.w.height);
	
	fmt.type = V4L2_BUF_TYPE_VIDEO_OVERLAY;
	fmt.fmt.win.w.left = 80;
	fmt.fmt.win.w.top = 0;
	fmt.fmt.win.w.width = 640;
	fmt.fmt.win.w.height = 480;

	if (-1 == xioctl(fd_video, VIDIOC_S_FMT, &fmt))
	{
		if (EINVAL == errno) 
		{
			fprintf(stderr, "%s is no V4L2 device\n",
				video_dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			errno_exit("VIDEO_OVERLAY: VIDIOC_S_FMT");
		}
	}

	CLEAR(req);
	req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	req.memory = V4L2_MEMORY_MMAP;
	req.count = VIDEO_BUF_NBR;
	if (-1 == xioctl(fd_video, VIDIOC_REQBUFS, &req)) {
		errno_exit("VIDEO_OUTPUT: VIDIOC_REQBUFS");
	}
	printf("v4l2_overlay_request_buffer, result: requested=%u\n", req.count);

	video_buffers = calloc(req.count, sizeof(*buffers));

	for (i = 0 ; i < VIDEO_BUF_NBR ; i++) {
		CLEAR(buf);
		buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index  = i;		
		if (-1 == xioctl(fd_video, VIDIOC_QUERYBUF, &buf)) {
			errno_exit("VIDEO_OUPUT: VIDIOC_QUERYBUF");
		}
		video_buffers[i].length = buf.length;
		video_buffers[i].start = mmap(NULL /* start anywhere */,
					     buf.length,
					     PROT_READ | PROT_WRITE /* required */,
					     MAP_SHARED /* recommended */,
					     fd_video, buf.m.offset);

		/* Temporary fill the buffer with nothing */
		memset(video_buffers[i].start, '\0', video_buffers[i].length);

		if (-1 == xioctl(fd_video, VIDIOC_QBUF, &buf)) {
			if (EINVAL == errno) 
			{
				fprintf(stderr, "%s is no V4L2 device\n",
				video_dev_name);
				exit(EXIT_FAILURE);
			} else {
				errno_exit("VIDEO_OUPUT: VIDIOC_QBUF");
			}
		}
	}
}

static void uninit_video_device(void)
{
	unsigned int i;

	for (i = 0; i < VIDEO_BUF_NBR; ++i) {
		if (-1 == munmap(video_buffers[i].start, video_buffers[i].length)) {
			errno_exit("munmap");
		}
	}
}

static void start_video_overlay(void)
{
	unsigned long type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	
	if (-1 == xioctl(fd_video, VIDIOC_STREAMON, &type)) {
		errno_exit("VIDEO_OUPUT: VIDIOC_STREAMON");
	}
}

static void stop_video_overlay(void)
{
	unsigned long type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	
	if (-1 == xioctl(fd_video, VIDIOC_STREAMOFF, &type)) {
		errno_exit("VIDEO_OUPUT: VIDIOC_STREAMOFF");
	}
}

static void open_capture_device(void)
{
	fd_capture = open(capture_dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

	if (-1 == fd_capture ) 	{
		fprintf(stderr, "Cannot open '%s': %d, %s\n",
			capture_dev_name, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

static void close_capture_device(void)
{
	
	if (-1 == close(fd_capture))
		errno_exit("close");

	fd_capture = -1;
}

static void init_capture_device(void)
{
	struct v4l2_capability cap;
	struct v4l2_cropcap cropcap;
	struct v4l2_crop crop;
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	unsigned int min;

	if (-1 == xioctl(fd_capture, VIDIOC_QUERYCAP, &cap))
	{
		if (EINVAL == errno)
		{
			fprintf(stderr, "%s is no V4L2 device\n",
				capture_dev_name);
			exit(EXIT_FAILURE);
		}
		else
		{
			errno_exit("VIDIOC_QUERYCAP");
		}
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
	{
		fprintf(stderr, "%s is no video capture device\n",
			capture_dev_name);
		exit(EXIT_FAILURE);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "%s does not support streaming i/o\n", capture_dev_name);
		exit(EXIT_FAILURE);
	}

	CLEAR(fmt);
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width	= 640;
	fmt.fmt.pix.height	= 480;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field	= V4L2_FIELD_INTERLACED;
	
	if (-1 == xioctl(fd_capture, VIDIOC_S_FMT, &fmt))
		errno_exit("VIDIOC_S_FMT");

	CLEAR(req);
	req.count  = CAPTURE_BUF_NBR;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd_capture, VIDIOC_REQBUFS, &req)) 	{
		if (EINVAL == errno) {
			fprintf(stderr, "%s does not support " 
					"memory mapping\n", capture_dev_name);
			exit(EXIT_FAILURE);
		} else {
			errno_exit("VIDIOC_REQBUFS");
		}
	}

	if (req.count < 2) {
		fprintf(stderr, "Insufficient buffer memory on %s\n",
			capture_dev_name);
		exit(EXIT_FAILURE);
	}

	buffers = calloc(req.count, sizeof(*buffers));

	for (n_buffers = 0; n_buffers < req.count; ++n_buffers)
	{
		struct v4l2_buffer buf;

		CLEAR(buf);

		buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory	= V4L2_MEMORY_MMAP;
		buf.index	= n_buffers;

		if (-1 == xioctl(fd_capture, VIDIOC_QUERYBUF, &buf))
			errno_exit("VIDIOC_QUERYBUF");

		buffers[n_buffers].length = buf.length;
		buffers[n_buffers].start =
		mmap(NULL /* start anywhere */,
		     buf.length,
		     PROT_READ | PROT_WRITE /* required */,
		     MAP_SHARED /* recommended */,
		     fd_capture, buf.m.offset);

		if (MAP_FAILED == buffers[n_buffers].start)
			errno_exit("mmap");
	}	
}

static void uninit_capture_device(void)
{
	unsigned int i;

	for (i = 0; i < n_buffers; ++i) {
		if (-1 == munmap(buffers[i].start, buffers[i].length)) {
			errno_exit("munmap");
		}
	}
}

static void start_capturing(void)
{
	unsigned int i;
	enum v4l2_buf_type type;

	for (i = 0; i < n_buffers; ++i) {
		struct v4l2_buffer buf;

		CLEAR(buf);
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (-1 == xioctl(fd_capture, VIDIOC_QBUF, &buf)) 
			errno_exit("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		if (-1 == xioctl(fd_capture, VIDIOC_STREAMON, &type))
			errno_exit("VIDIOC_STREAMON");
}

static void stop_capturing(void)

{
	enum v4l2_buf_type type;

	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == xioctl(fd_capture, VIDIOC_STREAMOFF, &type))
	errno_exit("VIDIOC_STREAMOFF");
}

static unsigned char color = 0;
static void process_image(const void *p, int size)
{
	struct v4l2_buffer buf;
	
	CLEAR(buf);
	buf.type   = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	buf.memory = V4L2_MEMORY_MMAP;
		
	if (-1 == xioctl(fd_video, VIDIOC_DQBUF, &buf)) {
		errno_exit("VIDEO_OUPUT: VIDIOC_DQBUF");
	}
	
	memcpy((void*) video_buffers[buf.index].start, p, size);
	
	if (-1 == xioctl(fd_video, VIDIOC_QBUF, &buf)) {
		errno_exit("VIDEO_OUPUT: VIDIOC_QBUF");
	}
}

static int read_frame(void)
{
	struct v4l2_buffer buf;
	CLEAR(buf);
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	if (-1 == xioctl(fd_capture, VIDIOC_DQBUF, &buf)) {
		switch (errno) {
			case EAGAIN:
				return 0;
				
			case EIO:
			default:
				errno_exit("VIDIOC_DQBUF");
			}
	}

	assert(buf.index < n_buffers);

	process_image(buffers[buf.index].start, buf.bytesused);

	if (-1 == xioctl(fd_capture, VIDIOC_QBUF, &buf))
		errno_exit("VIDIOC_QBUF");

	return 1;
}

static void mainloop(void)
{
	if(count) {
		while (count) {
			if(read_frame() == 1) count--;
			usleep(30000);
		}
	} else {
		while(1) {
			read_frame();
			usleep(30000);
		}
	}	
}

void handle_exit(int signal)
{
	stop_video_overlay();
	stop_capturing();
	uninit_capture_device();
	uninit_video_device();
	close_capture_device();
	close_video_device();
	
	exit(0);
}


int main(int argc, char **argv)
{
	capture_dev_name = "/dev/video1";
	video_dev_name   = "/dev/video0";
	count = 1000;
	
	for (;;) {
		int idx;
		int c;

		c = getopt_long(argc, argv, short_options, long_options, &idx);

	        if (-1 == c)
			break;

		switch (c) {
			case 0: /* getopt_long() flag */
				break;

			case 'd':
				capture_dev_name = optarg;
				break;

			case 'v':
				video_dev_name = optarg;
				break;

			case 'c':
				count = atoi(optarg);
				break;


			default:
				usage(stderr, argc, argv);
				exit(EXIT_FAILURE);
			}
	}

	struct sigaction myhandle; 
	myhandle.sa_handler = handle_exit;
	sigemptyset(&myhandle.sa_mask);
	myhandle.sa_flags = 0;
	sigaction(SIGINT, &myhandle, NULL);

	open_capture_device();
	open_video_device();
	init_capture_device();
	init_video_device();
	start_video_overlay();
	start_capturing();

	mainloop();

	handle_exit(0);
}



