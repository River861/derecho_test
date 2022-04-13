/**
 * @file repeated_rpc_test.cpp
 *
 * @date Oct 11, 2017
 * @author edward
 */

#include <iostream>

#include <derecho/core/derecho.hpp>
#include "sample_objects.hpp"
#include <derecho/conf/conf.hpp>

using derecho::ExternalCaller;
using derecho::Replicated;
using std::cout;
using std::endl;

int main(int argc, char** argv) {
    derecho::Conf::initialize(argc, argv);

    const int num_nodes_in_test = 16;
        derecho::SubgroupInfo subgroup_function {derecho::DefaultSubgroupAllocator({
        {std::type_index(typeid(FooInt)), derecho::one_subgroup_policy(derecho::fixed_even_shards(num_nodes_in_test / 2, 2))}
    })};

    auto foo_factory = [](persistent::PersistentRegistry*,derecho::subgroup_id_t) { return std::make_unique<FooInt>(-1); };

    derecho::Group<FooInt> group(derecho::UserMessageCallbacks{}, subgroup_function, {},
                                        std::vector<derecho::view_upcall_t>{},
                                        foo_factory);

    cout << "Finished constructing/joining Group" << endl;

    Replicated<FooInt>& foo_rpc_handle = group.get_subgroup<FooInt>();
    int trials = 10000;
    cout << "Changing Foo's state " << trials << " times" << endl;
    for(int count = 0; count < trials; ++count) {
        cout << "Sending query #" << count << std::endl;
        derecho::rpc::QueryResults<bool> results = foo_rpc_handle.ordered_send<RPC_NAME(change_state)>(count);
        bool results_total = true;
        for(auto& reply_pair : results.get()) {
            cout << "Waiting for results from " << reply_pair.first << endl;
            results_total = results_total && reply_pair.second.get();
        }
    }

    cout << "Reached end of main()" << endl;
    group.barrier_sync();
    group.leave();
    return 0;
}
