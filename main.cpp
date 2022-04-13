/**
 * @file simple_replicated_objects.cpp
 *
 * This test creates two subgroups, one of each type Foo and Bar (defined in sample_objects.h).
 * It requires at least 6 nodes to join the group; the first three are part of the Foo subgroup,
 * while the next three are part of the Bar subgroup.
 * Every node (identified by its node_id) makes some calls to ordered_send in their subgroup;
 * some also call p2p_send. By these calls they verify that the state machine operations are
 * executed properly.
 */
#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cassert>
#include <thread>

#include <derecho/conf/conf.hpp>
#include <derecho/core/derecho.hpp>
#include "sample_objects.hpp"
#include "aggregate_bandwidth.hpp"

using derecho::ExternalCaller;
using derecho::Replicated;
using std::cout;
using std::endl;


const int num_clients =3;          // clients数目
const int shard_size = 3;           // 也就是replica factor
const double test_time = 10.0;      // 测试时间
const int msg_size = 1024;


int main(int argc, char** argv) {
    // 1. 创建Group
    // Read configurations from the command line options as well as the default config file
    derecho::Conf::initialize(argc, argv);

    //Define subgroup membership using the default subgroup allocator function
    //Each Replicated type will have one subgroup and one shard, with three members in the shard
    derecho::SubgroupInfo subgroup_function {derecho::DefaultSubgroupAllocator({
        {std::type_index(typeid(Foo)), derecho::one_subgroup_policy(derecho::fixed_even_shards(1, shard_size))}
        // {std::type_index(typeid(Bar)), derecho::one_subgroup_policy(derecho::fixed_even_shards(num_clients / shard_size, shard_size))},  // TODO node数量可能要大于replica数量，可能需要改shared数目
    })};
    //Each replicated type needs a factory; this can be used to supply constructor arguments
    //for the subgroup's initial state. These must take a PersistentRegistry* argument, but
    //in this case we ignore it because the replicated objects aren't persistent.
    // auto foo_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<Foo>(-1); };
    auto bar_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<Bar>(); };

    derecho::Group<Foo> group(derecho::UserMessageCallbacks{}, subgroup_function, {},
                                          std::vector<derecho::view_upcall_t>{},
                                          foo_factory);

    cout << "Finished constructing/joining Group" << endl;
    auto members_order = group.get_members();
    uint32_t node_rank = group.get_my_rank();

    // 2. 发送消息的函数
    auto send_one = [&]() {
        Replicated<Foo>& foo_rpc_handle = group.get_subgroup<Foo>();
        // Replicated<Bar>& bar_rpc_handle = group.get_subgroup<Bar>();
        // the lambda function writes the message contents into the provided memory buffer
        // in this case, we do not touch the memory region
        uint64_t new_value = node_rank * 1e5 + i;  // 每次发送不同的值
        derecho::rpc::QueryResults<bool> results = foo_rpc_handle.ordered_send<RPC_NAME(change_state)>(new_value);
        decltype(results)::ReplyMap& replies = results.get();
        for(auto& reply_pair : replies) {
            cout << "Reply from node " << reply_pair.first << " was " << std::boolalpha << reply_pair.second.get() << endl;
        }

        // std::string new_value = std::to_string(node_rank);
        // new_value += std::string(msg_size - new_value.size(), 'x');
        // derecho::rpc::QueryResults<void> void_future = bar_rpc_handle.ordered_send<RPC_NAME(append)>(new_value);
        // derecho::rpc::QueryResults<void>::ReplyMap& sent_nodes = void_future.get();
        // for(const node_id_t& node : sent_nodes) {
        //     cout << node << " ";
        // }
        // cout << endl;
    };

    // auto check = [&]()  {
    //     // Replicated<Foo>& foo_rpc_handle = group.get_subgroup<Foo>();
    //     // derecho::rpc::QueryResults<uint64_t> foo_results = foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
    //     // std::vector<uint64_t> res;
    //     // for(auto& reply_pair : foo_results.get()) {
    //     //     res.push_back(reply_pair.second.get());
    //     // }

    //     // Replicated<Bar>& bar_rpc_handle = group.get_subgroup<Bar>();
    //     // derecho::rpc::QueryResults<std::string> bar_results = bar_rpc_handle.ordered_send<RPC_NAME(print)>();
    //     // std::vector<std::string> res;
    //     // for(auto& reply_pair : bar_results.get()) {
    //     //     res.push_back(reply_pair.second.get());
    //     // }

    //     // for(int i = 0; i < res.size() - 1; ++ i) {
    //     //     assert(res[i] == res[i + 1]);
    //     // }
    // };

    // 3. throughput测试逻辑
    group.barrier_sync();
    auto start_time = std::chrono::steady_clock::now();
    uint64_t cnt = 0, nanoseconds_elapsed;
    do {
        send_one();
        cnt ++;
        if(cnt % 10) cout << cnt << endl;
        nanoseconds_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start_time).count();
    } while(nanoseconds_elapsed < test_time * 1e9);
    double bw = (cnt + 0.0) / nanoseconds_elapsed *1e9;
    double total_bw = aggregate_bandwidth(members_order, members_order[node_rank], bw);

    // log the result at the leader node
    if(node_rank == 0) {
        std::ofstream file;
        file.open("result.txt");
        file << "total throughput: " << std::fixed << total_bw << endl;
        file.close();
    }
    group.barrier_sync();
    group.leave();
}
