#pragma once

#include <string>
#include <unordered_map>

#include <llvm/IR/Intrinsics.h>

// A list of supported LLVM infrastructure intrinsic function
static std::unordered_map<std::string, llvm::Intrinsic::ID> llvm_intrinsics_map = {
    {"llvm.cos", llvm::Intrinsic::cos},
};