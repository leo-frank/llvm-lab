
#pragma once

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

namespace tolerator {

enum class AnalysisType { LOGGING, IGNORING, DEFAULTING, BYPASSING };

struct Tolerator : public llvm::ModulePass {
  static char ID;
  char AT;
  llvm::DenseMap<llvm::Function *, int64_t> ids;

  Tolerator(AnalysisType analysisType) : llvm::ModulePass(ID) {
    switch (analysisType) {
    case AnalysisType::LOGGING:
      AT = 1;
      break;
    case AnalysisType::IGNORING:
      AT = 2;
      break;
    case AnalysisType::DEFAULTING:
      AT = 3;
      break;
    case AnalysisType::BYPASSING:
      AT = 4;
      break;
    };
  }

  bool runOnModule(llvm::Module &m) override;
};

} // namespace tolerator
