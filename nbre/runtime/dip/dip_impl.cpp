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
#include "fs/blockchain/blockchain_api_test.h"
#include "runtime/dip/dip_handler.h"
#include "runtime/dip/dip_reward.h"
#include "runtime/nr/impl/nebulas_rank.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

namespace neb {
namespace rt {
namespace dip {

dip_ret_type
entry_point_dip_impl(compatible_uint64_t start_block,
                     compatible_uint64_t end_block, version_t version,
                     compatible_uint64_t height,
                     const std::vector<std::shared_ptr<nr_info_t>> &nr_result,
                     dip_float_t alpha, dip_float_t beta) {

  std::unique_ptr<neb::fs::blockchain_api_base> pba;
  if (neb::use_test_blockchain) {
    pba = std::unique_ptr<neb::fs::blockchain_api_base>(
        new neb::fs::blockchain_api_test());
  } else {
    pba = std::unique_ptr<neb::fs::blockchain_api_base>(
        new neb::fs::blockchain_api());
  }
  nr::transaction_db_ptr_t tdb_ptr =
      std::make_unique<neb::fs::transaction_db>(pba.get());
  nr::account_db_ptr_t adb_ptr =
      std::make_unique<neb::fs::account_db>(pba.get());

  dip_ret_type ret;
  std::get<2>(ret) = dip_reward::get_dip_reward(
      start_block, end_block, height, nr_result, tdb_ptr, adb_ptr, alpha, beta);
  LOG(INFO) << "get dip reward resurned";
  std::vector<std::pair<std::string, uint64_t>> meta_info(
      {{"start_height", start_block},
       {"end_height", end_block},
       {"version", version}});
  LOG(INFO) << "meta info attached";

  return ret;
}

std::string dip_param_list(compatible_uint64_t dip_start_block,
                           compatible_uint64_t dip_block_interval,
                           const std::string &dip_reward_addr,
                           const std::string &dip_coinbase_addr) {
  dip_params_t param;
  param.set<start_block>(dip_start_block);
  param.set<block_interval>(dip_block_interval);
  param.set<reward_addr>(dip_reward_addr);
  param.set<coinbase_addr>(dip_coinbase_addr);
  LOG(INFO) << "init dip params: " << dip_start_block << ','
            << dip_block_interval << ',' << dip_reward_addr << ','
            << dip_coinbase_addr;
  return param.serialize_to_string();
}
} // namespace dip
} // namespace rt
} // namespace neb
