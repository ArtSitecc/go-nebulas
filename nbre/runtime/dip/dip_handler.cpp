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

#include "runtime/dip/dip_handler.h"
#include "fs/proto/ir.pb.h"
#include "jit/jit_driver.h"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <ff/ff.h>

namespace neb {
namespace rt {
namespace dip {

void dip_handler::start(neb::block_height_t nbre_max_height,
                        neb::block_height_t lib_height) {
  std::unique_lock<std::mutex> _l(m_sync_mutex);
  if (nbre_max_height < dip_start_block + dip_block_interval - 1) {
    return;
  }

  assert(nbre_max_height <= lib_height);
  if (nbre_max_height + dip_block_interval < lib_height) {
    return;
  }

  uint64_t height = (nbre_max_height - dip_start_block + 1) /
                    dip_block_interval * dip_block_interval;

  if (m_dip_reward.find(height) != m_dip_reward.end()) {
    return;
  }

  ff::para<> p;
  p([this, height]() {
    try {
      std::string dip_name = "dip";
      jit_driver &jd = jit_driver::instance();
      auto dip_reward = jd.run_ir<std::string>(
          dip_name, height, "_Z15entry_point_dipB5cxx11m", height);

      m_dip_reward.insert(std::make_pair(height, dip_reward));
    } catch (const std::exception &e) {
      LOG(INFO) << e.what();
    }
  });
}

std::string dip_handler::get_dip_reward(neb::block_height_t height) {
  std::unique_lock<std::mutex> _l(m_sync_mutex);

  auto dip_reward = m_dip_reward.find(height);
  if (dip_reward == m_dip_reward.end()) {
    return std::string("{\"err\":\"not complete yet\"}");
  }
  return dip_reward->second;
}
} // namespace dip
} // namespace rt
} // namespace neb
