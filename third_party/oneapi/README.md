
# Using Intel® Threading Building Blocks (TBB) concurrent_hash_map in Qt Project

## 1. 库用途

**Intel® Threading Building Blocks (TBB)** 是一个高效的并行编程库，提供了一些强大的线程安全数据结构，其中之一就是 **`concurrent_hash_map`**，它是一个线程安全的哈希映射容器，适用于多线程环境。使用 **`concurrent_hash_map`** 可以简化并行编程中的共享数据管理，避免使用锁来保护数据结构，从而提高程序的并发性和性能。

**`concurrent_hash_map`** 允许多个线程安全地读取、修改或插入键值对，而无需显式的锁机制。

## 2. 安装和配置

### 2.1 下载和安装 TBB

您可以从 [Intel® TBB GitHub 页面](https://github.com/oneapi-src/oneTBB) 下载 TBB 源码或使用操作系统的包管理工具（如 `apt` 或 `brew`）安装。

### 2.2 在 Qt 项目中配置 TBB

在您的 **Qt 项目** 的 `.pro` 文件中，添加 TBB 的头文件路径和库文件路径。

#### **2.2.1 示例 .pro 文件配置**

```pro
# Qt 项目的其他设置
QT += core

# 添加 TBB 头文件路径
INCLUDEPATH += /path/to/tbb/include

# 添加 TBB 库文件路径
LIBS += -L/path/to/tbb/lib

# 链接 TBB 库文件
LIBS += -ltbb
````

确保替换 `/path/to/tbb/include` 和 `/path/to/tbb/lib` 为实际的 TBB 安装路径。

## 3. 使用 TBB 的 concurrent_hash_map

### 3.1 引入 TBB 的头文件

在您的代码中，首先包括 `concurrent_hash_map` 的头文件：

```cpp
#include <tbb/concurrent_hash_map.h>
```

### 3.2 创建和使用 `concurrent_hash_map`

以下是一个使用 `concurrent_hash_map` 的基本示例，展示如何存储和操作线程安全的键值对：

```cpp
#include <tbb/concurrent_hash_map.h>
#include <iostream>
#include <thread>

using namespace tbb;

typedef concurrent_hash_map<int, std::string> MapType;

void insert_data(MapType &map) {
    map.insert(MapType::value_type(1, "Value 1"));
    map.insert(MapType::value_type(2, "Value 2"));
}

void read_data(MapType &map) {
    MapType::const_accessor accessor;
    if (map.find(accessor, 1)) {
        std::cout << "Key 1: " << accessor->second << std::endl;
    }
    if (map.find(accessor, 2)) {
        std::cout << "Key 2: " << accessor->second << std::endl;
    }
}

int main() {
    MapType map;

    // 使用两个线程分别插入和读取数据
    std::thread producer(insert_data, std::ref(map));
    std::thread consumer(read_data, std::ref(map));

    producer.join();
    consumer.join();

    return 0;
}
```

### 3.3 `concurrent_hash_map` 的重要方法

* **`insert()`**: 用于插入键值对到 `concurrent_hash_map`。
* **`find()`**: 查找特定键对应的值，返回一个 **`const_accessor`**，该对象可以读取键值对的内容。
* **`erase()`**: 删除特定的键值对。
* **`clear()`**: 清空整个映射。

### 3.4 线程安全

`concurrent_hash_map` 提供了高度并行的读写能力，可以在多个线程中并行操作同一个哈希表，而不需要手动管理锁。它通过内部的分区策略，保证了不同线程对不同键的操作不会互相阻塞。

## 4. 总结

**TBB 的 `concurrent_hash_map`** 提供了一种线程安全、无锁的方式来管理哈希映射。它允许多个线程并发地插入、查找和删除元素，适用于多线程环境下的高并发数据存储需求。与传统的线程同步方法（如使用 `mutex`）相比，`concurrent_hash_map` 提供了更高效、更易用的方式来处理并行数据结构。

通过简单的 API，您可以将 TBB 集成到现有的项目中，充分利用其并发处理能力，提升程序性能。
