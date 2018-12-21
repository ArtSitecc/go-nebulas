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
#include "core/driver.h"
#include "common/configuration.h"
#include "core/ir_warden.h"
#include "core/neb_ipc/server/ipc_configuration.h"
#include "fs/nbre/api/nbre_api.h"
#include "jit/jit_driver.h"
#include "runtime/dip/dip_handler.h"
#include "runtime/nr/impl/nr_handler.h"
#include "runtime/version.h"
#include <ff/ff.h>

namespace neb {
namespace core {
namespace internal {

driver_base::driver_base() : m_exit_flag(false) {}

bool driver_base::init() {
  m_client = std::unique_ptr<ipc_client_endpoint>(new ipc_client_endpoint());
  LOG(INFO) << "ipc client construct";
  add_handlers();

  //! we should make share wait_until_sync first

  bool ret = m_client->start();
  if (!ret)
    return ret;

  m_ipc_conn = m_client->ipc_connection();

  auto p = m_ipc_conn->construct<ipc_pkg::nbre_init_req>(
      nullptr, m_ipc_conn->default_allocator());
  m_ipc_conn->push_back(p);

  return true;
}

void driver_base::run() {
  neb::core::command_queue::instance().listen_command<neb::core::exit_command>(
      this, [this](const std::shared_ptr<neb::core::exit_command> &) {
        m_exit_flag = true;
      });
  neb::exception_queue &eq = neb::exception_queue::instance();
  while (!m_exit_flag) {
    std::shared_ptr<neb::neb_exception> ep = eq.pop_front();
    handle_exception(ep);
  }
}

void driver_base::handle_exception(
    const std::shared_ptr<neb::neb_exception> &p) {
  switch (p->type()) {
    LOG(ERROR) << p->what();
  case neb_exception::neb_std_exception:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_shm_queue_failure:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_shm_service_failure:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_shm_session_already_start:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_shm_session_timeout:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_shm_session_failure:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_configure_general_failure:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_json_general_failure:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_storage_exception_no_such_key:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_storage_exception_no_init:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  case neb_exception::neb_storage_general_failure:
    neb::core::command_queue::instance().send_command(
        std::make_shared<neb::core::exit_command>());
    break;
  default:
    break;
  }
}

void driver_base::init_timer_thread() {
  if (m_timer_thread) {
    return;
  }
  m_timer_thread = std::unique_ptr<std::thread>(new std::thread([this]() {
    boost::asio::io_service io_service;

    m_timer_loop = std::unique_ptr<timer_loop>(new timer_loop(&io_service));
    m_timer_loop->register_timer_and_callback(
        neb::configuration::instance().ir_warden_time_interval(),
        []() { ir_warden::instance().on_timer(); });

    m_timer_loop->register_timer_and_callback(
        1, []() { jit_driver::instance().timer_callback(); });

    io_service.run();
  }));
}

} // end namespace internal

driver::driver() : internal::driver_base() {}
void driver::add_handlers() {
  m_client->add_handler<ipc_pkg::nbre_version_req>(
      [this](ipc_pkg::nbre_version_req *req) {
        neb::core::ipc_pkg::nbre_version_ack *ack =
            m_ipc_conn->construct<neb::core::ipc_pkg::nbre_version_ack>(
                req->m_holder, m_ipc_conn->default_allocator());
        if (ack == nullptr) {
          return;
        }
        neb::util::version v = neb::rt::get_version();
        ack->set<neb::core::ipc_pkg::major>(v.major_version());
        ack->set<neb::core::ipc_pkg::minor>(v.minor_version());
        ack->set<neb::core::ipc_pkg::patch>(v.patch_version());
        m_ipc_conn->push_back(ack);
      });

  m_client->add_handler<ipc_pkg::nbre_init_ack>(
      [this](ipc_pkg::nbre_init_ack *ack) {
        LOG(INFO) << "get init ack";

        ipc_configuration::instance().nbre_root_dir() =
            ack->get<ipc_pkg::nbre_root_dir>().c_str();
        ipc_configuration::instance().nbre_exe_name() =
            ack->get<ipc_pkg::nbre_exe_name>().c_str();
        ipc_configuration::instance().neb_db_dir() =
            ack->get<ipc_pkg::neb_db_dir>().c_str();
        ipc_configuration::instance().nbre_db_dir() =
            ack->get<ipc_pkg::nbre_db_dir>().c_str();
        ipc_configuration::instance().nbre_log_dir() =
            ack->get<ipc_pkg::nbre_log_dir>().c_str();

        std::string addr_base58 = ack->get<ipc_pkg::admin_pub_addr>().c_str();
        neb::util::bytes addr_bytes =
            neb::util::bytes::from_base58(addr_base58);
        ipc_configuration::instance().admin_pub_addr() =
            neb::util::byte_to_string(addr_bytes);

        LOG(INFO) << ipc_configuration::instance().nbre_db_dir();
        LOG(INFO) << ipc_configuration::instance().admin_pub_addr();

        init_timer_thread();

        ff::para<> p;
        p([]() {
          try {
            jit_driver &jd = jit_driver::instance();
            jd.run_ir<std::string>("dip", std::numeric_limits<uint64_t>::max(),
                                   "_Z15entry_point_dipB5cxx11m", 0);
          } catch (const std::exception &e) {
            LOG(INFO) << "dip params init failed " << e.what();
          }
        });
        ir_warden::instance().wait_until_sync();
      });

  m_client->add_handler<ipc_pkg::nbre_ir_list_req>(
      [this](ipc_pkg::nbre_ir_list_req *req) {
        neb::core::ipc_pkg::nbre_ir_list_ack *ack =
            m_ipc_conn->construct<neb::core::ipc_pkg::nbre_ir_list_ack>(
                req->m_holder, m_ipc_conn->default_allocator());
        if (ack == nullptr) {
          return;
        }

        auto irs_ptr =
            neb::fs::nbre_api(
                neb::core::ipc_configuration::instance().nbre_db_dir())
                .get_irs();
        for (auto &ir : *irs_ptr) {
          neb::ipc::char_string_t ir_name(ir.c_str(),
                                          m_ipc_conn->default_allocator());
          ack->get<neb::core::ipc_pkg::ir_name_list>().push_back(ir_name);
        }
        m_ipc_conn->push_back(ack);
      });

  m_client->add_handler<ipc_pkg::nbre_ir_versions_req>(
      [this](ipc_pkg::nbre_ir_versions_req *req) {
        neb::core::ipc_pkg::nbre_ir_versions_ack *ack =
            m_ipc_conn->construct<neb::core::ipc_pkg::nbre_ir_versions_ack>(
                req->m_holder, m_ipc_conn->default_allocator());
        if (ack == nullptr) {
          return;
        }

        auto ir_name = req->get<ipc_pkg::ir_name>();
        ack->set<neb::core::ipc_pkg::ir_name>(ir_name);

        auto ir_versions_ptr =
            neb::fs::nbre_api(
                neb::core::ipc_configuration::instance().nbre_db_dir())
                .get_ir_versions(ir_name.c_str());
        for (auto &v : *ir_versions_ptr) {
          ack->get<neb::core::ipc_pkg::ir_versions>().push_back(v);
        }
        m_ipc_conn->push_back(ack);
      });

  m_client->add_handler<ipc_pkg::nbre_nr_handler_req>(
      [this](ipc_pkg::nbre_nr_handler_req *req) {
        neb::core::ipc_pkg::nbre_nr_handler_ack *ack =
            m_ipc_conn->construct<neb::core::ipc_pkg::nbre_nr_handler_ack>(
                req->m_holder, m_ipc_conn->default_allocator());
        if (ack == nullptr) {
          return;
        }

        if (!neb::rt::nr::nr_handler::instance().get_nr_handler_id().empty()) {
          ack->set<neb::core::ipc_pkg::nr_handler_id>(
              std::string("nr handler not available").c_str());
          m_ipc_conn->push_back(ack);
          return;
        }

        uint64_t start_block = req->get<ipc_pkg::start_block>();
        uint64_t end_block = req->get<ipc_pkg::end_block>();
        uint64_t nr_version = req->get<ipc_pkg::nr_version>();

        std::stringstream ss;
        ss << neb::util::number_to_byte<neb::util::bytes>(start_block).to_hex()
           << neb::util::number_to_byte<neb::util::bytes>(end_block).to_hex()
           << neb::util::number_to_byte<neb::util::bytes>(nr_version).to_hex();

        neb::ipc::char_string_t cstr_handler_id(
            ss.str().c_str(), m_ipc_conn->default_allocator());
        ack->set<neb::core::ipc_pkg::nr_handler_id>(cstr_handler_id);
        neb::rt::nr::nr_handler::instance().start(ss.str());
        m_ipc_conn->push_back(ack);

      });

  m_client->add_handler<ipc_pkg::nbre_nr_result_req>(
      [this](ipc_pkg::nbre_nr_result_req *req) {
        neb::core::ipc_pkg::nbre_nr_result_ack *ack =
            m_ipc_conn->construct<neb::core::ipc_pkg::nbre_nr_result_ack>(
                req->m_holder, m_ipc_conn->default_allocator());
        if (ack == nullptr) {
          return;
        }

        std::string nr_handler_id = req->get<ipc_pkg::nr_handler_id>().c_str();
        neb::ipc::char_string_t cstr_nr_result(
            neb::rt::nr::nr_handler::instance()
                .get_nr_result(nr_handler_id)
                .c_str(),
            m_ipc_conn->default_allocator());
        ack->set<neb::core::ipc_pkg::nr_result>(cstr_nr_result);
        m_ipc_conn->push_back(ack);
      });

  m_client->add_handler<ipc_pkg::nbre_dip_reward_req>(
      [this](ipc_pkg::nbre_dip_reward_req *req) {
        neb::core::ipc_pkg::nbre_dip_reward_ack *ack =
            m_ipc_conn->construct<neb::core::ipc_pkg::nbre_dip_reward_ack>(
                req->m_holder, m_ipc_conn->default_allocator());
        if (ack == nullptr) {
          return;
        }

        auto height = req->get<ipc_pkg::height>();
        neb::ipc::char_string_t cstr_dip_reward(
            neb::rt::dip::dip_handler::instance()
                .get_dip_reward(height)
                .c_str(),
            m_ipc_conn->default_allocator());
        ack->set<neb::core::ipc_pkg::dip_reward>(cstr_dip_reward);
        m_ipc_conn->push_back(ack);
      });
}
}
} // namespace neb
