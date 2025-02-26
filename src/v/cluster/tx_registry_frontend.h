/*
 * Copyright 2023 Redpanda Data, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */

#pragma once
#include "cluster/fwd.h"
#include "cluster/tx_gateway_frontend.h"
#include "cluster/tx_registry_stm.h"
#include "cluster/types.h"
#include "features/feature_table.h"
#include "rpc/fwd.h"

#include <seastar/core/abort_source.hh>
#include <seastar/core/sharded.hh>

#include <vector>

namespace cluster {

class tx_registry_frontend final
  : public ss::peering_sharded_service<tx_registry_frontend> {
public:
    tx_registry_frontend(
      ss::smp_service_group,
      ss::sharded<cluster::partition_manager>&,
      ss::sharded<cluster::shard_table>&,
      ss::sharded<cluster::metadata_cache>&,
      ss::sharded<rpc::connection_cache>&,
      ss::sharded<partition_leaders_table>&,
      std::unique_ptr<cluster::controller>&,
      ss::sharded<cluster::tx_coordinator_mapper>&,
      ss::sharded<features::feature_table>&);

    ss::future<bool> ensure_tx_topic_exists();

    ss::future<describe_tx_registry_reply>
    route_locally(describe_tx_registry_request&&);

    ss::future<find_coordinator_reply>
      find_coordinator(kafka::transactional_id, model::timeout_clock::duration);

    ss::future<find_coordinator_reply>
    route_locally(find_coordinator_request&&);

    ss::future<> stop() {
        _as.request_abort();
        return ss::make_ready_future<>();
    }

private:
    ss::abort_source _as;
    ss::gate _gate;
    ss::smp_service_group _ssg;
    ss::sharded<cluster::partition_manager>& _partition_manager;
    ss::sharded<cluster::shard_table>& _shard_table;
    ss::sharded<cluster::metadata_cache>& _metadata_cache;
    ss::sharded<rpc::connection_cache>& _connection_cache;
    ss::sharded<partition_leaders_table>& _leaders;
    std::unique_ptr<cluster::controller>& _controller;
    ss::sharded<cluster::tx_coordinator_mapper>& _tx_coordinator_ntp_mapper;
    ss::sharded<features::feature_table>& _feature_table;
    int16_t _metadata_dissemination_retries{1};
    std::chrono::milliseconds _metadata_dissemination_retry_delay_ms;

    template<typename T>
    ss::future<typename T::reply> do_route_locally(T&&);

    template<typename Func>
    auto with_stm(Func&& func);

    ss::future<find_coordinator_reply> dispatch_find_coordinator(
      model::node_id, kafka::transactional_id, model::timeout_clock::duration);

    ss::future<find_coordinator_reply> process_locally(
      ss::shared_ptr<cluster::tx_registry_stm>, find_coordinator_request&&);

    ss::future<describe_tx_registry_reply> process_locally(
      ss::shared_ptr<cluster::tx_registry_stm>, describe_tx_registry_request&&);

    ss::future<find_coordinator_reply>
      find_coordinator_statically(kafka::transactional_id);

    ss::future<bool> try_create_tx_registry_topic();
    ss::future<bool> try_create_tx_topic();
};
} // namespace cluster
