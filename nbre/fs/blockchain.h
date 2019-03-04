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
#include "common/common.h"
#include "fs/proto/block.pb.h"
#include "fs/rocksdb_storage.h"

namespace neb {
namespace fs {

class blockchain {
public:
  static constexpr char const *Block_LIB = "blockchain_lib";
  static constexpr char const *Block_Tail = "blockchain_tail";

  blockchain(const std::string &path,
             enum storage_open_flag open_flag = storage_open_default);
  ~blockchain();
  blockchain(const blockchain &bc) = delete;
  blockchain &operator=(const blockchain &bc) = delete;

  std::unique_ptr<corepb::Block> load_LIB_block();
  std::unique_ptr<corepb::Block> load_block_with_height(block_height_t height);

  // inline rocksdb_storage *storage_ptr() { return m_storage.get(); }

  void write_LIB_block(corepb::Block *block);

private:
  std::unique_ptr<corepb::Block>
  load_block_with_tag_string(const std::string &tag);

private:
  // std::unique_ptr<rocksdb_storage> m_storage;
  std::string m_path;
  enum storage_open_flag m_open_flag;
}; // end class blockchain
} // end namespace fs
} // end namespace neb
