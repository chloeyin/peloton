/**
 * @brief Header file for abstract executor.
 *
 * Copyright(c) 2015, CMU
 */

#pragma once

#include "executor/logical_tile.h"
#include "planner/abstract_plan_node.h"

#include <memory>
#include <vector>

namespace nstore {
namespace executor {

class AbstractExecutor {
 public:
  bool Init();

  LogicalTile *GetNextTile();

  void CleanUp();

 protected:
  AbstractExecutor(
      std::unique_ptr<planner::AbstractPlanNode> abstract_node,
      std::vector<AbstractExecutor *>& children);

  /** @brief Init function to be overriden by subclass. */
  virtual bool SubInit() = 0;

  /** @brief Workhorse function to be overriden by subclass. */
  virtual LogicalTile *SubGetNextTile() = 0;

  /** @brief Clean up function to be overriden by subclass. */
  virtual void SubCleanUp() = 0;

  /** @brief Plan node that corresponds to this executor. */
  std::unique_ptr<planner::AbstractPlanNode> abstract_node_;

  /** @brief Children nodes of this executor in the executor tree. */
  std::vector<AbstractExecutor *> children_;
};

} // namespace executor
} // namespace nstore
