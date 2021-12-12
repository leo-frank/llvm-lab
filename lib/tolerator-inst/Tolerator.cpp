

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

bool Tolerator::runOnModule(Module &m) {
  auto &context = m.getContext();

  // This analysis just prints a message when the program starts or exits.
  // You should modify this code as you see fit. 如你所见。
  auto *voidTy = Type::getVoidTy(context);

  auto helloworld = m.getOrInsertFunction("ToLeRaToR_helloworld", voidTy);
  appendToGlobalCtors(m, llvm::cast<Function>(helloworld.getCallee()), 0);

  auto goodbyeworld = m.getOrInsertFunction("ToLeRaToR_goodbyeworld", voidTy);
  appendToGlobalDtors(m, llvm::cast<Function>(goodbyeworld.getCallee()), 0);

  /* div */
  auto *int32Ty = Type::getInt32Ty(context);
  auto *divHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ int32Ty,
      /* isVarArg */ false);
  auto div = m.getOrInsertFunction("ToLeRaToR_div", divHelperTy);

  /* malloc */
  std::vector<Type *> alloc_args;
  // TODO will there always be a int8* type? int64* also possible
  alloc_args.push_back(Type::getInt8PtrTy(context)); // start address
  alloc_args.push_back(Type::getInt64Ty(context));   // malloc size
  auto *allocHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ alloc_args,
      /* isVarArg */ false);
  auto alloc = m.getOrInsertFunction("ToLeRaToR_malloc", allocHelperTy);

  /* free */
  auto *unallocHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ Type::getInt8PtrTy(context),
      /* isVarArg */ false);
  auto unalloc = m.getOrInsertFunction("ToLeRaToR_free", unallocHelperTy);

  /* local variable alloc */
  std::vector<Type *> local_args;
  local_args.push_back(Type::getInt64Ty(context));
  local_args.push_back(Type::getInt64Ty(context));
  auto *localHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ local_args,
      /* isVarArg */ false);
  auto local = m.getOrInsertFunction("ToLeRaToR_local", localHelperTy);

  /* store */
  std::vector<Type *> store_args;
  store_args.push_back(Type::getInt64Ty(context));
  store_args.push_back(Type::getInt32Ty(context));
  store_args.push_back(Type::getInt64Ty(context));
  auto *storeHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ store_args,
      /* isVarArg */ false);
  auto store = m.getOrInsertFunction("ToLeRaToR_store", storeHelperTy);

  /* load */
  std::vector<Type *> load_args;
  load_args.push_back(Type::getInt64Ty(context));
  load_args.push_back(Type::getInt64Ty(context));
  auto *loadHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ load_args,
      /* isVarArg */ false);
  auto load = m.getOrInsertFunction("ToLeRaToR_load", loadHelperTy);

  IRBuilder<> IRB(context);

  for (auto &f : m) {
    for (auto &bb : f) {
      for (auto &i : bb) {
        if (BinaryOperator *BO = dyn_cast<BinaryOperator>(&i)) {
          if (BO->getOpcode() == llvm::Instruction::BinaryOps::UDiv ||
              BO->getOpcode() == llvm::Instruction::BinaryOps::SDiv ||
              BO->getOpcode() == llvm::Instruction::BinaryOps::FDiv) {
            Value *op2 = BO->getOperand(1);
            IRB.SetInsertPoint(BO);
            IRB.CreateCall(div, ArrayRef<Value *>(op2));
          }
        } else if (CallBase *CB = dyn_cast<CallBase>(&i)) {
          /* Attention don't use: auto fn = CB->getCalledFunction(), which
           * result in non-complete cast */
          auto fn =
              dyn_cast<Function>(CB->getCalledOperand()->stripPointerCasts());
          if (fn) {
            /* indirect call result fn to NULL, so make sure fn valid */
            if (fn->getName() == "malloc") {
              /* malloc find */
              Value *args[2];
              args[0] = dyn_cast<Value>(CB);         // start address
              args[1] = CB->getOperand(0);           // malloc size
              IRB.SetInsertPoint(CB->getNextNode()); // !getNextNode
              IRB.CreateCall(alloc, args);
            } else if (fn->getName() == "free") {
              /* free find */
              Value *op0 = CB->getOperand(0);
              IRB.SetInsertPoint(CB);
              IRB.CreateCall(unalloc, ArrayRef<Value *>(op0));
            }
          }
        } else if (AllocaInst *AI = dyn_cast<AllocaInst>(&i)) {
          /* local stack allocate */
          IRB.SetInsertPoint(AI->getNextNode());
          const DataLayout &DL = f.getParent()->getDataLayout();
          uint64_t TypeSize = DL.getTypeAllocSize(AI->getAllocatedType());
          Value *Len = ConstantInt::get(IRB.getInt64Ty(), TypeSize);
          if (AI->isArrayAllocation())
            Len = IRB.CreateMul(Len, AI->getArraySize());
          IRB.CreateCall(local,
                         {IRB.CreatePointerCast(AI, IRB.getIntPtrTy(DL)), Len});
        } else if (StoreInst *SI = dyn_cast<StoreInst>(&i)) {
          /* StoreInst: store var *ptr */
          IRB.SetInsertPoint(SI);
          const DataLayout &DL = f.getParent()->getDataLayout();
          Value *Val = SI->getValueOperand();
          Value *Addr = SI->getPointerOperand();
          uint64_t StoreSize = DL.getTypeStoreSize(Val->getType());
          Value *Size = ConstantInt::get(IRB.getInt64Ty(), StoreSize);
          /* TODO a better way to type judge ? */
          if (Val->getType() == Type::getInt32Ty(context) ||
              Val->getType() == Type::getInt8Ty(context) ||
              Val->getType() == Type::getInt64Ty(context)) {
            IRB.CreateCall(
                store, {IRB.CreatePointerCast(Addr, IRB.getIntPtrTy(DL)),
                        IRB.CreateIntCast(Val, IRB.getInt32Ty(), false), Size});
          } else {
            IRB.CreateCall(
                store, {IRB.CreatePointerCast(Addr, IRB.getIntPtrTy(DL)),
                        IRB.CreatePointerCast(Val, IRB.getInt32Ty()), Size});
          }
        } else if (LoadInst *LI = dyn_cast<LoadInst>(&i)) {
          /* LoadInst: load var *ptr */
          IRB.SetInsertPoint(LI);
          Value *Addr = LI->getPointerOperand();
          const DataLayout &DL = f.getParent()->getDataLayout();
          uint64_t LoadSize = DL.getTypeStoreSize(LI->getType());
          Value *Size = ConstantInt::get(IRB.getInt64Ty(), LoadSize);
          IRB.CreateCall(
              load, {IRB.CreatePointerCast(Addr, IRB.getIntPtrTy(DL)), Size});
        }
      }
    }
    // TODO clear local variable
  }
  return true;
}
