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

#include "common/address.h"
#include "runtime/nr/impl/nebulas_rank.h"

namespace neb {
namespace rt {
namespace dip {

struct dip_info_t {
  std::string m_deployer;
  std::string m_contract;
  std::string m_reward;
};

class dip_reward {
public:
  static auto
  get_dip_reward(neb::block_height_t start_block, neb::block_height_t end_block,
                 neb::block_height_t height, const std::string &nr_result,
                 const neb::rt::nr::transaction_db_ptr_t &tdb_ptr,
                 const neb::rt::nr::account_db_ptr_t &adb_ptr, floatxx_t alpha,
                 floatxx_t beta) -> std::unique_ptr<std::vector<dip_info_t>>;

  static std::string dip_info_to_json(
      const std::vector<dip_info_t> &dip_infos,
      const std::vector<std::pair<std::string, uint64_t>> &meta = {});

  static auto json_to_dip_info(const std::string &dip_reward)
      -> std::unique_ptr<std::vector<dip_info_t>>;

private:
  static auto account_call_contract_count(
      const std::vector<neb::fs::transaction_info_t> &txs)
      -> std::unique_ptr<std::unordered_map<
          address_t, std::unordered_map<address_t, uint32_t>>>;

  static auto
  account_to_contract_votes(const std::vector<neb::fs::transaction_info_t> &txs,
                            const std::vector<neb::rt::nr::nr_info_t> &nr_infos)
      -> std::unique_ptr<std::unordered_map<
          address_t, std::unordered_map<address_t, floatxx_t>>>;

  static auto
  dapp_votes(const std::unordered_map<address_t,
                                      std::unordered_map<address_t, floatxx_t>>
                 &acc_contract_votes)
      -> std::unique_ptr<std::unordered_map<address_t, floatxx_t>>;

  static floatxx_t
  participate_lambda(floatxx_t alpha, floatxx_t beta,
                     const std::vector<neb::fs::transaction_info_t> &txs,
                     const std::vector<neb::rt::nr::nr_info_t> &nr_infos);

  static void
  full_fill_meta_info(const std::vector<std::pair<std::string, uint64_t>> &meta,
                      boost::property_tree::ptree &root);

  static void back_to_coinbase(std::vector<dip_info_t> &dip_infos,
                               floatxx_t reward_left,
                               const address_t &coinbase_addr);

  static void ignore_account_transfer_contract(
      std::vector<neb::fs::transaction_info_t> &txs,
      const std::string &tx_type);
};

} // namespace dip
} // namespace rt
} // namespace neb
