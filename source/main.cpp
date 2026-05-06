#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <climits>
#include <map>
#include <ranges>
#include "common.h"
#include "tpsModel.h"
#include "shardsExtension.h"

using namespace std;

// 配置项
namespace Config {
    // 配置项
    int orderingCapacity = 5000;
    int executionCapacity = 8000; // 暂未启用
    int batchFetchSize = 5000;
    int transactionSendRate = 5000;
    string workLoadDir = "/Users/tanghaibo_office/Desktop/Second_Work_Code/third_work/simulateHieraChain/workloadProfile";
    int leafShardNumber = 36; // 下层叶子分片数目
    int newTopShardId;
}

// 主函数
int main() {
    shardsExtension extenser;
    cout << "系统当前吞吐为" << extenser.tm->calculatePerformanceScore() << endl;
    extenser.printInitialTopology(); // 打印 initialParentChildMap
    extenser.startOptimize(extenser.initialParentChildMap, Config::newTopShardId); // 开始优化
    return 0;
}