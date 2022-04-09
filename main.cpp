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


const int num_clients = 24;  // clients数目
const int shard_size = 24;  // 也就是replica factor
const uint64_t num_messages = 10;  // 发送消息的数目


int main(int argc, char** argv) {
    // 1. 创建Group
    // Read configurations from the command line options as well as the default config file
    derecho::Conf::initialize(argc, argv);

    //Define subgroup membership using the default subgroup allocator function
    //Each Replicated type will have one subgroup and one shard, with three members in the shard
    derecho::SubgroupInfo subgroup_function {derecho::DefaultSubgroupAllocator({
        // {std::type_index(typeid(Foo)), derecho::one_subgroup_policy(derecho::fixed_even_shards(1, shard_size))}
        {std::type_index(typeid(Bar)), derecho::one_subgroup_policy(derecho::fixed_even_shards(1, shard_size))},  // TODO node数量可能要大于replica数量，可能需要改shared数目
    })};
    //Each replicated type needs a factory; this can be used to supply constructor arguments
    //for the subgroup's initial state. These must take a PersistentRegistry* argument, but
    //in this case we ignore it because the replicated objects aren't persistent.
    // auto foo_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<Foo>(-1); };
    auto bar_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<Bar>(); };

    // 1.1 创建callback函数，用于终止testing
    // variable 'done' tracks the end of the test
    volatile bool done = false;
    uint64_t total_num_messages = num_messages * num_clients;  // 消息总数(all sender mode)
    // callback into the application code at each message delivery
    auto stability_callback = [&done,
                               total_num_messages,
                               num_delivered = 0u](uint32_t subgroup,
                                                   uint32_t sender_id,
                                                   long long int index,
                                                   std::optional<std::pair<uint8_t*, long long int>> data,
                                                   persistent::version_t ver) mutable {
        // Count the total number of messages delivered
        ++num_delivered;
        // Check for completion
        cout << "num_delivered: " << num_delivered << endl;
        if(num_delivered == total_num_messages) {
            done = true;
        }
    };

    derecho::Group<Bar> group(derecho::UserMessageCallbacks{stability_callback}, subgroup_function, {},
                                          std::vector<derecho::view_upcall_t>{},
                                          bar_factory);

    cout << "Finished constructing/joining Group" << endl;
    auto members_order = group.get_members();
    uint32_t node_rank = group.get_my_rank();

    // 2. 创建发送消息的执行函数、验证一致性的验证函数
    // uint32_t my_id = derecho::getConfUInt32(CONF_DERECHO_LOCAL_ID);
    // this function sends all the messages
    auto send_all = [&]() {
        // Replicated<Foo>& foo_rpc_handle = group.get_subgroup<Foo>();
        Replicated<Bar>& bar_rpc_handle = group.get_subgroup<Bar>();
        for(uint i = 0; i < num_messages; ++i) {
            // the lambda function writes the message contents into the provided memory buffer
            // in this case, we do not touch the memory region
            // uint64_t new_value = node_rank * num_messages + i;  // 每次发送不同的值
            // derecho::rpc::QueryResults<bool> results = foo_rpc_handle.ordered_send<RPC_NAME(change_state)>(new_value);

            std::string new_value = std::to_string(node_rank);
            new_value += std::string(1024 - new_value.size(), 'x');
            derecho::rpc::QueryResults<void> void_future = bar_rpc_handle.ordered_send<RPC_NAME(append)>(new_value);
        }
    };

    auto check_consistency = [&]()  {
        // Replicated<Foo>& foo_rpc_handle = group.get_subgroup<Foo>();
        // derecho::rpc::QueryResults<uint64_t> foo_results = foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
        // std::vector<uint64_t> res;
        // for(auto& reply_pair : foo_results.get()) {
        //     res.push_back(reply_pair.second.get());
        // }

        Replicated<Bar>& bar_rpc_handle = group.get_subgroup<Bar>();
        // derecho::rpc::QueryResults<std::string> bar_results = bar_rpc_handle.ordered_send<RPC_NAME(print)>();
        // std::vector<std::string> res;
        // for(auto& reply_pair : bar_results.get()) {
        //     res.push_back(reply_pair.second.get());
        // }
        derecho::rpc::QueryResults<void> void_future = bar_rpc_handle.ordered_send<RPC_NAME(clear)>();

        // for(int i = 0; i < res.size() - 1; ++ i) {
        //     assert(res[i] == res[i + 1]);
        // }
    };

    // 3. throughput测试逻辑
    // start timer
    auto start_time = std::chrono::steady_clock::now();
    // send all messages or skip if not a sender
    send_all();
    // wait for the test to finish
    while(!done) {
    }
    // end timer
    auto end_time = std::chrono::steady_clock::now();

    // 4. 计算throughput
    long long int nanoseconds_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();  // 以纳秒级计算
    // calculate bandwidth measured locally
    double bw = (total_num_messages + 0.0) / nanoseconds_elapsed * 1e9;
    // aggregate bandwidth from all nodes
    double total_bw = aggregate_bandwidth(members_order, members_order[node_rank], bw);
    // log the result at the leader node
    if(node_rank == 0) {
        check_consistency();
        cout << "total throughput: " << total_bw << endl;
    }
    group.barrier_sync();
    group.leave();
}
