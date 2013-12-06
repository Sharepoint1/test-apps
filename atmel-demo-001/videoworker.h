#ifndef VIDEO_WORKER_H
#define VIDEO_WORKER_H

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtCore/QSize>
#include <QtCore/QWaitCondition>


struct video_buffer {
	void *start;
	size_t length;
};

class VideoWorker : public QObject
{
	Q_OBJECT

public:
	VideoWorker(const char *dev_capture, const char *dev_output,
				QSize &videoSize, QObject *parent = 0);
	~VideoWorker();

	void start();
	void pause();
	void stop();

public slots:
	void run();

signals:
	void started();
	void paused();

private:
	void initCapture();
	void initOutput();

	void processFrame(const void *p, size_t size);
	int readFrame();
	void processStream();

	const char *dev_capture;
	const char *dev_output;

	int fd_capture;
	int fd_output;

	unsigned buf_capture_count;
	unsigned buf_output_count;
	struct video_buffer *buf_capture;
	struct video_buffer *buf_output;

	QMutex mutex;
	QWaitCondition stateChanged;
	bool is_paused;
	bool is_stopped;

	QSize videoSize;
};

#endif	/* VIDEO_WORKER_H */
