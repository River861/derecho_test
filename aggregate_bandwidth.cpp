#include <fstream>

#include "aggregate_bandwidth.hpp"

double aggregate_bandwidth(std::vector<uint32_t> members, uint32_t node_id,
                           double bw) {
    OneResultSST sst(sst::SSTParams(members, node_id));
    sst.bw[sst.get_local_index()] = bw;
    sst.put();
    sst.sync_with_members();
    std::cout << "node: " << sst.get_local_index() << " bw: " << bw << std::endl;
    double total_bw = 0.0;
    unsigned int num_nodes = members.size();
    for(unsigned int i = 0; i < num_nodes; ++i) {
        total_bw += sst.bw[i];
    }
    // return total_bw / num_nodes;
    return total_bw;  // 吞吐量不需要计算成平均值
}

// std::pair<double, double> aggregate_bandwidth(std::vector<uint32_t> members, uint32_t node_id,
//                            std::pair<double, double> bw) {
//     TwoResultSST sst(sst::SSTParams(members, node_id));
//     sst.bw1[sst.get_local_index()] = bw.first;
//     sst.bw2[sst.get_local_index()] = bw.second;
//     sst.put();
//     sst.sync_with_members();
//     std::array<double, 2> total_bw = {0.0, 0.0};
//     unsigned int num_nodes = members.size();
//     for(unsigned int i = 0; i < num_nodes; ++i) {
//         total_bw[0] += sst.bw1[i];
//         total_bw[1] += sst.bw2[i];
//     }
//     // return {total_bw[0] / num_nodes, total_bw[1] / num_nodes};
//     return {total_bw[0], total_bw[1]};
// }
