/*
 * mainwindow.cpp
 *
 * Author: Johan Hovold <jhovold@gmail.com>
 */

#include <QtCore/QDebug>

#include <QtGui/QLabel>
#include <QtGui/QStyle>
#include <QtGui/QHBoxLayout>
#include <QtGui/QVBoxLayout>

#include "common.h"
#include "mainwindow.h"

#define ICON_SIZE	60

MainWindow::MainWindow(VideoWorker *worker, QSize &videoSize, QWidget *parent) :
        QMainWindow(parent),
	m_worker(worker)
{
	static const char *bg_style = "background-color:#0085c1;";
	static const char *bg_transparent = "background: transparent;" \
					    "border = none;";

	connect(m_worker, SIGNAL(started()), this, SLOT(videoStarted()));
	connect(m_worker, SIGNAL(paused()), this, SLOT(videoPaused()));

	m_playpause = new QToolButton;
	m_playpause->setIcon(QIcon(":/images/play.png"));
	m_playpause->setIconSize(QSize(ICON_SIZE, ICON_SIZE));
	m_playpause->setStyleSheet(bg_transparent);
	m_playpause->setFocusPolicy(Qt::NoFocus);
	connect(m_playpause, SIGNAL(clicked()), this, SLOT(onPlayPause()));

	QHBoxLayout *bl = new QHBoxLayout;
	bl->setAlignment(Qt::AlignBottom | Qt::AlignHCenter);
	bl->setContentsMargins(0, 0, 0, videoSize.height() / 20);
	bl->addWidget(m_playpause);

	QWidget *overlay = new QWidget;
	overlay->setLayout(bl);
	overlay->setFixedSize(videoSize);

	QWidget *top = new QWidget;
	QWidget *bottom = new QWidget;
	QWidget *left = new QWidget;
	QLabel *right = new QLabel;
	right->setMaximumHeight(videoSize.height());
	right->setAlignment(Qt::AlignCenter);
	right->setPixmap(QPixmap(":/images/logo-atmel-small.png"));

	top->setStyleSheet(bg_style);
	bottom->setStyleSheet(bg_style);
	left->setStyleSheet(bg_style);
	right->setStyleSheet(bg_style);

	QHBoxLayout *ml = new QHBoxLayout;
	ml->setSpacing(0);
	ml->setContentsMargins(0, 0, 0, 0);
	ml->addWidget(left);
	ml->addWidget(overlay);
	ml->addWidget(right);

	QWidget *middle = new QWidget;
	middle->setLayout(ml);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(top);
	layout->addWidget(middle);
	layout->addWidget(bottom);

	QWidget *base = new QWidget;
	base->setLayout(layout);

	setCentralWidget(base);
}

MainWindow::~MainWindow()
{
}

void MainWindow::onPlayPause()
{
	qDebug("%s", __func__);

	m_worker->pause();
}

void MainWindow::videoStarted()
{
	qDebug("%s", __func__);

	m_playpause->setIcon(QIcon(":/images/pause.png"));
}

void MainWindow::videoPaused()
{
	qDebug("%s", __func__);

	m_playpause->setIcon(QIcon(":/images/play.png"));
}
