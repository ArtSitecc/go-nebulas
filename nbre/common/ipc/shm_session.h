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
#include "common/ipc/shm_base.h"
#include "common/ipc/shm_bookkeeper.h"
#include "common/quitable_thread.h"
#include <atomic>
#include <thread>

namespace neb {
namespace ipc {

struct shm_session_failure : public std::exception {
  inline shm_session_failure(const std::string &msg) : m_msg(msg) {}
  inline const char *what() const throw() { return m_msg.c_str(); }
protected:
  std::string m_msg;
};

struct shm_session_timeout : public shm_session_failure {
  inline shm_session_timeout() : shm_session_failure("shm session timeout"){};
};

void clean_shm_session_env();

namespace internal {

class shm_session_base : public quitable_thread {
public:
  shm_session_base(const std::string &name);
  virtual ~shm_session_base();

  void start_session();

protected:
  inline std::string server_sema_name() { return m_name + ".server_sema"; }
  inline std::string client_sema_name() { return m_name + ".client_sema"; }

protected:
  std::string m_name;
  std::unique_ptr<shm_bookkeeper> m_bookkeeper;
  std::unique_ptr<boost::interprocess::named_semaphore> m_server_sema;
  std::unique_ptr<boost::interprocess::named_semaphore> m_client_sema;
  };

  class shm_session_server : public shm_session_base {
  public:
    shm_session_server(const std::string &name);

    void wait_until_client_start();
    bool is_client_alive();

  protected:
    virtual void thread_func();

  protected:
    std::atomic_bool m_client_started;
    std::atomic_bool m_client_alive;
  };

  class shm_session_client : public shm_session_base {
  public:
    shm_session_client(const std::string &name);

    bool is_server_alive();

  protected:
    virtual void thread_func();

  protected:
    std::atomic_bool m_server_alive;
  };
}
}
}
