#include "../include/jot_alias_table.hpp"
#include "../include/jot_primitives.hpp"

JotAliasTable::JotAliasTable() { config_type_alias_table(); }

auto JotAliasTable::define_alias(std::string alias, Shared<JotType> type) -> void
{
    type_alias_table[alias] = type;
}

auto JotAliasTable::resolve_alias(std::string alias) -> Shared<JotType>
{
    return type_alias_table[alias];
}

auto JotAliasTable::contains(std::string alias) -> bool { return type_alias_table.contains(alias); }

auto JotAliasTable::config_type_alias_table() -> void
{
    // Define singed integers primitives
    type_alias_table["int1"] = jot_int1_ty;
    type_alias_table["int8"] = jot_int8_ty;
    type_alias_table["int16"] = jot_int16_ty;
    type_alias_table["int32"] = jot_int32_ty;
    type_alias_table["int64"] = jot_int64_ty;

    // Define unsinged integers primitives
    type_alias_table["uint8"] = jot_uint8_ty;
    type_alias_table["uint16"] = jot_uint16_ty;
    type_alias_table["uint32"] = jot_uint32_ty;
    type_alias_table["uint64"] = jot_uint64_ty;

    // Define floating points types
    type_alias_table["float32"] = jot_float32_ty;
    type_alias_table["float64"] = jot_float64_ty;
}