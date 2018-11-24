// Copyright (C) 2018 go-nebulas authors
//
// This file is part of the go-nebulas library.
//
// the go-nebulas library is free software: you can redistribute it and/or
// modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// the go-nebulas library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the go-nebulas library.  If not, see
// <http://www.gnu.org/licenses/>.
//
#pragma once
#include "common/common.h"
#include "runtime/nr/graph/graph.h"

namespace neb {
namespace rt {

struct in_out_val_t {
  double m_in_val;
  double m_out_val;
};

struct in_out_degree_t {
  uint32_t m_in_degree;
  uint32_t m_out_degree;
};

class graph_algo {
public:
  static void remove_cycles_based_on_time_sequence(
      transaction_graph::internal_graph_t &graph);

  static void merge_edges_with_same_from_and_same_to(
      transaction_graph::internal_graph_t &graph);

  static transaction_graph_ptr_t
  merge_graphs(const std::vector<transaction_graph_ptr_t> &graphs);

  static void merge_topk_edges_with_same_from_and_same_to(
      transaction_graph::internal_graph_t &graph, uint32_t k = 3);

  static auto get_in_out_vals(const transaction_graph::internal_graph_t &graph)
      -> std::shared_ptr<std::unordered_map<address_t, in_out_val_t>>;

  static auto get_stakes(const transaction_graph::internal_graph_t &graph)
      -> std::shared_ptr<std::unordered_map<address_t, double>>;

  static auto
  get_in_out_degrees(const transaction_graph::internal_graph_t &graph)
      -> std::shared_ptr<std::unordered_map<address_t, in_out_degree_t>>;

  static auto get_degree_sum(const transaction_graph::internal_graph_t &graph)
      -> std::shared_ptr<std::unordered_map<address_t, uint32_t>>;

private:
  static void dfs_find_a_cycle_from_vertex_based_on_time_sequence(
      const transaction_graph::vertex_descriptor_t &start_vertex,
      const transaction_graph::vertex_descriptor_t &v,
      const transaction_graph::internal_graph_t &graph,
      std::set<transaction_graph::vertex_descriptor_t> &visited,
      std::vector<transaction_graph::edge_descriptor_t> &edges, bool &has_cycle,
      std::vector<transaction_graph::edge_descriptor_t> &ret);

  static auto find_a_cycle_from_vertex_based_on_time_sequence(
      const transaction_graph::vertex_descriptor_t &v,
      const transaction_graph::internal_graph_t &graph)
      -> std::vector<transaction_graph::edge_descriptor_t>;

  static auto find_a_cycle_based_on_time_sequence(
      const transaction_graph::internal_graph_t &graph)
      -> std::vector<transaction_graph::edge_descriptor_t>;

  static transaction_graph_ptr_t
  merge_two_graphs(transaction_graph_ptr_t tg,
                   const transaction_graph_ptr_t sg);
};
} // namespace rt
} // namespace neb
