# 基本Qt配置
QT += core
CONFIG += c++17 console

# 编译器选项
QMAKE_CXXFLAGS += /utf-8 /wd4200


DEFINES += NOMINMAX

win32 {
    LIBS += -lws2_32 -lmswsock -liphlpapi -luser32
}


INCLUDEPATH += \
    $$PWD/../../third_party/ \
    $$PWD/../../third_party/libuv-1.51.0/include \
    $$PWD/../../third_party/paho-mqtt/include \
    $$PWD/../../third_party/concurrentqueue


CONFIG(debug, debug|release): DESTDIR = $$PWD/../../bin/debug
CONFIG(release, debug|release): DESTDIR = $$PWD/../../bin/release

CONFIG(debug, debug|release): LIBS += -L$$PWD/../../bin/debug
CONFIG(release, debug|release): LIBS += -L$$PWD/../../bin/release

LIBS += -luv

DEFINES += PAHO_MQTTPP_IMPORTS
LIBS += -lpaho-mqtt3as -lpaho-mqtt3a -lpaho-mqttpp3

CONFIG(debug, debug|release): LIBS += -ltbb12_debug
CONFIG(release, debug|release): LIBS += -ltbb12

INCLUDEPATH += \
    accessControl \
    accessControl/core \
    accessControl/core/model \
    business/config \
    business/monitor \
    business/security/base \
    business/security/factory \
    business/security/impl \
    common/type \
    common/util \
    core/device/base \
    core/device/impl \
    core/event/base \
    core/event/impl \
    core/event/factory

HEADERS += \
    ../common/config/CConfigManager.h \
    ../common/eventloop/CUVMultiloop.h \
    ../common/eventloop/CUVOneloop.h \
    ../common/network/base/INetworkManager.h \
    ../common/network/base/NetworkType.h \
    ../common/network/impl/CNetworkManager.h \
    ../common/network/impl/mqttClient/CMqttMessage.h \
    ../common/network/impl/mqttClient/CPahoMqttClient.h \
    ../common/network/impl/tcp/CUVTcpClient.h \
    ../common/network/impl/tcp/CUVTcpServer.h \
    accessControl/CAccessManager.h \
    accessControl/core/CAccessServer.h \
    accessControl/core/model/CAccessArea.h \
    accessControl/core/model/CAccessTerminal.h \
    business/config/BusinessConfig.h \
    business/config/CConfigLoader.h \
    business/monitor/CControlDeviceScanner.h \
    business/monitor/COnlineDeviceRegistry.h \
    business/security/base/ISecurityStrategy.h \
    business/security/factory/CSecurityStrategyFactory.h \
    business/security/impl/CAccessAreaLinkageStrategy.h \
    business/security/impl/CControlDeviceEmergencyStrategy.h \
    business/security/impl/CDeviceOfflineStrategy.h \
    common/type/CommonEnum.h \
    common/util/CAppRunUtil.h \
    common/util/CBusinessDataUtil.h \
    common/util/CTimeUtil.h \
    core/device/base/CDeviceAttribute.h \
    core/device/base/IDevice.h \
    core/device/impl/CLhd.h \
    core/event/factory/CEventDispatcherFactory.h \
    core/event/base/IEventDispatcher.h \
    core/event/base/IEventDispatcher_copy.h \
    core/event/impl/CEventDispatcher.h \
    core/event/impl/CEventDispatcher_copy.h

SOURCES += \
    ../common/config/CConfigManager.cpp \
    ../common/eventloop/CUVMultiloop.cpp \
    ../common/eventloop/CUVOneloop.cpp \
    ../common/network/impl/CNetworkManager.cpp \
    ../common/network/impl/mqttClient/CPahoMqttClient.cpp \
    ../common/network/impl/tcp/CUVTcpClient.cpp \
    ../common/network/impl/tcp/CUVTcpServer.cpp \
    accessControl/CAccessManager.cpp \
    accessControl/core/model/CAccessArea.cpp \
    accessControl/core/model/CAccessTerminal.cpp \
    core/device/base/CDeviceAttribute.cpp \
    core/device/impl/CLhd.cpp \
    core/event/impl/CEventDispatcher.cpp \
    core/event/impl/CEventDispatcher_copy.cpp \
    main.cpp \



DISTFILES += \
    $$DESTDIR/resource/test_main.json \
    ../../bin/debug/resource/test_main_copy.json








