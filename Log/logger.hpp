#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <mutex>

#define Log_INFO_LAMBDA(msg, ...)     spdlog::info(msg, ##__VA_ARGS__)

#define Log_INFO(msg, ...)     spdlog::info("[{}] " msg, __FUNCTION__, ##__VA_ARGS__)
#define Log_DEBUG(msg, ...)    spdlog::debug("[{}:{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define Log_WARN(msg, ...)     spdlog::warn("[{}:{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define Log_ERROR(msg, ...)    spdlog::error("[{}:{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define Log_INFO_M(module, msg, ...)     spdlog::info("[{}] [{}] " msg, __FUNCTION__, module, ##__VA_ARGS__)
#define Log_DEBUG_M(module, msg, ...)    spdlog::debug("[{}:{}] [{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, module, ##__VA_ARGS__)
#define Log_WARN_M(module, msg, ...)     spdlog::warn("[{}:{}] [{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, module, ##__VA_ARGS__)
#define Log_ERROR_M(module, msg, ...)    spdlog::error("[{}:{}] [{}] [{}] " msg, __FILE__, __LINE__, __FUNCTION__, module, ##__VA_ARGS__)


class Logger {
public:
    static Logger &instance() {
        static Logger inst;
        return inst;
    }

    void init(spdlog::level::level_enum level = spdlog::level::debug) {
        std::call_once(m_once_flag, [&]() {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

            std::vector<spdlog::sink_ptr> sinks{console_sink};

            auto logger = std::make_shared<spdlog::logger>("logger", sinks.begin(), sinks.end());
            logger->set_level(level);
            logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] [thread %t] %v");
            spdlog::set_default_logger(logger);

            spdlog::info("Logger initialized (level: {})", spdlog::level::to_string_view(level));
        });
    }

private:
    Logger() = default;

    std::once_flag m_once_flag;
};
