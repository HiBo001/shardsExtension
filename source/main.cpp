#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <climits>
#include <map>

#include "common.h"
#include "tpsModel.h"
#include "shardsExtension.h"

using namespace std;

// // 全局变量，记录最终拓扑
// map<int, vector<int>> FinalpParentChildMaps;
// int order_capacity = 5000; // 分片的交易排序能力
// int added_shardid = 1000; // 新增的父亲分片id
// int temp_added_shardid = 2000; // 临时新增的父亲分片id

// 主函数
int main() {
    // 假设拓扑和负载数据已经准备好了
    vector<string> topologyLines = { // string 形式的拓扑关系
        // "(25,(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24))"
        "(37,(1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36))"
    };

    // vector<string> topologyLines = { // string 形式的拓扑关系
    //     "(10,(1000,1001))",
    //     "(1000,(1,2,3,6))",
    //     "(1001,(4,8,9,5,7))"
    // };

    // vector<string> topologyLines = { // string 形式的拓扑关系
    //     // "(10,(1002,1003,1004))",
    //     // "(1002,(1,2,3))",
    //     // "(1003,(4,8,9,5))",
    //     // "(1004,(7,6))"
    // };

    vector<string> loadLines = {
        "shard1_inner:2000", "shard2_inner:2000", "shard3_inner:2000", "shard4_inner:2000",
        "shard5_inner:2000", "shard6_inner:2000", "shard7_inner:2000", "shard8_inner:2000",
        "shard9_inner:2000", "shard10_inner:2000", "shard11_inner:2000", "shard12_inner:2000",
        "shard13_inner:2000", "shard14_inner:2000", "shard15_inner:2000", "shard16_inner:2000",
        "shard17_inner:2000", "shard18_inner:2000", "shard19_inner:2000", "shard20_inner:2000",
        "shard21_inner:2000", "shard22_inner:2000", "shard23_inner:2000", "shard24_inner:2000",
        "shard25_inner:2000", "shard26_inner:2000", "shard27_inner:2000", "shard28_inner:2000",
        "shard29_inner:2000", "shard30_inner:2000", "shard31_inner:2000", "shard32_inner:2000",
        "shard33_inner:2000", "shard34_inner:2000", "shard35_inner:2000", "shard36_inner:2000",

        "shard1_shard2:2000", "shard2_shard3:2000", "shard3_shard4:2000", "shard5_shard6:2000",
        "shard5_shard7:2000", "shard6_shard7:2000", "shard8_shard9:2000", "shard1_shard3:2000",
        "shard4_shard8:2000", "shard9_shard10:2000", "shard10_shard11:2000", "shard12_shard13:2000",
        "shard4_shard7:2000", "shard22_shard10:2000", "shard17_shard11:2000", "shard12_shard33:2000",
        "shard14_shard15:2000", "shard16_shard17:2000", "shard17_shard18:2000", "shard19_shard20:2000",
        "shard21_shard22:2000", "shard12_shard14:2000", "shard16_shard19:2000", "shard20_shard5:2000",
        "shard10_shard23:2000", "shard23_shard24:2000", "shard25_shard26:2000", "shard23_shard24:2000", 
        "shard27_shard5:2000", "shard18_shard28:2000", "shard11_shard32:2000", "shard1_shard34:2000",
        "shard27_shard28:2000", "shard29_shard30:2000", "shard31_shard32:2000", "shard33_shard34:2000",
    };

    shardsExtension sc(topologyLines, loadLines);
    cout << "系统当前吞吐为" << sc.tm->calculate_total_tps() << endl;

    map<int, vector<int>> parentChildMap; // map 形式的拓扑关系
    sc.parseTopology(topologyLines, parentChildMap); // 将 vector<string> 形式的拓扑结构转为 map 形式的结构
    
    sc.printTopology(); // 打印 finallParentChildMaps
    sc.start_optimize(parentChildMap, 37); // 开始优化
    
    return 0;
}
