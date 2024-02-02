// .pro: add library opencv

#ifndef VIDEO_LABEL_HPP
#define VIDEO_LABEL_HPP

#include <QLabel>
#include <QThread>
#include <QTimer>

#include <Windows.h>
#include <atomic>

#include "opencv2/opencv.hpp"


class VideoLabel;


class VideoLabelThread : public QThread {
    Q_OBJECT

public:
    VideoLabelThread(VideoLabel *video_label, unsigned int sleep_time = 1000, QObject *parent = nullptr);

    ~VideoLabelThread();

public:
    void setIntervalTime(unsigned int interval_time) {
        m_interval_time = interval_time;
    }

protected:
    void run() override;

signals:
    void signalGetImage();

    void signalPaintImage();

private:
    VideoLabel *m_video_label;

    unsigned int m_interval_time;  // ms

    std::atomic_bool m_get_finish_flag;
    std::atomic_bool m_thread_end_flag;
};


class VideoLabel : public QLabel {
    Q_OBJECT

public:
    explicit VideoLabel(QWidget *parent = nullptr)
            : QLabel(parent),
              m_thread(new VideoLabelThread(this)) {
        connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
        m_thread->start();
    }

    ~VideoLabel() {
        delete m_thread;
    }

public:
    void setIntervalTime(unsigned int interval_time) {
        m_thread->setIntervalTime(interval_time);
    }

    bool readImage(const QString &image_path, int flags = cv::IMREAD_COLOR) {
        FILE* f = nullptr;
        errno_t err = _wfopen_s(&f, string2wstring(image_path.toStdString()).c_str(), L"rb");

        if (err != 0) {
            std::cout << "readImage() error: Image file failed to open, error code is " << err << std::endl;
            return false;
        }

        fseek(f, 0, SEEK_END);
        size_t buffer_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<char> buffer(buffer_size);
        fread(&buffer[0], sizeof(char), buffer_size, f);

        fclose(f);

        cv::Mat image = cv::imdecode(buffer, flags);

        image.copyTo(m_image);

        return true;
    }

    // 获取 图像数据 到 m_image，主线程执行
    virtual void getImage();

    virtual void getImageDecorator() {
        getImage();
        emit signalGetImageFinish();
    }

    // 对 获取到的 m_image 进行进一步处理，子线程执行
    virtual void processImage();

    void paintImage() {
        if (!m_image.empty()) {
            // 将图像缩放到控件的尺寸
            cv::Mat scaled_image;
            cv::resize(m_image, scaled_image, cv::Size(this->width(), this->height()));

            // 将 OpenCV 的 Mat 转换为 Qt 的 QImage
            QImage qimage;
            if (scaled_image.channels() == 3) {
                qimage = QImage(scaled_image.data, (int) scaled_image.cols, (int) scaled_image.rows,
                                (int) scaled_image.step, QImage::Format_BGR888);
            } else if (scaled_image.channels() == 1) {
                qimage = QImage(scaled_image.data, (int) scaled_image.cols, (int) scaled_image.rows,
                                (int) scaled_image.step, QImage::Format_Grayscale8);
            } else {  // Error
                return;
            }

            this->setPixmap(QPixmap::fromImage(qimage));
        }
    }

protected:
    std::wstring string2wstring(const std::string &str) {
        int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
        std::wstring wstr(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, const_cast<wchar_t *>(wstr.data()), len);
        wstr.resize(wcslen(wstr.c_str()));
        return wstr;
    }

signals:
    void signalGetImageFinish();

    void signalGetImage(cv::Mat &image);

protected:
    VideoLabelThread *m_thread;

    cv::Mat m_image;
};


class VideoLabelTriggered : public VideoLabel {
public:
    void setImageInfo(std::string image_path, int image_flags = cv::IMREAD_COLOR) {
        m_image_path = image_path;
        m_image_flags = image_flags;
    }

    virtual void getImage() override;

    void getImageDecorator() override {
        getImage();
        loadTheImage();
    }

    virtual void processImage() override;

protected:
    bool readTheImage() {
        return readImage(QString::fromStdString(m_image_path), m_image_flags);
    }

    void loadTheImage() {
        QTimer::singleShot(m_load_image_time, this, [=]() {
            bool status = readTheImage();
            if (!status) {
                loadTheImage();
            } else {
                emit signalGetImageFinish();
            }
        });
    }

protected:
    std::string m_image_path;
    int m_image_flags;

    const int m_load_image_time = 5;
};


#endif // VIDEO_LABEL_HPP
