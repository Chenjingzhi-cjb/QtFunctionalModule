#include "video_label.hpp"

#include <random>


VideoLabelThread::VideoLabelThread(VideoLabel *video_label, unsigned int sleep_time, QObject *parent)
        : QThread(parent),
          m_video_label(video_label),
          m_interval_time(sleep_time),
          m_get_finish_flag(false),
          m_thread_end_flag(false) {
    connect(this, &VideoLabelThread::signalGetImage, m_video_label, &VideoLabel::getImageDecorator, Qt::QueuedConnection);
    connect(this, &VideoLabelThread::signalPaintImage, m_video_label, &VideoLabel::paintImage, Qt::QueuedConnection);
    connect(m_video_label, &VideoLabel::signalGetImageFinish, this, [=](){ m_get_finish_flag.store(true); });
}

VideoLabelThread::~VideoLabelThread() {
    while (!m_thread_end_flag.load()) {
        m_get_finish_flag.store(true);
        m_thread_end_flag.store(true);
    }

    quit();
    wait();
}

void VideoLabelThread::run() {
    while (!m_thread_end_flag.load()) {
        // 延时
        QThread::msleep(m_interval_time);

        // 获取图像
        emit signalGetImage();
        while (!m_get_finish_flag.load()) {
            QThread::msleep(1);
        }
        m_get_finish_flag.store(false);

        // 处理图像
        m_video_label->processImage();

        // 刷新控件
        emit signalPaintImage();
    }
}

void VideoLabel::getImage() {
    // 调用 api 获取更新图像
    // m_image = externalGetImageApi();
}

void VideoLabel::processImage() {
}

void VideoLabelTriggered::getImage() {
    // 调用 api 触发更新图像文件，再读取图像文件
    // externalUpdateImageApi();
}

void VideoLabelTriggered::processImage() {
}
