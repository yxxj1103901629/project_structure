cmake_minimum_required(VERSION 3.16)
project(network)

# ==================== 基础配置 ====================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 路径配置 ====================
set(BIN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# 设置第三方库的相关路径
set(THIRD_PARTY_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/include)
set(THIRD_PARTY_LIB ${CMAKE_CURRENT_SOURCE_DIR}/third_party/lib)
set(THIRD_PARTY_BIN ${CMAKE_CURRENT_SOURCE_DIR}/third_party/bin)

# ==================== 编译器配置 ====================
if(MSVC)
    add_compile_options(
        /MP                # 多核并行编译
        /utf-8             # 使用UTF-8编码
        /W4                # 警告等级4
        /wd4201            # 非标准扩展：无名结构体/联合体
        /wd4100            # 未引用的形参
        /wd4996            # 函数或变量已过时
        /wd4559            # 非标准扩展：对齐属性被忽略
        /wd4251            # 类需要 DLL-interface 才能在 DLL 中使用
    )
endif()

# ==================== Windows平台配置 ====================
if(WIN32)
    add_definitions(
        -DNOMINMAX                    # 禁止 Windows 头文件定义 min 和 max 宏
        -DWIN32_LEAN_AND_MEAN         # 减少 Windows 头文件的包含
        -D_WIN32_WINNT=0x0601         # 定义 Windows 版本为 Windows 7 或更高
    )
    link_libraries(
        ws2_32      # Windows 套接字库
        mswsock     # Microsoft Windows 套接字扩展
        iphlpapi    # IP 帮助库
        user32      # 用户界面库
        dbghelp     # 调试帮助库
    )
endif()

# ==================== 源文件配置 ====================
file(GLOB_RECURSE SRC_FILES "src/*")

# ==================== 创建库目标 ====================
add_library(${PROJECT_NAME} SHARED ${SRC_FILES})

# ==================== 库属性配置 ====================
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
    ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
    LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BIN_DIR}/debug
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}/release
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}/release
    LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BIN_DIR}/release
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR}/release
    ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR}/release
    LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${BIN_DIR}/release
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# ==================== 头文件配置 ====================
target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${THIRD_PARTY_INCLUDE}
        ${THIRD_PARTY_INCLUDE}/mosquitto-2.1.2
)

# ==================== 编译定义 ====================
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
    NETWORK_EXPORTS
)

# ==================== 链接配置 ====================
# 使用 $<CONFIG> 生成器表达式兼容多配置生成器（如 Visual Studio）
set(MOSQ_LIB_DIR ${THIRD_PARTY_LIB}/mosquitto-2.1.2)
target_link_directories(${PROJECT_NAME} PRIVATE
    ${MOSQ_LIB_DIR}/$<IF:$<CONFIG:Debug>,debug,release>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    mosquittopp
)

# ==================== 后期构建操作 ====================
# 复制 mosquitto DLL 到输出目录（使用 $<CONFIG> 按配置选择目录）
set(MOSQ_BIN_SUBDIR ${THIRD_PARTY_BIN}/mosquitto-2.1.2)
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MOSQ_BIN_SUBDIR}/$<CONFIG>/mosquitto.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MOSQ_BIN_SUBDIR}/$<CONFIG>/mosquittopp.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    COMMENT "Copying mosquitto DLLs to output directory"
)

# ==================== 测试配置 ====================
enable_testing()
add_subdirectory(tests)
