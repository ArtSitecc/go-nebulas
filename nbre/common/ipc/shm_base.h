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

struct shm_server {
  inline static std::string role_name(const std::string &name) {
    return name + std::string(".server");
  }
};
struct shm_client {
  inline static std::string role_name(const std::string &name) {
    return name + std::string(".client");
  }
};
typedef uint32_t shm_type_id_t;
namespace internal {
template <typename T> struct shm_other_side_role {};
template <> struct shm_other_side_role<shm_server> { typedef shm_client type; };
template <> struct shm_other_side_role<shm_client> { typedef shm_server type; };
}
}
}
