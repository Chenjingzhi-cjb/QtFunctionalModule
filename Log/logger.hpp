#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <memory>
#include <mutex>

#ifdef ENABLE_QT_LOGGING
#include "qt_sink.hpp"
#endif

#define Log_INFO(msg, ...)     spdlog::info(msg, ##__VA_ARGS__)
#define Log_DEBUG(msg, ...)    spdlog::debug("[{}:{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define Log_WARN(msg, ...)     spdlog::warn(msg, ##__VA_ARGS__)
#define Log_ERROR(msg, ...)    spdlog::error(msg, ##__VA_ARGS__)

#define Log_INFO_M(module, msg, ...)     spdlog::info("[{}] " msg, module, ##__VA_ARGS__)
#define Log_DEBUG_M(module, msg, ...)    spdlog::debug("[{}:{}] [{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, module, ##__VA_ARGS__)
#define Log_WARN_M(module, msg, ...)     spdlog::warn("[{}] " msg, module, ##__VA_ARGS__)
#define Log_ERROR_M(module, msg, ...)    spdlog::error("[{}] " msg, module, ##__VA_ARGS__)

#define THROW_ARG_ERROR(msg, ...) \
    do { \
        std::string _formatted_msg = fmt::format("[{}:{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__); \
        spdlog::error("{}", _formatted_msg); \
        throw std::invalid_argument(_formatted_msg); \
    } while (0)

#define ThisLoggerPattern "%Y-%m-%d %H:%M:%S.%e [%^%l%$] [thread %t] %v"


class Logger {
public:
    static Logger &instance() {
        static Logger inst;
        return inst;
    }

    void init(spdlog::level::level_enum level = spdlog::level::debug) {
        std::call_once(m_once_flag, [&]() {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern(ThisLoggerPattern);

            std::vector<spdlog::sink_ptr> sinks{console_sink};
            auto logger = std::make_shared<spdlog::logger>("logger", sinks.begin(), sinks.end());
            logger->set_level(level);

            logger->flush_on(level);

            spdlog::set_default_logger(logger);
            spdlog::info("Logger initialized (level: {})", spdlog::level::to_string_view(level));
        });
    }

    void removeConsoleSink() {
        try {
            auto logger = spdlog::default_logger();
            auto &sinks = logger->sinks();

            for (auto it = sinks.begin(); it != sinks.end(); ) {
                if (dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(it->get())) {
                    it = sinks.erase(it);
                } else {
                    ++it;
                }
            }
        } catch (const spdlog::spdlog_ex &ex) {
            spdlog::error("Console sink removed failed: {}", ex.what());
        }
    }

    void addFileSink(const std::string &file_path, bool truncate = true) {
        try {
            auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(file_path, truncate);
            file_sink->set_pattern(ThisLoggerPattern);

            spdlog::default_logger()->sinks().push_back(file_sink);
            spdlog::info("File sink added: {}", file_path);
        } catch (const spdlog::spdlog_ex &ex) {
            spdlog::error("File sink added failed: {}", ex.what());
        }
    }

    void addRotatingFileSink(const std::string &file_path,
                             size_t max_file_size = 1024 * 1024 * 10,
                             size_t max_files = 3) {
        try {
            auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                file_path, max_file_size, max_files);
            rotating_sink->set_pattern(ThisLoggerPattern);

            spdlog::default_logger()->sinks().push_back(rotating_sink);
            spdlog::info("Rotating file sink added: {} (max size: {} bytes, max files: {})",
                         file_path, max_file_size, max_files);
        } catch (const spdlog::spdlog_ex &ex) {
            spdlog::error("Rotating file sink add failed: {}", ex.what());
        }
    }

    void addDailyFileSink(const std::string &file_path, int rotation_hour = 0, int rotation_minute = 0) {
        try {
            auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
                file_path, rotation_hour, rotation_minute);
            daily_sink->set_pattern(ThisLoggerPattern);

            spdlog::default_logger()->sinks().push_back(daily_sink);
            spdlog::info("Daily file sink added: {}", file_path);
        } catch (const spdlog::spdlog_ex &ex) {
            spdlog::error("Daily file sink add failed: {}", ex.what());
        }
    }

#ifdef ENABLE_QT_LOGGING

    void addQtSink(QTextBrowser *text_browser) {
        if (!text_browser) return;

        auto qt_sink = std::make_shared<qt_text_browser_sink_mt>(text_browser);
        qt_sink->set_pattern(ThisLoggerPattern);

        spdlog::default_logger()->sinks().push_back(qt_sink);
        spdlog::info("Qt TextBrowser sink added");
    }

#endif

private:
    Logger() = default;

    std::once_flag m_once_flag;
};
