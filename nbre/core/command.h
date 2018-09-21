// Copyright (C) 2017 go-nebulas authors
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
#include "common/util/singleton.h"

namespace neb {
namespace core {
typedef uint64_t command_type_t;

struct base_command {}; // end class base_command

struct exit_command {
  const static command_type_t command_type = 1;
}; // end class exit_command

class command_queue : public util::singleton<command_queue> {
public:
  template <typename CT> void send_command(const std::shared_ptr<CT> &cmd) {
    std::lock_guard<std::mutex> _l(m_mutex);
    for (handlers_t::iterator it = m_handlers.begin(); it != m_handlers.end();
         ++it) {
      if (CT::command_type == it->first) {
        it->second(cmd);
      }
    }
  }

  template <typename CT>
  void
  listen_command(const std::function<void(const std::shared_ptr<CT> &)> &func) {
    std::lock_guard<std::mutex> _l(m_mutex);
    m_handlers.push_back(std::make_pair(
        CT::command_type, [func](const std::shared_ptr<base_command> &cmd) {
          func(std::dynamic_pointer_cast<CT>(cmd));
        }));
  }

  template <typename CT> void unlisten_command() {
    std::lock_guard<std::mutex> _l(m_mutex);
    for (handlers_t::reverse_iterator it = m_handlers.rbegin();
         it != m_handlers.rend(); ++it) {
      if (it->first == CT::command_type) {
        m_handlers.erase(it.base() - 1);
      }
    }
  }

protected:
  typedef std::vector<
      std::pair<command_type_t,
                std::function<void(const std::shared_ptr<base_command> &)>>>
      handlers_t;
  handlers_t m_handlers;
  std::mutex m_mutex;
}; // end class command_queue
} // end namespace core
}
