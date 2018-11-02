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
#include "util/singleton.h"

namespace neb {

struct configure_general_failure : public std::exception {
  inline configure_general_failure(const std::string &msg) : m_msg(msg) {}
  inline const char *what() const throw() { return m_msg.c_str(); }
protected:
  std::string m_msg;
};

class configuration : public util::singleton<configuration> {
public:
  configuration();
  configuration(const configuration &cf) = delete;
  configuration &operator=(const configuration &cf) = delete;
  configuration(configuration &&cf) = delete;
  ~configuration();

  const std::string &auth_table_nas_addr() const {
    return m_auth_table_nas_addr;
  }
  std::string &auth_table_nas_addr() { return m_auth_table_nas_addr; }
  inline const std::string &root_dir() const { return m_root_dir; }

  inline int32_t ir_warden_time_interval() const { return 1; }

  inline const char *tx_payload_type() const { return "protocol"; }
  inline const char *nbre_max_height_name() const { return "nbre_max_height"; }
  inline const char *nbre_auth_table_name() const { return "auth_table"; }
  inline const char *auth_module_name() const { return "auth"; }
  inline const char *auth_func_mangling_name() const {
    return "_Z16entry_point_authv";
  }

protected:
  std::string m_root_dir;
  std::string m_auth_table_nas_addr;
};
} // end namespace neb
