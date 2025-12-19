#ifndef CACCESSMANAGER_H
#define CACCESSMANAGER_H

/**
* 一、系统初始化与状态管理
* 1. 配置加载：从指定配置文件读取所有红区配置信息，包括各红区包含的门禁终端组成结构
* 2. 区域实例化：创建所有红区区域实例，初始化时所有门禁终端均设置为异常状态
* 3. 区域状态规则：区域状态由其包含的门禁终端状态决定，当区域内任何一个终端为异常状态时，该区域整体状态判定为异常
* 二、门禁终端管理
* 1. 服务器启动：实现门禁服务器启动功能，监听并接收门禁终端连接请求
* 2. 终端实例化：接收终端连接后，创建对应门禁终端实例，记录其关联的红区信息及当前状态
* 3. 数据处理流程：
*    a. 接收门禁终端上传数据并解析其状态
*    b. 状态处理分支：
*       - 正常状态：通过MQTT协议发布当前状态，使用当前门禁终端标识作为主题
*       - 异常状态：①通过MQTT协议发布当前状态，使用当前门禁终端标识作为主题；②更新该终端关联的所有红区状态为异常，并通过NNG协议发送数据至内部核心模块，数据格式为{所有关联的红区信息, 当前门禁终端信息}
* 三、指令处理机制
* 1. 临时关闭指令接口：对外提供接收临时关闭指令的公共方法
* 2. 指令处理流程：
*    a. 接收指令后，定位到对应的门禁终端实例
*    b. 将该终端状态更新为临时关闭
*    c. 通过MQTT协议发布临时关闭状态
*    d. 启动定时器，定时器到期后：
*       - 退出临时关闭状态
*       - 重新接收并解析该终端数据
*       - 通过MQTT协议发布恢复后的当前状态
*/

#include "core/CAccessServer.h"
#include "core/model/CAccessArea.h"
#include "core/model/CAccessTerminal.h"
#include <map>
#include <mutex>
#include <string>
#include <vector>

/** * @brief 门禁管理器类
 *
 * 负责管理门禁系统的整体运行，包括配置加载、区域实例化、终端管理等功能。
 */
class CAccessManager
{
public:
    /**
     * @brief 获取CAccessManager的单例实例
     * @return CAccessManager的单例引用
     */
    static CAccessManager& getInstance();

    /**
     * @brief 禁用拷贝构造函数
     */
    CAccessManager(const CAccessManager&) = delete;

    /**
     * @brief 禁用赋值运算符
     */
    CAccessManager& operator=(const CAccessManager&) = delete;

    /**
     * @brief 系统初始化
     * @param configPath 配置文件路径
     * @return 初始化成功返回true，否则返回false
     * @note 初始化过程包括：配置加载、区域实例化
     */
    bool initialize(const std::string& configPath);

    /**
     * @brief 启动门禁系统
     * @return 启动成功返回true，否则返回false
     * @note 启动门禁服务器，开始监听终端连接请求
     */
    bool start();

    /**
     * @brief 停止门禁系统
     */
    void stop();

    /**
     * @brief 处理临时关闭指令
     * @param terminalId 门禁终端ID
     * @param duration 临时关闭持续时间（秒）
     * @return 处理成功返回true，否则返回false
     */
    bool handleTemporaryClose(const std::string& terminalId, int duration);

    /**
     * @brief 处理门禁终端连接
     * @param terminalId 门禁终端ID
     * @param clientSocket 客户端套接字
     * @note 接收到终端连接后，创建对应门禁终端实例
     */
    void handleTerminalConnection(const std::string& terminalId, int clientSocket);

    /**
     * @brief 处理门禁终端数据
     * @param terminalId 门禁终端ID
     * @param data 终端上传的数据
     * @note 接收门禁终端上传数据并解析其状态，根据状态进行相应处理
     */
    void handleTerminalData(const std::string& terminalId, const std::string& data);

private:
    /**
     * @brief 构造函数
     */
    CAccessManager();

    /**
     * @brief 析构函数
     */
    ~CAccessManager();

    /**
     * @brief 加载配置文件
     * @param configPath 配置文件路径
     * @return 加载成功返回true，否则返回false
     * @note 从指定配置文件读取所有红区配置信息，包括各红区包含的门禁终端组成结构
     */
    bool loadConfig(const std::string& configPath);

    /**
     * @brief 实例化所有红区区域
     * @return 实例化成功返回true，否则返回false
     * @note 创建所有红区区域实例，初始化时所有门禁终端均设置为异常状态
     */
    bool instantiateAreas();

    /**
     * @brief 创建门禁终端实例
     * @param terminalId 门禁终端ID
     * @return 创建成功返回终端指针，否则返回nullptr
     * @note 记录终端关联的红区信息及当前状态
     */
    CAccessTerminal* createTerminal(const std::string& terminalId);

    /**
     * @brief 更新红区状态
     * @param areaId 红区ID
     * @param status 新的状态
     * @note 根据终端状态更新该终端关联的所有红区状态
     */
    void updateAreaStatus(const std::string& areaId, CAccessArea::AreaStatus status);

    /**
     * @brief 通过MQTT发布终端状态
     * @param terminalId 门禁终端ID
     * @param status 终端状态
     * @note 使用当前门禁终端标识作为主题
     */
    void publishStatusViaMQTT(const std::string& terminalId, CAccessTerminal::TerminalStatus status);

    /**
     * @brief 通过NNG发送异常数据
     * @param areaIds 关联的红区ID列表
     * @param terminalId 门禁终端ID
     * @param terminalStatus 终端状态
     * @note 数据格式为{所有关联的红区信息, 当前门禁终端信息}
     */
    void sendAbnormalDataViaNNG(const std::vector<std::string>& areaIds,
                                const std::string& terminalId,
                                CAccessTerminal::TerminalStatus terminalStatus);

    /**
     * @brief 解析终端上传的数据
     * @param data 终端上传的原始数据
     * @param[out] status 解析后的终端状态
     * @return 解析成功返回true，否则返回false
     */
    bool parseTerminalData(const std::string& data, CAccessTerminal::TerminalStatus& status);

    // 配置数据结构
    struct AreaConfig
    {
        std::string areaId;                   ///< 红区ID
        std::vector<std::string> terminalIds; ///< 红区包含的终端ID列表
    };

    std::vector<AreaConfig> m_areaConfigs; ///< 红区配置列表
    std::mutex m_configMutex;              ///< 配置数据互斥锁

    // 红区实例映射
    std::map<std::string, CAccessArea*> m_areas;
    std::mutex m_areaMutex; ///< 红区实例互斥锁

    // 门禁终端实例映射
    std::map<std::string, CAccessTerminal*> m_terminals;
    std::mutex m_terminalMutex; ///< 终端实例互斥锁

    // 门禁服务器实例
    CAccessServer m_server;

    std::atomic<bool> m_isRunning; ///< 系统运行状态
};

#endif // CACCESSMANAGER_H
