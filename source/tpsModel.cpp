#ifndef TPSMODEL      // ← 移到最顶部
#define TPSMODEL

#include "common.h"
#include "tpsModel.h"

vector<int> tpsModel:: parseLine(const string& line){

    vector<int> numbers;
    // 匹配整数的正则表达式
    regex re("-?\\d+");
    auto begin = sregex_iterator(line.begin(), line.end(), re);
    auto end = sregex_iterator();
    
    for (auto i = begin; i != end; ++i) {
        smatch match = *i;
        int num = stoi(match.str());
        numbers.push_back(num);
    }

    return numbers;
}

int tpsModel::findLCA(int shard1, int shard2){
    set<int> ancestors;
    while (shard1 != -1) {
        ancestors.insert(shard1);
        shard1 = shards[shard1].parent;
    }
    while (shard2 != -1) {
        if (ancestors.count(shard2)) return shard2; // 找到最近公共祖先
        shard2 = shards[shard2].parent;
    }
    return -1; // 不可能发生
}

void tpsModel::parseTopology(const vector<string>& topologyLines){

    shards.clear(); // 先清空shard

    for (auto line : topologyLines){

        vector<int> numbers = parseLine(line);

        int number_size = numbers.size();
        int parentId = numbers.at(0);

        if (shards.find(parentId) == shards.end()) {  // 添加父亲分片
            SimulationShard parentShard = {parentId, 0, 0, 0, 0, {}, -1};
            shards[parentId] = parentShard;
        }

        for(int i = 1; i < number_size; i++){ // 添加孩子分片
            int childId = numbers.at(i);

            if (shards.find(childId) == shards.end()) {
                SimulationShard childShard = {childId, 0, 0, 0, 0, {}, -1};
                shards[childId] = childShard;
            }
            shards[parentId].children.push_back(childId);
            shards[childId].parent = parentId; // 设置子节点的父节点
        }
    }

    // 为顶层分片设置 parent = -1
    for (auto& entry : shards) {
        if (entry.second.parent == -1 && entry.second.children.empty() == false) {
            // 如果一个分片既有子节点又没有父节点，它是顶层分片
            entry.second.parent = -1;
        }
    }
}

void tpsModel::printTopology() {
    // 打印网络拓扑
    for(auto iter = shards.begin(); iter != shards.end(); iter++){
        int children_size = iter->second.children.size();
        if(children_size != 0){
            cout << "分片" << iter->first << "的孩子分片包括:" << endl;
            auto children = iter->second.children;
            for(int i = 0; i < children.size(); i++){
                cout << children.at(i) << " ";
            }
            cout << endl;
        }
    }
}

void tpsModel::parseLoad(vector<string> loadLines){

    regex shardRegex(R"(shard(\d+)_inner:(\d+))");
    for (const string& line : loadLines) {
        smatch match;
        if (regex_search(line, match, shardRegex)) { // 分片的片内负载
            int shardId = stoi(match[1].str());
            int txCount = stoi(match[2].str());
            shards[shardId].internalTxCount = txCount;
        } else if (line.find("shard") != string::npos) {
            int shard1, shard2, txCount;
            sscanf(line.c_str(), "shard%d_shard%d:%d", &shard1, &shard2, &txCount);

            int lca = findLCA(shard1, shard2); // 找到最近公共祖先
            shards[lca].crossShardTxCount += txCount; // 将跨片交易负载分配到LCA

            // cout << "shard1 = " << shard1 << ", shard2 = " << shard2 << ", lca = " << lca << endl;

            cross_shard_workload workload1;
            workload1.non_leaf_shardid = lca;
            workload1.leaf_shardid = shard1;
            workload1.txnum = txCount;
            cross_shard_workloads.push_back(workload1);

            cross_shard_workload workload2;
            workload2.non_leaf_shardid = lca;
            workload2.leaf_shardid = shard2;
            workload2.txnum = txCount;
            cross_shard_workloads.push_back(workload2);
        }
    }

    for (auto& entry : shards) {
        entry.second.process_capacity = process_capacity;
        entry.second.order_capacity = order_capacity;
        // cout << "分片" << entry.first << " 跨片排序负载为" << entry.second.crossShardTxCount << endl;
    }
}

void tpsModel::parseFlatenLoad(const vector<string>& loadLines) {

    regex shardRegex(R"(shard(\d+)_inner:(\d+))");
    for (const string& line : loadLines) {
        smatch match;
        if (regex_search(line, match, shardRegex)) { // 分片的片内负载
            int shardId = stoi(match[1].str());
            int txCount = stoi(match[2].str());
            shards[shardId].internalTxCount = txCount;
            shards[shardId].id = shardId;
        } else if (line.find("shard") != string::npos) {
            int shard1, shard2, txCount;
            sscanf(line.c_str(), "shard%d_shard%d:%d", &shard1, &shard2, &txCount); 
            cross_traffic[to_string(shard1)][to_string(shard2)] = txCount;
            cross_traffic[to_string(shard2)][to_string(shard1)] = txCount;

            shards[shard1].id = shard1;
            shards[shard2].id = shard2;
        }
    }
}

int tpsModel::calculateNonLeafTxCount(SimulationShard& shard){
    return min(shard.crossShardTxCount, shard.order_capacity);
}

int tpsModel::calculateLeafThroughput(SimulationShard& shard, map<double, double> availableTxs) {
    double orderedCST = 0;
    double internalTxCount = static_cast<double>(shard.internalTxCount);

    // --- 第一阶段：排序能力限制 ---
    if (internalTxCount >= order_capacity) {
        return static_cast<int>(order_capacity);
    }

    double remainingOrderSpace = order_capacity - internalTxCount;
    // 确保 availableTxs.at(1) 存在，建议使用 find()
    double requestedCST = availableTxs.count(1) ? availableTxs.at(1) : 0;
    
    orderedCST = (remainingOrderSpace >= requestedCST) ? requestedCST : remainingOrderSpace;

    // --- 第二阶段：计算处理能力限制 ---
    // 假设 internalTx 消耗 1 单位处理能力，cross-shard 消耗 crossShardTxLoad 单位
    double remaining_process_capacity = static_cast<double>(process_capacity) - (internalTxCount * 1.0);
    
    // 如果处理能力已经被内部交易耗尽
    if (remaining_process_capacity <= 0) return static_cast<int>(internalTxCount);

    double total_workload_needed = orderedCST * crossShardTxLoad;

    if (total_workload_needed <= 0) {
        return static_cast<int>(internalTxCount); // 无需处理跨片交易
    }

    if (remaining_process_capacity >= total_workload_needed) {
        // 资源充足，全额处理。跨片交易每笔贡献 0.5 TPS
        return static_cast<int>(internalTxCount + orderedCST * 0.5);
    } else {
        // 资源不足，按比例缩减跨片交易的处理量
        double execution_ratio = remaining_process_capacity / total_workload_needed;
        return static_cast<int>(internalTxCount + (execution_ratio * orderedCST * 0.5));
    }
}

// int tpsModel::calculateLeafThroughput(SimulationShard& shard, map<double, double> availableTxs){

//     int orderedCST = 0;
//     int internalTxCount = shard.internalTxCount;
//     if(internalTxCount >= order_capacity){  // internalTxCount = 0, order_capacity = 5000
//         return order_capacity;
//     }
//     else{
//         int remainingTxCount = order_capacity - internalTxCount; // 还能够处理交易次数
//         if(remainingTxCount >= availableTxs.at(1)){
//             orderedCST = availableTxs.at(1);
//         }
//         else{
//             orderedCST = remainingTxCount;
//         }
//     }

//     // 计算 orderedTxs 中能够处理的负载
//     double remaining_process_capacity = process_capacity - internalTxCount * 1;
//     double remaining_workload = orderedCST * crossShardTxLoad; // + orderedTxs.at(2) * crossShardTxLoad;

//     if(remaining_process_capacity >= remaining_workload){
//         return internalTxCount + orderedCST * 0.5;
//     }
//     else{
//         return internalTxCount + (remaining_process_capacity / remaining_workload) * orderedCST * 0.5;
//     }
// }

// double tpsModel::calculate_average_latency(){

//     // 计算所有的交易数目
//     int total_sub_tx_num = 0;
//     double total_latency = 0;

//     for (auto& entry : shards) {
//         SimulationShard& shard = entry.second;
//         if (shard.children.empty()) {  // 叶子分片
//             total_sub_tx_num += shard.internalTxCount;
//         }
//         else{  // 上层非叶子分片
//             total_sub_tx_num += shard.crossShardTxCount * 2;
//         }
//     }

//     for (auto& entry : shards) {
//         SimulationShard& shard = entry.second;

//         if (shard.children.empty()) {  // 叶子分片
//             // 当前分片单位时间内接收到的所有跨片交易
//             // double received_cross_Txnum = 0;

//             map<int, int> received_cross_Txnums; // 记录所有跨层与非跨层的跨片交易
//             received_cross_Txnums.insert(make_pair(1, 0));
//             received_cross_Txnums.insert(make_pair(2, 0));

//             // 找到所有会给当前分片发送跨片交易的上层分片 id
//             int workload_size = cross_shard_workloads.size();
//             for(int i = 0; i < workload_size; i++){

//                 auto cross_shard_workload = cross_shard_workloads.at(i);
//                 int non_leaf_shard = cross_shard_workload.non_leaf_shardid;
//                 int leaf_shard = cross_shard_workload.leaf_shardid;

//                 if(leaf_shard == shard.id){
//                     // 如果 non_leaf_shard 分片需要排序的交易数量小于排序能力，那么这些交易全部能够排序完
//                     if(shards.at(non_leaf_shard).crossShardTxCount <= shards.at(non_leaf_shard).order_capacity){
//                         // received_cross_Txnum += cross_shard_workload.txnum;
//                         // 判断该交易是否为跨层交易(检查 leaf_shard 是否为 non_leaf_shard 的child)
//                         if(count(shards.at(non_leaf_shard).children.begin(), shards.at(non_leaf_shard).children.end(), leaf_shard)){
//                             received_cross_Txnums.at(1) = received_cross_Txnums.at(1) + cross_shard_workload.txnum;
//                         }
//                         else{
//                             received_cross_Txnums.at(2) = received_cross_Txnums.at(2) + cross_shard_workload.txnum;
//                         }
//                     }
//                     else{
//                         // received_cross_Txnum += shards.at(non_leaf_shard).order_capacity / shards.at(non_leaf_shard).crossShardTxCount * cross_shard_workload.txnum;
//                         // 判断该交易是否为跨层交易(检查 leaf_shard 是否为 non_leaf_shard 的child)
//                         if(count(shards.at(non_leaf_shard).children.begin(), shards.at(non_leaf_shard).children.end(), leaf_shard)){
//                             received_cross_Txnums.at(1) = received_cross_Txnums.at(1) + shards.at(non_leaf_shard).order_capacity / shards.at(non_leaf_shard).crossShardTxCount * cross_shard_workload.txnum;
//                         }
//                         else{
//                             received_cross_Txnums.at(2) = received_cross_Txnums.at(2) + shards.at(non_leaf_shard).order_capacity / shards.at(non_leaf_shard).crossShardTxCount * cross_shard_workload.txnum;
//                         }
//                     }
//                 }
//             }


//             double received_cross_Txnum = received_cross_Txnums.at(1) + received_cross_Txnums.at(2);

//             // 加上片内交易
//             double received_total_Txnum = received_cross_Txnum + shard.internalTxCount; // 片内交易加收到的跨片交易
//             double remaining_Txnum = received_total_Txnum;
//             double stage_num = received_total_Txnum / shard.order_capacity;

//             if(stage_num < 1){
//                 total_latency += received_total_Txnum / shard.order_capacity;
//             }
//             else{
//                 for(int i = 1; i <= stage_num; i++){
//                     if(remaining_Txnum >= shard.order_capacity){

//                         total_latency += shard.order_capacity * i;
//                         remaining_Txnum -= shard.order_capacity;
//                     }
//                     else{
//                         total_latency += remaining_Txnum * (i - 1) + (remaining_Txnum / shard.order_capacity);
//                     }
//                 }
//             }

//             total_latency += received_cross_Txnums.at(1) * 0.01;
//             total_latency += received_cross_Txnums.at(2) * 0.05;
//         }
//         else{  // 上层非叶子分片

//             double received_cross_Txnum = shard.crossShardTxCount;
//             int remaining_Txnum = received_cross_Txnum;
//             int stage_num = received_cross_Txnum / shard.order_capacity;

//             if(stage_num < 1){
//                 total_latency += received_cross_Txnum / shard.order_capacity;
//             }
//             else{
//                 for(int i = 1; i <= stage_num; i++){
//                     if(remaining_Txnum >= shard.order_capacity){
//                         total_latency += shard.order_capacity * i;
//                         remaining_Txnum -= shard.order_capacity;
//                     }
//                     else{
//                         total_latency += remaining_Txnum * (i - 1) + (remaining_Txnum / shard.order_capacity);
//                     }
//                 }
//             }
//         }

//         // cout << "average_latency = " << total_latency / total_sub_tx_num << endl;
//         return total_latency / total_sub_tx_num;
//     }
//     return 0;
// }

/**
 * @brief 计算系统的理论性能得分（估算 TPS）
 * * 该模型遍历所有叶子分片，计算它们在受到排序能力限制下能处理的有效交易量
 * 并调用底层吞吐量模型进行汇总。
 * * @return double 整个系统的总估算 TPS
 */
double tpsModel::calculatePerformanceScore() {

    estimatedTps = 0; // 初始化总 TPS 为 0

    // 遍历所有分片，筛选出叶子分片进行吞吐量计算
    for (auto& entry : shards) {
        SimulationShard& shard = entry.second;
        
        // 逻辑：只有没有子分片的节点才是实际处理业务的叶子分片
        if (shard.children.empty()) {
            
            // 用于存储各类跨片交易的有效数量（此处 Key=1 可能代表某种特定的交易类型或层级）
            map<double, double> cross_txs;
            cross_txs.insert(make_pair(1, 0)); 
            
            int workload_size = cross_shard_workloads.size(); 
            
            // 遍历工作负载记录，寻找所有发送给当前叶子分片的跨片请求
            for(int i = 0; i < workload_size; i++){
                auto& cross_shard_workload = cross_shard_workloads.at(i);
                int non_leaf_shard = cross_shard_workload.non_leaf_shardid; // 发起请求的非叶子分片 ID
                int leaf_shard = cross_shard_workload.leaf_shardid;         // 接收请求的叶子分片 ID

                // 如果当前负载记录的目标是正在处理的这个分片
                if(leaf_shard == shard.id){ 

                    // 安全检查：如果发起端分片在当前拓扑中不存在，则跳过
                    if(shards.count(non_leaf_shard) == 0){ 
                        continue;
                    }

                    // 核心逻辑：判断发起端分片的排序能力是否遇到瓶颈
                    // 情况 A：上层排序能力充沛，可以处理全部跨片交易
                    if(shards.at(non_leaf_shard).crossShardTxCount <= shards.at(non_leaf_shard).order_capacity){
                        double orderedTxnum = cross_shard_workload.txnum;
                        cross_txs.at(1) += orderedTxnum; // 累加全额交易量
                    }
                    // 情况 B：上层排序能力不足，需按比例折减发送到下层的有效交易量
                    else {
                        // 注意：此处 (capacity / count) 在 int 类型下可能存在精度截断风险，建议在实操中转为 double
                        double orderedTxnum = (shards.at(non_leaf_shard).order_capacity / 
                                            shards.at(non_leaf_shard).crossShardTxCount * cross_shard_workload.txnum);
                        cross_txs.at(1) += orderedTxnum; // 累加折减后的有效交易量
                    }
                }
            }

            // 根据当前叶子分片的硬件属性和计算出的有效负载，计算该分片的实际吞吐量
            int throughput = calculateLeafThroughput(shard, cross_txs);
            
            // 累加所有叶子分片的吞吐量得到系统总性能
            estimatedTps += throughput;
        }
    }
    return estimatedTps;
}


// double tpsModel::calculatePerformanceScore(){

//     estimatedTps = 0;

//     // // 计算系统总吞吐
//     for (auto& entry : shards) {
//         SimulationShard& shard = entry.second;
//         if (shard.children.empty()) {  // 叶子分片
            
//             map<double, double> cross_txs;
//             cross_txs.insert(make_pair(1, 0)); // 不跨层跨片交易数量
            
//             int workload_size = cross_shard_workloads.size(); // 找到所有会给当前分片发送跨片交易的上层分片 id
//             for(int i = 0; i < workload_size; i++){
//                 auto cross_shard_workload = cross_shard_workloads.at(i);
//                 int non_leaf_shard = cross_shard_workload.non_leaf_shardid; // 发送跨片交易的非下层分片
//                 int leaf_shard = cross_shard_workload.leaf_shardid; // 接收跨片交易的下层分片

//                 if(leaf_shard == shard.id){ // 分片作为其接收者

//                     if(shards.count(non_leaf_shard) == 0){ // 暂时分片还未形成
//                         continue;
//                     }

//                     // cout << "分片" << non_leaf_shard << "需要排序的跨片交易数量为" << shards.at(non_leaf_shard).crossShardTxCount << endl;

//                     if(shards.at(non_leaf_shard).crossShardTxCount <= shards.at(non_leaf_shard).order_capacity){
//                         int orderedTxnum = cross_shard_workload.txnum;
//                         cross_txs.at(1) += orderedTxnum;
//                     }
//                     else{
//                         int orderedTxnum = (shards.at(non_leaf_shard).order_capacity / shards.at(non_leaf_shard).crossShardTxCount * cross_shard_workload.txnum);
//                         cross_txs.at(1) += orderedTxnum;
//                     }
//                 }
//             }

//             int throughput = calculateLeafThroughput(shard, cross_txs);
//             estimatedTps += throughput;
//         }
//     }
//     return estimatedTps;
// }

void tpsModel::printWorkload(){

    // 输出跨片负载矩阵
    for (const auto& outer : cross_traffic) {
        for (const auto& inner : outer.second) {
            std::cout << outer.first << " - " << inner.first << ": " << inner.second << " transactions" << std::endl;
        }
    }
    // 输出片内负载
    for(auto iter = shards.begin(); iter != shards.end(); iter++){
        cout << "shardid = " << iter->first << ", internal_TxNum = " << iter->second.internalTxCount << endl;
    }

}

int tpsModel::getTraffic(const std::string& shard1, const std::string& shard2){

    if (cross_traffic.find(shard1) != cross_traffic.end() && cross_traffic[shard1].find(shard2) != cross_traffic[shard1].end()) {
        return cross_traffic[shard1][shard2];
    }
    return 0;  // 默认返回0表示没有交易
}


tpsModel::tpsModel(vector<string> topologyLines){
    for (const string& line : topologyLines) {
        istringstream iss(line);
        int parentId;
        char ch;

        iss >> ch;  // 跳过 '('
        iss >> parentId;  // 读取父节点ID
        iss >> ch;  // 跳过 ',' 
        iss >> ch;  // 跳过 '('

        // 创建父分片（若不存在则初始化）
        if (shards.find(parentId) == shards.end()) {
            SimulationShard parentShard = {parentId, 0, 0, 0, 0, {}, -1};
            shards[parentId] = parentShard;
        }

        // 读取子节点
        while (iss >> ch) {
            if (ch == ')') break; // 结束
            if (ch == ',') continue; // 跳过逗号

            int childId = ch - '0';
            if (shards.find(childId) == shards.end()) {
                SimulationShard childShard = {childId, 0, 0, 0, 0, {}, -1};
                shards[childId] = childShard;
            }
            shards[parentId].children.push_back(childId);
            shards[childId].parent = parentId; // 设置子节点的父节点
        }
    }

    // 为顶层分片设置 parent = -1
    for (auto& entry : shards) {
        if (entry.second.parent == -1 && entry.second.children.empty() == false) {
            // 如果一个分片既有子节点又没有父节点，它是顶层分片
            entry.second.parent = -1;
        }
    }
}




#endif // MESSAGE_H