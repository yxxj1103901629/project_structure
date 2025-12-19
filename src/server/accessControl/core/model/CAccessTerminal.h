#ifndef CACCESSTERMINAL_H
#define CACCESSTERMINAL_H

#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <condition_variable>

/**
 * @brief 门禁终端类
 *
 * 表示一个门禁终端，负责管理终端状态、归属红区信息以及临时关闭功能。
 */
class CAccessTerminal
{
public:
    /**
     * @brief 终端状态枚举
     */
    enum TerminalStatus {
        STATUS_NORMAL = 0,   ///< 正常状态
        STATUS_DISABLED = 1, ///< 临时关闭状态
        STATUS_ABNORMAL = 2  ///< 异常状态
    };

    /**
     * @brief 构造函数
     * @param terminalID 终端唯一标识
     * @note 初始化时，终端默认设置为异常状态
     */
    CAccessTerminal(const std::string& terminalID);
    
    /**
     * @brief 析构函数
     */
    ~CAccessTerminal();

    /**
     * @brief 获取终端唯一标识
     * @return 终端唯一标识
     */
    std::string getTerminalID() const;

    /**
     * @brief 获取终端当前状态
     * @return 终端状态
     */
    TerminalStatus getStatus() const;
    
    /**
     * @brief 设置终端状态
     * @param status 新的终端状态
     */
    void setStatus(TerminalStatus status);

    /**
     * @brief 获取归属红区ID列表
     * @return 归属红区ID列表
     */
    std::vector<std::string> getAreaIds() const;
    
    /**
     * @brief 添加归属红区ID
     * @param areaId 红区ID
     */
    void addAreaId(const std::string& areaId);
    
    /**
     * @brief 移除归属红区ID
     * @param areaId 红区ID
     */
    void removeAreaId(const std::string& areaId);
    
    /**
     * @brief 检查是否归属某红区
     * @param areaId 红区ID
     * @return 归属返回true，否则返回false
     */
    bool isInArea(const std::string& areaId) const;

    /**
     * @brief 设置临时关闭
     * @param duration 临时关闭持续时间（秒）
     * @return 设置成功返回true，否则返回false
     * @note 临时关闭状态下，终端不处理正常的门禁请求
     */
    bool setTemporaryClose(int duration);

    /**
     * @brief 退出临时关闭状态
     * @return 退出成功返回true，否则返回false
     */
    bool exitTemporaryClose();

    /**
     * @brief 检查是否处于临时关闭状态
     * @return 是返回true，否则返回false
     */
    bool isTemporarilyClosed() const;

private:
    /**
     * @brief 临时关闭定时器线程函数
     * @note 定时器到期后，自动退出临时关闭状态
     */
    void timerThreadFunc();

    std::string m_terminalId;             ///< 终端唯一标识
    std::atomic<TerminalStatus> m_status; ///< 终端状态
    std::vector<std::string> m_areaIds;   ///< 归属红区ID列表（支持多红区）
    mutable std::mutex m_areaMutex;       ///< 保护红区ID列表的互斥锁

    int m_disableDuration;     ///< 临时关闭持续时间（秒）
    time_t m_disableStartTime; ///< 临时关闭开始时间（时间戳）

    std::mutex m_timerMutex;              ///< 定时器互斥锁
    std::condition_variable m_timerCond;  ///< 定时器条件变量
    std::atomic<bool> m_timerRunning;     ///< 定时器运行状态
    std::thread m_timerThread;            ///< 定时器线程
};

#endif // CACCESSTERMINAL_H
