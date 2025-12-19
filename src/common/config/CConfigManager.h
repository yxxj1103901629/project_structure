#ifndef CCONFIGMANAGER_H
#define CCONFIGMANAGER_H
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace OutSide {
namespace Common {
namespace Config {

/**
 * @brief 配置管理器类
 * 
 * 单例模式实现的配置管理器，使用nlohmann/json库处理JSON配置，支持：
 * - 配置文件的加载和保存
 * - 嵌套路径访问（如 "server.port"）
 */
class CConfigManager
{
public:
    /**
     * @brief 获取单例实例
     * 
     * 使用懒汉式单例模式，确保全局只有一个配置管理器实例
     * @return CConfigManager的唯一实例
     */
    static CConfigManager& getInstance();

    // 删除拷贝构造和赋值运算符，确保单例
    CConfigManager(const CConfigManager&) = delete;
    CConfigManager& operator=(const CConfigManager&) = delete;

    /**
     * @brief 从文件加载配置
     * 
     * 从指定的JSON文件加载配置，会存储到对应的文件配置中
     * @param filePath 配置文件路径
     * @return 加载成功返回true，否则返回false
     */
    bool loadFromFile(const std::string& filePath);

    /**
     * @brief 将配置保存到文件
     * 
     * 将指定文件的配置以JSON格式保存到文件
     * @param filePath 配置文件路径
     * @return 保存成功返回true，否则返回false
     */
    bool saveToFile(const std::string& filePath) const;

    /**
     * @brief 获取配置值
     * 
     * 支持嵌套路径访问（如 "database.host"），如果键不存在则返回默认值
     * @tparam T 配置值的类型
     * @param key 配置项的键路径
     * @param defaultValue 当键不存在时返回的默认值
     * @return 配置项的值或默认值
     */
    template<typename T>
    T getValue(const std::string& key, const T& defaultValue = T{}) const;

    /**
     * @brief 设置配置值
     * 
     * 支持嵌套路径访问（如 "database.host"），如果路径不存在则创建
     * @tparam T 配置值的类型
     * @param key 配置项的键路径
     * @param value 配置项的新值
     */
    template<typename T>
    void setValue(const std::string& key, const T& value);

    /**
     * @brief 获取原始JSON对象
     * 
     * 用于高级操作，直接访问底层JSON对象
     * @return 原始JSON对象的常量引用
     */
    const nlohmann::json& getJson() const;

private:
    /**
     * @brief 私有构造函数
     * 
     * 确保只能通过getInstance()获取实例
     */
    CConfigManager() = default;

    /**
     * @brief 根据键路径获取JSON引用
     * 
     * 支持嵌套路径访问（如 "database.host"）
     * @tparam JsonType JSON类型（const或非const）
     * @param json JSON对象
     * @param path 键路径
     * @param createIfNotExists 如果路径不存在是否创建
     * @return 包含成功标志和JSON引用的pair
     */
    template<typename JsonType>
    static std::pair<bool, const JsonType&> findJsonByPath(const JsonType& json,
                                                           const std::string& path,
                                                           bool createIfNotExists = false);

    /**
     * @brief 根据键路径获取JSON引用（非const版本）
     * 
     * 支持嵌套路径访问（如 "database.host"）
     * @tparam JsonType JSON类型
     * @param json JSON对象
     * @param path 键路径
     * @param createIfNotExists 如果路径不存在是否创建
     * @return 包含成功标志和JSON引用的pair
     */
    template<typename JsonType>
    static std::pair<bool, JsonType&> findJsonByPath(JsonType& json,
                                                     const std::string& path,
                                                     bool createIfNotExists = false);

    // 主配置的 JSON 对象
    nlohmann::json m_json; /**< 主配置的JSON对象 */

    // 互斥锁，用于保护共享数据
    mutable std::mutex m_mutex; /**< 保护m_json的互斥锁 */
};

} // namespace Config
} // namespace Common
} // namespace OutSide

#endif // CCONFIGMANAGER_H
