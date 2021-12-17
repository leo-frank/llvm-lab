

#include "Tolerator.h"

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;
using tolerator::Tolerator;

namespace tolerator {

char Tolerator::ID = 0;

}
enum STATE { INVALID, VALID };
// Returns a map (Function* -> uint64_t).
// This is used for local valid address of nesting functions.
static DenseMap<Function *, int64_t>
computeFunctionIDs(llvm::ArrayRef<Function *> functions) {
  DenseMap<Function *, int64_t> idMap;

  size_t nextID = 0;
  for (auto f : functions) {
    if (!f->isDeclaration()) {
      idMap[f] = nextID;
      ++nextID;
    }
  }

  return idMap;
}

bool is_interesting(Instruction *inst) {
  if (isa<AllocaInst>(inst) || isa<LoadInst>(inst) || isa<StoreInst>(inst) ||
      isa<SDivOperator>(inst) || isa<UDivOperator>(inst))
    return true;
  if (isa<CallBase>(inst)) {
    auto called =
        dyn_cast<CallBase>(inst)->getCalledOperand()->stripPointerCasts();
    if (called->getName().compare(StringRef("free")) == 0 ||
        called->getName().compare(StringRef("malloc")) == 0) {
      return true;
    }
  }
  return false;
}

bool Tolerator::runOnModule(Module &m) {
  auto &context = m.getContext();

  // This analysis just prints a message when the program starts or exits.
  // You should modify this code as you see fit. 如你所见。
  auto *voidTy = Type::getVoidTy(context);
  auto *int1Ty = Type::getInt1Ty(context);

  auto helloworld = m.getOrInsertFunction("ToLeRaToR_helloworld", voidTy);
  appendToGlobalCtors(m, llvm::cast<Function>(helloworld.getCallee()), 0);

  auto goodbyeworld = m.getOrInsertFunction("ToLeRaToR_goodbyeworld", voidTy);
  appendToGlobalDtors(m, llvm::cast<Function>(goodbyeworld.getCallee()), 0);

  /* div */
  auto *int32Ty = Type::getInt32Ty(context);
  auto *divHelperTy = FunctionType::get(
      /* return type */ int1Ty,
      /* args vector */ int32Ty,
      /* isVarArg */ false);
  auto div = m.getOrInsertFunction("ToLeRaToR_div", divHelperTy);

  /* malloc */
  std::vector<Type *> alloc_args;
  // TODO: will there always be a int8* type? int64* also possible
  alloc_args.push_back(Type::getInt8PtrTy(context)); // start address
  alloc_args.push_back(Type::getInt64Ty(context));   // malloc size
  auto *allocHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ alloc_args,
      /* isVarArg */ false);
  auto alloc = m.getOrInsertFunction("ToLeRaToR_malloc", allocHelperTy);

  /* free */
  auto *unallocHelperTy = FunctionType::get(
      /* return type */ int1Ty,
      /* args vector */ Type::getInt8PtrTy(context),
      /* isVarArg */ false);
  auto unalloc = m.getOrInsertFunction("ToLeRaToR_free", unallocHelperTy);

  /* local variable alloc */
  std::vector<Type *> local_args;
  local_args.push_back(Type::getInt64Ty(context));
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
  store_args.push_back(Type::getInt64Ty(context));
  store_args.push_back(Type::getInt32Ty(context));
  store_args.push_back(Type::getInt64Ty(context));
  auto *storeHelperTy = FunctionType::get(
      /* return type */ int1Ty,
      /* args vector */ store_args,
      /* isVarArg */ false);
  auto store = m.getOrInsertFunction("ToLeRaToR_store", storeHelperTy);

  /* load */
  std::vector<Type *> load_args;
  load_args.push_back(Type::getInt64Ty(context));
  load_args.push_back(Type::getInt64Ty(context));
  load_args.push_back(Type::getInt64Ty(context));
  auto *loadHelperTy = FunctionType::get(
      /* return type */ int1Ty,
      /* args vector */ load_args,
      /* isVarArg */ false);
  auto load = m.getOrInsertFunction("ToLeRaToR_load", loadHelperTy);

  /* clear */
  std::vector<Type *> clear_args;
  clear_args.push_back(Type::getInt64Ty(context));
  auto *clearHelperTy = FunctionType::get(
      /* return type */ voidTy,
      /* args vector */ clear_args,
      /* isVarArg */ false);
  auto clear = m.getOrInsertFunction("ToLeRaToR_clear", clearHelperTy);

  /* setMode */
  auto setMode = m.getOrInsertFunction(
      "ToLeRaToR_setMode",
      FunctionType::get(voidTy, Type::getInt8Ty(context), false));

  std::vector<Function *> toCount;
  for (auto &f : m) {
    toCount.push_back(&f);
  }
  ids = computeFunctionIDs(toCount);

  auto &DL = m.getDataLayout();
  IRBuilder<> IRB(context);

  /* FIXME: Bug Here! Though it works fine for current testcase related to
   * globalVariable,but this implement may be incorrect, since FirstFunction may
   * not equal to entry Function. */
  auto FirstFunction = &*(m.begin());
  auto FirstBlock = &*(FirstFunction->begin());
  auto FirstInsertPoint = &*FirstBlock->getFirstInsertionPt();
  IRB.SetInsertPoint(FirstInsertPoint);

  for (auto G_iter = m.global_begin(); G_iter != m.global_end(); G_iter++) {
    GlobalVariable *G = &*G_iter;
    Type *Ty = G->getValueType();
    // only instrument global variables who defines
    // in within the same linkage unit
    if (!Ty->isSized() || !G->hasInitializer() || !G->isDSOLocal())
      continue;
    const uint64_t SizeInBytes = DL.getTypeAllocSize(Ty);
    auto GlobalAddr = IRB.CreatePointerCast(G, IRB.getIntPtrTy(DL));
    IRB.CreateCall(local, {IRB.getInt64(-1), GlobalAddr,
                           ConstantInt::get(IRB.getInt64Ty(), SizeInBytes)});
  }

  std::vector<Instruction *> InstList;
  std::vector<Instruction *> ExitPts;
  std::vector<Instruction *> InsertPts;
  for (auto &f : m) {

    /* Must have defines in current Module, we don't care functions that only
     * have declaration in this unit, but defines otherwhere. */

    if (f.isDeclaration()) {
      continue;
    }

    InsertPts.push_back(f.getEntryBlock().getFirstNonPHIOrDbg());

    for (auto &bb : f) {
      for (auto &inst : bb) {
        if (is_interesting(&inst)) {
          InstList.push_back(&inst);
        }
        if (isa<ReturnInst>(inst)) {
          ExitPts.push_back(&inst);
        }
      }
    }
  }

  for (auto *i : ExitPts) {
    auto f = i->getParent()->getParent();
    IRB.SetInsertPoint(i);
    IRB.CreateCall(clear, {IRB.getInt64(ids[f])});
  }

  for (auto *i : InsertPts) {
    IRB.SetInsertPoint(i);
    IRB.CreateCall(setMode, {ConstantInt::get(IRB.getInt8Ty(), AT)});
  }

  for (auto &i : InstList) {
    auto f = i->getParent()->getParent();
    if (BinaryOperator *BO = dyn_cast<BinaryOperator>(i)) {
      if (BO->getOpcode() == llvm::Instruction::BinaryOps::UDiv ||
          BO->getOpcode() == llvm::Instruction::BinaryOps::SDiv ||
          BO->getOpcode() == llvm::Instruction::BinaryOps::FDiv) {
        Value *op2 = BO->getOperand(1);
        IRB.SetInsertPoint(BO);
        auto CallDiv = IRB.CreateCall(div, ArrayRef<Value *>(op2));

        /* We want DIV's if-else behave like following:
         * 1. LOGGING and IGNORING mode
         * Let runtime lib decides whether or not exit. Nothing here.
         * 2. DEFAULTING mode
         * The lib should only print the error message, but never exit. If there
         * is a error, we want the div instruction itself be value 0. I am
         * trying to solve this problem with the help of "PHI" IR, which is
         * really useful.
         * 3. BYPASSING mode
         * The lib should only print the error msg. And we need to add 'ret' in
         * block which holds ElseTerm.
         */
        auto BONext = BO->getNextNode();
        Value *Cond = dyn_cast<Value>(CallDiv);
        Instruction *ThenTerm, *ElseTerm;
        SplitBlockAndInsertIfThenElse(Cond, BONext, &ThenTerm, &ElseTerm,
                                      nullptr);
        switch (AT) {
        case LOGGING:
        case IGNORING:
          break;
        case DEFAULTING: {
          BO->moveBefore(ThenTerm);
          IRB.SetInsertPoint(BONext);
          PHINode *PN = IRB.CreatePHI(BO->getType(), 2);
          // replaceAllUsesWith must be in front of addIncoming.
          BO->replaceAllUsesWith(PN);
          PN->addIncoming(BO, BO->getParent());
          PN->addIncoming(Constant::getNullValue(BO->getType()),
                          ElseTerm->getParent());
          break;
        }
        case BYPASSING: {
          BO->moveBefore(ThenTerm);
          IRB.SetInsertPoint(ElseTerm);
          if (f->getReturnType() == voidTy)
            IRB.CreateRetVoid();
          else {
            IRB.CreateRet(Constant::getNullValue(f->getReturnType()));
          }
          /* Only one terminator allowed in a basicblock */
          ElseTerm->eraseFromParent();
          break;
        }
        }
      }
    } else if (CallBase *CB = dyn_cast<CallBase>(i)) {
      /* Attention don't use: auto fn = CB->getCalledFunction(), which
       * result in non-complete cast */
      auto fn = dyn_cast<Function>(CB->getCalledOperand()->stripPointerCasts());
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
          auto CallFree = IRB.CreateCall(unalloc, ArrayRef<Value *>(op0));

          /* We need to branch by the value of CALLFREE, aka Cond. Except
           * LOGGING mode, we need to create if {A} else {B} statement and move
           * free() to the {A}. Both {A} and {B} contains a branch.
           *
           * 1. IGNORING & DEFAULTING mode:
           * Only valid free() will go to {A}, otherwise go to {B}.
           *
           * 2. BYPASSING mode:
           * Same as IGNORING mode, except that we need add ret to {B} and
           * erase br in {B}.
           *
           * 3. LOGGING mode:
           * Do nothing, and runtime lib will handle whether or not exiting.
           *  */

          Value *Cond = dyn_cast<Value>(CallFree);
          Instruction *ThenTerm, *ElseTerm;
          SplitBlockAndInsertIfThenElse(Cond, CB->getNextNode(), &ThenTerm,
                                        &ElseTerm, nullptr);
          /* Until Now, ThenTerm & ElseTerm become branch instruction. They all
           * branch to CB's next instruction. */
          switch (AT) {
          case LOGGING: {
            break;
          }
          case IGNORING:
          case DEFAULTING: {
            CB->moveBefore(ThenTerm);
            break;
          }
          case BYPASSING: {
            CB->moveBefore(ThenTerm);
            IRB.SetInsertPoint(ElseTerm);
            if (f->getReturnType() == voidTy)
              IRB.CreateRetVoid();
            else {
              IRB.CreateRet(Constant::getNullValue(f->getReturnType()));
            }
            /* Only one terminator allowed in a basicblock */
            ElseTerm->eraseFromParent();
            break;
          }
          }
          // TODO: break;
        }
      }
    } else if (AllocaInst *AI = dyn_cast<AllocaInst>(i)) {
      /* local stack allocate */
      IRB.SetInsertPoint(AI->getNextNode());
      uint64_t TypeSize = DL.getTypeAllocSize(AI->getAllocatedType());
      Value *Len = ConstantInt::get(IRB.getInt64Ty(), TypeSize);
      if (AI->isArrayAllocation())
        Len = IRB.CreateMul(Len, AI->getArraySize());
      IRB.CreateCall(local,
                     {IRB.getInt64(ids[f]),
                      IRB.CreatePointerCast(AI, IRB.getIntPtrTy(DL)), Len});
    } else if (StoreInst *SI = dyn_cast<StoreInst>(i)) {
      /* StoreInst: store var *ptr */
      IRB.SetInsertPoint(SI);
      Value *Val = SI->getValueOperand();
      Value *Addr = SI->getPointerOperand();
      uint64_t StoreSize = DL.getTypeStoreSize(Val->getType());
      Value *Size = ConstantInt::get(IRB.getInt64Ty(), StoreSize);
      /* TODO: a better way to type judge ? */
      llvm::CallInst *CallStore;
      if (Val->getType() == Type::getInt32Ty(context) ||
          Val->getType() == Type::getInt8Ty(context) ||
          Val->getType() == Type::getInt64Ty(context)) {
        CallStore = IRB.CreateCall(
            store, {IRB.getInt64(ids[f]),
                    IRB.CreatePointerCast(Addr, IRB.getIntPtrTy(DL)),
                    IRB.CreateIntCast(Val, IRB.getInt32Ty(), false), Size});
      } else {
        CallStore = IRB.CreateCall(
            store, {IRB.getInt64(ids[f]),
                    IRB.CreatePointerCast(Addr, IRB.getIntPtrTy(DL)),
                    IRB.CreatePointerCast(Val, IRB.getInt32Ty()), Size});
      }
      Value *Cond = dyn_cast<Value>(CallStore);
      Instruction *ThenTerm, *ElseTerm;
      SplitBlockAndInsertIfThenElse(Cond, SI->getNextNode(), &ThenTerm,
                                    &ElseTerm, nullptr);
      /* Until Now, ThenTerm & ElseTerm become branch instruction. They all
       * branch to SI's next instruction. */
      switch (AT) {
      case LOGGING: {
        break;
      }
      case IGNORING:
      case DEFAULTING: {
        SI->moveBefore(ThenTerm);
        break;
      }
      case BYPASSING: {
        SI->moveBefore(ThenTerm);
        IRB.SetInsertPoint(ElseTerm);
        if (f->getReturnType() == voidTy)
          IRB.CreateRetVoid();
        else {
          IRB.CreateRet(Constant::getNullValue(f->getReturnType()));
        }
        /* Only one terminator allowed in a basicblock */
        ElseTerm->eraseFromParent();
        break;
      }
      }
    } else if (LoadInst *LI = dyn_cast<LoadInst>(i)) {
      /* LoadInst: load var *ptr */
      IRB.SetInsertPoint(LI);
      Value *Addr = LI->getPointerOperand();
      uint64_t LoadSize = DL.getTypeStoreSize(LI->getType());
      Value *Size = ConstantInt::get(IRB.getInt64Ty(), LoadSize);
      auto CallLoad = IRB.CreateCall(
          load, {IRB.getInt64(ids[f]),
                 IRB.CreatePointerCast(Addr, IRB.getIntPtrTy(DL)), Size});

      /* We want Load's if-else behave like following:
       * 1. LOGGING & IGNORING Mode:
       * Let runtime lib decides whether or not exit. Nothing here.
       * 2. DEFAULTING mode
       * If there is an error, provide it with value 0, same as DIV, by the
       * means of "PHI" IR.
       * 3. BYPASSING mode
       * The lib should only print the error msg. And we need to add 'ret' in
       * block which holds ElseTerm.
       */
      auto LINext = LI->getNextNode();
      Value *Cond = dyn_cast<Value>(CallLoad);
      Instruction *ThenTerm, *ElseTerm;
      SplitBlockAndInsertIfThenElse(Cond, LINext, &ThenTerm, &ElseTerm,
                                    nullptr);
      switch (AT) {
      case LOGGING:
      case IGNORING:
        break;
      case DEFAULTING: {
        LI->moveBefore(ThenTerm);
        IRB.SetInsertPoint(LINext);
        PHINode *PN = IRB.CreatePHI(LI->getType(), 2);
        // replaceAllUsesWith must be in front of addIncoming.
        LI->replaceAllUsesWith(PN);
        PN->addIncoming(LI, LI->getParent());
        PN->addIncoming(Constant::getNullValue(LI->getType()),
                        ElseTerm->getParent());
        break;
      }
      case BYPASSING: {
        LI->moveBefore(ThenTerm);
        IRB.SetInsertPoint(ElseTerm);
        if (f->getReturnType() == voidTy)
          IRB.CreateRetVoid();
        else {
          IRB.CreateRet(Constant::getNullValue(f->getReturnType()));
        }
        /* Only one terminator allowed in a basicblock */
        ElseTerm->eraseFromParent();
        break;
      }
      }
    }
    IRB.SetInsertPoint(i);
  }

  return true;
}
