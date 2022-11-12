#include "../include/jot_typechecker.hpp"
#include "../include/jot_ast_visitor.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"

#include <any>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>

void JotTypeChecker::check_compilation_unit(std::shared_ptr<CompilationUnit> compilation_unit) {
    auto statements = compilation_unit->get_tree_nodes();
    try {
        // Iterate over top level nodes
        for (auto &statement : statements) {
            statement->accept(this);
        }
    } catch (...) {
    }
}

std::any JotTypeChecker::visit(BlockStatement *node) {
    for (auto &statement : node->get_nodes()) {
        statement->accept(this);
        // Here we can report warn for unreachable code after return, continue or break keyword
        // We can make it error after return, must modify the diagnostics engine first
    }
    return 0;
}

std::any JotTypeChecker::visit(FieldDeclaration *node) {
    auto left_type = node->get_type();
    auto right_value = node->get_value();
    auto name = node->get_name().get_literal();

    // If field has initalizer, check that initalizer type is matching the declaration type
    // If declaration type is null, infier it using the rvalue type
    if (right_value != nullptr) {
        auto right_type = node_jot_type(right_value->accept(this));

        if (node->is_global() and !right_value->is_constant()) {
            context->diagnostics.add_diagnostic_error(
                node->get_name().get_span(), "Initializer element is not a compile-time constant");
            throw "Stop";
        }

        bool is_left_none_type = is_none_type(left_type);
        bool is_right_none_type = is_none_type(right_type);

        if (is_left_none_type and is_right_none_type) {
            context->diagnostics.add_diagnostic_error(
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
            context->diagnostics.add_diagnostic_error(
                node->get_name().get_span(), "Type missmatch expect " + left_type->type_literal() +
                                                 " but got " + right_type->type_literal());
            throw "Stop";
        }
    }

    bool is_first_defined = symbol_table->define(name, left_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic_error(
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
    auto function_type =
        std::make_shared<JotFunctionType>(name, parameters, return_type, node->has_varargs());
    bool is_first_defined = symbol_table->define(name.get_literal(), function_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic_error(node->get_name().get_span(),
                                                  "function " + name.get_literal() +
                                                      " is defined twice in the same scope");
        throw "Stop";
    }
    return function_type;
}

std::any JotTypeChecker::visit(FunctionDeclaration *node) {
    auto prototype = node->get_prototype();
    auto function_type = node_jot_type(node->get_prototype()->accept(this));
    auto function = std::dynamic_pointer_cast<JotFunctionType>(function_type);
    current_function_return_type = function->get_return_type();

    push_new_scope();
    for (auto &parameter : prototype->get_parameters()) {
        symbol_table->define(parameter->get_name().get_literal(), parameter->get_type());
    }
    node->get_body()->accept(this);
    pop_current_scope();

    return function_type;
}

std::any JotTypeChecker::visit(StructDeclaration *node) {
    auto struct_type = node->get_struct_type();
    auto struct_name = struct_type->get_type_token().get_literal();
    symbol_table->define(struct_type->get_type_token().get_literal(), struct_type);
    return nullptr;
}

std::any JotTypeChecker::visit(EnumDeclaration *node) {
    auto name = node->get_name().get_literal();
    auto enum_type = std::dynamic_pointer_cast<JotEnumType>(node->get_enum_type());
    auto enum_element_type = enum_type->get_element_type();
    if (not is_integer_type(enum_element_type)) {
        context->diagnostics.add_diagnostic_error(node->get_name().get_span(),
                                                  "Enum element type must be aa integer type");
        throw "Stop";
    }

    auto element_size = enum_type->get_enum_values().size();
    if (element_size > 2 && is_boolean_type(enum_element_type)) {
        context->diagnostics.add_diagnostic_error(
            node->get_name().get_span(),
            "Enum with bool (int1) type can't has more than 2 elements");
        throw "Stop";
    }

    bool is_first_defined = symbol_table->define(name, enum_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic_error(node->get_name().get_span(),
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
            context->diagnostics.add_diagnostic_error(conditional_block->get_position().get_span(),
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
        context->diagnostics.add_diagnostic_error(node->get_position().get_span(),
                                                  "While condition mush be a number but got " +
                                                      left_type->type_literal());
        throw "Stop";
    }
    push_new_scope();
    node->get_body()->accept(this);
    pop_current_scope();
    return 0;
}

std::any JotTypeChecker::visit(SwitchStatement *node) {
    // Check that switch argument is integer type
    auto argument = node_jot_type(node->get_argument()->accept(this));
    if ((not is_integer_type(argument)) and (not is_enum_element_type(argument))) {
        context->diagnostics.add_diagnostic_error(
            argument->get_type_position(),
            "Switch argument type must be integer or enum element but found " +
                argument->type_literal());
        throw "Stop";
    }

    // Check that all cases values are integers or enum element, and no duplication
    // TODO: Optimize set by replacing std::string by integers for fast comparing
    std::unordered_set<std::string> cases_values;
    for (auto &branch : node->get_cases()) {
        auto values = branch->get_values();
        // Check each value of this case
        for (auto &value : values) {
            auto value_node_type = value->get_ast_node_type();
            if (value_node_type == AstNodeType::NumberExpr or
                value_node_type == AstNodeType::EnumElementExpr) {

                // No need to check if it enum element,
                // but if it number we need to assert that it integer
                if (value_node_type == AstNodeType::NumberExpr) {
                    auto value_type = node_jot_type(value->accept(this));
                    auto numeric_type = std::dynamic_pointer_cast<JotNumberType>(value_type);
                    if (not numeric_type->is_integer()) {
                        context->diagnostics.add_diagnostic_error(
                            branch->get_position().get_span(),
                            "Switch case value must be an integer but found " +
                                numeric_type->type_literal());
                        throw "Stop";
                    }
                }

                // Check that all cases values are uniques
                if (auto number = std::dynamic_pointer_cast<NumberExpression>(value)) {
                    if (not cases_values.insert(number->get_value().get_literal()).second) {
                        context->diagnostics.add_diagnostic_error(
                            branch->get_position().get_span(),
                            "Switch can't has more than case with the same constants value");
                        throw "Stop";
                    }
                } else if (auto enum_element =
                               std::dynamic_pointer_cast<EnumAccessExpression>(value)) {
                    auto enum_index_string = std::to_string(enum_element->get_enum_element_index());
                    if (not cases_values.insert(enum_index_string).second) {
                        context->diagnostics.add_diagnostic_error(
                            branch->get_position().get_span(),
                            "Switch can't has more than case with the same constants value");
                        throw "Stop";
                    }
                }

                // This value is valid, so continue to the next one
                continue;
            }

            // Report Error if value is not number or enum element
            context->diagnostics.add_diagnostic_error(
                branch->get_position().get_span(),
                "Switch case value must be an integer but found non constants integer type");
            throw "Stop";
        }

        // Check the branch body once inside new scope
        push_new_scope();
        branch->get_body()->accept(this);
        pop_current_scope();
    }

    // Check default branch body if exists inside new scope
    auto default_branch = node->get_default_case();
    if (default_branch) {
        auto body = default_branch;
        push_new_scope();
        default_branch->get_body()->accept(this);
        pop_current_scope();
    }

    return 0;
}

std::any JotTypeChecker::visit(ReturnStatement *node) {
    if (not node->has_value()) {
        if (current_function_return_type->get_type_kind() != TypeKind::Void) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().get_span(), "Expect return value to be " +
                                                     current_function_return_type->type_literal() +
                                                     " but got void");
            throw "Stop";
        }
        return 0;
    }

    auto return_type = node_jot_type(node->return_value()->accept(this));

    if (not current_function_return_type->equals(return_type)) {
        context->diagnostics.add_diagnostic_error(node->get_position().get_span(),
                                                  "Expect return value to be " +
                                                      current_function_return_type->type_literal() +
                                                      " but got " + return_type->type_literal());
        throw "Stop";
    }

    return 0;
}

std::any JotTypeChecker::visit(DeferStatement *node) {
    node->get_call_expression()->accept(this);
    return 0;
}

std::any JotTypeChecker::visit(BreakStatement *node) {
    if (node->is_has_times() and node->get_times() == 1) {
        context->diagnostics.add_diagnostic_warn(node->get_position().get_span(),
                                                 "`break 1;` can implicity written as `break;`");
    }
    return 0;
}

std::any JotTypeChecker::visit(ContinueStatement *node) {
    if (node->is_has_times() and node->get_times() == 1) {
        context->diagnostics.add_diagnostic_warn(
            node->get_position().get_span(), "`continue 1;` can implicity written as `continue;`");
    }
    return 0;
}

std::any JotTypeChecker::visit(ExpressionStatement *node) {
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(IfExpression *node) {
    auto condition = node_jot_type(node->get_condition()->accept(this));
    if (not is_number_type(condition)) {
        context->diagnostics.add_diagnostic_error(
            condition->get_type_position(),
            "If Expression condition mush be a number but got " + condition->type_literal());
        throw "Stop";
    }

    auto if_value = node_jot_type(node->get_if_value()->accept(this));
    auto else_value = node_jot_type(node->get_else_value()->accept(this));
    if (not if_value->equals(else_value)) {
        context->diagnostics.add_diagnostic_error(node->get_if_position().get_span(),
                                                  "If Expression Type missmatch expect " +
                                                      if_value->type_literal() + " but got " +
                                                      else_value->type_literal());
        throw "Stop";
    }

    return if_value;
}

std::any JotTypeChecker::visit(SwitchExpression *node) {
    auto argument = node_jot_type(node->get_argument()->accept(this));
    auto cases = node->get_switch_cases();
    auto cases_size = cases.size();
    for (size_t i = 0; i < cases_size; i++) {
        auto case_expression = cases[i];
        auto case_type = node_jot_type(case_expression->accept(this));
        if (not argument->equals(case_type)) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().get_span(),
                "Switch case type must be the same type of argument type " +
                    argument->type_literal() + " but got " + case_type->type_literal() +
                    " in case number " + std::to_string(i + 1));
            throw "Stop";
        }
    }

    auto values = node->get_switch_cases_values();
    auto expected_type = node_jot_type(values[0]->accept(this));
    for (size_t i = 1; i < cases_size; i++) {
        auto case_value = node_jot_type(values[i]->accept(this));
        if (not expected_type->equals(case_value)) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().get_span(), "Switch cases must be the same time but got " +
                                                     expected_type->type_literal() + " and " +
                                                     case_value->type_literal());
            throw "Stop";
        }
    }

    auto default_value_type = node_jot_type(node->get_default_case_value()->accept(this));
    if (not expected_type->equals(default_value_type)) {
        context->diagnostics.add_diagnostic_error(
            node->get_position().get_span(),
            "Switch case default values must be the same type of other cases expect " +
                expected_type->type_literal() + " but got " + default_value_type->type_literal());
        throw "Stop";
    }

    node->set_type_node(expected_type);
    return expected_type;
}

std::any JotTypeChecker::visit(GroupExpression *node) {
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(AssignExpression *node) {
    auto left_node = node->get_left();
    auto left_type = node_jot_type(left_node->accept(this));

    // Make sure to report error if use want to modify string literal using index expression
    if (left_node->get_ast_node_type() == AstNodeType::IndexExpr) {
        auto index_expression = std::dynamic_pointer_cast<IndexExpression>(left_node);
        auto value_type = index_expression->get_value()->get_type_node();
        if (value_type->type_literal() == "*Int8") {
            context->diagnostics.add_diagnostic_error(
                index_expression->get_position().get_span(),
                "String literal are readonly can't modify it using [i]");
            throw "Stop";
        }
    }

    auto right_type = node_jot_type(node->get_right()->accept(this));

    if (not left_type->equals(right_type)) {
        context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
                                                  "Type missmatch expect " +
                                                      left_type->type_literal() + " but got " +
                                                      right_type->type_literal());
        throw "Stop";
    }

    return right_type;
}

std::any JotTypeChecker::visit(BinaryExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    // Assert that right and left values are both numbers
    bool is_left_number = is_number_type(left_type);
    bool is_right_number = is_number_type(right_type);
    bool is_the_same = left_type->equals(right_type);
    if (not is_left_number || not is_right_number) {

        if (not is_left_number) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
                                                      "Expected binary left to be number but got " +
                                                          left_type->type_literal());
        }

        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Expected binary right to be number but got " + left_type->type_literal());
        }

        if (not is_the_same) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
                                                      "Binary Expression type missmatch " +
                                                          left_type->type_literal() + " and " +
                                                          right_type->type_literal());
        }

        throw "Stop";
    }

    return left_type;
}

std::any JotTypeChecker::visit(ShiftExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    bool is_left_number = is_integer_type(left_type);
    bool is_right_number = is_integer_type(right_type);
    bool is_the_same = left_type->equals(right_type);
    if (not is_left_number || not is_right_number || not is_the_same) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Shift Expressions Expected left to be integers but got " +
                    left_type->type_literal());
        }

        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Shift Expressions Expected right to be integers but got " +
                    left_type->type_literal());
        }

        if (not is_the_same) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
                                                      "Shift Expression type missmatch " +
                                                          left_type->type_literal() + " and " +
                                                          right_type->type_literal());
        }

        throw "Stop";
    }
    return node_jot_type(node->get_type_node());
}

std::any JotTypeChecker::visit(ComparisonExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    // TODO: Add support for more comparison expressions

    bool is_left_number = is_number_type(left_type) or is_enum_element_type(left_type);
    bool is_right_number = is_number_type(right_type) or is_enum_element_type(right_type);
    bool is_the_same = left_type->equals(right_type);
    if (not is_left_number || not is_right_number || not is_the_same) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Expected Comparison left to be number or enum element but got " +
                    left_type->type_literal());
        }

        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Expected Comparison right to be number or enum element but got " +
                    left_type->type_literal());
        }

        if (not is_the_same) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
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
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Expected Logical left to be number but got " + left_type->type_literal());
        }
        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Expected Logical right to be number but got " + left_type->type_literal());
        }
        throw "Stop";
    }

    return node_jot_type(node->get_type_node());
}

std::any JotTypeChecker::visit(PrefixUnaryExpression *node) {
    auto operand_type = node_jot_type(node->get_right()->accept(this));
    auto unary_operator = node->get_operator_token().get_kind();

    if (unary_operator == TokenKind::Minus) {
        if (is_number_type(operand_type)) {
            node->set_type_node(operand_type);
            return operand_type;
        }
        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().get_span(),
            "Unary - operator require number as an right operand but got " +
                operand_type->type_literal());
        throw "Stop";
    }

    if (unary_operator == TokenKind::Bang) {
        if (is_number_type(operand_type)) {
            node->set_type_node(operand_type);
            return operand_type;
        }
        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().get_span(),
            "Unary - operator require boolean as an right operand but got " +
                operand_type->type_literal());
        throw "Stop";
    }

    if (unary_operator == TokenKind::Not) {
        if (is_number_type(operand_type)) {
            node->set_type_node(operand_type);
            return operand_type;
        }
        context->diagnostics.add_diagnostic_error(
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

        context->diagnostics.add_diagnostic_error(
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

    if (unary_operator == TokenKind::PlusPlus || unary_operator == TokenKind::MinusMinus) {
        if (operand_type->get_type_kind() != TypeKind::Number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Unary ++ or -- expression expect variable to be number ttype but got " +
                    operand_type->type_literal());
            throw "Stop";
        }
        node->set_type_node(operand_type);
        return operand_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
                                              "Unsupported unary expression " +
                                                  operand_type->type_literal());
    throw "Stop";
}

std::any JotTypeChecker::visit(PostfixUnaryExpression *node) {
    auto operand_type = node_jot_type(node->get_right()->accept(this));
    auto unary_operator = node->get_operator_token().get_kind();

    if (unary_operator == TokenKind::PlusPlus or unary_operator == TokenKind::MinusMinus) {
        if (operand_type->get_type_kind() != TypeKind::Number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().get_span(),
                "Unary ++ or -- expression expect variable to be number ttype but got " +
                    operand_type->type_literal());
            throw "Stop";
        }
        node->set_type_node(operand_type);
        return operand_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_operator_token().get_span(),
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
                check_parameters_types(node->get_position().get_span(), arguments, parameters,
                                       type->has_varargs());
                return type->get_return_type();
            } else {
                context->diagnostics.add_diagnostic_error(
                    node->get_position().get_span(), "Call expression work only with function");
                throw "Stop";
            }
        } else {
            context->diagnostics.add_diagnostic_error(
                node->get_position().get_span(), "Can't resolve function call with name " + name);
            throw "Stop";
        }
    }

    if (auto call = std::dynamic_pointer_cast<CallExpression>(node->get_callee())) {
        auto call_result = node_jot_type(call->accept(this));
        auto function_pointer_type = std::dynamic_pointer_cast<JotPointerType>(call_result);
        auto function_type =
            std::dynamic_pointer_cast<JotFunctionType>(function_pointer_type->get_point_to());
        node->set_type_node(function_type);
        auto parameters = function_type->get_parameters();
        auto arguments = node->get_arguments();
        check_parameters_types(node->get_position().get_span(), arguments, parameters,
                               function_type->has_varargs());
        return function_type->get_return_type();
    }

    context->diagnostics.add_diagnostic_error(
        node->get_position().get_span(),
        "Call expression callee must be a literal or another call expression");
    throw "Stop";
}

std::any JotTypeChecker::visit(DotExpression *node) {
    auto callee = node->get_callee()->accept(this);
    auto callee_type = node_jot_type(callee);
    if (callee_type->get_type_kind() == TypeKind::Structure) {
        auto struct_type = std::dynamic_pointer_cast<JotStructType>(callee_type);
        auto field_name = node->get_field_name().get_literal();
        int index = -1;
        bool found = false;
        for (auto &field : struct_type->get_fields_names()) {
            index++;
            if (field.get_literal() == field_name) {
                found = true;
                break;
            }
        }

        if (not found) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().get_span(), "Can't find a field with name " + field_name +
                                                     " in struct " +
                                                     struct_type->get_type_token().get_literal());
            throw "Stop";
        }

        auto field_type = struct_type->get_fields_types().at(index);
        node->set_type_node(field_type);
        node->field_index = index;
        return field_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_position().get_span(),
                                              "Dot expression expect struct type as lvalue");
    throw "Stop";
}

std::any JotTypeChecker::visit(CastExpression *node) {
    auto value = node->get_value();
    auto value_type = node_jot_type(value->accept(this));
    auto cast_result_type = node->get_type_node();
    if (not value_type->castable(cast_result_type)) {
        context->diagnostics.add_diagnostic_error(node->get_position().get_span(),
                                                  "Can't cast from " + value_type->type_literal() +
                                                      " to " + cast_result_type->type_literal());
        throw "Stop";
    }
    return cast_result_type;
}

std::any JotTypeChecker::visit(TypeSizeExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(ValueSizeExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(IndexExpression *node) {
    auto callee_type = node_jot_type(node->get_value()->accept(this));
    if (callee_type->get_type_kind() == TypeKind::Array) {
        auto array_type = std::dynamic_pointer_cast<JotArrayType>(callee_type);
        node->set_type_node(array_type->get_element_type());
    } else if (callee_type->get_type_kind() == TypeKind::Pointer) {
        auto pointer_type = std::dynamic_pointer_cast<JotPointerType>(callee_type);
        node->set_type_node(pointer_type->get_point_to());
    } else {
        context->diagnostics.add_diagnostic_error(node->get_position().get_span(),
                                                  "Index expression require array but got " +
                                                      callee_type->type_literal());
        throw "Stop";
    }

    auto index_type = node_jot_type(node->get_index()->accept(this));
    if (index_type->get_type_kind() != TypeKind::Number) {
        context->diagnostics.add_diagnostic_error(node->get_position().get_span(),
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

        // TODO: Must optimized later and to be more accurate
        if (type->get_type_kind() == TypeKind::Number ||
            type->get_type_kind() == TypeKind::EnumerationElement) {
            node->set_constant(true);
        }
        return type;
    } else {
        context->diagnostics.add_diagnostic_error(node->get_name().get_span(),
                                                  "Can't resolve variable with name " +
                                                      node->get_name().get_literal());
        throw "Stop";
    }
}

std::any JotTypeChecker::visit(NumberExpression *node) {
    auto number_type = std::dynamic_pointer_cast<JotNumberType>(node->get_type_node());
    auto number_kind = number_type->get_kind();
    auto number_literal = node->get_value().get_literal();

    bool is_valid_range = check_number_limits(number_literal.c_str(), number_kind);
    if (not is_valid_range) {
        // TODO: Diagnostic message can be improved and provide more information
        // for example `value x must be in range s .. e or you should change the type to y`
        context->diagnostics.add_diagnostic_error(node->get_value().get_span(),
                                                  "Number Value " + number_literal +
                                                      " Can't be represented using type " +
                                                      number_type->type_literal());
        throw "Stop";
    }

    return number_type;
}

std::any JotTypeChecker::visit(ArrayExpression *node) {
    auto values = node->get_values();
    for (size_t i = 1; i < values.size(); i++) {
        if (not values[i]->get_type_node()->equals(values[i - 1]->get_type_node())) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().get_span(), "Array elements with index " +
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
    if (any_type.type() == typeid(std::shared_ptr<JotStructType>)) {
        return std::any_cast<std::shared_ptr<JotStructType>>(any_type);
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
    if (type == nullptr) {
        return true;
    }

    if (type->get_type_kind() == TypeKind::None) {
        return true;
    }

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

void JotTypeChecker::check_parameters_types(TokenSpan location,
                                            std::vector<std::shared_ptr<Expression>> &arguments,
                                            std::vector<std::shared_ptr<JotType>> &parameters,
                                            bool has_varargs) {
    // If hasent varargs, parameters and arguments must be the same size
    if (not has_varargs && arguments.size() != parameters.size()) {
        context->diagnostics.add_diagnostic_error(
            location, "Invalid number of arguments, expect " + std::to_string(parameters.size()) +
                          " but got " + std::to_string(arguments.size()));
        throw "Stop";
    }

    if (has_varargs && parameters.size() > arguments.size()) {
        context->diagnostics.add_diagnostic_error(
            location, "Invalid number of arguments, expect at last" +
                          std::to_string(parameters.size()) + " but got " +
                          std::to_string(arguments.size()));
        throw "Stop";
    }

    std::vector<std::shared_ptr<JotType>> arguments_types;
    for (auto &argument : arguments) {
        arguments_types.push_back(node_jot_type(argument->accept(this)));
    }

    size_t parameters_size = parameters.size();
    for (size_t i = 0; i < parameters_size; i++) {
        if (not parameters[i]->equals(arguments_types[i])) {
            context->diagnostics.add_diagnostic_error(
                location, "Argument type didn't match parameter type expect " +
                              parameters[i]->type_literal() + " got " +
                              arguments_types[i]->type_literal());
            throw "Stop";
        }
    }
}

bool JotTypeChecker::is_same_type(const std::shared_ptr<JotType> &left,
                                  const std::shared_ptr<JotType> &right) {
    return left->get_type_kind() == right->get_type_kind();
}

bool JotTypeChecker::check_number_limits(const char *literal, NumberKind kind) {
    switch (kind) {
    case NumberKind::Integer1: {
        auto value = strtoll(literal, NULL, 0);
        return value == 0 or value == 1;
    }
    case NumberKind::Integer8: {
        auto value = strtoll(literal, NULL, 0);
        return value >= std::numeric_limits<int8_t>::min() and
               value <= std::numeric_limits<int8_t>::max();
    }
    case NumberKind::Integer16: {
        auto value = strtoll(literal, NULL, 0);
        return value >= std::numeric_limits<int16_t>::min() and
               value <= std::numeric_limits<int16_t>::max();
    }
    case NumberKind::Integer32: {
        auto value = strtoll(literal, NULL, 0);
        return value >= std::numeric_limits<int32_t>::min() and
               value <= std::numeric_limits<int32_t>::max();
    }
    case NumberKind::Integer64: {
        auto value = strtoll(literal, NULL, 0);
        return value >= std::numeric_limits<int64_t>::min() and
               value <= std::numeric_limits<int64_t>::max();
    }
    case NumberKind::Float32: {
        auto value = std::atof(literal);
        return value >= -std::numeric_limits<float>::max() and
               value <= std::numeric_limits<float>::max();
    }
    case NumberKind::Float64: {
        auto value = std::atof(literal);
        return value >= -std::numeric_limits<double>::max() and
               value <= std::numeric_limits<double>::max();
    }
    }
}

void JotTypeChecker::push_new_scope() {
    symbol_table = std::make_shared<JotSymbolTable>(symbol_table);
}

void JotTypeChecker::pop_current_scope() { symbol_table = symbol_table->get_parent_symbol_table(); }