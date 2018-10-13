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

#include "fs/nbre_storage.h"
#include "common/util/byte.h"
#include "common/util/version.h"

namespace neb {
namespace fs {

nbre_storage::nbre_storage(const std::string &path,
                           const std::string &bc_path) {
  m_storage = std::make_unique<rocksdb_storage>();
  m_storage->open_database(path, storage_open_for_readwrite);

  m_blockchain =
      std::make_unique<blockchain>(bc_path, storage_open_for_readonly);
}

std::vector<std::shared_ptr<nbre::NBREIR>>
nbre_storage::read_nbre_by_height(const std::string &name,
                                  block_height_t height) {

  std::vector<std::shared_ptr<nbre::NBREIR>> ret;
  std::unordered_set<std::string> pkgs;

  std::shared_ptr<nbre::NBREIR> nbre_ir = std::make_shared<nbre::NBREIR>();
  neb::util::bytes bytes_versions = m_storage->get(name);

  size_t gap = sizeof(uint64_t) / sizeof(uint8_t);
  for (size_t i = 0; i < bytes_versions.size(); i += gap) {
    byte_t *bytes_version =
        bytes_versions.value() + (bytes_versions.size() - gap - i);

    if (bytes_version != nullptr) {
      uint64_t version =
          neb::util::byte_to_number<uint64_t>(bytes_version, gap);
      read_nbre_depends_recursive(name, version, height, pkgs, ret);
      if (!ret.empty()) {
        break;
      }
    }
  }
  return ret;
}

void nbre_storage::read_nbre_depends_recursive(
    const std::string &name, uint64_t version, block_height_t height,
    std::unordered_set<std::string> &pkg,
    std::vector<std::shared_ptr<nbre::NBREIR>> &irs) {

  std::shared_ptr<nbre::NBREIR> nbre_ir = std::make_shared<nbre::NBREIR>();
  std::string name_version = name + std::to_string(version);
  if (pkg.find(name_version) != pkg.end()) {
    return;
  }

  neb::util::bytes nbre_bytes = m_storage->get(name_version);
  bool ret = nbre_ir->ParseFromArray(nbre_bytes.value(), nbre_bytes.size());
  if (!ret) {
    throw std::runtime_error("parse nbre failed");
  }

  if (nbre_ir->height() <= height) {
    for (auto &dep : nbre_ir->depends()) {
      read_nbre_depends_recursive(dep.name(), dep.version(), height, pkg, irs);
    }
    irs.push_back(nbre_ir);
    pkg.insert(name_version);
  }
}

std::shared_ptr<nbre::NBREIR>
nbre_storage::read_nbre_by_name_version(const std::string &name,
                                        uint64_t version) {
  std::shared_ptr<nbre::NBREIR> nbre_ir = std::make_shared<nbre::NBREIR>();
  std::string name_version = name + std::to_string(version);

  neb::util::bytes nbre_bytes = m_storage->get(name_version);
  bool ret = nbre_ir->ParseFromArray(nbre_bytes.value(), nbre_bytes.size());
  if (!ret) {
    throw std::runtime_error("parse nbre failed");
  }

  return nbre_ir;
}

void nbre_storage::write_nbre() {
  std::shared_ptr<corepb::Block> end_block = m_blockchain->load_LIB_block();

  block_height_t start_height = 0;
  try {
    start_height = neb::util::byte_to_number<block_height_t>(
        m_storage->get(std::string(s_nbre_max_height, std::allocator<char>())));
  } catch (std::exception &e) {
    m_storage->put(std::string(s_nbre_max_height, std::allocator<char>()),
                   neb::util::number_to_byte<neb::util::bytes>(start_height));
  }

  block_height_t end_height = end_block->height();
  LOG(INFO) << "start height " << start_height << ',' << "end height "
            << end_height;

  //! TODO: we may consider parallel here!
  for (block_height_t h = start_height + 1; h <= end_height; h++) {
    LOG(INFO) << h;
    write_nbre_by_height(h);
    m_storage->put(std::string(s_nbre_max_height, std::allocator<char>()),
                   neb::util::number_to_byte<neb::util::bytes>(h));
  }
}

void nbre_storage::write_nbre_by_height(block_height_t height) {

  auto block = m_blockchain->load_block_with_height(height);
  for (auto &tx : block->transactions()) {
    auto &data = tx.data();
    const std::string &type = data.type();

    if (type == s_payload_type) {
      const std::string &payload = data.payload();
      neb::util::bytes payload_bytes = neb::util::string_to_byte(payload);

      std::shared_ptr<nbre::NBREIR> nbre_ir = std::make_shared<nbre::NBREIR>();
      bool ret =
          nbre_ir->ParseFromArray(payload_bytes.value(), payload_bytes.size());
      if (!ret) {
        throw std::runtime_error("parse transaction payload failed");
      }

      const std::string &name = nbre_ir->name();
      const uint64_t version = nbre_ir->version();

      try {
        neb::util::bytes bytes_versions = m_storage->get(name);
        bytes_versions.append_bytes(
            neb::util::number_to_byte<neb::util::bytes>(version));
        m_storage->put(name, bytes_versions);
      } catch (std::exception &e) {
        m_storage->put(name,
                       neb::util::number_to_byte<neb::util::bytes>(version));
      }

      m_storage->put(name + std::to_string(version), payload_bytes);
    }
  }
}

bool nbre_storage::is_latest_irreversible_block() {
  auto lib_block = m_blockchain->load_LIB_block();
  auto max_height_bytes =
      m_storage->get(std::string(s_nbre_max_height, std::allocator<char>()));
  return lib_block->height() ==
         neb::util::byte_to_number<neb::block_height_t>(max_height_bytes);
}
} // namespace fs
} // namespace neb
