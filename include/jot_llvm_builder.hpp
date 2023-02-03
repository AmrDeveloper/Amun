#pragma once

#include "jot_basic.hpp"
#include "jot_llvm_type.hpp"

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>

inline llvm::ConstantInt* create_llvm_int1(bool value)
{
    return llvm::ConstantInt::get(llvm_int1_type, value);
}

inline llvm::ConstantInt* create_llvm_int8(int32 value, bool singed)
{
    return llvm::ConstantInt::get(llvm_int8_type, value, singed);
}

inline llvm::ConstantInt* create_llvm_int16(int32 value, bool singed)
{
    return llvm::ConstantInt::get(llvm_int16_type, value, singed);
}

inline llvm::ConstantInt* create_llvm_int32(int32 value, bool singed)
{
    return llvm::ConstantInt::get(llvm_int32_type, value, singed);
}

inline llvm::ConstantInt* create_llvm_int64(int64 value, bool singed)
{
    return llvm::ConstantInt::get(llvm_int64_type, value, singed);
}

inline llvm::Constant* create_llvm_float32(float32 value)
{
    return llvm::ConstantFP::get(llvm_float32_type, value);
}

inline llvm::Constant* create_llvm_float64(float64 value, bool singed)
{
    return llvm::ConstantFP::get(llvm_float64_type, value);
}

inline llvm::Constant* create_llvm_null(llvm::Type* type)
{
    return llvm::Constant::getNullValue(type);
}

inline llvm::ArrayType* create_llvm_array_type(llvm::Type* element, uint64 size)
{
    return llvm::ArrayType::get(element, size);
}

inline llvm::Value* derefernecs_llvm_pointer(llvm::Value* pointer)
{
    assert(pointer->getType()->isPointerTy());
    auto pointer_element_type = pointer->getType()->getPointerElementType();
    return Builder.CreateLoad(pointer_element_type, pointer);
}
