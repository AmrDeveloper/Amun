#include "../include/jot_typechecker.hpp"
#include "../include/jot_ast_visitor.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"

#include <any>
#include <memory>
#include <string>

void JotTypeChecker::check_compilation_unit(std::shared_ptr<CompilationUnit> compilation_unit) {
    auto statements = compilation_unit->get_tree_nodes();
    try {
        for (auto &statement : statements) {
            statement->accept(this);
        }
    } catch (const std::bad_any_cast &e) {
        jot::loge << "TypeChecker: " << e.what() << '\n';
    } catch (const char *message) {
    }
}

std::any JotTypeChecker::visit(BlockStatement *node) {
    for (auto &statement : node->get_nodes()) {
        statement->accept(this);
    }
    return 0;
}

std::any JotTypeChecker::visit(FieldDeclaration *node) {
    auto left_type = node->get_type();
    auto right_type = node_jot_type(node->get_value()->accept(this));

    bool is_left_none_type = is_none_type(left_type);
    bool is_right_none_type = is_none_type(right_type);
    if (is_left_none_type and is_right_none_type) {
        context->diagnostics.add_diagnostic(
            node->get_name().get_span(),
            "Can't resolve field type when both rvalue and lvalue are unkown");
        throw "Stop";
    }

    if (is_left_none_type) {
        node->set_type(right_type);
        left_type = right_type;
    }

    if (is_right_none_type) {
        node->get_value()->set_type_node(left_type);
        right_type = left_type;
    }

    if (not left_type->equals(right_type)) {
        context->diagnostics.add_diagnostic(node->get_name().get_span(),
                                            "Type missmatch expect " + left_type->type_literal() +
                                                " but got " + right_type->type_literal());
        throw "Stop";
    }

    auto name = node->get_name().get_literal();
    bool is_first_defined = symbol_table->define(name, left_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic(
            node->get_name().get_span(), "Field " + name + " is defined twice in the same scope");
        throw "Stop";
    }

    return 0;
}

std::any JotTypeChecker::visit(FunctionPrototype *node) {
    auto name = node->get_name();
    std::vector<std::shared_ptr<JotType>> parameters;
    for (auto &parameter : node->get_parameters()) {
        parameters.push_back(parameter->get_type());
    }
    auto return_type = node->get_return_type();
    auto function_type = std::make_shared<JotFunctionType>(name, parameters, return_type);
    bool is_first_defined = symbol_table->define(name.get_literal(), function_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic(node->get_name().get_span(),
                                            "function " + name.get_literal() +
                                                " is defined twice in the same scope");
        throw "Stop";
    }
    return function_type;
}

std::any JotTypeChecker::visit(FunctionDeclaration *node) {
    auto prototype = node->get_prototype();
    auto function_type = node->get_prototype()->accept(this);
    push_new_scope();
    for (auto &parameter : prototype->get_parameters()) {
        symbol_table->define(parameter->get_name().get_literal(), parameter->get_type());
    }
    node->get_body()->accept(this);
    pop_current_scope();
    return function_type;
}

std::any JotTypeChecker::visit(EnumDeclaration *node) {
    auto name = node->get_name().get_literal();
    auto enum_type = std::dynamic_pointer_cast<JotEnumType>(node->get_enum_type());
    auto enum_element_type = enum_type->get_element_type();
    if (not is_integer_type(enum_element_type)) {
        context->diagnostics.add_diagnostic(node->get_name().get_span(),
                                            "Enum element type must be aa integer type");
        throw "Stop";
    }

    auto element_size = enum_type->get_enum_values().size();
    if (element_size > 2 && is_boolean_type(enum_element_type)) {
        context->diagnostics.add_diagnostic(
            node->get_name().get_span(),
            "Enum with bool (int1) type can't has more than 2 elements");
        throw "Stop";
    }

    bool is_first_defined = symbol_table->define(name, enum_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic(node->get_name().get_span(),
                                            "enumeration " + name +
                                                " is defined twice in the same scope");
        throw "Stop";
    }
    return is_first_defined;
}

std::any JotTypeChecker::visit(IfStatement *node) {
    for (auto &conditional_block : node->get_conditional_blocks()) {
        auto condition = node_jot_type(conditional_block->get_condition()->accept(this));
        if (not is_number_type(condition)) {
            context->diagnostics.add_diagnostic(conditional_block->get_position().get_span(),
                                                "if condition mush be a number but got " +
                                                    condition->type_literal());
            throw "Stop";
        }
        push_new_scope();
        conditional_block->get_body()->accept(this);
        pop_current_scope();
    }
    return 0;
}

std::any JotTypeChecker::visit(WhileStatement *node) {
    auto left_type = node_jot_type(node->get_condition()->accept(this));
    if (not is_number_type(left_type)) {
        context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                            "While condition mush be a number but got " +
                                                left_type->type_literal());
        throw "Stop";
    }
    push_new_scope();
    node->get_body()->accept(this);
    pop_current_scope();
    return 0;
}

std::any JotTypeChecker::visit(ReturnStatement *node) { return node->return_value()->accept(this); }

std::any JotTypeChecker::visit(ExpressionStatement *node) {
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(IfExpression *node) {
    auto if_value = node_jot_type(node->get_if_value()->accept(this));
    auto else_value = node_jot_type(node->get_else_value()->accept(this));
    if (not if_value->equals(else_value)) {
        context->diagnostics.add_diagnostic(node->get_if_position().get_span(),
                                            "If Expression Type missmatch expect " +
                                                if_value->type_literal() + " but got " +
                                                else_value->type_literal());
        throw "Stop";
    }

    return if_value;
}

std::any JotTypeChecker::visit(GroupExpression *node) {
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(AssignExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    if (not left_type->equals(right_type)) {
        context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                            "Type missmatch expect " + left_type->type_literal() +
                                                " but got " + right_type->type_literal());
        throw "Stop";
    }

    return right_type;
}

std::any JotTypeChecker::visit(BinaryExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    bool is_left_number = is_number_type(left_type);
    bool is_right_number = is_number_type(right_type);
    if (not is_left_number || not is_right_number) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                                "Expected binary left to be number but got " +
                                                    left_type->type_literal());
        }
        if (not is_right_number) {
            context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                                "Expected binary right to be number but got " +
                                                    left_type->type_literal());
        }
        throw "Stop";
    }

    return left_type;
}

std::any JotTypeChecker::visit(ComparisonExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    bool is_left_number = is_number_type(left_type) or is_enum_element_type(left_type);
    bool is_right_number = is_number_type(right_type) or is_enum_element_type(right_type);
    bool is_the_same = left_type->equals(right_type);
    if (not is_left_number || not is_right_number || not is_the_same) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic(
                node->get_operator_token().get_span(),
                "Expected Comparison left to be number or enum element but got " +
                    left_type->type_literal());
        }
        if (not is_right_number) {
            context->diagnostics.add_diagnostic(
                node->get_operator_token().get_span(),
                "Expected Comparison right to be number or enum element but got " +
                    left_type->type_literal());
        }

        if (not is_the_same) {
            context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                                "Expected Comparison type missmatch " +
                                                    left_type->type_literal() + " and " +
                                                    right_type->type_literal());
        }

        throw "Stop";
    }

    return node_jot_type(node->get_type_node());
}

std::any JotTypeChecker::visit(LogicalExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    // TODO: Assert that they are int1 (bool)
    bool is_left_number = is_number_type(left_type);
    bool is_right_number = is_number_type(right_type);
    if (not is_left_number || not is_right_number) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                                "Expected Logical left to be number but got " +
                                                    left_type->type_literal());
        }
        if (not is_right_number) {
            context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                                "Expected Logical right to be number but got " +
                                                    left_type->type_literal());
        }
        throw "Stop";
    }

    return node_jot_type(node->get_type_node());
}

std::any JotTypeChecker::visit(UnaryExpression *node) {
    auto operand_type = node_jot_type(node->get_right()->accept(this));
    auto unary_operator = node->get_operator_token().get_kind();

    if (unary_operator == TokenKind::Minus) {
        if (is_number_type(operand_type))
            return operand_type;
        context->diagnostics.add_diagnostic(
            node->get_operator_token().get_span(),
            "Unary - operator require number as an right operand but got " +
                operand_type->type_literal());
        throw "Stop";
    }

    if (unary_operator == TokenKind::Bang) {
        if (is_number_type(operand_type))
            return operand_type;
        context->diagnostics.add_diagnostic(
            node->get_operator_token().get_span(),
            "Unary - operator require boolean as an right operand but got " +
                operand_type->type_literal());
        throw "Stop";
    }

    if (unary_operator == TokenKind::Not) {
        if (is_number_type(operand_type))
            return operand_type;
        context->diagnostics.add_diagnostic(
            node->get_operator_token().get_span(),
            "Unary ~ operator require number as an right operand but got " +
                operand_type->type_literal());
        throw "Stop";
    }

    if (unary_operator == TokenKind::Star) {
        if (operand_type->get_type_kind() == TypeKind::Pointer) {
            auto pointer_type = std::dynamic_pointer_cast<JotPointerType>(operand_type);
            auto type = pointer_type->get_point_to();
            node->set_type_node(type);
            return type;
        }

        context->diagnostics.add_diagnostic(
            node->get_operator_token().get_span(),
            "Derefernse operator require pointer as an right operand but got " +
                operand_type->type_literal());
        throw "Stop";
    }

    if (unary_operator == TokenKind::And) {
        auto pointer_type =
            std::make_shared<JotPointerType>(operand_type->get_type_token(), operand_type);
        node->set_type_node(pointer_type);
        return pointer_type;
    }

    context->diagnostics.add_diagnostic(node->get_operator_token().get_span(),
                                        "Unsupported unary expression " +
                                            operand_type->type_literal());
    throw "Stop";
}

std::any JotTypeChecker::visit(CallExpression *node) {
    if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(node->get_callee())) {
        auto name = literal->get_name().get_literal();
        if (symbol_table->is_defined(name)) {
            auto lookup = symbol_table->lookup(name);
            auto value = node_jot_type(symbol_table->lookup(name));
            if (auto functional_type = std::dynamic_pointer_cast<JotPointerType>(value)) {
                value = functional_type->get_point_to();
            }
            node->set_type_node(value);
            if (value->get_type_kind() == TypeKind::Function) {
                auto type = std::dynamic_pointer_cast<JotFunctionType>(value);
                node->set_type_node(type);
                auto parameters = type->get_parameters();
                auto arguments = node->get_arguments();
                if (parameters.size() != arguments.size()) {
                    context->diagnostics.add_diagnostic(
                        node->get_position().get_span(),
                        "Invalid number of arguments " + name + " expect " +
                            std::to_string(parameters.size()) + " but got " +
                            std::to_string(arguments.size()));
                    throw "Stop";
                }

                std::vector<std::shared_ptr<JotType>> arguments_types;
                for (auto &argument : arguments) {
                    arguments_types.push_back(node_jot_type(argument->accept(this)));
                }

                size_t arguments_size = arguments_types.size();
                for (size_t i = 0; i < arguments_size; i++) {
                    if (not parameters[i]->equals(arguments_types[i])) {
                        context->diagnostics.add_diagnostic(
                            node->get_position().get_span(),
                            "Argument type didn't match parameter type expect " +
                                parameters[i]->type_literal() + " got " +
                                arguments_types[i]->type_literal());
                        throw "Stop";
                    }
                }

                return type->get_return_type();
            } else {
                context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                                    "Call expression work only with function");
                throw "Stop";
            }
        } else {
            context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                                "Can't resolve function call with name " + name);
            throw "Stop";
        }
    } else {
        context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                            "Call expression must be a literal");
        throw "Stop";
    }
}

std::any JotTypeChecker::visit(IndexExpression *node) {
    auto callee_type = node_jot_type(node->get_value()->accept(this));
    if (callee_type->get_type_kind() == TypeKind::Array) {
        auto array_type = std::dynamic_pointer_cast<JotArrayType>(callee_type);
        node->set_type_node(array_type->get_element_type());
    } else if (callee_type->get_type_kind() == TypeKind::Pointer) {
        auto pointer_type = std::dynamic_pointer_cast<JotPointerType>(callee_type);
        node->set_type_node(pointer_type->get_point_to());
    } else {
        context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                            "Index expression require array but got " +
                                                callee_type->type_literal());
        throw "Stop";
    }

    auto index_type = node_jot_type(node->get_index()->accept(this));
    if (index_type->get_type_kind() != TypeKind::Number) {
        context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                            "Index must be a number but got " +
                                                index_type->type_literal());
        throw "Stop";
    }

    return node->get_type_node();
}

std::any JotTypeChecker::visit(EnumAccessExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(LiteralExpression *node) {
    auto name = node->get_name().get_literal();
    if (symbol_table->is_defined(name)) {
        auto value = symbol_table->lookup(name);
        auto type = node_jot_type(value);
        node->set_type(type);
        return type;
    } else {
        context->diagnostics.add_diagnostic(node->get_name().get_span(),
                                            "Can't resolve variable with name " +
                                                node->get_name().get_literal());
        throw "Stop";
    }
}

std::any JotTypeChecker::visit(NumberExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(ArrayExpression *node) {
    auto values = node->get_values();
    for (int i = 1; i < values.size(); i++) {
        if (not values[i]->get_type_node()->equals(values[i - 1]->get_type_node())) {
            context->diagnostics.add_diagnostic(node->get_position().get_span(),
                                                "Array elements with index " +
                                                    std::to_string(i - 1) + " and " +
                                                    std::to_string(i) + " are not the same types");
            throw "Stop";
        }
    }
    return node->get_type_node();
}

std::any JotTypeChecker::visit(StringExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(CharacterExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(BooleanExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(NullExpression *node) { return node->get_type_node(); }

std::shared_ptr<JotType> JotTypeChecker::node_jot_type(std::any any_type) {
    if (any_type.type() == typeid(std::shared_ptr<JotFunctionType>)) {
        return std::any_cast<std::shared_ptr<JotFunctionType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotPointerType>)) {
        return std::any_cast<std::shared_ptr<JotPointerType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotNumberType>)) {
        return std::any_cast<std::shared_ptr<JotNumberType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotArrayType>)) {
        return std::any_cast<std::shared_ptr<JotArrayType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotEnumType>)) {
        return std::any_cast<std::shared_ptr<JotEnumType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotEnumElementType>)) {
        return std::any_cast<std::shared_ptr<JotEnumElementType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotNoneType>)) {
        return std::any_cast<std::shared_ptr<JotNoneType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotVoidType>)) {
        return std::any_cast<std::shared_ptr<JotVoidType>>(any_type);
    }
    if (any_type.type() == typeid(std::shared_ptr<JotNullType>)) {
        return std::any_cast<std::shared_ptr<JotNullType>>(any_type);
    }
    return std::any_cast<std::shared_ptr<JotType>>(any_type);
}

bool JotTypeChecker::is_number_type(const std::shared_ptr<JotType> &type) {
    return type->get_type_kind() == TypeKind::Number;
}

bool JotTypeChecker::is_integer_type(std::shared_ptr<JotType> &type) {
    if (type->get_type_kind() == TypeKind::Number) {
        auto number_type = std::dynamic_pointer_cast<JotNumberType>(type);
        return number_type->is_integer();
    }
    return false;
}

bool JotTypeChecker::is_enum_element_type(const std::shared_ptr<JotType> &type) {
    return type->get_type_kind() == TypeKind::EnumerationElement;
}

bool JotTypeChecker::is_boolean_type(std::shared_ptr<JotType> &type) {
    if (type->get_type_kind() == TypeKind::Number) {
        auto number_type = std::dynamic_pointer_cast<JotNumberType>(type);
        return number_type->is_boolean();
    }
    return false;
}

bool JotTypeChecker::is_pointer_type(const std::shared_ptr<JotType> &type) {
    return type->get_type_kind() == TypeKind::Pointer;
}

bool JotTypeChecker::is_none_type(const std::shared_ptr<JotType> &type) {
    if (type->get_type_kind() == TypeKind::None)
        return true;
    if (type->get_type_kind() == TypeKind::Array) {
        auto array_type = std::dynamic_pointer_cast<JotArrayType>(type);
        return array_type->get_element_type()->get_type_kind() == TypeKind::None;
    }
    if (type->get_type_kind() == TypeKind::Pointer) {
        auto array_type = std::dynamic_pointer_cast<JotPointerType>(type);
        return array_type->get_point_to()->get_type_kind() == TypeKind::None;
    }
    return false;
}

bool JotTypeChecker::is_same_type(const std::shared_ptr<JotType> &left,
                                  const std::shared_ptr<JotType> &right) {
    return left->get_type_kind() == right->get_type_kind();
}

void JotTypeChecker::push_new_scope() {
    symbol_table = std::make_shared<JotSymbolTable>(symbol_table);
}

void JotTypeChecker::pop_current_scope() { symbol_table = symbol_table->get_parent_symbol_table(); }