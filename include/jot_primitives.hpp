#pragma once

#include "jot_type.hpp"

static auto jot_int1_ty = std::make_shared<JotNumberType>(NumberKind::INTEGER_1);
static auto jot_int8_ty = std::make_shared<JotNumberType>(NumberKind::INTEGER_8);
static auto jot_int16_ty = std::make_shared<JotNumberType>(NumberKind::INTEGER_16);
static auto jot_int32_ty = std::make_shared<JotNumberType>(NumberKind::INTEGER_32);
static auto jot_int64_ty = std::make_shared<JotNumberType>(NumberKind::INTEGER_64);

static auto jot_uint8_ty = std::make_shared<JotNumberType>(NumberKind::U_INTEGER_8);
static auto jot_uint16_ty = std::make_shared<JotNumberType>(NumberKind::U_INTEGER_16);
static auto jot_uint32_ty = std::make_shared<JotNumberType>(NumberKind::U_INTEGER_32);
static auto jot_uint64_ty = std::make_shared<JotNumberType>(NumberKind::U_INTEGER_64);

static auto jot_int32ptr_ty = std::make_shared<JotPointerType>(jot_int32_ty);

static auto jot_float32_ty = std::make_shared<JotNumberType>(NumberKind::FLOAT_32);
static auto jot_float64_ty = std::make_shared<JotNumberType>(NumberKind::FLOAT_64);

static auto jot_int8ptr_ty = std::make_shared<JotPointerType>(jot_int8_ty);

static auto jot_void_ty = std::make_shared<JotVoidType>();
static auto jot_null_ty = std::make_shared<JotNullType>();
static auto jot_none_ty = std::make_shared<JotNoneType>();

static auto jot_none_ptr_ty = std::make_shared<JotPointerType>(jot_none_ty);