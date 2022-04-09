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

#include <derecho/conf/conf.hpp>
#include <derecho/core/derecho.hpp>
#include "sample_objects.hpp"
#include "aggregate_bandwidth.hpp"

using derecho::ExternalCaller;
using derecho::Replicated;
using std::cout;
using std::endl;


const int num_nodes = 3;  // 结点数目
const int shard_size = 3;  // 也就是replica factor
const uint64_t num_messages = 100000;  // 发送消息的数目


int main(int argc, char** argv) {
    // 1. 创建Group
    // Read configurations from the command line options as well as the default config file
    derecho::Conf::initialize(argc, argv);

    //Define subgroup membership using the default subgroup allocator function
    //Each Replicated type will have one subgroup and one shard, with three members in the shard
    derecho::SubgroupInfo subgroup_function {derecho::DefaultSubgroupAllocator({
        {std::type_index(typeid(Bar)), derecho::one_subgroup_policy(derecho::fixed_even_shards(1, shard_size))},  // TODO node数量可能要大于replica数量，可能需要改shared数目
        // {std::type_index(typeid(Bar)), derecho::one_subgroup_policy(derecho::fixed_even_shards(1,3))}
    })};
    //Each replicated type needs a factory; this can be used to supply constructor arguments
    //for the subgroup's initial state. These must take a PersistentRegistry* argument, but
    //in this case we ignore it because the replicated objects aren't persistent.
    // auto foo_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<Foo>(-1); };
    auto bar_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<Bar>(); };

    // 1.1 创建callback函数，用于终止testing
    // variable 'done' tracks the end of the test
    volatile bool done = false;
    uint64_t total_num_messages = num_messages * num_nodes;  // 消息总数(all sender mode)
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
        if(num_delivered == total_num_messages) {
            done = true;
        }
    };

    derecho::Group<Bar> group(derecho::UserMessageCallbacks{stability_callback}, subgroup_function, {},
                                          std::vector<derecho::view_upcall_t>{},
                                          bar_factory);

    cout << "Finished constructing/joining Group" << endl;

    // 2. 创建发送消息的执行函数
    uint32_t my_id = derecho::getConfUInt32(CONF_DERECHO_LOCAL_ID);
    // this function sends all the messages
    auto send_all = [&]() {
        Replicated<Bar>& bar_rpc_handle = group.get_subgroup<Bar>();
        for(uint i = 0; i < num_messages; ++i) {
            // the lambda function writes the message contents into the provided memory buffer
            // in this case, we do not touch the memory region
            // uint64_t new_value = my_id * num_messages + i;  // 每次发送不同的值
            cout << "Appending to Bar." << endl;
            derecho::rpc::QueryResults<void> void_future = bar_rpc_handle.ordered_send<RPC_NAME(append)>("[Write from "+std::to_string(my_id)+"]");
            derecho::rpc::QueryResults<void>::ReplyMap& sent_nodes = void_future.get();
            cout << "Append delivered to nodes: ";
            for(const node_id_t& node : sent_nodes) {
                cout << node << " ";
            }
            cout << endl;
        }
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
    auto members_order = group.get_members();
    uint32_t node_rank = group.get_my_rank();
    long long int nanoseconds_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
    // calculate bandwidth measured locally
    double bw = (total_num_messages + 0.0) / nanoseconds_elapsed;
    // aggregate bandwidth from all nodes
    double total_bw = aggregate_bandwidth(members_order, members_order[node_rank], bw);
    // log the result at the leader node
    if(node_rank == 0) {
        cout << "total throughput: " << total_bw << endl;
    }
    group.barrier_sync();
    group.leave();

    // //Now have each node send some updates to the Replicated objects.
    // //The code must be different depending on which subgroup this node is in,
    // //which we can determine by attempting to locate its shard in each subgroup.
    // //Note that since there is only one subgroup of each type we can omit the
    // //subgroup_index parameter to many of the group.get_X() methods
    // int32_t my_foo_shard = group.get_my_shard<Foo>();
    // // int32_t my_bar_shard = group.get_my_shard<Bar>();
    // if(my_foo_shard != -1) {
    //     std::vector<node_id_t> foo_members = group.get_subgroup_members<Foo>()[my_foo_shard];
    //     uint32_t rank_in_foo = derecho::index_of(foo_members, my_id);
    //     Replicated<Foo>& foo_rpc_handle = group.get_subgroup<Foo>();
    //     //Each member within the shard sends a different multicast
    //     if(rank_in_foo == 0) {
    //         uint64_t new_value = 1;
    //         cout << "Changing Foo's state to " << new_value << endl;
    //         derecho::rpc::QueryResults<bool> results = foo_rpc_handle.ordered_send<RPC_NAME(change_state)>(new_value);
    //         decltype(results)::ReplyMap& replies = results.get();
    //         cout << "Got a reply map!" << endl;
    //         for(auto& reply_pair : replies) {
    //             cout << "Reply from node " << reply_pair.first << " was " << std::boolalpha << reply_pair.second.get() << endl;
    //         }
    //         cout << "Reading Foo's state just to allow node 1's message to be delivered" << endl;
    //         foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
    //     } else if(rank_in_foo == 1) {
    //         uint64_t new_value = 3;
    //         cout << "Changing Foo's state to " << new_value << endl;
    //         derecho::rpc::QueryResults<bool> results = foo_rpc_handle.ordered_send<RPC_NAME(change_state)>(new_value);
    //         decltype(results)::ReplyMap& replies = results.get();
    //         cout << "Got a reply map!" << endl;
    //         for(auto& reply_pair : replies) {
    //             cout << "Reply from node " << reply_pair.first << " was " << std::boolalpha << reply_pair.second.get() << endl;
    //         }
    //     } else if(rank_in_foo == 2) {
    //         std::this_thread::sleep_for(std::chrono::seconds(1));
    //         cout << "Reading Foo's state from the group" << endl;
    //         derecho::rpc::QueryResults<uint64_t> foo_results = foo_rpc_handle.ordered_send<RPC_NAME(read_state)>();
    //         for(auto& reply_pair : foo_results.get()) {
    //             cout << "Node " << reply_pair.first << " says the state is: " << reply_pair.second.get() << endl;
    //         }
    //     }
    // } else if(my_bar_shard != -1) {
    //     std::vector<node_id_t> bar_members = group.get_subgroup_members<Bar>()[my_bar_shard];
    //     uint32_t rank_in_bar = derecho::index_of(bar_members, my_id);
    //     Replicated<Bar>& bar_rpc_handle = group.get_subgroup<Bar>();
    //     if(rank_in_bar == 0) {
    //         cout << "Appending to Bar." << endl;
    //         derecho::rpc::QueryResults<void> void_future = bar_rpc_handle.ordered_send<RPC_NAME(append)>("Write from 0...");
    //         derecho::rpc::QueryResults<void>::ReplyMap& sent_nodes = void_future.get();
    //         cout << "Append delivered to nodes: ";
    //         for(const node_id_t& node : sent_nodes) {
    //             cout << node << " ";
    //         }
    //         cout << endl;
    //     } else if(rank_in_bar == 1) {
    //         cout << "Appending to Bar" << endl;
    //         bar_rpc_handle.ordered_send<RPC_NAME(append)>("Write from 1...");
    //         //Send to node rank 2 in shard 0 of the Foo subgroup
    //         node_id_t p2p_target = group.get_subgroup_members<Foo>()[0][2];
    //         cout << "Reading Foo's state from node " << p2p_target << endl;
    //         ExternalCaller<Foo>& p2p_foo_handle = group.get_nonmember_subgroup<Foo>();
    //         derecho::rpc::QueryResults<int> foo_results = p2p_foo_handle.p2p_send<RPC_NAME(read_state)>(p2p_target);
    //         int response = foo_results.get().get(p2p_target);
    //         cout << "  Response: " << response << endl;
    //     } else if(rank_in_bar == 2) {
    //         bar_rpc_handle.ordered_send<RPC_NAME(append)>("Write from 2...");
    //         cout << "Printing log from Bar" << endl;
    //         derecho::rpc::QueryResults<std::string> bar_results = bar_rpc_handle.ordered_send<RPC_NAME(print)>();
    //         for(auto& reply_pair : bar_results.get()) {
    //             cout << "Node " << reply_pair.first << " says the log is: " << reply_pair.second.get() << endl;
    //         }
    //         cout << "Clearing Bar's log" << endl;
    //         derecho::rpc::QueryResults<void> void_future = bar_rpc_handle.ordered_send<RPC_NAME(clear)>();
    //     }
    // } else {
    //     std::cout << "This node was not assigned to any subgroup!" << std::endl;
    // }

    // cout << "Reached end of main(), entering infinite loop so program doesn't exit" << std::endl;
    // while(true) {
    // }
}
