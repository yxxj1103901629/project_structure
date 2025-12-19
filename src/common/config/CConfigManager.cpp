#include "CConfigManager.h"
#include <fstream>

using namespace OutSide::Common::Config;

// 实现单例获取方法
CConfigManager& CConfigManager::getInstance()
{
    static CConfigManager instance;
    return instance;
}

// 内部辅助函数：共享的路径查找逻辑（不处理const）
template<typename JsonType>
JsonType* findJsonByPathImpl(JsonType& json, const std::string& path, bool createIfNotExists)
{
    // 处理空路径情况
    if (path.empty()) {
        return &json;
    }

    JsonType* current = &json;
    size_t pos = 0;
    const size_t pathSize = path.size();

    while (pos < pathSize) {
        // 跳过开头的点号
        if (path[pos] == '.') {
            pos++;
            continue;
        }

        // 找到下一个点号的位置
        size_t nextDotPos = path.find('.', pos);
        size_t keyLen = (nextDotPos == std::string::npos) ? (pathSize - pos) : (nextDotPos - pos);

        // 如果键长度为0，跳过这个点
        if (keyLen == 0) {
            pos = (nextDotPos == std::string::npos) ? pathSize : (nextDotPos + 1);
            continue;
        }

        // 检查当前节点是否为对象
        if (!current->is_object()) {
            if (createIfNotExists) {
                // 创建对象
                *current = nlohmann::json::object();
            } else {
                return nullptr;
            }
        }

        // 使用字符串视图避免不必要的字符串复制
        std::string_view keyView(&path[pos], keyLen);
        std::string key(keyView);

        // 检查键是否存在
        auto it = current->find(key);
        if (it == current->end()) {
            if (createIfNotExists) {
                // 创建键
                current = &((*current)[key]);
            } else {
                return nullptr;
            }
        } else {
            current = &(*it);
        }

        // 更新位置
        pos = (nextDotPos == std::string::npos) ? pathSize : (nextDotPos + 1);
    }

    return current;
}

// 辅助函数：根据键路径获取JSON引用（const版本）
template<typename JsonType>
std::pair<bool, const JsonType&> CConfigManager::findJsonByPath(const JsonType& json,
                                                                const std::string& path,
                                                                bool createIfNotExists)
{
    // const版本不允许创建路径
    if (createIfNotExists) {
        return {false, json};
    }

    // 使用const_cast调用非const版本的辅助函数
    JsonType* result = findJsonByPathImpl(const_cast<JsonType&>(json), path, false);
    if (result != nullptr) {
        return {true, *result};
    }
    return {false, json};
}

// 辅助函数：根据键路径获取JSON引用（非const版本）
template<typename JsonType>
std::pair<bool, JsonType&> CConfigManager::findJsonByPath(JsonType& json,
                                                          const std::string& path,
                                                          bool createIfNotExists)
{
    JsonType* result = findJsonByPathImpl(json, path, createIfNotExists);
    if (result != nullptr) {
        return {true, *result};
    }
    return {false, json};
}

// 实现从文件读取配置的方法
bool CConfigManager::loadFromFile(const std::string& filePath)
{
    try {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) {
            return false;
        }

        // 直接从文件流解析JSON
        nlohmann::json newJson = nlohmann::json::parse(ifs);

        // 加锁更新配置
        std::lock_guard<std::mutex> lock(m_mutex);
        // 将新JSON合并到主配置中
        m_json.merge_patch(newJson);

        return true;
    } catch (const nlohmann::json::exception&) {
        // JSON解析错误
        return false;
    } catch (const std::exception&) {
        // 其他标准异常
        return false;
    }
}

// 实现获取原始JSON对象的方法
const nlohmann::json& CConfigManager::getJson() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_json;
}

// 实现保存配置到文件的方法
bool CConfigManager::saveToFile(const std::string& filePath) const
{
    try {
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) {
            return false;
        }

        // 加锁读取配置
        std::lock_guard<std::mutex> lock(m_mutex);

        // 格式化输出，缩进为4个空格
        ofs << m_json.dump(4);

        return true;
    } catch (const std::exception&) {
        // 标准异常
        return false;
    }
}

// 实现获取配置值的模板方法
template<typename T>
T CConfigManager::getValue(const std::string& key, const T& defaultValue) const
{
    try {
        // 加锁读取配置
        std::lock_guard<std::mutex> lock(m_mutex);

        // 从主配置中获取
        auto [success, valueJson] = findJsonByPath(m_json, key, false);
        if (success) {
            return valueJson.get<T>();
        }
    } catch (...) {
        // 转换失败
    }
    return defaultValue;
}

// 实现设置配置值的模板方法
template<typename T>
void CConfigManager::setValue(const std::string& key, const T& value)
{
    try {
        // 加锁更新配置
        std::lock_guard<std::mutex> lock(m_mutex);

        // 更新配置值
        auto [success, jsonRef] = findJsonByPath(m_json, key, true);
        if (success) {
            jsonRef = value;
        }
    } catch (...) {
        // 设置失败，忽略
    }
}

// 显式实例化常用类型的模板
template int CConfigManager::getValue(const std::string&, const int&) const;
template double CConfigManager::getValue(const std::string&, const double&) const;
template bool CConfigManager::getValue(const std::string&, const bool&) const;
template std::string CConfigManager::getValue(const std::string&, const std::string&) const;
template nlohmann::json CConfigManager::getValue(const std::string&, const nlohmann::json&) const;
template void CConfigManager::setValue(const std::string&, const int&);
template void CConfigManager::setValue(const std::string&, const double&);
template void CConfigManager::setValue(const std::string&, const bool&);
template void CConfigManager::setValue(const std::string&, const std::string&);
template void CConfigManager::setValue(const std::string&, const nlohmann::json&);
