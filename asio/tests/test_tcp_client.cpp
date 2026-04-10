#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <tcp/CTcpClient.h>
#include <tcp/CTcpServer.h>

using namespace asio;
using namespace std::chrono_literals;

namespace {

constexpr std::chrono::milliseconds kDefaultWait = 4000ms;
constexpr uint16_t kPort = 40114;

struct SharedState
{
    std::mutex mtx;
    std::condition_variable cv;

    int clientConnected = 0;
    int clientDisconnected = 0;
    int serverConnected = 0;
    int serverDisconnected = 0;

    std::string clientRx;
    std::string serverRx;
    std::vector<std::string> clientErrors;
    std::vector<std::string> serverErrors;
};

bool waitFor(SharedState& state,
             const std::function<bool(const SharedState&)>& predicate,
             std::chrono::milliseconds timeout = kDefaultWait)
{
    std::unique_lock<std::mutex> lock(state.mtx);
    return state.cv.wait_for(lock, timeout, [&] { return predicate(state); });
}

bool expect(bool condition, const std::string& message)
{
    if (condition) {
        return true;
    }
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
}

struct EchoFixture
{
    SharedState state;
    CTcpServer server;
    CTcpClient client;
    NetAddr serverAddr{"127.0.0.1", kPort};
    bool serverRunning = false;
    bool clientReady = false;

    ~EchoFixture()
    {
        client.disconnect();
        if (serverRunning) {
            server.stop();
        }
    }

    bool startServer()
    {
        if (!server.init()) {
            std::cerr << "server init failed" << std::endl;
            return false;
        }

        ServerCallback serverCb;
        serverCb.clientConnected = [this](const NetAddr&) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                ++state.serverConnected;
            }
            state.cv.notify_all();
        };
        serverCb.clientDisconnected = [this](const NetAddr&) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                ++state.serverDisconnected;
            }
            state.cv.notify_all();
        };
        serverCb.messageReceived = [this](const NetAddr& clientAddr, std::string_view msg) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.serverRx.append(msg.data(), msg.size());
            }
            server.sendToClient(clientAddr, msg);
            state.cv.notify_all();
        };
        serverCb.errorOccurred = [this](const std::string& err) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.serverErrors.push_back(err);
            }
            state.cv.notify_all();
        };
        server.setCallback(std::move(serverCb));

        if (!server.listen(kPort)) {
            std::cerr << "server listen failed" << std::endl;
            return false;
        }

        serverRunning = true;
        return true;
    }

    bool startClient()
    {
        if (!client.init()) {
            std::cerr << "client init failed" << std::endl;
            return false;
        }

        ClientCallback clientCb;
        clientCb.connected = [this](const NetAddr&) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                ++state.clientConnected;
            }
            state.cv.notify_all();
        };
        clientCb.disconnected = [this](const NetAddr&) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                ++state.clientDisconnected;
            }
            state.cv.notify_all();
        };
        clientCb.messageReceived = [this](std::string_view msg) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.clientRx.append(msg.data(), msg.size());
            }
            state.cv.notify_all();
        };
        clientCb.errorOccurred = [this](const std::string& err) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.clientErrors.push_back(err);
            }
            state.cv.notify_all();
        };
        client.setCallback(std::move(clientCb));
        clientReady = true;
        return true;
    }

    bool connectAndWait()
    {
        if (!expect(client.connect(serverAddr), "connect should start successfully")) {
            return false;
        }

        if (!expect(waitFor(state,
                            [](const SharedState& s) {
                                return s.clientConnected >= 1 && s.serverConnected >= 1;
                            }),
                    "connect callbacks timeout")) {
            return false;
        }

        return true;
    }
};

bool testUninitializedClientApis()
{
    CTcpClient client;
    ClientCallback cb;
    cb.connected = [](const NetAddr&) {};

    client.setCallback(std::move(cb));
    client.disconnect();

    // connect() before init() triggers a debug assertion in current implementation,
    // so this case is intentionally excluded from executable assertions.
    return expect(!client.send(std::string_view("hello")),
                  "send(string_view) should fail before init")
           && expect(!client.send(std::string("hello")),
                     "send(const string&) should fail before init")
           && expect(!client.send(std::string("world")), "send(string&&) should fail before init")
           && expect(!client.send("abc", 3), "send(const char*, size_t) should fail before init");
}

bool testInvalidAddressReportsError()
{
    SharedState state;
    CTcpClient client;

    if (!expect(client.init(), "client init should succeed")) {
        return false;
    }

    ClientCallback cb;
    cb.connected = [&](const NetAddr&) {
        std::lock_guard<std::mutex> lock(state.mtx);
        ++state.clientConnected;
        state.cv.notify_all();
    };
    cb.errorOccurred = [&](const std::string& err) {
        std::lock_guard<std::mutex> lock(state.mtx);
        state.clientErrors.push_back(err);
        state.cv.notify_all();
    };
    client.setCallback(std::move(cb));

    if (!expect(client.connect(NetAddr{"256.1.1.1", kPort}),
                "connect should start even with invalid textual address")) {
        return false;
    }

    if (!expect(waitFor(state, [](const SharedState& s) { return !s.clientErrors.empty(); }),
                "invalid address should trigger error callback")) {
        return false;
    }

    bool ok = true;
    {
        std::lock_guard<std::mutex> lock(state.mtx);
        ok = ok
             && expect(state.clientConnected == 0,
                       "client should not report connected for invalid address");
        ok = ok
             && expect(state.clientErrors.front().find("地址解析失败") != std::string::npos,
                       "error should mention address parse failure");
    }

    client.disconnect();
    return ok;
}

bool testEchoAndBoundaryBehavior()
{
    EchoFixture fx;
    if (!expect(fx.startServer(), "server should start")) {
        return false;
    }
    if (!expect(fx.startClient(), "client should start")) {
        return false;
    }

    bool ok = true;
    ok = ok
         && expect(!fx.client.send(std::string_view("pre-connect")),
                   "send before connect should fail");

    if (!fx.connectAndWait()) {
        return false;
    }

    ok = ok
         && expect(!fx.client.connect(fx.serverAddr), "second connect while connected should fail");
    ok = ok && expect(!fx.client.send(std::string_view("")), "empty string_view should fail");
    ok = ok && expect(!fx.client.send("", 0), "zero length buffer should fail");

    const std::string msg1 = "m1:ping\n";
    const std::string msg2 = "m2:std_string\n";
    std::string msg3 = "m3:rvalue\n";
    const char* msg4 = "m4:c_buffer\n";
    const std::string msg5 = std::string("m5:") + std::string(8192, 'x') + "\n";
    const std::string expected = msg1 + msg2 + msg3 + msg4 + msg5;

    ok = ok && expect(fx.client.send(std::string_view(msg1)), "send(string_view) should succeed");
    ok = ok && expect(fx.client.send(msg2), "send(const string&) should succeed");
    ok = ok && expect(fx.client.send(std::move(msg3)), "send(string&&) should succeed");
    ok = ok
         && expect(fx.client.send(msg4, std::char_traits<char>::length(msg4)),
                   "send(const char*, size_t) should succeed");
    ok = ok && expect(fx.client.send(msg5), "send large payload should succeed");

    ok = ok
         && expect(waitFor(
                       fx.state,
                       [&](const SharedState& s) { return s.clientRx.size() >= expected.size(); },
                       6000ms),
                   "did not receive expected echoed bytes in time");

    {
        std::lock_guard<std::mutex> lock(fx.state.mtx);
        ok = ok
             && expect(fx.state.clientRx.substr(0, expected.size()) == expected,
                       "echoed byte stream should match sent byte stream");
    }

    fx.client.disconnect();
    ok = ok
         && expect(waitFor(fx.state,
                           [](const SharedState& s) {
                               return s.clientDisconnected >= 1 && s.serverDisconnected >= 1;
                           }),
                   "disconnect callbacks timeout");

    ok = ok
         && expect(!fx.client.send(std::string_view("post-disconnect")),
                   "send after disconnect should fail");

    {
        std::lock_guard<std::mutex> lock(fx.state.mtx);
        ok = ok
             && expect(fx.state.clientErrors.empty(),
                       "normal echo path should not trigger client errors");
        ok = ok
             && expect(fx.state.serverErrors.empty(),
                       "normal echo path should not trigger server errors");
    }

    return ok;
}

bool testReconnectAfterManualDisconnect()
{
    EchoFixture fx;
    if (!expect(fx.startServer(), "server should start")) {
        return false;
    }
    if (!expect(fx.startClient(), "client should start")) {
        return false;
    }
    if (!fx.connectAndWait()) {
        return false;
    }

    fx.client.disconnect();
    if (!expect(waitFor(fx.state,
                        [](const SharedState& s) {
                            return s.clientDisconnected >= 1 && s.serverDisconnected >= 1;
                        }),
                "first disconnect callbacks timeout")) {
        return false;
    }

    if (!expect(fx.client.connect(fx.serverAddr),
                "reconnect should start after manual disconnect")) {
        return false;
    }

    if (!expect(waitFor(fx.state,
                        [](const SharedState& s) {
                            return s.clientConnected >= 2 && s.serverConnected >= 2;
                        }),
                "reconnect callbacks timeout")) {
        return false;
    }

    const std::string probe = "after-reconnect\n";
    if (!expect(fx.client.send(probe), "send after reconnect should succeed")) {
        return false;
    }

    if (!expect(waitFor(fx.state,
                        [&](const SharedState& s) {
                            return s.clientRx.find(probe) != std::string::npos;
                        }),
                "echo after reconnect timeout")) {
        return false;
    }

    return true;
}

} // namespace

int main()
{
    const struct
    {
        const char* name;
        bool (*fn)();
    } tests[] = {
        {"uninitialized client apis", testUninitializedClientApis},
        {"invalid address reports error", testInvalidAddressReportsError},
        {"echo and boundary behavior", testEchoAndBoundaryBehavior},
        {"reconnect after manual disconnect", testReconnectAfterManualDisconnect},
    };

    int failed = 0;
    for (const auto& t : tests) {
        std::cout << "[RUN ] " << t.name << std::endl;
        const bool ok = t.fn();
        if (ok) {
            std::cout << "[ OK ] " << t.name << std::endl;
        } else {
            ++failed;
            std::cout << "[FAIL] " << t.name << std::endl;
        }
    }

    if (failed == 0) {
        std::cout << "all client tests passed" << std::endl;
        return 0;
    }

    std::cout << failed << " test(s) failed" << std::endl;
    return 1;
}
