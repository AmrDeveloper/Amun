#pragma once

#include <llvm/IR/Function.h>

namespace amun {

enum class DeferCallKind : int8_t {
    DEFER_FUNCTION_CALL,
    DEFER_FUNCTION_PTR_CALL,
};

struct DeferCall {
    DeferCallKind defer_kind;
};

struct DeferFunctionCall : public DeferCall {
    DeferFunctionCall(llvm::Function* function, std::vector<llvm::Value*> arguments)
        : function(function), arguments(std::move(arguments))
    {
        defer_kind = DeferCallKind::DEFER_FUNCTION_CALL;
    }
    llvm::Function* function;
    std::vector<llvm::Value*> arguments;
};

struct DeferFunctionPtrCall : public DeferCall {
    DeferFunctionPtrCall(llvm::FunctionType* function_type, llvm::Value* callee,
                         std::vector<llvm::Value*> arguments)
        : function_type(function_type), callee(callee), arguments(std::move(arguments))
    {
        defer_kind = DeferCallKind::DEFER_FUNCTION_PTR_CALL;
    }
    llvm::FunctionType* function_type;
    llvm::Value* callee;
    std::vector<llvm::Value*> arguments;
};

} // namespace amun