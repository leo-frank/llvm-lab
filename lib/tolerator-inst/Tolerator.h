
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

namespace tolerator {

enum AnalysisType { LOGGING, IGNORING, DEFAULTING, BYPASSING };

struct Tolerator : public llvm::ModulePass {
  static char ID;
  char AT;
  llvm::DenseMap<llvm::Function *, int64_t> ids;

  Tolerator(AnalysisType analysisType) : llvm::ModulePass(ID) {
    AT = analysisType;
  }

  bool runOnModule(llvm::Module &m) override;
};

} // namespace tolerator
