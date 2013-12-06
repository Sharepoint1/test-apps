/*
 * main.cpp -- Atmel V4L Capture and Overlay Demo
 *
 * Author: Johan Hovold <jhovold@gmail.com>
 */

#include <QtCore/QThread>
#include <QtGui/QApplication>
#include <QtGui/QWSServer>

#include <fcntl.h>
#include <linux/fb.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "common.h"
#include "mainwindow.h"
#include "videoworker.h"


#define FB_DEV_OVERLAY	"/dev/fb1"
#define V4L_DEV_CAPTURE	"/dev/video1"
#define V4L_DEV_OUTPUT	"/dev/video0"


static int fb_setup(const char *dev, size_t width, size_t height)
{
	struct fb_var_screeninfo vinfo;
	int fd;
	int ret;

	fd = open(dev, O_RDWR);
	if (fd < 0)
		die_errno("failed to open framebuffer device");

	ret = ioctl(fd, FBIOGET_VSCREENINFO, &vinfo);
	if (ret == -1)
		die_errno("FBIOGET_VSCREENINFO");

	vinfo.xres = width;
	vinfo.yres = height;
	vinfo.xres_virtual = width;
	vinfo.yres_virtual = height;
	vinfo.bits_per_pixel = 32;
	vinfo.activate = FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;
	vinfo.nonstd = (1<< 31) | (0 << 10) | 0;	/* offset 0,0 */
	if (!width)
		vinfo.nonstd = 0;

	ret = ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo);
	if (ret == -1)
		die_errno("FBIOPUT_VSCREENINFO");

	close(fd);

	return ret;
}

static void signalhandler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM)
		qApp->quit();
}

int main(int argc, char *argv[])
{
	QSize videoSize;
	int ret;

	fb_setup(FB_DEV_OVERLAY, SCREEN_WIDTH, SCREEN_HEIGHT);

	QApplication app(argc, argv);
	app.setApplicationName("atmel-demo");

	if (app.arguments().count() > 1)
		videoSize = QSize(320, 240);
	else
		videoSize = QSize(640, 480);

	QWSServer *server = QWSServer::instance();
	if(server)
		server->setCursorVisible(false);

	VideoWorker *worker = new VideoWorker(V4L_DEV_CAPTURE,
							V4L_DEV_OUTPUT,
							videoSize);
	MainWindow window(worker, videoSize);
	window.setAttribute(Qt::WA_OpaquePaintEvent);
	window.setAttribute(Qt::WA_NoSystemBackground);
	window.setWindowFlags(Qt::FramelessWindowHint);
	window.setFixedSize(SCREEN_WIDTH, SCREEN_HEIGHT);
	window.show();

	QThread *thread = new QThread();
	worker->moveToThread(thread);
	QObject::connect(thread, SIGNAL(started()), worker, SLOT(run()));

	signal(SIGINT, signalhandler);
	signal(SIGTERM, signalhandler);

	thread->start();

	ret = app.exec();

	worker->stop();
	thread->wait();

	fb_setup(FB_DEV_OVERLAY, 0, 0);

	return ret;
}
