#pragma once

#include "jot_type.hpp"
#include "jot_token.hpp"

class TypeNode {
public:
    TypeNode(JotType type, TokenSpan position) 
        : type(type), position(position) {}

    JotType get_type() { return type; }

    TokenSpan get_position() { return position; }

private:
    JotType type;
    TokenSpan position;
};
