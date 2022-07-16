#pragma once

#include "jot_type.hpp"
#include "jot_token.hpp"

#include <memory>

class TypeNode {

};

class AbsoluteTypeNode : public TypeNode {
public:
    AbsoluteTypeNode(JotType type, TokenSpan position)
        : type(type), position(position) {}

    JotType get_type() { return type; }

    TokenSpan get_position() { return position; }

private:
    JotType type;
    TokenSpan position;
};

class UnaryTypeNode : public TypeNode {
public:
    UnaryTypeNode(Token unary_type, std::shared_ptr<TypeNode> type)
        : unary_type(unary_type), type(type) {}

private:
    Token unary_type;
    std::shared_ptr<TypeNode> type;
};

class NamedTypeNode : public TypeNode {
public:
    NamedTypeNode(Token name): name(name) {}
private:
    Token name;
};