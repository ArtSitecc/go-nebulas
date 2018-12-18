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

#include "runtime/nr/impl/nebulas_rank.h"
#include "common/util/conversion.h"
#include <boost/foreach.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <chrono>
#include <ff/ff.h>
#include <thread>

namespace neb {
namespace rt {

namespace nr {

std::shared_ptr<std::vector<std::vector<neb::fs::transaction_info_t>>>
nebulas_rank::split_transactions_by_block_interval(
    const std::vector<neb::fs::transaction_info_t> &txs,
    int32_t block_interval) {

  std::vector<std::vector<neb::fs::transaction_info_t>> ret;

  if (block_interval < 1 || txs.empty()) {
    return std::make_shared<
        std::vector<std::vector<neb::fs::transaction_info_t>>>(ret);
  }

  auto it = txs.begin();
  block_height_t block_first = it->m_height;
  it = txs.end();
  it--;
  block_height_t block_last = it->m_height;

  std::vector<neb::fs::transaction_info_t> v;
  it = txs.begin();
  block_height_t b = block_first;
  while (b <= block_last) {
    block_height_t h = it->m_height;
    if (h < b + block_interval) {
      v.push_back(*it++);
    } else {
      ret.push_back(v);
      v.clear();
      b += block_interval;
    }
    if (it == txs.end()) {
      ret.push_back(v);
      break;
    }
  }
  return std::make_shared<
      std::vector<std::vector<neb::fs::transaction_info_t>>>(ret);
}

void nebulas_rank::filter_empty_transactions_this_interval(
    std::vector<std::vector<neb::fs::transaction_info_t>> &txs) {
  for (auto it = txs.begin(); it != txs.end();) {
    if (it->empty()) {
      it = txs.erase(it);
    } else {
      it++;
    }
  }
}

transaction_graph_ptr_t nebulas_rank::build_graph_from_transactions(
    const std::vector<neb::fs::transaction_info_t> &trans) {
  transaction_graph_ptr_t ret = std::make_shared<neb::rt::transaction_graph>();

  for (auto ite = trans.begin(); ite != trans.end(); ite++) {
    std::string from = ite->m_from;
    std::string to = ite->m_to;
    wei_t value = ite->m_tx_value;
    int64_t timestamp = ite->m_timestamp;
    ret->add_edge(from, to, value, timestamp);
  }
  return ret;
}

std::vector<transaction_graph_ptr_t> nebulas_rank::build_transaction_graphs(
    const std::vector<std::vector<neb::fs::transaction_info_t>> &txs) {
  std::vector<transaction_graph_ptr_t> tgs;

  for (auto it = txs.begin(); it != txs.end(); it++) {
    auto p = build_graph_from_transactions(*it);
    tgs.push_back(p);
  }
  return tgs;
}

block_height_t nebulas_rank::get_max_height_this_block_interval(
    const std::vector<neb::fs::transaction_info_t> &txs) {
  if (txs.size() > 0) {
    return txs[txs.size() - 1].m_height;
  }
  return 0;
}

std::shared_ptr<std::unordered_set<std::string>>
nebulas_rank::get_normal_accounts(
    const std::vector<neb::fs::transaction_info_t> &txs) {

  std::unordered_set<std::string> ret;

  for (auto it = txs.begin(); it != txs.end(); it++) {
    std::string from = it->m_from;
    ret.insert(from);

    std::string to = it->m_to;
    ret.insert(to);
  }
  return std::make_shared<std::unordered_set<std::string>>(ret);
}

std::shared_ptr<std::unordered_map<std::string, floatxx_t>>
nebulas_rank::get_account_balance_median(
    const std::unordered_set<std::string> &accounts,
    const std::vector<std::vector<neb::fs::transaction_info_t>> &txs,
    const account_db_ptr_t db_ptr,
    std::unordered_map<address_t, wei_t> &addr_balance) {

  std::unordered_map<std::string, floatxx_t> ret;
  std::unordered_map<std::string, std::vector<wei_t>> addr_balance_v;

  for (auto it = txs.begin(); it != txs.end(); it++) {
    block_height_t max_height_this_interval =
        get_max_height_this_block_interval(*it);
    for (auto ite = accounts.begin(); ite != accounts.end(); ite++) {
      std::string addr = *ite;
      wei_t balance =
          db_ptr->get_account_balance_internal(addr, max_height_this_interval);
      addr_balance_v[addr].push_back(balance);
    }
  }

  for (auto it = addr_balance_v.begin(); it != addr_balance_v.end(); it++) {
    std::vector<wei_t> v = it->second;
    sort(v.begin(), v.end());
    size_t v_len = v.size();
    floatxx_t median = int128_conversion(v[v_len >> 1]).to_float<floatxx_t>();
    if ((v_len & 0x1) == 0) {
      median = (median +
                int128_conversion(v[(v_len >> 1) - 1]).to_float<floatxx_t>()) /
               2;
    }

    floatxx_t normalized_median = db_ptr->get_normalized_value(median);
    ret.insert(std::make_pair(it->first,
                              neb::math::max(floatxx_t(0), normalized_median)));
  }

  return std::make_shared<std::unordered_map<std::string, floatxx_t>>(ret);
}

floatxx_t nebulas_rank::f_account_weight(floatxx_t in_val, floatxx_t out_val) {
  floatxx_t pi = math::constants<floatxx_t>::pi();
  floatxx_t atan_val = (in_val == 0 ? pi / 2 : math::arctan(out_val / in_val));
  return (in_val + out_val) * math::exp((-2) * math::sin(pi / 4.0 - atan_val) *
                                        math::sin(pi / 4.0 - atan_val));
}

std::shared_ptr<std::unordered_map<std::string, floatxx_t>>
nebulas_rank::get_account_weight(
    const std::unordered_map<std::string, neb::rt::in_out_val_t> &in_out_vals,
    const account_db_ptr_t db_ptr) {

  std::unordered_map<std::string, floatxx_t> ret;

  for (auto it = in_out_vals.begin(); it != in_out_vals.end(); it++) {
    wei_t in_val = it->second.m_in_val;
    wei_t out_val = it->second.m_out_val;

    floatxx_t normalized_in_val = db_ptr->get_normalized_value(
        int128_conversion(in_val).to_float<floatxx_t>());
    floatxx_t normalized_out_val = db_ptr->get_normalized_value(
        int128_conversion(out_val).to_float<floatxx_t>());
    ret.insert(std::make_pair(
        it->first, f_account_weight(normalized_in_val, normalized_out_val)));
  }
  return std::make_shared<std::unordered_map<std::string, floatxx_t>>(ret);
}

floatxx_t nebulas_rank::f_account_rank(floatxx_t a, floatxx_t b, floatxx_t c,
                                       floatxx_t d, int64_t mu, int64_t lambda,
                                       floatxx_t S, floatxx_t R) {

  auto ret =
      math::pow(S * a / (S + b), mu) * math::pow(R * c / (R + d), lambda);
  return ret;
}

std::shared_ptr<std::unordered_map<std::string, floatxx_t>>
nebulas_rank::get_account_rank(
    const std::unordered_map<std::string, floatxx_t> &account_median,
    const std::unordered_map<std::string, floatxx_t> &account_weight,
    const rank_params_t &rp) {

  std::unordered_map<std::string, floatxx_t> ret;

  for (auto it_m = account_median.begin(); it_m != account_median.end();
       it_m++) {
    auto it_w = account_weight.find(it_m->first);
    if (it_w != account_weight.end()) {
      floatxx_t rank_val =
          f_account_rank(rp.m_a, rp.m_b, rp.m_c, rp.m_d, rp.m_mu, rp.m_lambda,
                         it_m->second, it_w->second);
      ret.insert(std::make_pair(it_m->first, rank_val));
    }
  }

  // parallel run
  // std::mutex __l;
  // typedef std::tuple<std::string, floatxx_t, floatxx_t> item_t;
  // std::vector<item_t> vcs;

  // for (auto it_m = account_median.begin(); it_m != account_median.end();
  // it_m++) {
  // auto it_w = account_weight.find(it_m->first);
  // if (it_w != account_weight.end()) {
  // vcs.push_back(std::make_tuple(it_m->first, it_m->second, it_w->second));
  //}
  //}
  // ff::paragroup pg;
  // typedef std::unordered_map<std::string, floatxx_t> ret_type_t;
  // ff::thread_local_var<ret_type_t> thread_ret;
  // pg.for_each(
  // vcs.begin(), vcs.end(), [&rp, &thread_ret](item_t it) {
  //// thread_local static std::unordered_map<std::string, floatxx_t>
  //// local_ret;
  // floatxx_t rank_val =
  // f_account_rank(rp.m_a, rp.m_b, rp.m_c, rp.m_d, rp.m_mu, rp.m_lambda,
  // std::get<1>(it), std::get<2>(it));

  //// local_ret.insert(std::make_pair(std::get<0>(it), rank_val));
  ////__l.lock();
  // thread_ret.current().insert(std::make_pair(std::get<0>(it), rank_val));
  ////__l.unlock();
  //});
  // ff::ff_wait(ff::all(pg));
  // thread_ret.for_each([&ret](ret_type_t local_ret) {
  // for (auto it : local_ret) {
  // ret.insert(it);
  //}
  //});

  // ff::paracontainer pc;
  // for (auto it_m = account_median.begin(); it_m != account_median.end();
  // it_m++) {
  // auto it_w = account_weight.find(it_m->first);
  // if (it_w != account_weight.end()) {
  // ff::para<> p;
  // p([&rp, &__l, it_m, it_w, &ret]() {
  // floatxx_t rank_val =
  // f_account_rank(rp.m_a, rp.m_b, rp.m_c, rp.m_d, rp.m_mu, rp.m_lambda,
  // it_m->second, it_w->second);
  //__l.lock();
  // ret.insert(std::make_pair(it_m->first, rank_val));
  //__l.unlock();
  //});
  // pc.add(p);
  //}
  //}
  // ff::ff_wait(ff::all(pc));
  return std::make_shared<std::unordered_map<std::string, floatxx_t>>(ret);
}

std::unique_ptr<std::vector<nr_info_t>> nebulas_rank::get_nr_score(
    const transaction_db_ptr_t &tdb_ptr, const account_db_ptr_t &adb_ptr,
    const rank_params_t &rp, neb::block_height_t start_block,
    neb::block_height_t end_block) {

  auto start_time = std::chrono::high_resolution_clock::now();
  auto it_txs =
      tdb_ptr->read_transactions_from_db_with_duration(start_block, end_block);
  auto txs = *it_txs;

  // account inter transactions
  auto it_account_inter_txs =
      neb::fs::transaction_db::read_transactions_with_address_type(txs, 0x57,
                                                                   0x57);
  auto account_inter_txs = *it_account_inter_txs;
  LOG(INFO) << "account to account: " << account_inter_txs.size();

  // graph
  auto it_txs_v = split_transactions_by_block_interval(account_inter_txs);
  auto txs_v = *it_txs_v;
  LOG(INFO) << "split by block interval: " << txs_v.size();

  neb::rt::nr::transaction_graph_ptr_t tg =
      std::make_shared<neb::rt::transaction_graph>();

  filter_empty_transactions_this_interval(txs_v);
  std::vector<neb::rt::nr::transaction_graph_ptr_t> tgs =
      build_transaction_graphs(txs_v);
  if (tgs.empty()) {
    return std::make_unique<std::vector<nr_info_t>>();
  }
  LOG(INFO) << "we have " << tgs.size() << " subgraphs.";
  for (auto it = tgs.begin(); it != tgs.end(); it++) {
    neb::rt::nr::transaction_graph_ptr_t ptr = *it;
    neb::rt::graph_algo::remove_cycles_based_on_time_sequence(
        ptr->internal_graph());
    neb::rt::graph_algo::merge_edges_with_same_from_and_same_to(
        ptr->internal_graph());
  }
  LOG(INFO) << "done with remove cycle.";

  tg = neb::rt::graph_algo::merge_graphs(tgs);
  neb::rt::graph_algo::merge_topk_edges_with_same_from_and_same_to(
      tg->internal_graph());
  LOG(INFO) << "done with merge graphs.";

  // median
  auto it_accounts = get_normal_accounts(account_inter_txs);
  auto accounts = *it_accounts;
  LOG(INFO) << "account size: " << accounts.size();

  std::unordered_map<neb::address_t, neb::wei_t> addr_balance;
  for (auto &acc : accounts) {
    auto balance = adb_ptr->get_balance(acc, start_block);
    addr_balance.insert(std::make_pair(acc, balance));
  }
  adb_ptr->set_height_address_val_internal(txs, addr_balance);
  LOG(INFO) << "done with set height address";

  auto it_account_median =
      get_account_balance_median(accounts, txs_v, adb_ptr, addr_balance);
  auto account_median = *it_account_median;
  LOG(INFO) << "done with get account balance median";

  // degree and in_out amount
  auto it_in_out_degrees =
      neb::rt::graph_algo::get_in_out_degrees(tg->internal_graph());
  auto in_out_degrees = *it_in_out_degrees;
  auto it_degrees = neb::rt::graph_algo::get_degree_sum(tg->internal_graph());
  auto degrees = *it_degrees;
  auto it_in_out_vals =
      neb::rt::graph_algo::get_in_out_vals(tg->internal_graph());
  auto in_out_vals = *it_in_out_vals;
  auto it_stakes = neb::rt::graph_algo::get_stakes(tg->internal_graph());
  auto stakes = *it_stakes;
  LOG(INFO) << "done with get stakes";

  // weight and rank
  auto it_account_weight = get_account_weight(in_out_vals, adb_ptr);
  auto account_weight = *it_account_weight;
  LOG(INFO) << "done with get account weight";
  auto it_account_rank = get_account_rank(account_median, account_weight, rp);
  auto account_rank = *it_account_rank;
  LOG(INFO) << "account rank size: " << account_rank.size();

  std::vector<nr_info_t> infos;
  for (auto it = accounts.begin(); it != accounts.end(); it++) {
    std::string addr = *it;
    if (account_median.find(addr) == account_median.end() ||
        account_rank.find(addr) == account_rank.end() ||
        in_out_degrees.find(addr) == in_out_degrees.end() ||
        in_out_vals.find(addr) == in_out_vals.end() ||
        stakes.find(addr) == stakes.end()) {
      continue;
    }

    neb::floatxx_t nas_in_val = adb_ptr->get_normalized_value(
        neb::int128_conversion(in_out_vals.find(addr)->second.m_in_val)
            .to_float<neb::floatxx_t>());
    neb::floatxx_t nas_out_val = adb_ptr->get_normalized_value(
        neb::int128_conversion(in_out_vals.find(addr)->second.m_out_val)
            .to_float<neb::floatxx_t>());
    neb::floatxx_t nas_stake = adb_ptr->get_normalized_value(
        neb::int128_conversion(stakes.find(addr)->second)
            .to_float<neb::floatxx_t>());

    nr_info_t info{addr,
                   in_out_degrees[addr].m_in_degree,
                   in_out_degrees[addr].m_out_degree,
                   degrees[addr],
                   nas_in_val,
                   nas_out_val,
                   nas_stake,
                   account_median[addr],
                   account_weight[addr],
                   account_rank[addr]};
    infos.push_back(info);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  LOG(INFO) << "time spend: "
            << std::chrono::duration_cast<std::chrono::seconds>(end_time -
                                                                start_time)
                   .count()
            << " seconds";
  return std::make_unique<std::vector<nr_info_t>>(infos);
}

void nebulas_rank::convert_nr_info_to_ptree(const nr_info_t &info,
                                            boost::property_tree::ptree &p) {

  neb::util::bytes addr_bytes = neb::util::string_to_byte(info.m_address);

  uint32_t in_degree = info.m_in_degree;
  uint32_t out_degree = info.m_out_degree;
  uint32_t degrees = info.m_degrees;

  floatxx_t f_in_val = info.m_in_val;
  floatxx_t f_out_val = info.m_out_val;
  floatxx_t f_in_outs = info.m_in_outs;

  floatxx_t f_median = info.m_median;
  floatxx_t f_weight = info.m_weight;
  floatxx_t f_nr_score = info.m_nr_score;

  std::vector<std::pair<std::string, std::string>> kv_pair(
      {{"address", addr_bytes.to_base58()},
       {"in_degree", neb::math::to_string(in_degree)},
       {"out_degree", neb::math::to_string(out_degree)},
       {"degrees", neb::math::to_string(degrees)},
       {"in_val", neb::math::to_string(f_in_val)},
       {"out_val", neb::math::to_string(f_out_val)},
       {"in_outs", neb::math::to_string(f_in_outs)},
       {"median", neb::math::to_string(f_median)},
       {"weight", neb::math::to_string(f_weight)},
       {"score", neb::math::to_string(f_nr_score)}});

  for (auto &ele : kv_pair) {
    p.put(ele.first, ele.second);
  }
}

std::string nebulas_rank::nr_info_to_json(const std::vector<nr_info_t> &rs) {

  if (rs.empty()) {
    return std::string("{\"nrs\":[]}");
  }

  boost::property_tree::ptree root;
  boost::property_tree::ptree arr;

  for (auto it = rs.begin(); it != rs.end(); it++) {
    const neb::rt::nr::nr_info_t &info = *it;
    boost::property_tree::ptree p;
    convert_nr_info_to_ptree(info, p);
    arr.push_back(std::make_pair(std::string(), p));
  }
  root.add_child("nrs", arr);

  std::stringstream ss;
  boost::property_tree::json_parser::write_json(ss, root, false);
  return ss.str();
}

std::unique_ptr<std::vector<nr_info_t>>
nebulas_rank::json_to_nr_info(const std::string &nr_result) {

  boost::property_tree::ptree pt;
  std::stringstream ss(nr_result);
  boost::property_tree::json_parser::read_json(ss, pt);

  boost::property_tree::ptree nrs = pt.get_child("nrs");
  std::vector<nr_info_t> infos;

  BOOST_FOREACH (boost::property_tree::ptree::value_type &v, nrs) {
    boost::property_tree::ptree nr = v.second;
    nr_info_t info;
    neb::util::bytes addr_bytes =
        neb::util::bytes::from_base58(nr.get<std::string>("address"));
    info.m_address = neb::util::byte_to_string(addr_bytes);

    info.m_in_degree = nr.get<uint32_t>("in_degree");
    info.m_out_degree = nr.get<uint32_t>("out_degree");
    info.m_degrees = nr.get<uint32_t>("degrees");

    info.m_in_val =
        neb::math::from_string<floatxx_t>(nr.get<std::string>("in_val"));
    info.m_out_val =
        neb::math::from_string<floatxx_t>(nr.get<std::string>("out_val"));
    info.m_in_outs =
        neb::math::from_string<floatxx_t>(nr.get<std::string>("in_outs"));

    info.m_median =
        neb::math::from_string<floatxx_t>(nr.get<std::string>("median"));
    info.m_weight =
        neb::math::from_string<floatxx_t>(nr.get<std::string>("weight"));
    info.m_nr_score =
        neb::math::from_string<floatxx_t>(nr.get<std::string>("score"));
    infos.push_back(info);
  }

  return std::make_unique<std::vector<nr_info_t>>(infos);
}

} // namespace nr
} // namespace rt
} // namespace neb
