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
#include "core/neb_ipc/ipc_endpoint.h"

#include "core/neb_ipc/ipc_client.h"
#include "core/neb_ipc/ipc_interface.h"
#include "core/neb_ipc/ipc_pkg.h"
#include "fs/util.h"

int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;
  ::google::InitGoogleLogging(argv[0]);

  neb::core::ipc_client ic;
  neb::core::ipc_client_t *ipc_conn;

  ic.add_handler<neb::core::nbre_version_req>(
      [](neb::core::nbre_version_req *req) { LOG(INFO) << "got version req"; });

  ic.start();
  ipc_conn = ic.ipc_connection();

  std::this_thread::sleep_for(std::chrono::seconds(15));
  LOG(INFO) << "to quit nbre";

  return 0;
}
