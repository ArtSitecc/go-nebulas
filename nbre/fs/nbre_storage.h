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
#include "fs/blockchain.h"
#include "fs/proto/ir.pb.h"

namespace neb {
namespace fs {

class nbre_storage {
public:
  nbre_storage(const std::string &path, const std::string &bc_path);
  nbre_storage(const nbre_storage &ns) = delete;
  nbre_storage &operator=(const nbre_storage &ns) = delete;

  std::vector<std::shared_ptr<nbre::NBREIR>>
  read_nbre_by_height(const std::string &name, block_height_t height);

  std::shared_ptr<nbre::NBREIR>
  read_nbre_by_name_version(const std::string &name, uint64_t version);

  void write_nbre();

  bool is_latest_irreversible_block();

private:
  void
  read_nbre_depends_recursive(const std::string &name, uint64_t version,
                              block_height_t height,
                              std::unordered_set<std::string> &pkg,
                              std::vector<std::shared_ptr<nbre::NBREIR>> &irs);
  void write_nbre_by_height(
      block_height_t height,
      const std::map<std::pair<module_t, address_t>,
                     std::pair<start_block_t, end_block_t>> &auth_table);

public:
  std::shared_ptr<std::map<std::pair<module_t, address_t>,
                           std::pair<start_block_t, end_block_t>>>
  get_auth_table();

private:
  std::unique_ptr<rocksdb_storage> m_storage;
  std::unique_ptr<blockchain> m_blockchain;

  static constexpr char const *s_payload_type = "protocol";
  static constexpr char const *s_nbre_max_height = "nbre_max_height";
  static constexpr char const *s_nbre_auth_table = "nbre_auth_table";
};
} // namespace fs
} // namespace neb
