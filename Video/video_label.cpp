#include "video_label.hpp"


void VideoLabelThread::run() {
    // 延时
    QThread::msleep(m_interval_time);

    // 更新图像
    m_video_label->updateImage();

    // 刷新控件
    m_video_label->update();
}

void VideoLabel::updateImage() {

}
