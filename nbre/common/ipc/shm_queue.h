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
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_condition.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
namespace neb {
namespace ipc {
typedef uint32_t shm_type_id_t;
struct shm_queue_failure : public std::exception {
  inline shm_queue_failure(const std::string &msg) : m_msg(msg) {}
  inline const char *what() const throw() { return m_msg.c_str(); }

protected:
  std::string m_msg;
};
namespace internal {
class shm_queue {
public:
  shm_queue(const std::string &name,
            boost::interprocess::managed_shared_memory *shmem, size_t capacity);

  template <typename T> void push_back(T *ptr) {
    boost::interprocess::scoped_lock<boost::interprocess::named_mutex> _l(
        *m_mutex);
    if (m_buffer->size() == m_capacity) {
      m_full_cond->wait(_l);
    }
    if (m_buffer->size() >= m_capacity)
      return;
    boost::interprocess::managed_shared_memory::handle_t h =
        m_shmem->get_handle_from_address(ptr);
    vector_elem_t e;
    e.m_handle = h;
    e.m_type = T::pkg_identifier;
    m_buffer->push_back(e);
    if (m_buffer->size() == 1) {
      m_empty_cond->notify_one();
    }
  }

  std::pair<void *, shm_type_id_t> pop_front();

  size_t size() const;

  void wake_up_if_empty();
  size_t empty() const;

  virtual ~shm_queue();

private:
  std::string mutex_name() { return m_name + ".mutex"; }
  std::string empty_cond_name() { return m_name + ".empty_cond"; }
  std::string full_cond_name() { return m_name + ".full_cond"; }

protected:
  struct vector_elem_t {
    boost::interprocess::managed_shared_memory::handle_t m_handle;
    shm_type_id_t m_type;
  };
  typedef boost::interprocess::allocator<
      vector_elem_t,
      boost::interprocess::managed_shared_memory::segment_manager>
      shmem_allocator_t;
  typedef boost::interprocess::vector<vector_elem_t, shmem_allocator_t>
      shm_vector_t;
  std::string m_name;
  boost::interprocess::managed_shared_memory *m_shmem;
  shmem_allocator_t *m_shm_allocator;
  shmem_allocator_t *m_allocator;

  shm_vector_t *m_buffer;
  size_t m_capacity;

  boost::interprocess::named_mutex *m_mutex;
  boost::interprocess::named_condition *m_empty_cond;
  boost::interprocess::named_condition *m_full_cond;
}; // end class shm_queue
}
}
}
