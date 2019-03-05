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
#include "cmd/dummy_neb/dummies/random_dummy.h"

random_dummy::random_dummy(const std::string &name, int initial_account_num,
                           nas initial_nas, double account_increase_ratio,
                           double auth_ratio)
    : dummy_base(name), m_initial_account_num(initial_account_num),
      m_initial_nas(initial_nas),
      m_account_increase_ratio(account_increase_ratio),
      m_auth_ratio(auth_ratio) {}

random_dummy::~random_dummy() {}

std::shared_ptr<generate_block> random_dummy::generate_LIB_block() {
  std::shared_ptr<generate_block> ret =
      std::make_shared<generate_block>(&m_all_accounts, m_current_height);

  if (m_current_height == 0) {
    genesis_generator g(ret.get(), m_initial_account_num, m_initial_nas);
    g.run();
  } else {
    if (!m_tx_gen) {
      int account_num = m_account_increase_ratio * m_initial_account_num;
      int tx_num = account_num + std::rand() % m_initial_account_num;

      m_tx_gen = std::make_unique<transaction_generator>(
          &m_all_accounts, ret.get(),
          m_account_increase_ratio * m_initial_account_num, tx_num);
    }
    m_tx_gen->run();
    // TODO add auth_gen
  }

  m_current_height++;
  return ret;
}

std::shared_ptr<checker_task_base> random_dummy::generate_checker_task() {
  return nullptr;
}

