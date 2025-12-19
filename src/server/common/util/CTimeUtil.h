#ifndef CTIMEUTIL_H
#define CTIMEUTIL_H

#include <chrono>
#include <ctime>
#include <string>


/** * @brief 获取当前时间的字符串表示
 * @param fmt 时间格式字符串，默认格式为 "yyyy-MM-dd HH:mm:ss.zzz"
 * @return 当前时间的字符串表示
 */
std::string getCurrentTimeString(const std::string& fmt = "yyyy-MM-dd HH:mm:ss.zzz")
{
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm buf;
#ifdef _WIN32
    localtime_s(&buf, &in_time_t);
#else
    localtime_r(&in_time_t, &buf);
#endif

    std::string result = fmt;
    char tmp[32];

    // 替换年
    size_t pos = result.find("yyyy");
    if (pos != std::string::npos) {
        std::strftime(tmp, sizeof(tmp), "%Y", &buf);
        result.replace(pos, 4, tmp);
    }

    // 替换月
    pos = result.find("MM");
    if (pos != std::string::npos) {
        std::strftime(tmp, sizeof(tmp), "%m", &buf);
        result.replace(pos, 2, tmp);
    }

    // 替换日
    pos = result.find("dd");
    if (pos != std::string::npos) {
        std::strftime(tmp, sizeof(tmp), "%d", &buf);
        result.replace(pos, 2, tmp);
    }

    // 替换时
    pos = result.find("HH");
    if (pos != std::string::npos) {
        std::strftime(tmp, sizeof(tmp), "%H", &buf);
        result.replace(pos, 2, tmp);
    }

    // 替换分
    pos = result.find("mm");
    if (pos != std::string::npos) {
        std::strftime(tmp, sizeof(tmp), "%M", &buf);
        result.replace(pos, 2, tmp);
    }

    // 替换秒
    pos = result.find("ss");
    if (pos != std::string::npos) {
        std::strftime(tmp, sizeof(tmp), "%S", &buf);
        result.replace(pos, 2, tmp);
    }

    // 替换毫秒
    pos = result.find("zzz");
    if (pos != std::string::npos) {
        std::snprintf(tmp, sizeof(tmp), "%03lld", static_cast<long long>(ms.count()));
        result.replace(pos, 3, tmp);
    }

    return result;
}

#endif // CTIMEUTIL_H
