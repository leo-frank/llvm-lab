

#include "Tolerator.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;
using tolerator::Tolerator;

namespace tolerator {

char Tolerator::ID = 0;

}

bool Tolerator::runOnModule(Module& m) {
  auto& context = m.getContext();

  // This analysis just prints a message when the program starts or exits.
  // You should modify this code as you see fit. 如你所见。
  auto* voidTy = Type::getVoidTy(context);

  auto helloworld = m.getOrInsertFunction("ToLeRaToR_helloworld", voidTy);
  appendToGlobalCtors(m, llvm::cast<Function>(helloworld.getCallee()), 0);

  auto goodbyeworld = m.getOrInsertFunction("ToLeRaToR_goodbyeworld", voidTy);
  appendToGlobalDtors(m, llvm::cast<Function>(goodbyeworld.getCallee()), 0);

  /* div */
  auto* int32Ty = Type::getInt32Ty(context);
  auto* divHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ int32Ty,
      /* isVarArg */ false);
  auto div = m.getOrInsertFunction("ToLeRaToR_div", divHelperTy);

  /* malloc */
  std::vector<Type*> alloc_args;
  // TODO will there always be a int8* type? int64* also possible
  alloc_args.push_back(Type::getInt8PtrTy(context));  // start address
  alloc_args.push_back(Type::getInt64Ty(context));    // malloc size
  auto* allocHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ alloc_args,
      /* isVarArg */ false);
  auto alloc = m.getOrInsertFunction("ToLeRaToR_malloc", allocHelperTy);

  /* free */
  auto* unallocHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ Type::getInt8PtrTy(context),
      /* isVarArg */ false);
  auto unalloc = m.getOrInsertFunction("ToLeRaToR_free", unallocHelperTy);

  IRBuilder<> builder(context);
  for (auto& f : m) {
    for (auto& bb : f) {
      for (auto& i : bb) {
        if (BinaryOperator* BO = dyn_cast<BinaryOperator>(&i)) {
          if (BO->getOpcode() == llvm::Instruction::BinaryOps::UDiv ||
              BO->getOpcode() == llvm::Instruction::BinaryOps::SDiv ||
              BO->getOpcode() == llvm::Instruction::BinaryOps::FDiv) {
            Value* op2 = BO->getOperand(1);
            builder.SetInsertPoint(BO);
            builder.CreateCall(div, ArrayRef<Value*>(op2));
          }
        } else if (CallBase* CB = dyn_cast<CallBase>(&i)) {
          /* Attention don't use: auto fn = CB->getCalledFunction(), which
           * result in non-complete cast */
          auto fn =
              dyn_cast<Function>(CB->getCalledOperand()->stripPointerCasts());
          if (fn) {
            /* indirect call result fn to NULL, so make sure fn valid */
            if (fn->getName() == "malloc") {
              /* malloc find */
              Value* args[2];
              args[0] = dyn_cast<Value>(CB);              // start address
              args[1] = CB->getOperand(0);                // malloc size
              builder.SetInsertPoint(CB->getNextNode());  // !getNextNode
              builder.CreateCall(alloc, args);
            } else if (fn->getName() == "free") {
              /* free find */
              Value* op0 = CB->getOperand(0);
              builder.SetInsertPoint(CB);
              builder.CreateCall(unalloc, ArrayRef<Value*>(op0));
            }
          }
        }
      }
    }
  }
  return true;
}
