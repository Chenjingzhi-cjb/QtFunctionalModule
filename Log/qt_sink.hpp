#pragma once

#include <spdlog/sinks/base_sink.h>
#include <QTextBrowser>
#include <QMetaObject>
#include <QScrollBar>
#include <mutex>


// Qt TextBrowser Sink
template<typename Mutex>
class qt_text_browser_sink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit qt_text_browser_sink(QTextBrowser* text_browser)
        : text_browser_(text_browser) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (!text_browser_) return;

        // 格式化日志消息
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        std::string log_text = fmt::to_string(formatted);

        // 根据日志级别设置颜色
        QString color = get_level_color(msg.level);
        QString html = QString("<span style='color:%1'>%2</span>")
                           .arg(color)
                           .arg(QString::fromStdString(log_text).toHtmlEscaped());

        // 线程安全地更新 UI（使用 Qt 的信号槽机制）
        QMetaObject::invokeMethod(text_browser_, [this, html]() {
            text_browser_->append(html);
            text_browser_->verticalScrollBar()->setValue(
                text_browser_->verticalScrollBar()->maximum());
        }, Qt::QueuedConnection);
    }

    void flush_() override {
        // QTextBrowser 不需要显式 flush
    }

private:
    QString get_level_color(spdlog::level::level_enum level) {
        switch (level) {
        case spdlog::level::trace:    return "#808080"; // 灰色
        case spdlog::level::debug:    return "#00BFFF"; // 深天蓝
        case spdlog::level::info:     return "#000000"; // 黑色
        case spdlog::level::warn:     return "#FFA500"; // 橙色
        case spdlog::level::err:      return "#FF0000"; // 红色
        case spdlog::level::critical: return "#FF00FF"; // 品红
        default:                      return "#FFFFFF"; // 白色
        }
    }

    QTextBrowser* text_browser_;
};

using qt_text_browser_sink_mt = qt_text_browser_sink<std::mutex>;
using qt_text_browser_sink_st = qt_text_browser_sink<spdlog::details::null_mutex>;
