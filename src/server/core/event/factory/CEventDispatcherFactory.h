#ifndef CEVENTDISPATCHERFACTORY_H
#define CEVENTDISPATCHERFACTORY_H

#include "../impl/CEventDispatcher.h"

class CEventDispatcherFactory
{
public:
    /**
     * @brief 创建事件分发器实例
     * @return 事件分发器实例指针
     */
    static IEventDispatcher *createEventDispatcher()
    {
        static CEventDispatcher dispatcher;
        if (dispatcher.init()) {
            return &dispatcher;
        }
        return nullptr;
    }
};

#endif // CEVENTDISPATCHERFACTORY_H
