#ifndef STAGE_CONTROLLER_HPP
#define STAGE_CONTROLLER_HPP

#include <QtConcurrent>
#include <QThread>

#include <mutex>

#include "LTDMC.h"
#include "LTSMC.h"
#include "logger.hpp"

// #define NO_STAGE

// 1 - DMC; 2 - SMC
#define DRIVER_TYPE 2


class IStageDriver {
public:
    IStageDriver()
        : m_card_no(0),
          m_stop_flag_xy(true) {}

    virtual ~IStageDriver() {}

    virtual void openController() const = 0;
    virtual void closeController() const = 0;
    virtual long getAxisPosX() const = 0;
    virtual long getAxisPosY() const = 0;
    virtual void setPosZeroXY() const = 0;
    virtual bool isMoveX() const = 0;
    virtual bool isMoveY() const = 0;
    virtual bool isMoveXY() const = 0;
    virtual void stopStageXY() = 0;
    virtual void stageMoveXY(int dis_x, int dis_y, int max_speed) = 0;
    virtual void stageMoveXY(int speed_x, int speed_y) = 0;
    virtual void blockMoveFlagXY(int block_time = 500) = 0;
    virtual void blockMoveFlagXY(bool &exit_flag, int block_time = 500) = 0;
    virtual void blockMoveSimulateLimitStopXY(int block_time = 500) = 0;

public:
    short m_card_init_status = 0;
    short m_card_info_list_status = 0;

protected:
#pragma region "雷赛运动控制卡 运动参数配置" {

    const WORD axis_x = 1;
    const WORD axis_y = 0;

    const WORD stop_mode = 0;  // 停止模式： 0 为减速停止 ； 1 为立刻停止

    const double min_vel = 0;
    const double tacc = 0.5;
    const double tdec = 0.1;
    const double stop_vel = 0;

    const WORD posi_mode = 0;

    const WORD s_mode = 0;
    const double s_para = 0.5;

    const int controller_switch = 8;
    const int enable_x = 9;
    const int enable_y = 10;

#pragma endregion }

    WORD m_card_no;

    bool m_stop_flag_xy;
    std::mutex m_state_mutex_xy;

    WORD m_io_enabled = 1;
    WORD m_io_disabled = 0;
};


class DMCStageDriver : public IStageDriver {
public:
    DMCStageDriver()
            : IStageDriver() {
        initBoard();
    }

    ~DMCStageDriver() {
        closeEnableXY();
    }

public:
    void openController() const override {
        dmc_write_outbit(m_card_no, controller_switch, 0);
    }

    void closeController() const override {
        dmc_write_outbit(m_card_no, controller_switch, 1);
    }

    long getAxisPosX() const override {
        return dmc_get_position(m_card_no, axis_x);
    }

    long getAxisPosY() const override {
        return dmc_get_position(m_card_no, axis_y);
    }

    void setPosZeroXY() const override {
        dmc_set_position_unit(m_card_no, axis_x, 0);
        dmc_set_encoder_unit(m_card_no, axis_x, 0);
        dmc_set_position_unit(m_card_no, axis_y, 0);
        dmc_set_encoder_unit(m_card_no, axis_y, 0);
    }

    bool isMoveX() const override {
        return dmc_check_done(m_card_no, axis_x) == 0;
    }

    bool isMoveY() const override {
        return dmc_check_done(m_card_no, axis_y) == 0;
    }

    bool isMoveXY() const override {
        return dmc_check_done(m_card_no, axis_x) == 0 || dmc_check_done(m_card_no, axis_y) == 0;
    }

    /**
     * @brief 直角坐标 定长 运动
     *
     * @param dis_x x 轴的运动距离
     * @param dis_y y 轴的运动距离
     * @param max_speed x 轴 或 y 轴 的最大速度
     * @return None
     */
    void stageMoveXY(int dis_x, int dis_y, int max_speed) override {
        if ((dis_x == 0 && dis_y == 0) || (max_speed <= 0)) return;

        // 1. 检查当前运动状态
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (!m_stop_flag_xy || isMoveXY()) {
            return;
        }
        m_stop_flag_xy = false;

        QtConcurrent::run(this, &DMCStageDriver::stageMoveXYDistanceTask, dis_x, dis_y, max_speed);
    }

    /**
     * @brief 直角坐标 定速 运动
     *
     * @param speed_x x 轴的运动速度
     * @param speed_y y 轴的运动速度
     * @return None
     */
    void stageMoveXY(int speed_x, int speed_y) override {
        if (speed_x == 0 && speed_y == 0) return;

        // 1. 检查当前运动状态
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (!m_stop_flag_xy || isMoveXY()) {
            return;
        }
        m_stop_flag_xy = false;

        QtConcurrent::run(this, &DMCStageDriver::stageMoveXYSpeedTask, speed_x, speed_y);
    }

    void stopStageXY() override {
        setStopFlagXYTrue();

        dmc_stop(m_card_no, axis_x, stop_mode);
        dmc_stop(m_card_no, axis_y, stop_mode);

        // 延迟关闭 IO (Enable 信号)
        QtConcurrent::run(this, &DMCStageDriver::stopStageXYCloseEnableTask);
    }

    void blockMoveFlagXY(int block_time = 500) override {
        do {
            QThread::msleep(block_time);
        } while (!m_stop_flag_xy);
    }

    void blockMoveFlagXY(bool &exit_flag, int block_time = 500) override {
        do {
            QThread::msleep(block_time);
        } while (!exit_flag && !m_stop_flag_xy);
    }

    void blockMoveSimulateLimitStopXY(int block_time = 500) override {
        do {
            QThread::msleep(block_time);
        } while (isMoveXY());

        setStopFlagXYTrue();

        // 延迟关闭 IO (Enable 信号)
        QtConcurrent::run(this, &DMCStageDriver::stopStageXYCloseEnableTask);
    }

private:
    void initBoard() {
        WORD card_num = 0;
        DWORD card_types[8];
        WORD card_ids[8];

        m_card_init_status = dmc_board_init();
        m_card_info_list_status = dmc_get_CardInfList(&card_num, card_types, card_ids);  // 获取控制卡信息
        m_card_no = card_ids[0];

        Log_INFO_M("Stage", "Motion control card initial state: " + std::to_string(m_card_init_status)
                   + ". Info list state: " + std::to_string(m_card_info_list_status) + ".");

        // 配置限位信号
        dmc_set_el_mode(m_card_no, axis_x, 1, 0, 0);  // low: 1 0 0; high: 1 1 0
        dmc_set_el_mode(m_card_no, axis_y, 1, 0, 0);
    }

    void openEnableX() {
        dmc_write_outbit(m_card_no, enable_x, m_io_enabled);
    }

    void closeEnableX() {
        dmc_write_outbit(m_card_no, enable_x, m_io_disabled);
    }

    void openEnableY() {
        dmc_write_outbit(m_card_no, enable_y, m_io_enabled);
    }

    void closeEnableY() {
        dmc_write_outbit(m_card_no, enable_y, m_io_disabled);
    }

    void openEnableXY() {
        dmc_write_outbit(m_card_no, enable_x, m_io_enabled);
        dmc_write_outbit(m_card_no, enable_y, m_io_enabled);
    }

    void closeEnableXY() {
        dmc_write_outbit(m_card_no, enable_x, m_io_disabled);
        dmc_write_outbit(m_card_no, enable_y, m_io_disabled);
    }

    void setStopFlagXYTrue() {
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        m_stop_flag_xy = true;
    }

    void setStopFlagXYFalse() {
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        m_stop_flag_xy = false;
    }

    void blockMoveXY(int block_time = 500) {
        do {
            QThread::msleep(block_time);
        } while (isMoveXY());
    }

    void stageMoveXYDistanceTask(int dis_x, int dis_y, int max_speed) {
        // 2. 处理运动参数 - 运动速度
        int abs_dis_x = abs(dis_x), abs_dis_y = abs(dis_y);
        int speed_x, speed_y;
        if (abs_dis_x > abs_dis_y) {
            speed_x = max_speed;
            speed_y = (int) ((double) max_speed * abs_dis_y / abs_dis_x);
            if (abs_dis_y != 0 && speed_y == 0) speed_y = 1;
        } else if (abs_dis_x < abs_dis_y) {
            speed_x = (int) ((double) max_speed * abs_dis_x / abs_dis_y);
            if (abs_dis_x != 0 && speed_x == 0) speed_x = 1;
            speed_y = max_speed;
        } else {  // abs_dis_x == abs_dis_y
            speed_x = max_speed;
            speed_y = max_speed;
        }

        {
            std::lock_guard<std::mutex> locker(m_state_mutex_xy);
            if (m_stop_flag_xy) return;

            // 3. 开启 IO (Enable 信号)
            if (dis_x != 0) {
                openEnableX();
            }
            if (dis_y != 0) {
                openEnableY();
            }
            QThread::msleep(100);

            // 4. 发送运动指令
            short return_value_x = 0;
            short return_value_y = 0;
            if (dis_x != 0) {
                dmc_set_profile_unit(m_card_no, axis_x, min_vel, speed_x, tacc, tdec, stop_vel);
                dmc_set_s_profile(m_card_no, axis_x, s_mode, s_para);
                return_value_x = dmc_pmove_unit(m_card_no, axis_x, dis_x, posi_mode);
            }
            if (dis_y != 0) {
                dmc_set_profile_unit(m_card_no, axis_y, min_vel, speed_y, tacc, tdec, stop_vel);
                dmc_set_s_profile(m_card_no, axis_y, s_mode, s_para);
                return_value_y = dmc_pmove_unit(m_card_no, axis_y, dis_y, posi_mode);
            }

            Log_INFO_M("Stage", "Rect distance run ( {}, {} ) --> Exec status ( {}, {} ).",
                       dis_x, dis_y, return_value_x, return_value_y);
        }

        // 5. 等待运动完成 - 预睡眠
        int move_times = 0;
        if (dis_x != 0 && abs_dis_x > abs_dis_y) {
            move_times = abs(dis_x) / abs(speed_x);
        } else {  // dis_x == 0 => dis_y != 0
            move_times = abs(dis_y) / abs(speed_y);
        }
        move_times *= 4;  // 判断时长 1/4s
        for (int t = 0; t < move_times; ++t) {
            QThread::msleep(250);
            if (m_stop_flag_xy) return;
        }

        // 6. 等待运动完成 - 实判断
        blockMoveXY();

        {
            std::lock_guard<std::mutex> locker(m_state_mutex_xy);
            if (m_stop_flag_xy) return;

            // 7. 运动完成，关闭 IO (Enable 信号)
            m_stop_flag_xy = true;
            closeEnableXY();
        }
    }

    void stageMoveXYSpeedTask(int speed_x, int speed_y) {
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (m_stop_flag_xy) return;

        // 2. 处理运动参数 - 运动方向: 1 - 正方向; 0 - 负方向
        WORD dir_x = speed_x > 0 ? (WORD) 1 : (WORD) 0;
        WORD dir_y = speed_y > 0 ? (WORD) 1 : (WORD) 0;
        speed_x = abs(speed_x);
        speed_y = abs(speed_y);

        // 3. 开启 IO (Enable 信号)
        if (speed_x != 0) {
            openEnableX();
        }
        if (speed_y != 0) {
            openEnableY();
        }
        QThread::msleep(100);

        // 4. 发送运动指令
        short return_value_x = 0;
        short return_value_y = 0;
        if (speed_x != 0) {
            dmc_set_profile_unit(m_card_no, axis_x, min_vel, speed_x, tacc, tdec, stop_vel);
            dmc_set_s_profile(m_card_no, axis_x, s_mode, s_para);
            return_value_x = dmc_vmove(m_card_no, axis_x, dir_x);
        }
        if (speed_y != 0) {
            dmc_set_profile_unit(m_card_no, axis_y, min_vel, speed_y, tacc, tdec, stop_vel);
            dmc_set_s_profile(m_card_no, axis_y, s_mode, s_para);
            return_value_y = dmc_vmove(m_card_no, axis_y, dir_y);
        }

        Log_INFO_M("Stage", "Rect speed run ( {}, {} ) --> Exec status ( {}, {} ).",
                   speed_x, speed_y, return_value_x, return_value_y);
    }

    void stopStageXYCloseEnableTask() {
        QThread::msleep(1000);
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (m_stop_flag_xy) closeEnableXY();
    }
};


class SMCStageDriver : public IStageDriver {
public:
    SMCStageDriver()
            : IStageDriver() {
        initBoard("192.168.5.11");
    }

    ~SMCStageDriver() {
        closeEnableXY();
    }

public:
    void openController() const override {
        smc_write_outbit(m_card_no, controller_switch, 0);
    }

    void closeController() const override {
        smc_write_outbit(m_card_no, controller_switch, 1);
    }

    long getAxisPosX() const override {
        double pos = 0;
        smc_get_position_unit(m_card_no, axis_x, &pos);

        return pos;
    }

    long getAxisPosY() const override {
        double pos = 0;
        smc_get_position_unit(m_card_no, axis_y, &pos);

        return pos;
    }

    void setPosZeroXY() const override {
        smc_set_position_unit(m_card_no, axis_x, 0);
        smc_set_encoder_unit(m_card_no, axis_x, 0);
        smc_set_position_unit(m_card_no, axis_y, 0);
        smc_set_encoder_unit(m_card_no, axis_y, 0);
    }

    bool isMoveX() const override {
        return smc_check_done(m_card_no, axis_x) == 0;
    }

    bool isMoveY() const override {
        return smc_check_done(m_card_no, axis_y) == 0;
    }

    bool isMoveXY() const override {
        return smc_check_done(m_card_no, axis_x) == 0 || smc_check_done(m_card_no, axis_y) == 0;
    }

    /**
     * @brief 直角坐标 定长 运动
     *
     * @param dis_x x 轴的运动距离
     * @param dis_y y 轴的运动距离
     * @param max_speed x 轴 或 y 轴 的最大速度
     * @return None
     */
    void stageMoveXY(int dis_x, int dis_y, int max_speed) override {
        if ((dis_x == 0 && dis_y == 0) || (max_speed <= 0)) return;

        // 1. 检查当前运动状态
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (!m_stop_flag_xy || isMoveXY()) {
            return;
        }
        m_stop_flag_xy = false;

        QtConcurrent::run(this, &SMCStageDriver::stageMoveXYDistanceTask, dis_x, dis_y, max_speed);
    }

    /**
     * @brief 直角坐标 定速 运动
     *
     * @param speed_x x 轴的运动速度
     * @param speed_y y 轴的运动速度
     * @return None
     */
    void stageMoveXY(int speed_x, int speed_y) override {
        if (speed_x == 0 && speed_y == 0) return;

        // 1. 检查当前运动状态
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (!m_stop_flag_xy || isMoveXY()) {
            return;
        }
        m_stop_flag_xy = false;

        QtConcurrent::run(this, &SMCStageDriver::stageMoveXYSpeedTask, speed_x, speed_y);
    }

    void stopStageXY() override {
        setStopFlagXYTrue();

        smc_stop(m_card_no, axis_x, stop_mode);
        smc_stop(m_card_no, axis_y, stop_mode);

        // 延迟关闭 IO (Enable 信号)
        QtConcurrent::run(this, &SMCStageDriver::stopStageXYCloseEnableTask);
    }

    void blockMoveFlagXY(int block_time = 500) override {
        do {
            QThread::msleep(block_time);
        } while (!m_stop_flag_xy);
    }

    void blockMoveFlagXY(bool &exit_flag, int block_time = 500) override {
        do {
            QThread::msleep(block_time);
        } while (!exit_flag && !m_stop_flag_xy);
    }

    void blockMoveSimulateLimitStopXY(int block_time = 500) override {
        do {
            QThread::msleep(block_time);
        } while (isMoveXY());

        setStopFlagXYTrue();

        // 延迟关闭 IO (Enable 信号)
        QtConcurrent::run(this, &SMCStageDriver::stopStageXYCloseEnableTask);
    }

private:
    void initBoard(const QString &device_ip_str) {
        QByteArray device_ip = device_ip_str.toLocal8Bit();
        m_card_init_status = smc_board_init(m_card_no, 2, device_ip.data(), 0);

        Log_INFO_M("Stage", "Motion control card initial state: " + std::to_string(m_card_init_status) + ".");

        // 配置限位信号
        smc_set_el_mode(m_card_no, axis_x, 1, 0, 0);  // low: 1 0 0; high: 1 1 0
        smc_set_el_mode(m_card_no, axis_y, 1, 0, 0);
    }

    void openEnableX() {
        smc_write_outbit(m_card_no, enable_x, m_io_enabled);
    }

    void closeEnableX() {
        smc_write_outbit(m_card_no, enable_x, m_io_disabled);
    }

    void openEnableY() {
        smc_write_outbit(m_card_no, enable_y, m_io_enabled);
    }

    // 关闭Y轴使能
    void closeEnableY() {
        smc_write_outbit(m_card_no, enable_y, m_io_disabled);
    }

    void openEnableXY() {
        smc_write_outbit(m_card_no, enable_x, m_io_enabled);
        smc_write_outbit(m_card_no, enable_y, m_io_enabled);
    }

    void closeEnableXY() {
        smc_write_outbit(m_card_no, enable_x, m_io_disabled);
        smc_write_outbit(m_card_no, enable_y, m_io_disabled);
    }

    void setStopFlagXYTrue() {
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        m_stop_flag_xy = true;
    }

    void setStopFlagXYFalse() {
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        m_stop_flag_xy = false;
    }

    void blockMoveXY(int block_time = 500) {
        do {
            QThread::msleep(block_time);
        } while (isMoveXY());
    }

    void stageMoveXYDistanceTask(int dis_x, int dis_y, int max_speed) {
        // 2. 处理运动参数 - 运动速度
        int abs_dis_x = abs(dis_x), abs_dis_y = abs(dis_y);
        int speed_x, speed_y;
        if (abs_dis_x > abs_dis_y) {
            speed_x = max_speed;
            speed_y = (int) ((double) max_speed * abs_dis_y / abs_dis_x);
            if (abs_dis_y != 0 && speed_y == 0) speed_y = 1;
        } else if (abs_dis_x < abs_dis_y) {
            speed_x = (int) ((double) max_speed * abs_dis_x / abs_dis_y);
            if (abs_dis_x != 0 && speed_x == 0) speed_x = 1;
            speed_y = max_speed;
        } else {  // abs_dis_x == abs_dis_y
            speed_x = max_speed;
            speed_y = max_speed;
        }

        {
            std::lock_guard<std::mutex> locker(m_state_mutex_xy);
            if (m_stop_flag_xy) return;

            // 3. 开启 IO (Enable 信号)
            if (dis_x != 0) {
                openEnableX();
            }
            if (dis_y != 0) {
                openEnableY();
            }
            QThread::msleep(100);

            // 4. 发送运动指令
            short return_value_x = 0;
            short return_value_y = 0;
            if (dis_x != 0) {
                smc_set_profile_unit(m_card_no, axis_x, min_vel, speed_x, tacc, tdec, stop_vel);
                smc_set_s_profile(m_card_no, axis_x, s_mode, s_para);
                return_value_x = smc_pmove_unit(m_card_no, axis_x, dis_x, posi_mode);
            }
            if (dis_y != 0) {
                smc_set_profile_unit(m_card_no, axis_y, min_vel, speed_y, tacc, tdec, stop_vel);
                smc_set_s_profile(m_card_no, axis_y, s_mode, s_para);
                return_value_y = smc_pmove_unit(m_card_no, axis_y, dis_y, posi_mode);
            }

            Log_INFO_M("Stage", "Rect distance run ( {}, {} ) --> Exec status ( {}, {} ).",
                       dis_x, dis_y, return_value_x, return_value_y);
        }

        // 5. 等待运动完成 - 预睡眠
        int move_times = 0;
        if (dis_x != 0 && abs_dis_x > abs_dis_y) {
            move_times = abs(dis_x) / abs(speed_x);
        } else {  // dis_x == 0 => dis_y != 0
            move_times = abs(dis_y) / abs(speed_y);
        }
        move_times *= 4;  // 判断时长 1/4s
        for (int t = 0; t < move_times; ++t) {
            QThread::msleep(250);
            if (m_stop_flag_xy) return;
        }

        // 6. 等待运动完成 - 实判断
        blockMoveXY();

        {
            std::lock_guard<std::mutex> locker(m_state_mutex_xy);
            if (m_stop_flag_xy) return;

            // 7. 运动完成，关闭 IO (Enable 信号)
            m_stop_flag_xy = true;
            closeEnableXY();
        }
    }

    void stageMoveXYSpeedTask(int speed_x, int speed_y) {
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (m_stop_flag_xy) return;

        // 2. 处理运动参数 - 运动方向: 1 - 正方向; 0 - 负方向
        WORD dir_x = speed_x > 0 ? (WORD) 1 : (WORD) 0;
        WORD dir_y = speed_y > 0 ? (WORD) 1 : (WORD) 0;
        speed_x = abs(speed_x);
        speed_y = abs(speed_y);

        // 3. 开启 IO (Enable 信号)
        if (speed_x != 0) {
            openEnableX();
        }
        if (speed_y != 0) {
            openEnableY();
        }
        QThread::msleep(100);

        // 4. 发送运动指令
        short return_value_x = 0;
        short return_value_y = 0;
        if (speed_x != 0) {
            smc_set_profile_unit(m_card_no, axis_x, min_vel, speed_x, tacc, tdec, stop_vel);
            smc_set_s_profile(m_card_no, axis_x, s_mode, s_para);
            return_value_x = smc_vmove(m_card_no, axis_x, dir_x);
        }
        if (speed_y != 0) {
            smc_set_profile_unit(m_card_no, axis_y, min_vel, speed_y, tacc, tdec, stop_vel);
            smc_set_s_profile(m_card_no, axis_y, s_mode, s_para);
            return_value_y = smc_vmove(m_card_no, axis_y, dir_y);
        }

        Log_INFO_M("Stage", "Rect speed run ( {}, {} ) --> Exec status ( {}, {} ).",
                   speed_x, speed_y, return_value_x, return_value_y);
    }

    void stopStageXYCloseEnableTask() {
        QThread::msleep(1000);
        std::lock_guard<std::mutex> locker(m_state_mutex_xy);
        if (m_stop_flag_xy) closeEnableXY();
    }
};


class StageDriverFactory {
public:
    static std::unique_ptr<IStageDriver> createDriver() {

#if (DRIVER_TYPE == 1)

        return std::make_unique<DMCStageDriver>();

#elif (DRIVER_TYPE == 2)

        return std::make_unique<SMCStageDriver>();

#else
        throw std::runtime_error("Unknown stage driver type.");

#endif

    }
};


class StageController {
public:
    StageController() {
#ifndef NO_STAGE

        m_driver = StageDriverFactory::createDriver();

#endif
    }

    ~StageController() = default;

public:
    void openController() const {
        m_driver->openController();
    }

    void closeController() const {
        m_driver->closeController();
    }

    long getAxisPosX() const {
        return m_driver->getAxisPosX();
    }

    long getAxisPosY() const {
        return m_driver->getAxisPosY();
    }

    void setPosZeroXY() const {
        m_driver->setPosZeroXY();
    }

    bool isMoveX() const {
        return m_driver->isMoveX();
    }

    bool isMoveY() const {
        return m_driver->isMoveY();
    }

    bool isMoveXY() const {
        return m_driver->isMoveXY();
    }

    void stageMoveXY(int dis_x, int dis_y, int max_speed) {
        m_driver->stageMoveXY(dis_x, dis_y, max_speed);
    }

    void stageMoveXY(int speed_x, int speed_y) {
        m_driver->stageMoveXY(speed_x, speed_y);
    }

    void stopStageXY() {
        m_driver->stopStageXY();
    }

    void blockMoveFlagXY(int block_time = 500) {
        m_driver->blockMoveFlagXY(block_time);
    }

    void blockMoveFlagXY(bool &exit_flag, int block_time = 500) {
        m_driver->blockMoveFlagXY(exit_flag, block_time);
    }

    void blockMoveSimulateLimitStopXY(int block_time = 500) {
        m_driver->blockMoveSimulateLimitStopXY(block_time);
    }

private:
    std::unique_ptr<IStageDriver> m_driver;
};


#endif // STAGE_CONTROLLER_HPP
