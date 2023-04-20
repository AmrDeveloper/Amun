#include "../include/amun_alias_table.hpp"
#include "../include/amun_primitives.hpp"

amun::AliasTable::AliasTable() { config_type_alias_table(); }

auto amun::AliasTable::define_alias(std::string alias, Shared<amun::Type> type) -> void
{
    type_alias_table[alias] = type;
}

auto amun::AliasTable::resolve_alias(std::string alias) -> Shared<amun::Type>
{
    return type_alias_table[alias];
}

auto amun::AliasTable::contains(std::string alias) -> bool
{
    return type_alias_table.contains(alias);
}

auto amun::AliasTable::config_type_alias_table() -> void
{
    // Define singed integers primitives
    type_alias_table["int1"] = amun::i1_type;
    type_alias_table["int8"] = amun::i8_type;
    type_alias_table["int16"] = amun::i6_type;
    type_alias_table["int32"] = amun::i32_type;
    type_alias_table["int64"] = amun::i64_type;

    // Define unsinged integers primitives
    type_alias_table["uint8"] = amun::u8_type;
    type_alias_table["uint16"] = amun::u16_type;
    type_alias_table["uint32"] = amun::u32_type;
    type_alias_table["uint64"] = amun::u64_type;

    // Define floating points types
    type_alias_table["float32"] = amun::f32_type;
    type_alias_table["float64"] = amun::f64_type;
}