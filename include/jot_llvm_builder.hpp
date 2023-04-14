#pragma once

#include "jot_basic.hpp"
#include "jot_llvm_type.hpp"

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>

inline auto create_llvm_int1(bool value) -> llvm::ConstantInt*
{
    return llvm::ConstantInt::get(llvm_int1_type, value);
}

inline auto create_llvm_int8(int32 value, bool singed) -> llvm::ConstantInt*
{
    return llvm::ConstantInt::get(llvm_int8_type, value, singed);
}

inline auto create_llvm_int16(int32 value, bool singed) -> llvm::ConstantInt*
{
    return llvm::ConstantInt::get(llvm_int16_type, value, singed);
}

inline auto create_llvm_int32(int32 value, bool singed) -> llvm::ConstantInt*
{
    return llvm::ConstantInt::get(llvm_int32_type, value, singed);
}

inline auto create_llvm_int64(int64 value, bool singed) -> llvm::ConstantInt*
{
    return llvm::ConstantInt::get(llvm_int64_type, value, singed);
}

inline auto create_llvm_float32(float32 value) -> llvm::Constant*
{
    return llvm::ConstantFP::get(llvm_float32_type, value);
}

inline auto create_llvm_float64(float64 value, bool singed) -> llvm::Constant*
{
    return llvm::ConstantFP::get(llvm_float64_type, value);
}

inline auto create_llvm_null(llvm::Type* type) -> llvm::Constant*
{
    return llvm::Constant::getNullValue(type);
}

inline auto create_llvm_array_type(llvm::Type* element, uint64 size) -> llvm::ArrayType*
{
    return llvm::ArrayType::get(element, size);
}

inline auto derefernecs_llvm_pointer(llvm::Value* pointer) -> llvm::Value*
{
    assert(pointer->getType()->isPointerTy());
    auto pointer_element_type = pointer->getType()->getPointerElementType();
    return Builder.CreateLoad(pointer_element_type, pointer);
}
