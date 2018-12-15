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

#include "fs/blockchain/account/account_db.h"
#include "fs/blockchain/transaction/transaction_db.h"
#include "runtime/nr/graph/algo.h"
#include <boost/property_tree/ptree.hpp>

namespace neb {
namespace rt {

namespace nr {

struct nr_info_t {
  std::string m_address;
  uint32_t m_in_degree;
  uint32_t m_out_degree;
  uint32_t m_degrees;
  floatxx_t m_in_val;
  floatxx_t m_out_val;
  floatxx_t m_in_outs;
  floatxx_t m_median;
  floatxx_t m_weight;
  floatxx_t m_nr_score;
};

struct rank_params_t {
  floatxx_t m_a;
  floatxx_t m_b;
  floatxx_t m_c;
  floatxx_t m_d;
  int64_t m_mu;
  int64_t m_lambda;
};

using uintxx_t = uint64_t;
using transaction_db_ptr_t = std::shared_ptr<neb::fs::transaction_db>;
using account_db_ptr_t = std::shared_ptr<neb::fs::account_db>;
using transaction_graph_ptr_t = std::shared_ptr<neb::rt::transaction_graph>;

class nebulas_rank {
public:
  static auto
  get_nr_score(const transaction_db_ptr_t &tdb_ptr,
               const account_db_ptr_t &adb_ptr, const rank_params_t &rp,
               neb::block_height_t start_block, neb::block_height_t end_block)
      -> std::unique_ptr<std::vector<nr_info_t>>;

  static std::string nr_info_to_json(const std::vector<nr_info_t> &nr_infos);

  static auto json_to_nr_info(const std::string &nr_result)
      -> std::unique_ptr<std::vector<nr_info_t>>;

private:
  static auto split_transactions_by_block_interval(
      const std::vector<neb::fs::transaction_info_t> &txs,
      int32_t block_interval = 128)
      -> std::shared_ptr<std::vector<std::vector<neb::fs::transaction_info_t>>>;

  static void filter_empty_transactions_this_interval(
      std::vector<std::vector<neb::fs::transaction_info_t>> &txs);

  static auto build_transaction_graphs(
      const std::vector<std::vector<neb::fs::transaction_info_t>> &txs)
      -> std::vector<transaction_graph_ptr_t>;

  static auto
  get_normal_accounts(const std::vector<neb::fs::transaction_info_t> &txs)
      -> std::shared_ptr<std::unordered_set<std::string>>;

  static auto get_account_balance_median(
      const std::unordered_set<std::string> &accounts,
      const std::vector<std::vector<neb::fs::transaction_info_t>> &txs,
      const account_db_ptr_t db_ptr,
      std::unordered_map<address_t, wei_t> &addr_balance)
      -> std::shared_ptr<std::unordered_map<std::string, floatxx_t>>;

  static auto get_account_weight(
      const std::unordered_map<std::string, neb::rt::in_out_val_t> &in_out_vals,
      const account_db_ptr_t db_ptr)
      -> std::shared_ptr<std::unordered_map<std::string, floatxx_t>>;

  static auto get_account_rank(
      const std::unordered_map<std::string, floatxx_t> &account_median,
      const std::unordered_map<std::string, floatxx_t> &account_weight,
      const rank_params_t &rp)
      -> std::shared_ptr<std::unordered_map<std::string, floatxx_t>>;

private:
  static transaction_graph_ptr_t build_graph_from_transactions(
      const std::vector<neb::fs::transaction_info_t> &trans);

  static block_height_t get_max_height_this_block_interval(
      const std::vector<neb::fs::transaction_info_t> &txs);

  static floatxx_t f_account_weight(floatxx_t in_val, floatxx_t out_val);

  static floatxx_t f_account_rank(floatxx_t a, floatxx_t b, floatxx_t c,
                                  floatxx_t d, int64_t mu, int64_t lambda,
                                  floatxx_t S, floatxx_t R);

  static void convert_nr_info_to_ptree(const nr_info_t &info,
                                       boost::property_tree::ptree &pt);

}; // class nebulas_rank
namespace internal {
template <typename T> std::string to_string(const T &val) {
  std::stringstream ss;
  ss << val;
  return ss.str();
}
} // namespace internal
} // namespace nr
} // namespace rt
} // namespace neb

