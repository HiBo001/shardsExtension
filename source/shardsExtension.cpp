#ifndef SHARDSEXTENSION
#define SHARDSEXTENSION

#include "shardsExtension.h"
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <random>

using namespace std;

vector<string> shardsExtension::parseWorkload(){
    std::vector<string> workloadDistribution;
    std::vector<std::string> lines;
    std::ifstream file(Config::workLoadDir);

    if (!file.is_open()) {
        std::cerr << "无法打开文件1: " << Config::workLoadDir << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        // 去除行首尾空白
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty()) continue;

        // 如果一行中有多个配置（逗号分隔），可以进一步分割
        std::istringstream iss(line);
        std::string item;

        // 如果你想将每个逗号分隔的部分作为独立元素
        while (std::getline(iss, item, ',')) {
            // 去除每个item前后的空格
            item.erase(0, item.find_first_not_of(" \t"));
            item.erase(item.find_last_not_of(" \t") + 1);

            if (!item.empty()) {
                workloadDistribution.push_back(item);
            }
        }
    }

    file.close();

    for (auto item : workloadDistribution) {
        cout << item << endl;
    }

    return workloadDistribution;
}

vector<string> shardsExtension::initialTopology(){
    Config::newTopShardId = Config::leafShardNumber + 1; // 新增上层分片id
    vector<int> leafShardIds;
    for(int i = 1; i <= Config::leafShardNumber; i++) { leafShardIds.push_back(i); }
    initialParentChildMap.insert(make_pair(Config::newTopShardId, leafShardIds));

    std::ostringstream oss;
    // 拼接前缀: (37,(
    oss << "(" << Config::newTopShardId << ",(";

    // 拼接中间的数字: 1,2,3...
    for (size_t i = 0; i < leafShardIds.size(); ++i) {
        oss << leafShardIds[i];
        if (i != leafShardIds.size() - 1) {
            oss << ","; // 只有不是最后一个元素时才加逗号
        }
    }

    // 拼接后缀: ))
    oss << "))";

    // 得到最终字符串
    vector<string> topologyLines = { oss.str() };
    return topologyLines;
}

shardsExtension::shardsExtension(){
    vector<string> topologyLines = initialTopology(); // 当前拓扑结构
    vector<string> workloadLines = parseWorkload(); // 当前负载
    parseLoad(workloadLines); // 解析负载

    tm = make_shared<tpsModel>(); // 初始化 tpsModel
    tm->parseTopology(topologyLines); // 导入负载拓扑
    tm->parseLoad(workloadLines); // 解析负载

    printTopology();
}

// void shardsExtension::parseLoad(vector<string>& loadLines){
//     this->workloadLines = loadLines;

//     for (const std::string& line : loadLines) {
//         size_t delimiterPos = line.find(':');
//         std::string key = line.substr(0, delimiterPos);
//         int load = std::stoi(line.substr(delimiterPos + 1));

//         if (key.find("inner") != std::string::npos) {
//             size_t shardPos = key.find("shard");
//             int shardId = std::stoi(key.substr(shardPos + 5));
//             innerLoad[shardId] = load;
//         } else {
//             size_t shardPos1 = key.find("shard");
//             int shardId1 = std::stoi(key.substr(shardPos1 + 5, key.find('_') - shardPos1 - 5));
//             int shardId2 = std::stoi(key.substr(key.find('_') + 6));
//             crossShardLoad[shardId1][shardId2] = load;
//             crossShardLoad[shardId2][shardId1] = load;
//         }
//     }
// }

void shardsExtension::parseLoad(vector<string>& loadLines){
    this->workloadLines = loadLines;

    for (const std::string& line : loadLines) {
        if (line.empty()) continue;

        size_t delimiterPos = line.find(':');
        if (delimiterPos == std::string::npos) continue;

        std::string key = line.substr(0, delimiterPos);
        int load = std::stoi(line.substr(delimiterPos + 1));

        if (key.find("inner") != std::string::npos) {
            // 解析示例: "shard12_inner"
            size_t sPos = key.find("shard");
            // 找到下划线位置，动态计算数字长度
            size_t uPos = key.find('_'); 
            int shardId = std::stoi(key.substr(sPos + 5, uPos - (sPos + 5)));
            innerLoad[shardId] = load;
        } 
        else {
            // 解析示例: "shard1_shard12"
            size_t firstUnderscore = key.find('_');
            
            // 提取第一个 ID (在第一个 shard 之后，第一个下划线之前)
            int shardId1 = std::stoi(key.substr(5, firstUnderscore - 5));
            
            // 提取第二个 ID (在第二个 shard 之后)
            size_t secondShardPos = key.find("shard", firstUnderscore);
            int shardId2 = std::stoi(key.substr(secondShardPos + 5));

            crossShardLoad[shardId1][shardId2] = load;
            crossShardLoad[shardId2][shardId1] = load;
        }
    }
}

pair<int, int> shardsExtension::getMaxCrossShardTxPair(vector<int>& remainingShardids) {
    int maxCount = -1;
    std::pair<int, int> maxShards = {-1, -1};

    for (size_t i = 0; i < remainingShardids.size(); ++i) {
        int s1 = remainingShardids[i];
        
        // 1. 先在外层查找 s1，减少重复查找
        auto it1 = crossShardLoad.find(s1);
        if (it1 == crossShardLoad.end()) continue;

        const auto& innerMap = it1->second;

        for (size_t j = i + 1; j < remainingShardids.size(); ++j) {
            int s2 = remainingShardids[j];

            // 2. 在 s1 的关联 map 中查找 s2
            auto it2 = innerMap.find(s2);
            if (it2 != innerMap.end()) {
                int currentCount = it2->second;
                
                if (currentCount > maxCount) {
                    maxCount = currentCount;
                    maxShards = {s1, s2};
                }
            }
        }
    }
    return maxShards;
}

// 从 remainingShardids 中寻找与 parentChildren 中跨片交易最多的祖先
int shardsExtension::findMostFrequentShard(map<int, vector<int>> parentChildren, int remainingShardid){

    int targetShardId = -1; // 最终返回的分片ID
    int maxCrossTransactions = -1; // 最大的跨片交易总和
    int currentCrossTransactions = -1;

    for (auto pair : parentChildren) {
        int parentShardid = pair.first;
        auto childrenShardids = pair.second;

        // 计算 分片 remainingShardid 与所有子分片的跨片交易总和
        for (int child : childrenShardids) {
            // 通过 crossShardLoad 获取分片 shardid 与子分片 child 之间的跨片交易数量
            if (crossShardLoad.find(remainingShardid) != crossShardLoad.end() && crossShardLoad[remainingShardid].find(child) != crossShardLoad[remainingShardid].end()) {                
                currentCrossTransactions += crossShardLoad[remainingShardid][child];
            }
        }

        // 更新跨片交易总和最多的分片
        if(currentCrossTransactions > maxCrossTransactions){
            maxCrossTransactions = currentCrossTransactions;
            targetShardId = parentShardid;
        }
    }

    if(targetShardId == -1){ // 如果和当前所有已分片孩子分片没有交易，则随机选择一个父亲节点

        // 提取所有 keys
        std::vector<int> keys;
        for (const auto& pair : parentChildren) {
            keys.push_back(pair.first);
        }

        // 随机选择
        if (!keys.empty()) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, keys.size() - 1);
            targetShardId = keys[dis(gen)];
            // std::cout << "Random shardid: " << randomKey << std::endl;
        }

    }
    return targetShardId;
}

int shardsExtension::findMostFrequentShard(vector<int>& childrenShardids, vector<int>& remainingShardids){ // 从 remainingShardids 中寻找与 childrenShardids 中分片跨片交易最多的分片

    int mostFrequentShard = -1; // 最终返回的分片ID
    int maxCrossTransactions = -1; // 最大的跨片交易总和

    for(auto shardid : remainingShardids){
        int totalCrossTransactions = 0; // 当前分片与所有孩子分片的跨片交易总和

        // 计算当前分片与所有子分片的跨片交易总和
        for (int child : childrenShardids) {
            // 通过 crossShardLoad 获取分片 shardid 与子分片 child 之间的跨片交易数量
            if (crossShardLoad.find(shardid) != crossShardLoad.end() && crossShardLoad[shardid].find(child) != crossShardLoad[shardid].end()) {
                totalCrossTransactions += crossShardLoad[shardid][child];
            }
        }

        // 更新跨片交易总和最多的分片
        if(totalCrossTransactions > maxCrossTransactions){
            maxCrossTransactions = totalCrossTransactions;
            mostFrequentShard = shardid;
        }
    }

    return mostFrequentShard;
}

double shardsExtension::getCrossShardLoad(vector<int>& children_shardids){

    double total_crossShardLoad = 0;
    for(auto shardid1 : children_shardids){
        for(auto shardid2 : children_shardids){
            if(shardid1 != shardid2 && crossShardLoad.find(shardid1) != crossShardLoad.end()
                            && crossShardLoad[shardid1].find(shardid2) != crossShardLoad[shardid1].end()){
                total_crossShardLoad += crossShardLoad[shardid1][shardid2];
            }
        }
    }
    return total_crossShardLoad / 2;
}

set<int> shardsExtension::findSubLeafShardids(map<int, vector<int>> parentChildren){

    set<int> leafParents;  // 使用set确保父节点不重复

    // 遍历每一个父节点和它的子节点
    for (const auto& entry : parentChildren) {
        int parent = entry.first;
        vector<int> children = entry.second;

        // 检查每个子节点是否是叶子节点
        for (int child : children) {
            // 如果子节点没有其他子节点，那么它是叶子节点
            if (parentChildren.count(child) == 0) {
                leafParents.insert(parent); // 这个父节点是叶子节点的父节点
            }
        }
    }
    return leafParents;
}

vector<string> shardsExtension::parentChildMapToStrVec(map<int, vector<int>>& parentChildMap){
    vector<string> parentChildStrVec;
    for(auto iter = parentChildMap.begin(); iter != parentChildMap.end(); iter++){
        string str = "(";
        int parentId = iter->first;
        str = str + to_string(parentId) + ",(";
        auto children_shardids = iter->second;

        for(int i = 0; i < children_shardids.size(); i++){
            if(i == 0){
                str = str + to_string(children_shardids.at(i));
            }
            else{
                str = str + "," + to_string(children_shardids.at(i));
            }
        }
        str = str + "))";
        parentChildStrVec.push_back(str);
    }
    return parentChildStrVec;
}


void shardsExtension::printInitialTopology(){

    cout << "输出当前分片的拓扑结构" << endl;

    // 打印网络拓扑
    for(auto iter = initialParentChildMap.begin(); iter != initialParentChildMap.end(); iter++){
        int children_size = iter->second.size();
        if(children_size != 0){
            cout << "分片" << iter->first << "的孩子分片包括: ";
            auto children = iter->second;
            for(int i = 0; i < children.size(); i++){
                cout << children.at(i) << " ";
            }
            cout << endl;
        }
    }
}


void shardsExtension::printTopology(){

    cout << "输出当前分片的拓扑结构" << endl;

    // 打印网络拓扑
    for(auto iter = finallParentChildMaps.begin(); iter != finallParentChildMaps.end(); iter++){
        int children_size = iter->second.size();
        if(children_size != 0){
            cout << "分片" << iter->first << "的孩子分片包括: ";
            auto children = iter->second;
            for(int i = 0; i < children.size(); i++){
                cout << children.at(i) << " ";
            }
            cout << endl;
        }
    }
}

void shardsExtension::startOptimize(map<int, vector<int>> parentChildren, int extentedShardId){
    
    int parentShardId = extentedShardId;
    auto childrenSharIds = parentChildren.at(extentedShardId); // 当前 extension_shardid 的所有孩子结点

    cout << "开始准备对节点 = " << parentShardId << "进行扩展, " << "其孩子分片包括: " << endl;
    for(auto shardid : childrenSharIds){
        cout << shardid << ", ";
    }
    cout << endl;

    double crossShardTxLoad = getCrossShardLoad(childrenSharIds);
    cout << "子分片的跨片交易总和为: " << crossShardTxLoad << endl;

    if(crossShardTxLoad <= Config::orderingCapacity){ // 跨片交易负载低于分片的排序能力，不用继续分裂，算法停止
        finallParentChildMaps.insert(make_pair(parentShardId, childrenSharIds));
        cout << "跨片负载没有超出排序能力，" << endl;
        return;
    } else {

        int maxChildCount = crossShardTxLoad / Config::orderingCapacity + 1; // 可能需要的下层分片数量
        cout << "跨片交易负载超出了祖先的排序能力, 开始扩容, 最多可能要" << maxChildCount << "个排序者分片" << endl;

        vector<topology_with_tps> topologys; // 记录不同拓扑下最高吞吐

        auto temp_finallParentChildMaps = parentChildren;
        auto temp_pre_ParentChildStrVec = parentChildMapToStrVec(temp_finallParentChildMaps); // 将 map<int, vector<int>> temporary_ParentChildMaps 转为 vector<string> 格式

        for(auto str : temp_pre_ParentChildStrVec){
            cout << str << " ";
        }
        cout << endl;

        double preMax_throughput = tm->calculatePerformanceScore(); // 计算当前理论吞吐
        cout << "当前系统理论吞吐为: " << preMax_throughput << endl;

        
        for(int child_num = 2; child_num <= maxChildCount; child_num++){ // 已经有了 1 个分片

            cout << endl << "开始优化另一种拓扑..." << endl;
            auto pre_ParentChildMaps = temp_finallParentChildMaps; // 优化前拓扑结构
            vector<int> remaining_shardids = pre_ParentChildMaps.at(parentShardId); // 所有孩子分片
            vector<int> sec_shards; // 第二轮处理的分片

            pre_ParentChildMaps.at(parentShardId).clear(); // 删除 pre_ParentChildMaps 中原本 parent_shardid 的孩子结点，并且添加新的扩展的分片

            // 将 parent 结点新增的孩子结点添加上去
            for(int i = 0; i < child_num; i++){
                Config::newTopShardId++;
                pre_ParentChildMaps.at(parentShardId).push_back(Config::newTopShardId);
                vector<int> childids;
                pre_ParentChildMaps[Config::newTopShardId] = childids;
            }

            // // 第一轮分配
            auto added_parentids = pre_ParentChildMaps.at(parentShardId);
            for(auto added_parentid : added_parentids){
                cout << "正在为新增父亲分片" << added_parentid << "分配孩子结点" << endl;

                // 1. 从 remaining_shardids 中选择发生跨片交易最频繁的两个分片
                pair<int, int> ids_pair = getMaxCrossShardTxPair(remaining_shardids);
                int shardid1 = ids_pair.first;
                int shardid2 = ids_pair.second;

                // 2. 将 shardid1 和 shardid2 加入到 added_childid 的孩子分片中
                pre_ParentChildMaps.at(added_parentid).push_back(shardid1);
                pre_ParentChildMaps.at(added_parentid).push_back(shardid2);

                // 3. 将 shardid1 和 shardid2 从 remaining_shardids 中删除
                remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), shardid1), remaining_shardids.end());
                remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), shardid2), remaining_shardids.end());


                // 4. 尝试 remaining_shardids 剩下的结点作为 added_childid 的孩子分片
                while(remaining_shardids.size() != 0){
                    int most_frequent_shard = findMostFrequentShard(pre_ParentChildMaps.at(added_parentid), remaining_shardids); // 从 remaining_shardid 中寻找与 pre_ParentChildMaps.at(added_parentid) 跨片最频繁的分片
                    auto temp_children_shardids = pre_ParentChildMaps.at(added_parentid);
                    temp_children_shardids.push_back(most_frequent_shard); // 尝试将 most_frequent_shard 加入到 pre_ParentChildMaps.at(added_parentid)中
                    crossShardTxLoad = getCrossShardLoad(temp_children_shardids); // 计算当前跨片交易负载

                    if(crossShardTxLoad <= Config::orderingCapacity){
                        pre_ParentChildMaps.at(added_parentid).push_back(most_frequent_shard);
                        remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), most_frequent_shard), remaining_shardids.end()); // 将 most_frequent_shard 从remaining_shardids中删除
                    } else{
                        break;
                    }
                }
            }

            // // 将 remaining_shardids 中剩余的分片加入到与其交易最多的分片
            // cout << "开始输出remaining_shardids" << endl;
            // for(auto shardid : remaining_shardids){
            //     cout << "remaining shardid = " << shardid << " ";
            // }

            // 将 remaining_shardids 中的分片逐个添加到跨片交易数量最多的子集中
            for(auto remaining_shardid : remaining_shardids){
                // 从 remaining_shardid 中寻找与 pre_ParentChildMaps.at(added_parentid) 跨片最频繁的分片
                int most_frequent_shard = findMostFrequentShard(pre_ParentChildMaps, remaining_shardid); 
                cout << "11111" << endl;
                pre_ParentChildMaps.at(most_frequent_shard).push_back(remaining_shardid);
                cout << "22222" << endl;
            }

            // 将 map<int, vector<int>> 转为 vector<string> 类型
            auto pre_ParentChildStrVec = parentChildMapToStrVec(pre_ParentChildMaps);
            for(auto str : pre_ParentChildStrVec){
                cout << str << " " << endl;
            }
            
            tm->shards.clear();
            tm->cross_shard_workloads.clear();
            tm->parseTopology(pre_ParentChildStrVec);
            tm->parseLoad(this->workloadLines);

            double throughput = tm->calculatePerformanceScore(); // 计算添加后的吞吐
            cout << "throughput = " << throughput << endl;

            topology_with_tps t;
            t.parent_children = pre_ParentChildMaps;
            t.throughput = throughput;
            topologys.push_back(t);
        }

        // 从 topologys 中选择吞吐最高的网络结构
        topology_with_tps best_topology;
        double max_throughput = -1;
        double current_throughput = -1;

        for(int i = 0; i < topologys.size(); i++){
            current_throughput = topologys.at(i).throughput;
            if(current_throughput > max_throughput){
                max_throughput = current_throughput;
                best_topology = topologys.at(i);
            }
        }

        if(preMax_throughput < max_throughput){

            preMax_throughput = max_throughput;

            cout << "当前找到的吞吐最高值为" << max_throughput << "其拓扑结构为: "<< endl;

            finallParentChildMaps = best_topology.parent_children; // 更新 finallParentChildMaps

            auto finallParentChildStrVec = parentChildMapToStrVec(finallParentChildMaps);
            for(auto str : finallParentChildStrVec){
                cout << str << " " << endl;
            }

            cout << "-----------------开始递归优化排序结点-----------------" << endl;

            // auto pre_ParentChildStrVec = parentChildMapToStrVec(finallParentChildMaps);
            // for(auto str : pre_ParentChildStrVec){
            //     cout << str << " " << endl;
            // }
            // cout << "优化后的吞吐为" << max_throughput << endl;
            // 寻找当前 finallParentChildMaps 的所有倒数第二层的父亲结点
            auto sub_leaf_shardids = findSubLeafShardids(finallParentChildMaps);
            for(auto sub_leaf_shardid : sub_leaf_shardids){
                cout << "开始对排序结点" << sub_leaf_shardid << "进行扩展" << endl;
                startOptimize(finallParentChildMaps, sub_leaf_shardid);
            }
        }
        else{
            return;
        }
    }
}

// void shardsExtension::startOptimize(map<int, vector<int>> parentChildren, int extentedShardId){
    
//     int parent_shardid = extentedShardId;
//     auto children_shardids = parentChildren.at(extentedShardId); // 当前 extension_shardid 的所有孩子结点

//     // cout << "parent_shardid = " << parent_shardid << endl;
//     // for(auto children_shardid : children_shardids){
//     //     cout << children_shardid << ", ";
//     // }
//     // cout << endl;
//     double total_cross_Tx_Load = children_cross_txs_load(children_shardids);
//     // cout << "子分片间跨片交易总和为: " << total_cross_Tx_Load << endl;

//     if(total_cross_Tx_Load <= Config::orderingCapacity){ // 跨片交易负载低于分片的排序能力，不用继续分裂，算法停止
//         finallParentChildMaps.insert(make_pair(parent_shardid, children_shardids));
//         cout << "跨片交易负载没有超出父结点排序能力" << endl;
//         return;
//     }

//     else{
//         int possible_child_num = total_cross_Tx_Load / Config::orderingCapacity + 1; // 可能需要的下层分片数量
//         cout << "跨片交易负载超出了祖先的排序能力, 开始继续扩容, 最多可能要" << possible_child_num << "个排序者分片" << endl;

//         vector<topology_with_tps> topologys; // 记录不同拓扑下最高吞吐

//         // auto temp_finallParentChildMaps = finallParentChildMaps;
//         auto temp_finallParentChildMaps = parentChildren;
//         auto temp_pre_ParentChildStrVec = parentChildMapToStrVec(temp_finallParentChildMaps); // 将 map<int, vector<int>> temporary_ParentChildMaps 转为 vector<string> 格式

//         for(auto str : temp_pre_ParentChildStrVec){
//             cout << str << " ";
//         }

//         double preMax_throughput = tm->calculatePerformanceScore(); // 计算添加后的吞吐
//         cout << "优化前的系统吞吐为: " << preMax_throughput << endl;

//         for(int child_num = 2; child_num <= possible_child_num; child_num++){ // 已经有了 1 个分片

//             cout << endl << "开始优化另一种拓扑......." << endl;
//             auto pre_ParentChildMaps = temp_finallParentChildMaps; // 优化前拓扑结构
//             vector<int> remaining_shardids = pre_ParentChildMaps.at(parent_shardid); // 所有孩子分片
//             vector<int> sec_shards; // 第二轮处理的分片

//             pre_ParentChildMaps.at(parent_shardid).clear(); // 删除 pre_ParentChildMaps 中原本 parent_shardid 的孩子结点，并且添加新的扩展的分片

//             // int added_parentid = 10000;  // 将 parent 结点新增的孩子结点添加上去
//             for(int i = 0; i < child_num; i++){
//                 pre_ParentChildMaps.at(parent_shardid).push_back(Config::newTopShardId);
//                 vector<int> childids;
//                 pre_ParentChildMaps[Config::newTopShardId] = childids;
//                 Config::newTopShardId++;
//             }

//             // 第一轮分配
//             auto added_parentids = pre_ParentChildMaps.at(parent_shardid);
//             for(auto added_parentid : added_parentids){
//                 cout << "正在为新增父亲分片" << added_parentid << "分配孩子结点" << endl;

//                 // 1. 从 remaining_shardids 中选择发生跨片交易最频繁的两个分片
//                 pair<int, int> ids_pair = getMaxCrossShardTxPair(remaining_shardids);
//                 int shardid1 = ids_pair.first;
//                 int shardid2 = ids_pair.second;

//                 // 2. 将 shardid1 和 shardid2 加入到 added_childid 的孩子分片中
//                 pre_ParentChildMaps.at(added_parentid).push_back(shardid1);
//                 pre_ParentChildMaps.at(added_parentid).push_back(shardid2);

//                 // 3. 将 shardid1 和 shardid2 从 remaining_shardids 中删除
//                 remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), shardid1), remaining_shardids.end());
//                 remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), shardid2), remaining_shardids.end());

//                 // 4. 尝试 remaining_shardids 剩下的结点作为 added_childid 的孩子分片
//                 while(remaining_shardids.size() != 0){
                    
//                     int most_frequent_shard = findMostFrequentShard(pre_ParentChildMaps.at(added_parentid), remaining_shardids); // 从remaining_shardid中寻找与pre_ParentChildMaps.at(added_parentid)跨片最频繁的分片
                    
//                     auto temp_children_shardids = pre_ParentChildMaps.at(added_parentid);
//                     temp_children_shardids.push_back(most_frequent_shard); // 尝试将 most_frequent_shard 加入到 pre_ParentChildMaps.at(added_parentid)中

//                     double cross_tx_load = children_cross_txs_load(temp_children_shardids); // 当前 added_parentid 的所有孩子分片
//                     if(cross_tx_load <= Config::orderingCapacity){ // 跨片交易负载大于排序能力

//                         pre_ParentChildMaps.at(added_parentid).push_back(most_frequent_shard);
//                         remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), most_frequent_shard), remaining_shardids.end()); // 将 most_frequent_shard 从remaining_shardids中删除
//                         // cout << "分片位置确定" << endl;
//                     }
//                     else{
//                         remaining_shardids.erase(std::remove(remaining_shardids.begin(), remaining_shardids.end(), most_frequent_shard), remaining_shardids.end()); // 将 most_frequent_shard 从remaining_shardids中删除
//                         sec_shards.push_back(most_frequent_shard);

//                         // cout << "将分片" << most_frequent_shard << "添加到分片 " << added_parentid << "的孩子" endl;

//                         break;
//                     }
//                 }
//             }

//             // 将 remaining_shardids 中剩余的元素加到 sec_shards 中
//             for(auto shardids : remaining_shardids){
//                 sec_shards.push_back(shardids);
//             }

//             // 第二轮分配，依次将 sec_shards 中的分片添加到使整体吞吐最高的方案
//             for(auto shardid : sec_shards){
                
//                 double max_throuhput = -1;
//                 int destin_parentid = -1;

//                 auto added_parentids = pre_ParentChildMaps.at(parent_shardid);
//                 for(auto added_parentid : added_parentids){
                    
//                     auto temp_pre_ParentChildMaps = pre_ParentChildMaps; // 临时拓扑结构
//                     temp_pre_ParentChildMaps.at(added_parentid).push_back(shardid); // 新的拓扑结构

//                     auto temp_pre_ParentChildStrVec = parentChildMapToStrVec(temp_pre_ParentChildMaps); // 将 map<int, vector<int>> temporary_ParentChildMaps 转为 vector<string> 格式

//                     tm->shards.clear();
//                     tm->cross_shard_workloads.clear();
//                     tm->parseTopology(temp_pre_ParentChildStrVec);
//                     tm->parseLoad(this->workloadLines);

//                     double current_throuhput = tm->calculatePerformanceScore(); // 计算添加后的吞吐
//                     // for(auto str : temp_pre_ParentChildStrVec){
//                     //     cout << str << " " << endl;
//                     // }

//                     if(current_throuhput > max_throuhput){
//                         destin_parentid = added_parentid;
//                         max_throuhput = current_throuhput;
//                     }
//                 }

//                 pre_ParentChildMaps.at(destin_parentid).push_back(shardid); // 将 shardid 添加到 pre_ParentChildMaps 中
//             }

//             // 将 map<int, vector<int>> 转为 vector<string> 类型
//             auto pre_ParentChildStrVec = parentChildMapToStrVec(pre_ParentChildMaps);
//             for(auto str : pre_ParentChildStrVec){
//                 cout << str << " " << endl;
//             }
            
//             tm->shards.clear();
//             tm->cross_shard_workloads.clear();
//             tm->parseTopology(pre_ParentChildStrVec);
//             tm->parseLoad(this->workloadLines);

//             double throughput = tm->calculatePerformanceScore(); // 计算添加后的吞吐
//             cout << "throughput = " << throughput << endl;

//             topology_with_tps t;
//             t.parent_children = pre_ParentChildMaps;
//             t.throughput = throughput;
//             topologys.push_back(t);
//         }

//         // 从 topologys 中选择吞吐最高的网络结构
//         topology_with_tps best_topology;
//         double max_throughput = -1;
//         double current_throughput = -1;

//         for(int i = 0; i < topologys.size(); i++){
//             current_throughput = topologys.at(i).throughput;
//             if(current_throughput > max_throughput){
//                 max_throughput = current_throughput;
//                 best_topology = topologys.at(i);
//             }
//         }

//         if(preMax_throughput < max_throughput){

//             preMax_throughput = max_throughput;

//             cout << "当前找到的吞吐最高值为" << max_throughput << "其拓扑结构为: "<< endl;

//             finallParentChildMaps = best_topology.parent_children; // 更新 finallParentChildMaps

//             auto finallParentChildStrVec = parentChildMapToStrVec(finallParentChildMaps);
//             for(auto str : finallParentChildStrVec){
//                 cout << str << " " << endl;
//             }

//             cout << "-----------------开始递归优化排序结点-----------------" << endl;

//             // auto pre_ParentChildStrVec = parentChildMapToStrVec(finallParentChildMaps);
//             // for(auto str : pre_ParentChildStrVec){
//             //     cout << str << " " << endl;
//             // }
//             // cout << "优化后的吞吐为" << max_throughput << endl;
//             // 寻找当前 finallParentChildMaps 的所有倒数第二层的父亲结点
//             auto sub_leaf_shardids = findSubLeafShardids(finallParentChildMaps);
//             for(auto sub_leaf_shardid : sub_leaf_shardids){
//                 cout << "开始对排序结点" << sub_leaf_shardid << "进行扩展" << endl;
//                 startOptimize(finallParentChildMaps, sub_leaf_shardid);
//             }
//         }
//         else{
//             return;
//         }
//     }
// }

#endif // MESSAGE_H