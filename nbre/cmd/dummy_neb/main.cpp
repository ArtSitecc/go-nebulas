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
#include "cmd/dummy_neb/dummy_common.h"
#include "cmd/dummy_neb/dummy_driver.h"
#include "common/configuration.h"
#include "fs/util.h"
#include <boost/process.hpp>
#include <boost/program_options.hpp>

std::mutex local_mutex;
std::condition_variable local_cond_var;
bool to_quit = false;

void nbre_version_callback(ipc_status_code isc, void *handler, uint32_t major,
                           uint32_t minor, uint32_t patch) {
  LOG(INFO) << "got version: " << major << ", " << minor << ", " << patch;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
  // local_cond_var.notify_one();
}

void nbre_ir_list_callback(ipc_status_code isc, void *handler,
                           const char *ir_name_list) {
  LOG(INFO) << ir_name_list;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}

void nbre_ir_versions_callback(ipc_status_code isc, void *handler,
                               const char *ir_versions) {
  LOG(INFO) << ir_versions;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}

void nbre_nr_handle_callback(ipc_status_code isc, void *holder,
                             const char *nr_handle_id) {
  LOG(INFO) << nr_handle_id;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}

void nbre_nr_result_callback(ipc_status_code isc, void *holder,
                             const char *nr_result) {
  LOG(INFO) << nr_result;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}

void nbre_dip_reward_callback(ipc_status_code isc, void *holder,
                              const char *dip_reward) {
  LOG(INFO) << dip_reward;
  std::unique_lock<std::mutex> _l(local_mutex);
  to_quit = true;
  _l.unlock();
}
#if 0
int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;

  //::google::InitGoogleLogging(argv[0]);
  neb::glog_log_to_stderr = true;
  neb::use_test_blockchain = true;

  const char *root_dir = neb::configuration::instance().nbre_root_dir().c_str();
  std::string nbre_path = neb::fs::join_path(root_dir, "bin/nbre");

  set_recv_nbre_version_callback(nbre_version_callback);
  set_recv_nbre_ir_list_callback(nbre_ir_list_callback);
  set_recv_nbre_ir_versions_callback(nbre_ir_versions_callback);
  set_recv_nbre_nr_handle_callback(nbre_nr_handle_callback);
  set_recv_nbre_nr_result_callback(nbre_nr_result_callback);
  set_recv_nbre_dip_reward_callback(nbre_dip_reward_callback);

  nbre_params_t params{root_dir,
                       nbre_path.c_str(),
                       neb::configuration::instance().neb_db_dir().c_str(),
                       neb::configuration::instance().nbre_db_dir().c_str(),
                       neb::configuration::instance().nbre_log_dir().c_str(),
                       "auth address here!"};
  params.m_nipc_port = 6987;

  auto ret = start_nbre_ipc(params);
  if (ret != ipc_status_succ) {
    to_quit = false;
    nbre_ipc_shutdown();
    return -1;
  }

  uint64_t height = 100;

  ipc_nbre_version(&local_mutex, height);
  ipc_nbre_ir_list(&local_mutex);
  // ipc_nbre_ir_versions(&local_mutex, "dip");

  ipc_nbre_nr_handle(&local_mutex, 6600, 6650,
                     neb::util::version(0, 1, 0).data());
  while (true) {
    ipc_nbre_nr_result(&local_mutex,
                       "00000000000019c800000000000019fa0000000100000000");
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  // while (true) {
  // ipc_nbre_dip_reward(&local_mutex, 60000);
  // std::this_thread::sleep_for(std::chrono::seconds(1));
  //}
  std::unique_lock<std::mutex> _l(local_mutex);
  if (to_quit) {
    return 0;
  }
  local_cond_var.wait(_l);

  return 0;
}
#endif

namespace po = boost::program_options;
namespace bp = boost::process;
po::variables_map get_variables_map(int argc, char *argv[]) {
  po::options_description desc("Generate IR Payload");
  desc.add_options()("help", "show help message")(
      "input", po::value<std::string>(), "IR configuration file")(
      "output", po::value<std::string>(),
      "output file")("mode", po::value<std::string>()->default_value("payload"),
                     "Generate ir bitcode or ir payload. - [bitcode | "
                     "payload], default:payload");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);
  if (vm.count("help")) {
    std::cout << desc << "\n";
    exit(1);
  }

  if (!vm.count("input")) {
    std::cout << "You must specify \"input\"!";
    exit(1);
  }
  if (vm.count("mode")) {
    std::string m = vm["mode"].as<std::string>();
    if (m != "bitcode" && m != "payload") {
      std::cout << "Wrong mode, should be either bitcode or payload."
                << std::endl;
      exit(1);
    }
  }
  if (!vm.count("output")) {
    std::cout << "You must specify output!";
    exit(1);
  }

  return vm;
}

void init_dummy_driver(dummy_driver &dd) {
  dd.add_dummy(std::make_shared<random_dummy>("default_random", 10, 100_nas,
                                              0.05, 0.02));
}
int main(int argc, char *argv[]) {
  FLAGS_logtostderr = true;

  //::google::InitGoogleLogging(argv[0]);
  neb::glog_log_to_stderr = true;
  neb::use_test_blockchain = true;

  const char *root_dir = neb::configuration::instance().nbre_root_dir().c_str();
  std::string nbre_path = neb::fs::join_path(root_dir, "bin/nbre");

  // set_recv_nbre_version_callback(nbre_version_callback);
  // set_recv_nbre_ir_list_callback(nbre_ir_list_callback);
  // set_recv_nbre_ir_versions_callback(nbre_ir_versions_callback);
  // set_recv_nbre_nr_handle_callback(nbre_nr_handle_callback);
  // set_recv_nbre_nr_result_callback(nbre_nr_result_callback);
  // set_recv_nbre_dip_reward_callback(nbre_dip_reward_callback);

  nbre_params_t params{root_dir,
                       nbre_path.c_str(),
                       neb::configuration::instance().neb_db_dir().c_str(),
                       neb::configuration::instance().nbre_db_dir().c_str(),
                       neb::configuration::instance().nbre_log_dir().c_str(),
                       "auth address here!"};
  params.m_nipc_port = 6987;

  auto ret = start_nbre_ipc(params);
  if (ret != ipc_status_succ) {
    to_quit = false;
    nbre_ipc_shutdown();
    return -1;
  }

  dummy_driver dd;
  init_dummy_driver(dd);
  dd.run("default_random", 10);

  return 0;
}
