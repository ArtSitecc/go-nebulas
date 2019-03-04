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

#include "fs/blockchain.h"
#include "common/util/byte.h"
#include "fs/bc_storage_session.h"

namespace neb {
namespace fs {

blockchain::blockchain(const std::string &path,
                       enum storage_open_flag open_flag)
    : m_path(path), m_open_flag(open_flag) {
  bc_storage_session::instance().init(path, open_flag);
}

blockchain::~blockchain() {
}

std::unique_ptr<corepb::Block> blockchain::load_LIB_block() {
  return load_block_with_tag_string(
      std::string(Block_LIB, std::allocator<char>()));
}

std::unique_ptr<corepb::Block>
blockchain::load_block_with_height(block_height_t height) {
  std::unique_ptr<corepb::Block> block = std::make_unique<corepb::Block>();

  neb::util::bytes height_hash = bc_storage_session::instance().get_bytes(
      neb::util::number_to_byte<neb::util::bytes>(height));

  neb::util::bytes block_bytes =
      bc_storage_session::instance().get_bytes(height_hash);

  bool ret = block->ParseFromArray(block_bytes.value(), block_bytes.size());
  if (!ret) {
    throw std::runtime_error("parse block failed");
  }
  return block;
}

std::unique_ptr<corepb::Block>
blockchain::load_block_with_tag_string(const std::string &tag) {

  std::unique_ptr<corepb::Block> block = std::make_unique<corepb::Block>();
  neb::util::bytes tail_hash =
      bc_storage_session::instance().get_bytes(neb::util::string_to_byte(tag));

  neb::util::bytes block_bytes =
      bc_storage_session::instance().get_bytes(tail_hash);

  bool ret = block->ParseFromArray(block_bytes.value(), block_bytes.size());
  if (!ret) {
    throw std::runtime_error("parse block failed");
  }
  return block;
}

void blockchain::write_LIB_block(corepb::Block *block) {
  if (block == nullptr)
    return;

  std::string key_str = std::string(Block_LIB, std::allocator<char>());
  auto size = block->ByteSizeLong();
  neb::util::bytes val(size);
  block->SerializeToArray(val.value(), size);
  bc_storage_session::instance().put_bytes(util::string_to_byte(key_str), val);
}
} // namespace fs
} // namespace neb

