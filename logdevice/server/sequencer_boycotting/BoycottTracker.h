/**
 * Copyright (c) 2017-present, Facebook, Inc. and its affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#pragma once

#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <folly/Synchronized.h>

#include "logdevice/common/sequencer_boycotting/Boycott.h"

/**
 * @file BoycottTracker is used to have a single place where boycott information
 *       is tracked. It consolidates the information received in the gossip
 *       as well as any local requests for boycotts from this node. It ensures
 *       that only a set amount of outliers are boycotted by this node. It also
 *       removes any old boycotts that are older than the set duration. The
 *       BoycottTracker may be queried to get the most up-to-date view of the
 *       active boycotts.
 */

namespace facebook { namespace logdevice {

class Request;

class BoycottTracker {
 public:
  virtual ~BoycottTracker() = default;
  /**
   * Re-calculates what nodes should be boycotted based on the current time.
   * Also removes any old boycotts in the reported_boycotts_ set
   */
  void calculateBoycotts(std::chrono::system_clock::time_point current_time);

  /**
   * Have to call calculateBoycotts before calling this function to have the
   * most up-to-date boycotts
   *
   * @params node The index of the node to be checked if it's boycotted
   * @return      Returns true if it's boycotted, false if not, in O(1) time
   */
  bool isBoycotted(node_index_t node);

  /**
   * Will update nodes in the internal structure with any nodes that have a
   * greater time
   *
   * @params boycotts     The boycotts received in the gossip
   */
  void updateReportedBoycotts(const std::vector<Boycott>& boycotts);

  /**
   * @returns a map of the boycotts received in the gossip messages. If
   *          calculateBoycotts is called before getting the boycotts, any
   *          expired boycotts will have been removed
   */
  const std::unordered_map<node_index_t, Boycott>& getBoycottsForGossip() const;

  /**
   * Re-calculates what nodes should be boycotted before returning the vector
   * @retuns a vector of nodes that should be boycotted
   */
  std::vector<node_index_t>
  getBoycottedNodes(std::chrono::system_clock::time_point now);

  /**
   * Thread safe. This is one of two functions that may be called on a different
   * thread than the gossip thread during normal operation
   *
   * Will save the outliers, to later be calculated by
   * calculateBoycottsByThisNode
   * @params outliers Ordered by worst to best performance
   */
  void setLocalOutliers(std::vector<NodeID> outliers);

  /**
   * Thread safe. This is one of two functions that may be called on a different
   * thread than the gossip thread during normal operation
   *
   * Will reset the boycott of a node
   */
  void resetBoycott(node_index_t node_index);

 private:
  /**
   * The time a node was started notified to be boycotted + the spread time will
   * be the time that a boycott goes into effect. This is good because then the
   * entire cluster will put a boycott into effect at the same time.
   * @returns the amount of time required for a boycott to propagate throughout
   *          the cluster
   */
  virtual std::chrono::milliseconds getBoycottSpreadTime() const;

  virtual unsigned int getMaxBoycottCount() const;
  virtual std::chrono::milliseconds getBoycottDuration() const;

  // given the previous update to the local outliers, calculate which of these
  // should be boycotted
  void calculateBoycottsByThisNode(
      std::chrono::system_clock::time_point current_time,
      unsigned int max_boycott_count,
      std::chrono::milliseconds boycott_duration);

  void removeExpiredBoycotts(std::chrono::system_clock::time_point current_time,
                             std::chrono::milliseconds boycott_duration);

  // removes any nodes that are not boycotted but that are still part of the
  // reported boycotts. Will keep resets and boycotts that are not yet in effect
  void removeUnusedBoycotts(std::chrono::system_clock::time_point current_time);

  // defers to Processor::postRequest
  virtual int postRequest(std::unique_ptr<Request>& rq);

  // set of boycotts that have been reported through gossip
  std::unordered_map<node_index_t, Boycott> reported_boycotts_;

  std::unordered_map<node_index_t, Boycott> boycotted_nodes_;
  std::vector<Boycott> boycotts_by_this_node_{};

  folly::Synchronized<std::vector<NodeID>> local_outliers_;
  folly::Synchronized<std::unordered_set<node_index_t>> local_resets_;
};
}} // namespace facebook::logdevice
