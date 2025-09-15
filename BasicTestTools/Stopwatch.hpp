#ifndef STOPWATCH_HPP
#define STOPWATCH_HPP

#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
#include <iomanip>


enum class ReportMode {
    FromStart,      // 计算并报告每个 lap 相对于总起点的耗时
    BetweenLaps     // 计算并报告每个 lap 相对于上一个 lap 的耗时（增量）
};


class Stopwatch {
public:
    /**
     * @brief 构造函数，初始化并开始计时
     * @param name - 计时器名称
     * @param mode - 选择报告模式（FromStart 或 BetweenLaps），默认为 FromStart
     */
    explicit Stopwatch(std::string name = "Default", ReportMode mode = ReportMode::FromStart)
            : m_Name(std::move(name)), m_Mode(mode), m_StartTime(std::chrono::high_resolution_clock::now()) {
        m_Laps.reserve(10);  // 预分配内存以提高效率
    }

    /**
     * @brief 记录一个时间戳节点（lap）
     * @param lapName - 时间戳节点名称
     */
    void lap(const std::string &lapName) {
        m_Laps.emplace_back(lapName, std::chrono::high_resolution_clock::now());
    }

    /**
     * @brief 输出时间戳报告
     */
    void report() const {
        std::cout << "\n--- Stopwatch [" << m_Name << "] Results ---\n";

        if (m_Mode == ReportMode::FromStart) {
            printLapsFromStart();
        } else {  // ReportMode::BetweenLaps
            printLapsBetween();
        }

        std::cout << "-----------------------------------\n" << std::endl;
    }

private:
    // 禁止拷贝构造和赋值，确保计时器的唯一性
    Stopwatch(const Stopwatch &) = delete;
    Stopwatch &operator=(const Stopwatch &) = delete;

    /**
     * @brief 打印每个 lap 相对于总起点的耗时
     */
    void printLapsFromStart() const {
        std::cout << "  Mode: From Start (Total Elapsed Time)\n";
        for (const auto &lap : m_Laps) {
            auto duration = std::chrono::duration<double, std::milli>(lap.second - m_StartTime).count();
            std::cout << std::fixed << std::setprecision(3)
                      << "  - Lap '" << lap.first << "': " << duration << " ms\n";
        }
    }

    /**
     * @brief 打印每个 lap 相对于上一个 lap 的耗时
     */
    void printLapsBetween() const {
        std::cout << "  Mode: Between Laps (Delta Time)\n";
        auto previousTimePoint = m_StartTime;

        for (const auto &lap : m_Laps) {
            auto duration = std::chrono::duration<double, std::milli>(lap.second - previousTimePoint).count();
            std::cout << std::fixed << std::setprecision(3)
                      << "  - Lap '" << lap.first << "': " << duration << " ms\n";
            previousTimePoint = lap.second;
        }
    }

private:
    std::string m_Name;
    ReportMode m_Mode;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTime;
    std::vector<std::pair<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>>> m_Laps;
};


#endif // STOPWATCH_HPP
