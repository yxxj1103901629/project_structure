#ifndef CACCESSAREA_H
#define CACCESSAREA_H

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

/**
 * @brief 门禁区域类
 *
 * 表示一个门禁区域，包含多个门禁终端，负责管理区域状态和关联的终端信息。
 */
class CAccessArea
{
public:
    /**
    * @brief 区域状态枚举
    */
    enum AreaStatus {
        AREA_NORMAL = 0, ///< 区域正常状态
        AREA_ALARM = 1   ///< 区域异常状态（警报）
    };

    /**
     * @brief 构造函数
     * @param areaID 区域唯一标识
     */
    CAccessArea(const std::string& areaID);

    /**
     * @brief 析构函数
     */
    ~CAccessArea();

    /**
     * @brief 获取区域唯一标识
     * @return 区域唯一标识
     */
    std::string getAreaID() const;

    /**
     * @brief 获取区域当前状态
     * @return 区域状态
     */
    AreaStatus getAreaStatus() const;

    /**
     * @brief 设置区域状态
     * @param status 新的区域状态
     */
    void setAreaStatus(AreaStatus status);

    /**
     * @brief 获取关联的终端ID列表
     * @return 终端ID列表
     */
    std::vector<std::string> getTerminalIds() const;

    /**
     * @brief 添加关联的终端ID
     * @param terminalId 终端ID
     * @note 初始化时，所有新添加的终端默认设置为异常状态
     */
    void addTerminalId(const std::string& terminalId);

    /**
     * @brief 移除关联的终端ID
     * @param terminalId 终端ID
     */
    void removeTerminalId(const std::string& terminalId);

    /**
     * @brief 检查终端是否关联到该区域
     * @param terminalId 终端ID
     * @return 关联返回true，否则返回false
     */
    bool hasTerminal(const std::string& terminalId) const;

    /**
     * @brief 根据终端状态更新区域状态
     * @param terminalId 终端ID
     * @param terminalStatus 终端状态
     * @note 区域状态由其包含的所有终端状态决定，任何一个终端异常则区域异常
     */
    void updateStatusByTerminal(const std::string& terminalId, int terminalStatus);

    /**
     * @brief 检查区域是否异常
     * @return 异常返回true，否则返回false
     */
    bool isAbnormal() const;

private:
    /**
     * @brief 检查所有终端状态并更新区域状态
     * @note 区域状态规则：当区域内任何一个终端为异常状态时，该区域整体状态判定为异常
     */
    void checkAndUpdateStatus();

    std::string m_areaId;                   ///< 区域唯一标识
    std::atomic<AreaStatus> m_areaStatus;   ///< 区域状态
    std::vector<std::string> m_terminalIds; ///< 关联的终端ID列表

    /**
     * @brief 终端状态映射表
     * @details 存储区域内所有终端的当前状态，用于计算区域整体状态
     */
    std::map<std::string, int> m_terminalStatusMap;

    mutable std::mutex m_mutex; ///< 保护内部数据的互斥锁
};

#endif // CACCESSAREA_H
