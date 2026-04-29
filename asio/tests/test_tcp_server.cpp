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
        std::cerr << "server init failed" << std::endl;
        return 1;
    }

    ServerCallback callback;
    callback.errorOccurred = [](const std::string& err) { std::cerr << "server error: " << err << std::endl; };
    callback.clientDisconnected = [](const NetAddr& clientAddr) {
        std::cout << "client disconnected: " << clientAddr.toString() << std::endl;
    };
    callback.clientConnected = [](const NetAddr& clientAddr) {
        std::cout << "client connected: " << clientAddr.toString() << std::endl;
    };
    callback.dataReceived = [&](const NetAddr& clientAddr, std::string_view data) {
        server.sendToClient(clientAddr, data);
    };
    server.setCallback(callback);

    if (!server.listen(40004)) {
        std::cerr << "server listen failed" << std::endl;
        return 1;
    }

    std::cout << "server started on 40004" << std::endl;
    std::this_thread::sleep_for(3s);
    std::cout << "test finished" << std::endl;
    return 0;
}
