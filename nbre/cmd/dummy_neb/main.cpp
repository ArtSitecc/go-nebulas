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
#include "common/common.h"

#include "common/configuration.h"
#include "core/neb_ipc/server/ipc_server_endpoint.h"

#include "core/neb_ipc/ipc_interface.h"
#include "fs/util.h"

std::mutex local_mutex;
std::condition_variable local_cond_var;
bool to_quit = false;

void nbre_version_callback(ipc_status_code isc, void *handler, uint32_t major,
                           uint32_t minor, uint32_t patch) {
  std::cout << "got version: " << major << ", " << minor << ", " << patch
            << std::endl;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
  // local_cond_var.notify_one();
}

void nbre_ir_list_callback(ipc_status_code isc, void *handler,
                           const char *ir_name_list) {
  std::cout << ir_name_list << std::endl;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}

void nbre_ir_versions_callback(ipc_status_code isc, void *handler,
                               const char *ir_versions) {
  std::cout << ir_versions << std::endl;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;

  ::google::InitGoogleLogging(argv[0]);

  const char *root_dir = neb::configuration::instance().nbre_root_dir().c_str();
  std::string nbre_path = neb::fs::join_path(root_dir, "bin/nbre");

  // set_recv_nbre_version_callback(nbre_version_callback);
  // set_recv_nbre_ir_list_callback(nbre_ir_list_callback);
  set_recv_nbre_ir_versions_callback(nbre_ir_versions_callback);

  nbre_params_t params{root_dir,
                       nbre_path.c_str(),
                       neb::configuration::instance().neb_db_dir().c_str(),
                       neb::configuration::instance().nbre_db_dir().c_str(),
                       neb::configuration::instance().nbre_log_dir().c_str(),
                       "auth address here!"};
  auto ret = start_nbre_ipc(params);
  if (ret != ipc_status_succ)
    to_quit = false;

  uint64_t height = 100;

  // ipc_nbre_version(&local_mutex, height);
  // ipc_nbre_ir_list(&local_mutex);
  ipc_nbre_ir_versions(&local_mutex, "dip");

  std::unique_lock<std::mutex> _l(local_mutex);
  if (to_quit)
    return 0;
  local_cond_var.wait(_l);

  return 0;
}
