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

#include "runtime/dip/dip_impl.h"
#include "common/address.h"
#include "common/common.h"
#include "common/configuration.h"
#include "runtime/dip/dip_reward.h"
#include "runtime/nr/impl/nebulas_rank.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace neb {
namespace rt {
namespace dip {

std::string entry_point_dip_impl(compatible_uint64_t start_block,
                                 compatible_uint64_t end_block,
                                 version_t version, compatible_uint64_t height,
                                 const std::string &nr_result,
                                 dip_float_t alpha, dip_float_t beta) {

  std::string neb_db_path = neb::configuration::instance().neb_db_dir();
  neb::fs::blockchain bc(neb_db_path);
  neb::fs::blockchain_api ba(&bc);
  nr::transaction_db_ptr_t tdb_ptr =
      std::make_unique<neb::fs::transaction_db>(&ba);
  nr::account_db_ptr_t adb_ptr = std::make_unique<neb::fs::account_db>(&ba);

  auto ret = dip_reward::get_dip_reward(
      start_block, end_block, height, nr_result, tdb_ptr, adb_ptr, alpha, beta);
  LOG(INFO) << "get dip reward resurned";
  std::vector<std::pair<std::string, uint64_t>> meta_info(
      {{"start_height", start_block},
       {"end_height", end_block},
       {"version", version}});
  LOG(INFO) << "meta info attached";

  return dip_reward::dip_info_to_json(*ret, meta_info);
}

void init_dip_params(compatible_uint64_t dip_start_block,
                     compatible_uint64_t dip_block_interval,
                     const std::string &dip_reward_addr,
                     const std::string &coinbase_addr) {
  neb::configuration::instance().dip_start_block() = dip_start_block;
  neb::configuration::instance().dip_block_interval() = dip_block_interval;

  neb::configuration::instance().dip_reward_addr() =
      base58_to_address(dip_reward_addr);
  neb::configuration::instance().coinbase_addr() =
      base58_to_address(coinbase_addr);

  LOG(INFO) << "init dip params, dip_start_block " << dip_start_block
            << ", dip_block_interval " << dip_block_interval
            << ", dip_reward_addr " << dip_reward_addr << ", coinbase_addr "
            << coinbase_addr;
}

std::string dip_param_list(compatible_uint64_t dip_start_block,
                           compatible_uint64_t dip_block_interval,
                           const std::string &dip_reward_addr,
                           const std::string &coinbase_addr) {
  boost::property_tree::ptree pt;
  pt.put("start_block", dip_start_block);
  pt.put("block_interval", dip_block_interval);
  pt.put("reward_addr", dip_reward_addr);
  pt.put("coinbase_addr", coinbase_addr);

  std::stringstream ss;
  boost::property_tree::json_parser::write_json(ss, pt, false);
  std::string json_str = ss.str();
  return json_str;
}
} // namespace dip
} // namespace rt
} // namespace neb
