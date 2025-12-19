#ifndef CBUSINESSDATAUTIL_H
#define CBUSINESSDATAUTIL_H

#include <nlohmann/json.hpp>
#include <sstream>
#include <string>
#include <vector>

#include "../../business/config/BusinessConfig.h"
#include "CTimeUtil.h"

using byte = unsigned char;

class CBusinessDataUtil
{
public:
    /**
     * @brief 生成8位随机反馈码
     * @return 8位随机反馈码
     */
    static std::string generateRandomFeedbackCode()
    {
        const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz"
                                  "0123456789";
        std::string feedbackCode;
        feedbackCode.reserve(8);

        for (int i = 0; i < 8; ++i) {
            feedbackCode += chars[rand() % chars.size()];
        }

        return feedbackCode;
    }

    /**
     * @brief 生成8位校验码
     * @param timeStr 时间字符串，格式为"yyyy-MM-dd HH:mm:ss"，为空则使用当前时间
     * @param srcId 源标识字符串
     * @return 8位校验码字符串
     */
    static std::string generateCheckId(const std::string& timeStr, const std::string& srcId)
    {
        // 步骤1：处理时间，生成yyyyMMddHHmmss的14位字符串（替换原有的仅保留数字逻辑）
        std::string timeFactor;
        if (timeStr.empty()) {
            // 获取当前系统时间（使用线程安全的localtime_s/localtime_r替代localtime）
            std::time_t now = std::time(nullptr);
            std::tm localTime = {}; // 初始化tm结构体，避免未定义行为

// 跨平台处理：Windows使用localtime_s，Linux/macOS使用localtime_r
#ifdef _WIN32 // Windows平台
            errno_t err = localtime_s(&localTime, &now);
            if (err != 0) {
                throw std::invalid_argument("获取当前时间失败");
            }
#else // Linux/macOS平台
            if (localtime_r(&now, &localTime) == nullptr) {
                throw std::invalid_argument("获取当前时间失败");
            }
#endif

            std::ostringstream oss;
            // 格式化：年4位，月/日/时/分/秒各2位（补零）
            oss << std::setfill('0') << std::setw(4)
                << (localTime.tm_year + 1900)             // 年（tm_year是从1900开始的偏移量）
                << std::setw(2) << (localTime.tm_mon + 1) // 月（tm_mon是0-11）
                << std::setw(2) << localTime.tm_mday      // 日
                << std::setw(2) << localTime.tm_hour      // 时
                << std::setw(2) << localTime.tm_min       // 分
                << std::setw(2) << localTime.tm_sec;      // 秒
            timeFactor = oss.str();
        } else {
            // 解析指定时间：格式为"yyyy-MM-dd HH:mm:ss"（替换原有的仅保留数字逻辑）
            std::tm tm = {};
            std::istringstream iss(timeStr);
            iss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (iss.fail()) {
                throw std::invalid_argument("指定时间格式错误，需为yyyy-MM-dd HH:mm:ss");
            }
            std::ostringstream oss;
            oss << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << std::setw(2)
                << (tm.tm_mon + 1) << std::setw(2) << tm.tm_mday << std::setw(2) << tm.tm_hour
                << std::setw(2) << tm.tm_min << std::setw(2) << tm.tm_sec;
            timeFactor = oss.str();
        }

        // 验证时间因子长度（必须14位）
        if (timeFactor.length() != 14) {
            throw std::invalid_argument("时间因子生成失败，必须为14位（yyyyMMddHHmmss）");
        }

        // 步骤2：拼接混淆因子，转换为字节数组（UTF8编码，替换原有的字符串直接处理）
        std::string confusionBase = "safemine" + srcId;
        std::vector<byte> confusionBytes(confusionBase.begin(), confusionBase.end());
        std::vector<byte> timeFactorBytes(timeFactor.begin(), timeFactor.end());

        // 步骤3：异或运算（混淆因子循环复用，字节级异或，替换原有的字符异或）
        std::vector<byte> xorResult(timeFactorBytes.size());
        for (size_t i = 0; i < timeFactorBytes.size(); ++i) {
            size_t confusionIndex = i % confusionBytes.size();
            xorResult[i] = timeFactorBytes[i] ^ confusionBytes[confusionIndex];
        }

        // 步骤4：字节数组转十进制数（256进制转10进制），并限制范围（替换原有的逐字符取余）
        long long xorDecimal = 0;              // 使用long long避免溢出
        const long long maxLimit = 1000000000; // 9位，方便后续截取8位
        for (byte b : xorResult) {
            xorDecimal = xorDecimal * 256 + b;
            xorDecimal %= maxLimit; // 防止数值过大
        }

        // 步骤5：转换为8位字符串（不足补前导零，超过取后8位，逻辑优化但结果一致）
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(8) << xorDecimal;
        std::string checkId = oss.str();
        if (checkId.length() > 8) {
            checkId = checkId.substr(checkId.length() - 8);
        }
        return checkId;
    }

    /**
     * @brief 生成业务数据JSON字符串
     * @param topic 主题字符串
     * @param payload 载荷JSON对象
     * @return 业务数据JSON字符串
     */
    static std::string generateBusinessData(const std::string& topic, const nlohmann::json& payload)
    {
        // 获取业务数据ID
        int id = BusinessConfig::DEFAULT_MQTT_BUSINESS_DATA.count(topic) > 0
                     ? BusinessConfig::DEFAULT_MQTT_BUSINESS_DATA.at(topic)
                     : 0;
        // 获取当前时间字符串
        std::string time = getCurrentTimeString("yyyy-MM-dd HH:mm:ss");

        std::string srcId = "dmps_adsServer";
        // 从topic中提取srcId（第二个层级）
        // if (auto levels = splitLevels(topic, "/"); levels.size() > 1) {
        //     srcId = levels[1];
        // }

        // 构建业务数据JSON对象
        nlohmann::json businessData;
        businessData["header"]["id"] = id;
        businessData["header"]["ver"] = 1;
        businessData["header"]["time"] = time;
        businessData["header"]["srcId"] = srcId;
        businessData["header"]["fbId"] = generateRandomFeedbackCode();
        businessData["header"]["tarId"] = "00000000";
        businessData["header"]["ccId"] = std::stoi(generateCheckId(time, srcId));
        businessData["data"] = payload;
        return businessData.dump(4);
    }

private:
    // 按层级拆分字符串
    static std::vector<std::string> splitLevels(const std::string& source,
                                                const std::string& delimiter = "/")
    {
        std::vector<std::string> levels;
        size_t start = 0;
        size_t end = source.find(delimiter);
        while (end != std::string::npos) {
            levels.push_back(source.substr(start, end - start));
            start = end + delimiter.length();
            end = source.find(delimiter, start);
        }
        levels.push_back(source.substr(start));
        return levels;
    }
};

#endif // CBUSINESSDATAUTIL_H
