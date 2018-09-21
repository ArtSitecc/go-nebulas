// Copyright (C) 2018 go-nebulas authors
//
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

#include "jit/jit_driver.h"
#include "common/configuration.h"
#include "jit/OrcLazyJIT.h"

#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/CodeGen/CommandFlags.def"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/ExecutionEngine/GenericValue.h"
#include "llvm/ExecutionEngine/Interpreter.h"
#include "llvm/ExecutionEngine/JITEventListener.h"
#include "llvm/ExecutionEngine/MCJIT.h"
#include "llvm/ExecutionEngine/ObjectCache.h"
#include "llvm/ExecutionEngine/Orc/OrcRemoteTargetClient.h"
#include "llvm/ExecutionEngine/OrcMCJITReplacement.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Object/Archive.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include <cerrno>
namespace neb {
namespace internal {
class jit_driver_impl {
public:
  jit_driver_impl() {
    llvm::sys::PrintStackTraceOnErrorSignal(
        configuration::instance().exec_name().c_str());
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::sys::Process::PreventCoreFiles();
  }
  jit_driver_impl(const jit_driver_impl &) = delete;
  jit_driver_impl &operator=(const jit_driver_impl &) = delete;
  jit_driver_impl(jit_driver_impl &&) = delete;

  void run(const std::vector<std::shared_ptr<nbre::NBREIR>> &irs) {
    const std::string &runtime_library_path =
        configuration::instance().runtime_library_path();
    // FIXME need remove this library later
    llvm::sys::DynamicLibrary::LoadLibraryPermanently(
        runtime_library_path.c_str());
    std::vector<std::unique_ptr<llvm::Module>> modules;
    for (auto it = irs.begin(); it != irs.end(); ++it) {
      auto ir = *it;
      std::string ir_str = ir->ir();
      llvm::StringRef sr(ir_str);
      auto mem_buf = llvm::MemoryBuffer::getMemBuffer(sr, "", false);
      llvm::SMDiagnostic err;

      modules.push_back(
          llvm::parseIR(mem_buf->getMemBufferRef(), err, m_context));
    }
    auto ret =
        llvm::runOrcLazyJIT(std::move(modules), std::vector<std::string>());
    LOG(INFO) << "jit return : " << ret;
  }

  virtual ~jit_driver_impl() { llvm::llvm_shutdown(); }

protected:
  llvm::LLVMContext m_context;

}; // end class jit_driver_impl
} // end namespace internal

jit_driver::jit_driver() {
  m_impl = std::unique_ptr<internal::jit_driver_impl>(
      new internal::jit_driver_impl());
}

void jit_driver::run(const std::vector<std::shared_ptr<nbre::NBREIR>> &irs) {
  m_impl->run(irs);
}
}
