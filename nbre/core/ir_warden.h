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
#include "common/quitable_thread.h"
#include "common/util/singleton.h"
#include "fs/nbre_storage.h"
#include "fs/proto/ir.pb.h"
#include "fs/storage.h"
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <condition_variable>
#include <thread>

namespace neb {
namespace core {
class ir_warden : public util::singleton<ir_warden>, public quitable_thread {
public:
  ir_warden();
  virtual ~ir_warden();

  void async_run();

  std::shared_ptr<nbre::NBREIR> get_ir_by_name_version(const std::string &name,
                                                       uint64_t version);

  std::vector<std::shared_ptr<nbre::NBREIR>> get_ir_by_name_height(const std::string &name,
                                                       uint64_t height);
  bool is_sync_already() const;

  void wait_until_sync();

protected:
  virtual void thread_func();

  void on_timer();

private:
  boost::asio::io_service m_io_service;
  std::unique_ptr<fs::nbre_storage> m_nbre_storage;

  bool m_is_sync_already;
  std::mutex m_sync_mutex;
  std::condition_variable m_sync_cond_var;
};
}
}
