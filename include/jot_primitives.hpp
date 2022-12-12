#pragma once

#include "jot_type.hpp"

static auto jot_int1_ty = std::make_shared<JotNumberType>(NumberKind::Integer1);
static auto jot_int8_ty = std::make_shared<JotNumberType>(NumberKind::Integer8);
static auto jot_int16_ty = std::make_shared<JotNumberType>(NumberKind::Integer16);
static auto jot_int32_ty = std::make_shared<JotNumberType>(NumberKind::Integer32);
static auto jot_int64_ty = std::make_shared<JotNumberType>(NumberKind::Integer64);

static auto jot_float32_ty = std::make_shared<JotNumberType>(NumberKind::Float32);
static auto jot_float64_ty = std::make_shared<JotNumberType>(NumberKind::Float64);

static auto jot_int8ptr_ty = std::make_shared<JotPointerType>(jot_int8_ty);

static auto jot_void_ty = std::make_shared<JotVoidType>();
static auto jot_null_ty = std::make_shared<JotNullType>();
static auto jot_none_ty = std::make_shared<JotNoneType>();

static auto jot_none_ptr_ty = std::make_shared<JotPointerType>(jot_none_ty);