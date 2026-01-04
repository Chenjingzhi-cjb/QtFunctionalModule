// QT += opengl

#ifndef VIDEO_WIDGET_HPP
#define VIDEO_WIDGET_HPP

#include <QOpenGLWidget>
#include <QImage>
#include <QPainter>

#include <utility>

#include "camera_controller.h"


class VideoWidget : public QOpenGLWidget {
    Q_OBJECT

public:
    explicit VideoWidget(QWidget *parent = nullptr)
            : QOpenGLWidget(parent) {
        auto &camera = CameraController::getInstance();
        connect(&camera, &CameraController::signalUpdateImage, this, &VideoWidget::slotUpdateImage);
    }

    ~VideoWidget() = default;

public slots:
    void paintEvent(QPaintEvent *event) override {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);

        if (!m_image.isNull()) {
            // 计算图像绘制区域（保持长宽比，居中）并绘制图像
            QRect image_rect = calculateImageRect();
            painter.drawImage(image_rect, m_image);

            // 使用 painter 进行其他绘制操作
            // ...
        }
    }

    void slotUpdateImage(QImage image) {
        m_image = image;

        update();
    }

private:
    QRect calculateImageRect() const {
        if (m_image.isNull()) {
            return QRect();
        }

        QSize widget_size = this->size();
        QSize image_size = m_image.size();

        // 计算缩放比例（保持长宽比）
        float widget_aspect = (float) widget_size.width() / widget_size.height();
        float image_aspect = (float) image_size.width() / image_size.height();

        int draw_width, draw_height;

        if (image_aspect > widget_aspect) {
            // 图像更宽，以宽度为准
            draw_width = widget_size.width();
            draw_height = (int)(draw_width / image_aspect);
        } else {
            // 图像更高，以高度为准
            draw_height = widget_size.height();
            draw_width = (int)(draw_height * image_aspect);
        }

        // 计算居中位置
        int x = (widget_size.width() - draw_width) / 2;
        int y = (widget_size.height() - draw_height) / 2;

        return QRect(x, y, draw_width, draw_height);
    }

private:
    QImage m_image;
};


#endif // VIDEO_WIDGET_HPP
