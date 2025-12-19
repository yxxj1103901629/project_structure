#pragma once

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
    EventDispatcherTest();
    ~EventDispatcherTest();

    bool init();
    void testEventRegistrationAndDispatch();
    void testMultipleEventHandlers();
    void testEventConversion();
    void testPostTask();
    void runAllTests();

private:
    IEventDispatcher* m_dispatcher;
    atomic<bool> m_eventReceived;
    string m_dataReceived;
    NetworkEventType m_eventType;

    void onDataReceived(const EventData& eventData);
};
