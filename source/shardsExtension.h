#ifndef SHARDSEXTENSION_H
#define SHARDSEXTENSION_H

#include <vector>
#include <map>
#include <set>
#include <string>
#include "tpsModel.h"

using namespace std;


// 配置项
namespace Config {
    // 配置项
    extern int orderingCapacity;
    extern int executionCapacity; // 暂未启用
    extern int batchFetchSize;
    extern int transactionSendRate;
    extern string workLoadDir;
    extern int leafShardNumber; // 下层叶子分片数目
    extern int newTopShardId;
}

class tpsModel;

class shardsExtension {

    public:
        vector<string> workloadLines;
        unordered_map<int, int> innerLoad; // 每个分片的片内交易
        unordered_map<int, std::unordered_map<int, int>> crossShardLoad; // 不同分片之间的跨片交易数量

        map<int, vector<int>> finallParentChildMaps; //最终拓扑结构
        map<int, vector<int>> initialParentChildMap; // map 形式的拓扑关系

        shared_ptr<tpsModel> tm;
        int parent = -1;           // 父节点ID，默认值为 -1 表示无父节点

    public:
        shardsExtension(); // 初始化函数

        vector<string> initialTopology();

        vector<string> parseWorkload();


        void parseLoad(vector<string>& loadLines); // 解析负载字符串loadLines，更新片内负载和跨片负载 innerLoad 和 crossShardLoad
        
        pair<int, int> getMaxCrossShardTxPair(vector<int>& remainingShardids); // 从 remaining_shardids 中寻找一批最频繁发生跨片交易的分片对

        int findMostFrequentShard(vector<int>& children_shardids, vector<int>& remaining_shardids); // 寻找 remaining_shardids 中与 children_shardids 所有分片跨片交易最频繁的分片

        int findMostFrequentShard(map<int, vector<int>> parentChildren, int remainingShardid);

        double getCrossShardLoad(vector<int>& children_shardids); // 计算一批分片之间的跨片交易负载

        set<int> findSubLeafShardids(map<int, vector<int>> parentChildren); // 寻找每一个倒数第二层的结点数

        vector<string> parentChildMapToStrVec(map<int, vector<int>>& parentChildMap);

        void startOptimize(map<int, vector<int>> parentChildren, int extentedShardId); // 启动优化算法，对一个父亲分片若干个孩子分片的拓扑进行优化

        // double calucate_total_tps(map<int, vector<int>>& temporary_parent_children); // 计算特定拓扑和负载下系统的吞吐

        // vector<int> parseLine(const string& line);

        // void parseTopology(vector<string>& topologyLines, map<int, vector<int>>& parentChildMap); // 解析拓扑函数，根据 topologyLines（只会有一行） 获得 parentChildMap

        // void start_optimize(); // 启动优化算法，对一个父亲分片若干个孩子分片的拓扑进行优化

        // 输出拓扑信息
        void printTopology();

        void printInitialTopology();
};

#endif // MESSAGE_H
