#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
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

    std::thread::id mainThread{};
    std::thread::id clientConnectedThread{};
    std::thread::id clientDisconnectedThread{};
    std::thread::id clientDataThread{};
    std::thread::id clientErrorThread{};
    std::thread::id serverConnectedThread{};
    std::thread::id serverDisconnectedThread{};
    std::thread::id serverDataThread{};
    std::thread::id serverErrorThread{};

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

bool expectRunsOffMainThread(std::thread::id tid,
                             std::thread::id mainThread,
                             const std::string& message)
{
    return expect(tid != std::thread::id{}, message + " should run")
           && expect(tid != mainThread, message + " should not run on main thread");
}

struct EchoFixture
{
    SharedState state;
    CTcpServer server;
    CTcpClient client;
    NetAddr serverAddr{"127.0.0.1", kPort};
    bool serverRunning = false;

    EchoFixture()
    {
        state.mainThread = std::this_thread::get_id();
    }

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
                state.serverConnectedThread = std::this_thread::get_id();
            }
            state.cv.notify_all();
        };
        serverCb.clientDisconnected = [this](const NetAddr&) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                ++state.serverDisconnected;
                state.serverDisconnectedThread = std::this_thread::get_id();
            }
            state.cv.notify_all();
        };
        serverCb.dataReceived = [this](const NetAddr& clientAddr, std::string_view data) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.serverRx.append(data.data(), data.size());
                state.serverDataThread = std::this_thread::get_id();
            }
            server.sendToClient(clientAddr, data);
            state.cv.notify_all();
        };
        serverCb.errorOccurred = [this](const std::string& err) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.serverErrors.push_back(err);
                state.serverErrorThread = std::this_thread::get_id();
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
                state.clientConnectedThread = std::this_thread::get_id();
            }
            state.cv.notify_all();
        };
        clientCb.disconnected = [this](const NetAddr&) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                ++state.clientDisconnected;
                state.clientDisconnectedThread = std::this_thread::get_id();
            }
            state.cv.notify_all();
        };
        clientCb.dataReceived = [this](std::string_view data) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.clientRx.append(data.data(), data.size());
                state.clientDataThread = std::this_thread::get_id();
            }
            state.cv.notify_all();
        };
        clientCb.errorOccurred = [this](const std::string& err) {
            {
                std::lock_guard<std::mutex> lock(state.mtx);
                state.clientErrors.push_back(err);
                state.clientErrorThread = std::this_thread::get_id();
            }
            state.cv.notify_all();
        };
        client.setCallback(std::move(clientCb));
        return true;
    }

    bool connectAndWait()
    {
        if (!expect(client.connect(serverAddr), "connect should start successfully")) {
            return false;
        }

        return expect(waitFor(state, [](const SharedState& s) {
                          return s.clientConnected >= 1 && s.serverConnected >= 1;
                      }),
                      "connect callbacks timeout");
    }
};

bool testUninitializedClientApis()
{
    CTcpClient client;
    ClientCallback cb;
    cb.connected = [](const NetAddr&) {};

    client.setCallback(std::move(cb));
    client.disconnect();

    return expect(!client.send(std::string_view("hello")), "send(string_view) should fail before init")
           && expect(!client.send(std::string("hello")), "send(const string&) should fail before init")
           && expect(!client.send("abc", 3), "send(const char*, size_t) should fail before init");
}

bool testInvalidAddressReportsError()
{
    SharedState state;
    state.mainThread = std::this_thread::get_id();
    CTcpClient client;

    if (!expect(client.init(), "client init should succeed")) {
        return false;
    }

    ClientCallback cb;
    cb.connected = [&](const NetAddr&) {
        {
            std::lock_guard<std::mutex> lock(state.mtx);
            ++state.clientConnected;
            state.clientConnectedThread = std::this_thread::get_id();
        }
        state.cv.notify_all();
    };
    cb.errorOccurred = [&](const std::string& err) {
        {
            std::lock_guard<std::mutex> lock(state.mtx);
            state.clientErrors.push_back(err);
            state.clientErrorThread = std::this_thread::get_id();
        }
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

    std::lock_guard<std::mutex> lock(state.mtx);
    return expect(state.clientConnected == 0, "client should not report connected for invalid address")
           && expect(state.clientErrors.front().find("failed to parse address") != std::string::npos,
                     "error should mention address parse failure")
           && expect(state.clientErrorThread != std::thread::id{}, "client error callback should run")
           && expect(state.clientErrorThread != state.mainThread, "client error callback should run on task thread");
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
    ok = ok && expect(!fx.client.send(std::string_view("pre-connect")), "send before connect should fail");

    if (!fx.connectAndWait()) {
        return false;
    }

    ok = ok && expect(!fx.client.connect(fx.serverAddr), "second connect while connected should fail");
    ok = ok && expect(!fx.client.send(std::string_view("")), "empty string_view should fail");
    ok = ok && expect(!fx.client.send("", 0), "zero length buffer should fail");

    const std::string msg1 = "m1:ping\n";
    const std::string msg2 = "m2:std_string\n";
    const std::string msg3 = "m3:rvalue\n";
    const char* msg4 = "m4:c_buffer\n";
    const std::string msg5 = std::string("m5:") + std::string(8192, 'x') + "\n";
    const std::string expected = msg1 + msg2 + msg3 + msg4 + msg5;

    ok = ok && expect(fx.client.send(std::string_view(msg1)), "send(string_view) should succeed");
    ok = ok && expect(fx.client.send(msg2), "send(const string&) should succeed");
    ok = ok && expect(fx.client.send(msg3), "send(string copy) should succeed");
    ok = ok && expect(fx.client.send(msg4, std::char_traits<char>::length(msg4)),
                      "send(const char*, size_t) should succeed");
    ok = ok && expect(fx.client.send(msg5), "send large payload should succeed");

    ok = ok && expect(waitFor(fx.state,
                              [&](const SharedState& s) { return s.clientRx.size() >= expected.size(); },
                              6000ms),
                      "did not receive expected echoed bytes in time");

    {
        std::lock_guard<std::mutex> lock(fx.state.mtx);
        ok = ok && expect(fx.state.clientRx.substr(0, expected.size()) == expected,
                          "echoed byte stream should match sent byte stream");
        ok = ok && expect(fx.state.serverRx.substr(0, expected.size()) == expected,
                          "server byte stream should match sent byte stream");
        ok = ok && expectRunsOffMainThread(fx.state.clientConnectedThread,
                                           fx.state.mainThread,
                                           "client connected callback");
        ok = ok && expectRunsOffMainThread(fx.state.clientDataThread,
                                           fx.state.mainThread,
                                           "client data callback");
        ok = ok && expectRunsOffMainThread(fx.state.serverConnectedThread,
                                           fx.state.mainThread,
                                           "server connected callback");
        ok = ok && expectRunsOffMainThread(fx.state.serverDataThread,
                                           fx.state.mainThread,
                                           "server data callback");
    }

    fx.client.disconnect();
    ok = ok && expect(waitFor(fx.state, [](const SharedState& s) {
                          return s.clientDisconnected >= 1 && s.serverDisconnected >= 1;
                      }),
                      "disconnect callbacks timeout");

    ok = ok && expect(!fx.client.send(std::string_view("post-disconnect")),
                      "send after disconnect should fail");

    {
        std::lock_guard<std::mutex> lock(fx.state.mtx);
        ok = ok && expect(fx.state.clientDisconnected == 1, "client disconnect callback should fire once");
        ok = ok && expect(fx.state.serverDisconnected == 1, "server disconnect callback should fire once");
        ok = ok && expect(fx.state.clientDisconnectedThread == fx.state.clientConnectedThread,
                          "client disconnect callback should stay on the client task thread");
        ok = ok && expectRunsOffMainThread(fx.state.serverDisconnectedThread,
                                           fx.state.mainThread,
                                           "server disconnect callback");
        ok = ok && expect(fx.state.clientErrors.empty(), "normal echo path should not trigger client errors");
        ok = ok && expect(fx.state.serverErrors.empty(), "normal echo path should not trigger server errors");
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
    if (!expect(waitFor(fx.state, [](const SharedState& s) {
                    return s.clientDisconnected >= 1 && s.serverDisconnected >= 1;
                }),
                "first disconnect callbacks timeout")) {
        return false;
    }

    if (!expect(fx.client.connect(fx.serverAddr), "reconnect should start after manual disconnect")) {
        return false;
    }

    if (!expect(waitFor(fx.state, [](const SharedState& s) {
                    return s.clientConnected >= 2 && s.serverConnected >= 2;
                }),
                "reconnect callbacks timeout")) {
        return false;
    }

    const std::string probe = "after-reconnect\n";
    if (!expect(fx.client.send(probe), "send after reconnect should succeed")) {
        return false;
    }

    return expect(waitFor(fx.state, [&](const SharedState& s) {
                      return s.clientRx.find(probe) != std::string::npos;
                  }),
                  "echo after reconnect timeout");
}

bool testConcurrentConnectReturnsSingleWinner()
{
    EchoFixture fx;
    if (!expect(fx.startServer(), "server should start")) {
        return false;
    }
    if (!expect(fx.startClient(), "client should start")) {
        return false;
    }

    std::atomic<int> successCount{0};
    std::thread t1([&] {
        if (fx.client.connect(fx.serverAddr)) {
            successCount.fetch_add(1, std::memory_order_relaxed);
        }
    });
    std::thread t2([&] {
        if (fx.client.connect(fx.serverAddr)) {
            successCount.fetch_add(1, std::memory_order_relaxed);
        }
    });
    t1.join();
    t2.join();

    if (!expect(successCount.load(std::memory_order_relaxed) == 1, "concurrent connect should have one winner")) {
        return false;
    }

    if (!expect(waitFor(fx.state, [](const SharedState& s) {
                    return s.clientConnected >= 1 && s.serverConnected >= 1;
                }),
                "winning connect should complete")) {
        return false;
    }

    std::lock_guard<std::mutex> lock(fx.state.mtx);
    return expect(fx.state.clientConnected == 1, "client connected callback should fire once")
           && expect(fx.state.serverConnected == 1, "server connected callback should fire once");
}

bool testServerStopReportsSingleDisconnect()
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

    fx.server.stop();
    fx.serverRunning = false;

    if (!expect(waitFor(fx.state, [](const SharedState& s) {
                    return s.clientDisconnected >= 1 && s.serverDisconnected >= 1;
                }),
                "server stop should report disconnect")) {
        return false;
    }

    std::lock_guard<std::mutex> lock(fx.state.mtx);
    return expect(fx.state.clientDisconnected == 1, "client disconnect callback should fire once on server stop")
           && expect(fx.state.serverDisconnected == 1, "server disconnect callback should fire once on server stop")
           && expect(fx.state.clientDisconnectedThread == fx.state.clientConnectedThread,
                     "client disconnect should stay on the same task thread")
           && expectRunsOffMainThread(fx.state.serverDisconnectedThread,
                                      fx.state.mainThread,
                                      "server disconnect callback");
}

bool testCallbackSetBeforeConnectDoesNotMissFirstEvents()
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

    std::lock_guard<std::mutex> lock(fx.state.mtx);
    return expect(fx.state.clientConnected == 1, "client should receive first connected event")
           && expect(fx.state.serverConnected == 1, "server should receive first connected event");
}

bool testNetAddrParse()
{
    const auto valid = NetAddr::parse("127.0.0.1:65535");
    const auto zeroPort = NetAddr::parse("127.0.0.1:0");
    const auto overflowPort = NetAddr::parse("127.0.0.1:65536");
    const auto negativePort = NetAddr::parse("127.0.0.1:-1");
    const auto trailingChars = NetAddr::parse("127.0.0.1:80abc");
    const auto empty = NetAddr::parse("");

    return expect(valid.has_value() && valid->port == 65535, "valid address should parse")
           && expect(!zeroPort.has_value(), "port zero should fail")
           && expect(!overflowPort.has_value(), "overflow port should fail")
           && expect(!negativePort.has_value(), "negative port should fail")
           && expect(!trailingChars.has_value(), "trailing characters should fail")
           && expect(!empty.has_value(), "empty address should fail");
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
        {"concurrent connect returns single winner", testConcurrentConnectReturnsSingleWinner},
        {"server stop reports single disconnect", testServerStopReportsSingleDisconnect},
        {"callback set before connect does not miss first events", testCallbackSetBeforeConnectDoesNotMissFirstEvents},
        {"net addr parse", testNetAddrParse},
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
