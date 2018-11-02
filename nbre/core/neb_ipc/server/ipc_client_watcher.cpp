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
#include "core/neb_ipc/server/ipc_client_watcher.h"
#include <boost/process/child.hpp>
#include <thread>

namespace neb {
namespace core {
ipc_client_watcher::ipc_client_watcher(const std::string &path)
    : m_path(path), m_restart_times(0), m_last_start_time(),
      m_b_client_alive(false) {}

void ipc_client_watcher::thread_func() {
  std::chrono::seconds threshold(8);
  while (!m_exit_flag) {
    std::chrono::system_clock::time_point now =
        std::chrono::system_clock::now();

    std::chrono::seconds duration =
        std::chrono::duration_cast<std::chrono::seconds>(now -
                                                         m_last_start_time);

    std::chrono::seconds delta = threshold - duration;
    if (delta >= std::chrono::seconds(0)) {
      std::this_thread::sleep_for(delta + std::chrono::seconds(1));
    }

    LOG(INFO) << "to start nbre ";
    m_last_start_time = now;
    boost::process::child client(m_path);
    if (client.valid()) {
      m_b_client_alive = true;
    }
    client.wait();
    m_b_client_alive = false;
  }
}

ipc_client_watcher::~ipc_client_watcher() {
  if (m_thread) {
    m_thread->join();
    m_thread.reset();
  }
}
} // namespace core
} // namespace neb
