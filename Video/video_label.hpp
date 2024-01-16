#ifndef VIDEO_LABEL_HPP
#define VIDEO_LABEL_HPP

#include <QLabel>
#include <QThread>

#include "opencv2/opencv.hpp"


class VideoLabel;


class VideoLabelThread : public QThread {
public:
    VideoLabelThread(VideoLabel *video_label, unsigned int sleep_time = 40, QObject *parent = nullptr)
        : QThread(parent),
          m_video_label(video_label),
        m_interval_time(sleep_time) {}

    ~VideoLabelThread() = default;

public:
    void setIntervalTime(unsigned int interval_time) {
        m_interval_time = interval_time;
    }

protected:
    void run() override;

private:
    VideoLabel *m_video_label;

    unsigned int m_interval_time;  // ms
};


class VideoLabel : public QLabel {
    Q_OBJECT

public:
    explicit VideoLabel(QWidget *parent = nullptr)
        : QLabel(parent),
          m_thread(new VideoLabelThread(this)) {}

    ~VideoLabel() {
        delete m_thread;
    }

public:
    void setIntervalTime(unsigned int interval_time) {
        m_thread->setIntervalTime(interval_time);
    }

    // 更新图像，对图像进行相关处理
    virtual void updateImage();

protected:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        if (!m_image.empty()) {
            // 将图像缩放到控件的尺寸
            cv::Mat scaled_image;
            cv::resize(m_image, scaled_image, cv::Size(this->width(), this->height()));

            // 将 OpenCV 的 Mat 转换为 Qt 的 QImage
            QImage qimage;
            if (m_image.channels() == 3) {
                qimage = QImage(scaled_image.data, (int) scaled_image.cols, (int) scaled_image.rows,
                                (int) scaled_image.step, QImage::Format_BGR888);
            } else if (m_image.channels() == 1) {
                qimage = QImage(scaled_image.data, (int) scaled_image.cols, (int) scaled_image.rows,
                                (int) scaled_image.step, QImage::Format_Grayscale8);
            } else {  // Error
                return;
            }

            // 将 QImage 转化为 QPixmap
            QPixmap qpixmap = QPixmap::fromImage(qimage);

            // 显示 QPixmap
            this->setPixmap(qpixmap);
        }
    }

private:
    VideoLabelThread *m_thread;

    cv::Mat m_image;
};


#endif // VIDEO_LABEL_HPP
