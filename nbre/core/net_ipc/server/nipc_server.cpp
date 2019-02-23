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
#include "core/net_ipc/server/nipc_server.h"
#include "core/ipc_configuration.h"
#include "core/net_ipc/nipc_pkg.h"

namespace neb {
namespace core {
nipc_server::nipc_server() : m_server(nullptr), m_conn(nullptr) {}
nipc_server::~nipc_server() { LOG(INFO) << "~nipc_server"; }

void nipc_server::init_params(const nbre_params_t &params) {
  neb::core::ipc_configuration::instance().nbre_root_dir() =
      params.m_nbre_root_dir;
  neb::core::ipc_configuration::instance().nbre_exe_name() =
      params.m_nbre_exe_name;
  neb::core::ipc_configuration::instance().neb_db_dir() = params.m_neb_db_dir;
  neb::core::ipc_configuration::instance().nbre_db_dir() = params.m_nbre_db_dir;
  neb::core::ipc_configuration::instance().nbre_log_dir() =
      params.m_nbre_log_dir;
  neb::core::ipc_configuration::instance().admin_pub_addr() =
      params.m_admin_pub_addr;
  neb::core::ipc_configuration::instance().nbre_start_height() =
      params.m_nbre_start_height;
  neb::core::ipc_configuration::instance().port() = params.m_port;

}

bool nipc_server::start() {
  m_mutex.lock();
  m_is_started = false;
  m_mutex.unlock();

  m_thread = std::make_unique<std::thread>([this]() {
    m_server = std::make_unique<::ff::net::net_nervure>();
    m_pkg_hub = std::make_unique<::ff::net::typed_pkg_hub>();
    add_all_callbacks();
    m_server->add_pkg_hub(*m_pkg_hub);
    m_server->add_tcp_server("127.0.0.1",
                             neb::core::ipc_configuration::instance().port());
    m_request_timer = std::make_unique<api_request_timer>(
        &m_server->ioservice(), &ipc_callback_holder::instance());


    m_server->get_event_handler()
        ->listen<::ff::net::event::more::tcp_server_accept_connection>(
            [this](::ff::net::tcp_connection_base_ptr conn) {
              m_conn = conn;
              m_mutex.lock();
              m_is_started = true;
              m_mutex.unlock();
              m_request_timer->reset_conn(conn);
            });
    m_server->get_event_handler()
        ->listen<::ff::net::event::tcp_lost_connection>(
            [this](::ff::net::tcp_connection_base *) {
              // TODO we may restart the client
              m_conn.reset();
              m_request_timer->reset_conn(nullptr);
            });

    // We need start the client here
    m_client_watcher =
        std::unique_ptr<ipc_client_watcher>(new ipc_client_watcher(
            neb::core::ipc_configuration::instance().nbre_exe_name()));
    m_client_watcher->start();
    m_server->run();
  });
  std::unique_lock<std::mutex> _l(m_mutex);
  if (m_is_started) {
    _l.unlock();
  } else {
    m_start_complete_cond_var.wait(_l);
  }

  return true;
}

void nipc_server::shutdown() {
  if (m_conn)
    m_conn->close();
  m_server->stop();
}

void nipc_server::add_all_callbacks() {
#define define_ipc_param(type, name)
#define define_ipc_pkg(type, ...)
#define define_ipc_api(req, ack) to_recv_pkg<ack>();

#include "core/net_ipc/ipc_interface_impl.h"

#undef define_ipc_api
#undef define_ipc_pkg
#undef define_ipc_param

  m_pkg_hub->to_recv_pkg<nbre_init_req>([this](std::shared_ptr<nbre_init_req>) {
    std::shared_ptr<nbre_init_ack> ack = std::make_shared<nbre_init_ack>();
    ipc_configuration &conf = ipc_configuration::instance();
    ack->set<p_nbre_root_dir>(conf.nbre_root_dir());
    ack->set<p_nbre_exe_name>(conf.nbre_exe_name());
    ack->set<p_neb_db_dir>(conf.neb_db_dir());
    ack->set<p_nbre_log_dir>(conf.nbre_log_dir());
    ack->set<p_admin_pub_addr>(conf.admin_pub_addr());
    ack->set<p_nbre_start_height>(conf.nbre_start_height());
    m_conn->send(ack);
  });
}
} // namespace core
} // namespace neb
