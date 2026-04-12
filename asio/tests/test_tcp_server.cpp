#include <chrono>
#include <iostream>
#include <tcp/CTcpServer.h>
#include <thread>

#include <windows.h>

using namespace asio;
using namespace std::chrono_literals;

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    CTcpServer server;
    if (!server.init()) {
        std::cerr << "服务器初始化失败" << std::endl;
        return 1;
    }

    ServerCallback callback;
    callback.errorOccurred = [](const std::string& err) {
        std::cerr << "服务器错误: " << err << std::endl;
    };
    callback.clientDisconnected = [](const NetAddr& clientAddr) {
        std::cout << "客户端断开连接: " << clientAddr.toString() << std::endl;
    };
    callback.clientConnected = [](const NetAddr& clientAddr) {
        std::cout << "客户端连接: " << clientAddr.toString() << std::endl;
    };
    callback.messageReceived = [&](const NetAddr& clientAddr, std::string_view msg) {
        server.sendToClient(clientAddr, msg); // 回显收到的消息
    };
    server.setCallback(callback);

    if (!server.listen(40004)) {
        std::cerr << "服务器监听失败" << std::endl;
        return 1;
    }

    std::cout << "服务器已启动，监听端口 40004" << std::endl;
    std::cout << "将在 3 秒后自动退出..." << std::endl;

    // 运行 3 秒后退出
    std::this_thread::sleep_for(3s);

    std::cout << "服务器停止中..." << std::endl;
    // server.stop();
    // std::this_thread::sleep_for(1s); // 等待服务器完全停止
    std::cout << "测试完成" << std::endl;

    return 0;
}