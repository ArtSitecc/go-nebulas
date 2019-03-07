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
#include "cmd/dummy_neb/dummy_common.h"

class callback_handler : public neb::util::singleton<callback_handler> {
public:
  callback_handler();

  template <typename Func> void add_nr_handler(uint64_t holder, Func &&f) {
    std::unique_lock<std::mutex> _l(m_mutex);
    m_nr_handlers.insert(std::make_pair(holder, f));
  }
  void handle_nr(void *holder, const char *nr_handler_id);
  void handle_nr_result(void *holder, const char *nr_result);

  template <typename Func>
  void add_nr_result_handler(uint64_t holder, Func &&f) {
    std::unique_lock<std::mutex> _l(m_mutex);
    m_nr_result_handlers.insert(std::make_pair(holder, f));
  }

protected:
  template <typename Handlers, typename... ARGS>
  void handle(Handlers &hs, void *holder, ARGS... args) {
    std::unique_lock<std::mutex> _l(m_mutex);
    auto h = reinterpret_cast<uint64_t>(holder);
    auto it = hs.find(h);
    if (it == hs.end())
      return;
    it->second(h, args...);
    hs.erase(it);
  }

protected:
  std::mutex m_mutex;
  std::unordered_map<uint64_t, std::function<void(uint64_t, const char *)>>
      m_nr_handlers;
  std::unordered_map<uint64_t, std::function<void(uint64_t, const char *)>>
      m_nr_result_handlers;
};

void nbre_version_callback(ipc_status_code isc, void *handler, uint32_t major,
                           uint32_t minor, uint32_t patch);

void nbre_ir_list_callback(ipc_status_code isc, void *handler,
                           const char *ir_name_list);
void nbre_ir_versions_callback(ipc_status_code isc, void *handler,
                               const char *ir_versions);

void nbre_nr_handle_callback(ipc_status_code isc, void *holder,
                             const char *nr_handle_id);

void nbre_nr_result_callback(ipc_status_code isc, void *holder,
                             const char *nr_result);

void nbre_dip_reward_callback(ipc_status_code isc, void *holder,
                              const char *dip_reward);
