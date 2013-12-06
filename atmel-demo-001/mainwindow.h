#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtGui/QMainWindow>
#include <QtGui/QToolButton>

#include "videoworker.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(VideoWorker *worker, QSize &videoSize,
				QWidget *parent = 0);
	~MainWindow();

private slots:
	void onPlayPause();

	void videoStarted();
	void videoPaused();

private:
	VideoWorker *m_worker;
	QToolButton *m_playpause;
};

#endif	/* MAIN_WINDOW_H */
