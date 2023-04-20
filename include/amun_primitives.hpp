#pragma once

#include "amun_type.hpp"

namespace amun {

static auto i1_type = std::make_shared<amun::NumberType>(amun::NumberKind::INTEGER_1);
static auto i8_type = std::make_shared<amun::NumberType>(amun::NumberKind::INTEGER_8);
static auto i6_type = std::make_shared<amun::NumberType>(amun::NumberKind::INTEGER_16);
static auto i32_type = std::make_shared<amun::NumberType>(amun::NumberKind::INTEGER_32);
static auto i64_type = std::make_shared<amun::NumberType>(amun::NumberKind::INTEGER_64);

static auto u8_type = std::make_shared<amun::NumberType>(amun::NumberKind::U_INTEGER_8);
static auto u16_type = std::make_shared<amun::NumberType>(amun::NumberKind::U_INTEGER_16);
static auto u32_type = std::make_shared<amun::NumberType>(amun::NumberKind::U_INTEGER_32);
static auto u64_type = std::make_shared<amun::NumberType>(amun::NumberKind::U_INTEGER_64);

static auto i8_ptr_type = std::make_shared<amun::PointerType>(amun::i8_type);
static auto i32_ptr_type = std::make_shared<amun::PointerType>(amun::i32_type);

static auto f32_type = std::make_shared<amun::NumberType>(amun::NumberKind::FLOAT_32);
static auto f64_type = std::make_shared<amun::NumberType>(amun::NumberKind::FLOAT_64);

static auto void_type = std::make_shared<amun::VoidType>();
static auto null_type = std::make_shared<amun::NullType>();
static auto none_type = std::make_shared<amun::NoneType>();

static auto none_ptr_type = std::make_shared<amun::PointerType>(amun::none_type);

} // namespace amun