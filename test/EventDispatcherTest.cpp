#include <iostream>
#include <thread>
#include <chrono>
#include <atomic>
#include "../src/server/core/event/base/IEventDispatcher.h"
#include "../src/server/core/event/factory/EventDispatcherFactory.h"
#include "../src/common/network/base/NetworkType.h"

using namespace std;
using namespace Common::Network;
using namespace Server::Core::Event;

class EventDispatcherTest {
public:
    EventDispatcherTest() : m_dispatcher(nullptr), m_eventReceived(false), m_dataReceived(""), m_eventType(NetworkEventType::DATA_RECEIVED) {
        m_dispatcher = EventDispatcherFactory::createDispatcher();
    }

    ~EventDispatcherTest() {
        if (m_dispatcher) {
            m_dispatcher->stop();
            delete m_dispatcher;
        }
    }

    bool init() {
        if (!m_dispatcher) {
            cout << "Failed to create dispatcher" << endl;
            return false;
        }

        if (!m_dispatcher->init()) {
            cout << "Failed to initialize dispatcher" << endl;
            return false;
        }

        return true;
    }

    void testEventRegistrationAndDispatch() {
        cout << "Testing Event Registration and Dispatch..." << endl;

        // Register event handler
        bool result = m_dispatcher->registerHandler(
            NetworkType::TCP_SERVER,
            NetworkEventType::DATA_RECEIVED,
            [this](const EventData& eventData) {
                this->onDataReceived(eventData);
            }
        );

        if (!result) {
            cout << "Failed to register event handler" << endl;
            return;
        }

        // Create test event data
        EventData eventData = m_dispatcher->convertToEvent(
            NetworkType::TCP_SERVER,
            "connection_123",
            "192.168.1.100:8080",
            "192.168.1.200:12345",
            NetworkEventType::DATA_RECEIVED,
            "Hello, Event Dispatcher!"
        );

        // Dispatch event
        result = m_dispatcher->dispatchEvent(eventData);
        if (!result) {
            cout << "Failed to dispatch event" << endl;
            return;
        }

        // Wait for event to be processed
        this_thread::sleep_for(chrono::milliseconds(100));

        if (m_eventReceived) {
            cout << "✓ Event received successfully!" << endl;
            cout << "  Event Type: DATA_RECEIVED" << endl;
            cout << "  Data: " << m_dataReceived << endl;
        } else {
            cout << "✗ Event not received" << endl;
        }

        // Reset flags
        m_eventReceived = false;
        m_dataReceived = "";
    }

    void testMultipleEventHandlers() {
        cout << "\nTesting Multiple Event Handlers..." << endl;

        atomic<int> handlerCount(0);
        const int expectedHandlers = 3;

        // Register multiple handlers for the same event type
        for (int i = 0; i < expectedHandlers; ++i) {
            bool result = m_dispatcher->registerHandler(
                NetworkType::MQTT_CLIENT,
                NetworkEventType::DATA_RECEIVED,
                [&handlerCount](const EventData& eventData) {
                    handlerCount++;
                }
            );

            if (!result) {
                cout << "Failed to register handler " << i + 1 << endl;
                return;
            }
        }

        // Create and dispatch event
        EventData eventData = m_dispatcher->convertToEvent(
            NetworkType::MQTT_CLIENT,
            "mqtt_client_1",
            "localhost:1883",
            "topic/test",
            NetworkEventType::DATA_RECEIVED,
            "Test MQTT message"
        );

        bool result = m_dispatcher->dispatchEvent(eventData);
        if (!result) {
            cout << "Failed to dispatch event" << endl;
            return;
        }

        this_thread::sleep_for(chrono::milliseconds(100));

        if (handlerCount.load() == expectedHandlers) {
            cout << "✓ All " << expectedHandlers << " handlers executed successfully!" << endl;
        } else {
            cout << "✗ Expected " << expectedHandlers << " handlers, but only " << handlerCount.load() << " executed" << endl;
        }
    }

    void testEventConversion() {
        cout << "\nTesting Event Conversion..." << endl;

        EventData eventData = m_dispatcher->convertToEvent(
            NetworkType::UDP_SERVER,
            "udp_connection_456",
            "127.0.0.1:5000",
            "192.168.1.100:6000",
            NetworkEventType::DATA_RECEIVED,
            "UDP test data"
        );

        // Verify event data
        bool testPassed = true;

        if (eventData.source.networkType != NetworkType::UDP_SERVER) {
            cout << "✗ NetworkType mismatch" << endl;
            testPassed = false;
        }

        if (eventData.source.connectionId != "udp_connection_456") {
            cout << "✗ ConnectionId mismatch" << endl;
            testPassed = false;
        }

        if (eventData.source.sourceAddress != "127.0.0.1:5000") {
            cout << "✗ SourceAddress mismatch" << endl;
            testPassed = false;
        }

        if (eventData.source.destinationAddress != "192.168.1.100:6000") {
            cout << "✗ DestinationAddress mismatch" << endl;
            testPassed = false;
        }

        if (eventData.eventType != NetworkEventType::DATA_RECEIVED) {
            cout << "✗ EventType mismatch" << endl;
            testPassed = false;
        }

        if (eventData.data != "UDP test data") {
            cout << "✗ Data mismatch" << endl;
            testPassed = false;
        }

        if (testPassed) {
            cout << "✓ Event conversion successful!" << endl;
        }
    }

    void testPostTask() {
        cout << "\nTesting Post Task..." << endl;

        atomic<bool> taskExecuted(false);

        // Post a task to the event loop
        m_dispatcher->postTask([&taskExecuted]() {
            taskExecuted = true;
            cout << "Task executed in event loop!" << endl;
        });

        // Wait for task to be processed
        this_thread::sleep_for(chrono::milliseconds(100));

        if (taskExecuted) {
            cout << "✓ Task posted and executed successfully!" << endl;
        } else {
            cout << "✗ Task not executed" << endl;
        }
    }

    void runAllTests() {
        if (!init()) {
            return;
        }

        testEventRegistrationAndDispatch();
        testMultipleEventHandlers();
        testEventConversion();
        testPostTask();

        cout << "\nAll tests completed!" << endl;
    }

private:
    IEventDispatcher* m_dispatcher;
    atomic<bool> m_eventReceived;
    string m_dataReceived;
    NetworkEventType m_eventType;

    void onDataReceived(const EventData& eventData) {
        m_eventReceived = true;
        m_dataReceived = eventData.data;
        m_eventType = eventData.eventType;

        cout << "Data received: " << eventData.data << endl;
        cout << "Source: " << eventData.source.sourceAddress << " -> " << eventData.source.destinationAddress << endl;
        cout << "Connection ID: " << eventData.source.connectionId << endl;
        cout << "Network Type: " << static_cast<int>(eventData.source.networkType) << endl;
    }
};

int main() {
    cout << "=== Event Dispatcher Test Suite ===" << endl << endl;

    EventDispatcherTest test;
    test.runAllTests();

    return 0;
}
