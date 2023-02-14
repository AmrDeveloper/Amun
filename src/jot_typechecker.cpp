#include "../include/jot_typechecker.hpp"
#include "../include/jot_ast_visitor.hpp"
#include "../include/jot_basic.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"

#include <any>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>

void JotTypeChecker::check_compilation_unit(std::shared_ptr<CompilationUnit> compilation_unit)
{
    auto statements = compilation_unit->get_tree_nodes();
    try {
        // Iterate over top level nodes
        for (auto& statement : statements) {
            statement->accept(this);
        }
    }
    catch (...) {
    }
}

std::any JotTypeChecker::visit(BlockStatement* node)
{
    push_new_scope();
    for (auto& statement : node->get_nodes()) {
        statement->accept(this);
        // Here we can report warn for unreachable code after return, continue or break keyword
        // We can make it error after return, must modify the diagnostics engine first
    }
    pop_current_scope();
    return 0;
}

std::any JotTypeChecker::visit(FieldDeclaration* node)
{
    auto left_type = node->get_type();
    auto right_value = node->get_value();
    auto name = node->get_name().literal;

    // If field has initalizer, check that initalizer type is matching the declaration type
    // If declaration type is null, infier it using the rvalue type
    if (right_value != nullptr) {
        auto right_type = node_jot_type(right_value->accept(this));

        if (node->is_global() and !right_value->is_constant()) {
            context->diagnostics.add_diagnostic_error(
                node->get_name().position, "Initializer element is not a compile-time constant");
            throw "Stop";
        }

        bool is_left_none_type = is_none_type(left_type);
        bool is_left_ptr_type = is_pointer_type(left_type);
        bool is_right_none_type = is_none_type(right_type);
        bool is_right_null_type = is_null_type(right_type);
        bool is_hanlded = false;

        if (is_left_none_type and is_right_none_type) {
            context->diagnostics.add_diagnostic_error(
                node->get_name().position,
                "Can't resolve field type when both rvalue and lvalue are unkown");
            throw "Stop";
        }

        if (is_left_none_type and is_right_null_type) {
            context->diagnostics.add_diagnostic_error(
                node->get_name().position,
                "Can't resolve field type rvalue is null, please add type to the varaible");
            throw "Stop";
        }

        if (!is_left_ptr_type and is_right_null_type) {
            context->diagnostics.add_diagnostic_error(
                node->get_name().position, "Can't declare non pointer variable with null value");
            throw "Stop";
        }

        if (is_left_none_type) {
            node->set_type(right_type);
            left_type = right_type;
            is_hanlded = true;
        }

        if (is_right_none_type) {
            node->get_value()->set_type_node(left_type);
            right_type = left_type;
            is_hanlded = true;
        }

        if (is_left_ptr_type and is_right_null_type) {
            auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->get_value());
            null_expr->null_base_type = left_type;
            is_hanlded = true;
        }

        if (!is_hanlded && !is_jot_types_equals(left_type, right_type)) {
            context->diagnostics.add_diagnostic_error(
                node->get_name().position, "Type missmatch expect " + jot_type_literal(left_type) +
                                               " but got " + jot_type_literal(right_type));
            throw "Stop";
        }
    }

    bool is_first_defined = types_table.define(name, left_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic_error(
            node->get_name().position, "Field " + name + " is defined twice in the same scope");
        throw "Stop";
    }
    return 0;
}

std::any JotTypeChecker::visit(FunctionPrototype* node)
{
    auto                                  name = node->get_name();
    std::vector<std::shared_ptr<JotType>> parameters;
    for (auto& parameter : node->get_parameters()) {
        parameters.push_back(parameter->get_type());
    }
    auto return_type = node->get_return_type();
    auto function_type = std::make_shared<JotFunctionType>(
        name, parameters, return_type, node->has_varargs(), node->get_varargs_type());
    bool is_first_defined = types_table.define(name.literal, function_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic_error(node->get_name().position,
                                                  "function " + name.literal +
                                                      " is defined twice in the same scope");
        throw "Stop";
    }
    return function_type;
}

std::any JotTypeChecker::visit(FunctionDeclaration* node)
{
    auto prototype = node->get_prototype();
    auto function_type = node_jot_type(node->get_prototype()->accept(this));
    auto function = std::static_pointer_cast<JotFunctionType>(function_type);
    return_types_stack.push(function->return_type);

    push_new_scope();
    for (auto& parameter : prototype->get_parameters()) {
        types_table.define(parameter->get_name().literal, parameter->get_type());
    }
    node->get_body()->accept(this);
    pop_current_scope();

    return_types_stack.pop();

    return function_type;
}

std::any JotTypeChecker::visit(StructDeclaration* node)
{
    auto struct_type = node->get_struct_type();
    auto struct_name = struct_type->name;
    types_table.define(struct_name, struct_type);
    return nullptr;
}

std::any JotTypeChecker::visit(EnumDeclaration* node)
{
    auto name = node->get_name().literal;
    auto enum_type = std::static_pointer_cast<JotEnumType>(node->get_enum_type());
    auto enum_element_type = enum_type->element_type;
    if (not is_integer_type(enum_element_type)) {
        context->diagnostics.add_diagnostic_error(node->get_name().position,
                                                  "Enum element type must be aa integer type");
        throw "Stop";
    }

    auto element_size = enum_type->values.size();
    if (element_size > 2 && is_boolean_type(enum_element_type)) {
        context->diagnostics.add_diagnostic_error(
            node->get_name().position, "Enum with bool (int1) type can't has more than 2 elements");
        throw "Stop";
    }

    bool is_first_defined = types_table.define(name, enum_type);
    if (not is_first_defined) {
        context->diagnostics.add_diagnostic_error(node->get_name().position,
                                                  "enumeration " + name +
                                                      " is defined twice in the same scope");
        throw "Stop";
    }
    return is_first_defined;
}

std::any JotTypeChecker::visit(IfStatement* node)
{
    for (auto& conditional_block : node->get_conditional_blocks()) {
        auto condition = node_jot_type(conditional_block->get_condition()->accept(this));
        if (not is_number_type(condition)) {
            context->diagnostics.add_diagnostic_error(conditional_block->get_position().position,
                                                      "if condition mush be a number but got " +
                                                          jot_type_literal(condition));
            throw "Stop";
        }
        push_new_scope();
        conditional_block->get_body()->accept(this);
        pop_current_scope();
    }
    return 0;
}

std::any JotTypeChecker::visit(ForRangeStatement* node)
{
    const auto start_type = node_jot_type(node->range_start->accept(this));
    const auto end_type = node_jot_type(node->range_end->accept(this));

    // For range start and end must be number type and has the same exacily type
    if (is_number_type(start_type) && is_jot_types_equals(start_type, end_type)) {
        // User declared step must be the same type as range start and end
        if (node->step) {
            const auto step_type = node_jot_type(node->step->accept(this));
            if (!is_jot_types_equals(step_type, start_type)) {
                context->diagnostics.add_diagnostic_error(
                    node->position.position,
                    "For range declared step must be the same type as range start and end");
                throw "Stop";
            }
        }

        push_new_scope();

        // Define element name only inside loop scope
        types_table.define(node->element_name, start_type);

        node->body->accept(this);

        pop_current_scope();

        return 0;
    }

    context->diagnostics.add_diagnostic_error(node->position.position,
                                              "For range start and end must be integers");
    throw "Stop";
}

std::any JotTypeChecker::visit(ForEachStatement* node)
{
    auto collection_type = node_jot_type(node->collection->accept(this));

    if (collection_type->type_kind != TypeKind::Array) {
        context->diagnostics.add_diagnostic_error(node->position.position,
                                                  "For each expect array as paramter");
        throw "Stop";
    }

    auto array_type = std::static_pointer_cast<JotArrayType>(collection_type);
    auto element_type = array_type->element_type;

    push_new_scope();

    // Define element name only inside loop scope
    types_table.define(node->element_name, element_type);
    types_table.define("it_index", jot_int64_ty);

    node->body->accept(this);
    pop_current_scope();
    return 0;
}

std::any JotTypeChecker::visit(ForeverStatement* node)
{
    push_new_scope();
    node->body->accept(this);
    pop_current_scope();
    return 0;
}

std::any JotTypeChecker::visit(WhileStatement* node)
{
    auto left_type = node_jot_type(node->get_condition()->accept(this));
    if (not is_number_type(left_type)) {
        context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                  "While condition mush be a number but got " +
                                                      jot_type_literal(left_type));
        throw "Stop";
    }
    push_new_scope();
    node->get_body()->accept(this);
    pop_current_scope();
    return 0;
}

std::any JotTypeChecker::visit(SwitchStatement* node)
{
    // Check that switch argument is integer type
    auto argument = node_jot_type(node->get_argument()->accept(this));
    if ((not is_integer_type(argument)) and (not is_enum_element_type(argument))) {
        context->diagnostics.add_diagnostic_error(
            node->get_position().position,
            "Switch argument type must be integer or enum element but found " +
                jot_type_literal(argument));
        throw "Stop";
    }

    // Check that all cases values are integers or enum element, and no duplication
    // TODO: Optimize set by replacing std::string by integers for fast comparing
    std::unordered_set<std::string> cases_values;
    for (auto& branch : node->get_cases()) {
        auto values = branch->get_values();
        // Check each value of this case
        for (auto& value : values) {
            auto value_node_type = value->get_ast_node_type();
            if (value_node_type == AstNodeType::NumberExpr or
                value_node_type == AstNodeType::EnumElementExpr) {

                // No need to check if it enum element,
                // but if it number we need to assert that it integer
                if (value_node_type == AstNodeType::NumberExpr) {
                    auto value_type = node_jot_type(value->accept(this));
                    if (!is_number_type(value_type)) {
                        context->diagnostics.add_diagnostic_error(
                            branch->get_position().position,
                            "Switch case value must be an integer but found " +
                                jot_type_literal(value_type));
                        throw "Stop";
                    }
                }

                // Check that all cases values are uniques
                if (auto number = std::dynamic_pointer_cast<NumberExpression>(value)) {
                    if (!cases_values.insert(number->get_value().literal).second) {
                        context->diagnostics.add_diagnostic_error(
                            branch->get_position().position,
                            "Switch can't has more than case with the same constants value");
                        throw "Stop";
                    }
                }
                else if (auto enum_element =
                             std::dynamic_pointer_cast<EnumAccessExpression>(value)) {
                    auto enum_index_string = std::to_string(enum_element->get_enum_element_index());
                    if (not cases_values.insert(enum_index_string).second) {
                        context->diagnostics.add_diagnostic_error(
                            branch->get_position().position,
                            "Switch can't has more than case with the same constants value");
                        throw "Stop";
                    }
                }

                // This value is valid, so continue to the next one
                continue;
            }

            // Report Error if value is not number or enum element
            context->diagnostics.add_diagnostic_error(
                branch->get_position().position,
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

std::any JotTypeChecker::visit(ReturnStatement* node)
{
    if (not node->has_value()) {
        if (return_types_stack.top()->type_kind != TypeKind::Void) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().position, "Expect return value to be " +
                                                   jot_type_literal(return_types_stack.top()) +
                                                   " but got void");
            throw "Stop";
        }
        return 0;
    }

    auto return_type = node_jot_type(node->return_value()->accept(this));
    auto function_return_type = return_types_stack.top();

    if (!is_jot_types_equals(function_return_type, return_type)) {
        // If Function return type is pointer and return value is null
        // set null pointer base type to function return type
        if (is_pointer_type(function_return_type) and is_null_type(return_type)) {
            auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->return_value());
            null_expr->null_base_type = function_return_type;
            return 0;
        }

        // If function return type is not pointer, you can't return null
        if (!is_pointer_type(function_return_type) and is_null_type(return_type)) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().position,
                "Can't return null from function that return non pointer type");
            throw "Stop";
        }

        // Prevent returning function with implicit capture from other functions
        if (is_function_pointer_type(function_return_type) and
            is_function_pointer_type(return_type)) {
            auto expected_fun_ptr_type =
                std::static_pointer_cast<JotPointerType>(function_return_type);
            auto expected_fun_type =
                std::static_pointer_cast<JotFunctionType>(expected_fun_ptr_type->base_type);

            auto return_fun_ptr = std::static_pointer_cast<JotPointerType>(return_type);
            auto return_fun = std::static_pointer_cast<JotFunctionType>(return_fun_ptr->base_type);

            if (expected_fun_type->implicit_parameters_count !=
                return_fun->implicit_parameters_count) {
                context->diagnostics.add_diagnostic_error(
                    node->get_position().position,
                    "Can't return lambda that implicit capture values from function");
                throw "Stop";
            }
        }

        context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                  "Expect return value to be " +
                                                      jot_type_literal(function_return_type) +
                                                      " but got " + jot_type_literal(return_type));
        throw "Stop";
    }

    return 0;
}

std::any JotTypeChecker::visit(DeferStatement* node)
{
    node->get_call_expression()->accept(this);
    return 0;
}

std::any JotTypeChecker::visit(BreakStatement* node)
{
    if (node->is_has_times() and node->get_times() == 1) {
        context->diagnostics.add_diagnostic_warn(node->get_position().position,
                                                 "`break 1;` can implicity written as `break;`");
    }
    return 0;
}

std::any JotTypeChecker::visit(ContinueStatement* node)
{
    if (node->is_has_times() and node->get_times() == 1) {
        context->diagnostics.add_diagnostic_warn(
            node->get_position().position, "`continue 1;` can implicity written as `continue;`");
    }
    return 0;
}

std::any JotTypeChecker::visit(ExpressionStatement* node)
{
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(IfExpression* node)
{
    auto condition = node_jot_type(node->get_condition()->accept(this));
    if (not is_number_type(condition)) {
        context->diagnostics.add_diagnostic_error(
            node->get_if_position().position,
            "If Expression condition mush be a number but got " + jot_type_literal(condition));
        throw "Stop";
    }

    auto if_value = node_jot_type(node->get_if_value()->accept(this));
    auto else_value = node_jot_type(node->get_else_value()->accept(this));
    if (!is_jot_types_equals(if_value, else_value)) {
        context->diagnostics.add_diagnostic_error(node->get_if_position().position,
                                                  "If Expression Type missmatch expect " +
                                                      jot_type_literal(if_value) + " but got " +
                                                      jot_type_literal(else_value));
        throw "Stop";
    }
    node->set_type_node(if_value);
    return if_value;
}

std::any JotTypeChecker::visit(SwitchExpression* node)
{
    auto argument = node_jot_type(node->get_argument()->accept(this));
    auto cases = node->get_switch_cases();
    auto cases_size = cases.size();
    for (size_t i = 0; i < cases_size; i++) {
        auto case_expression = cases[i];
        auto case_type = node_jot_type(case_expression->accept(this));
        if (!is_jot_types_equals(argument, case_type)) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().position,
                "Switch case type must be the same type of argument type " +
                    jot_type_literal(argument) + " but got " + jot_type_literal(case_type) +
                    " in case number " + std::to_string(i + 1));
            throw "Stop";
        }
    }

    auto values = node->get_switch_cases_values();
    auto expected_type = node_jot_type(values[0]->accept(this));
    for (size_t i = 1; i < cases_size; i++) {
        auto case_value = node_jot_type(values[i]->accept(this));
        if (!is_jot_types_equals(expected_type, case_value)) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().position, "Switch cases must be the same time but got " +
                                                   jot_type_literal(expected_type) + " and " +
                                                   jot_type_literal(case_value));
            throw "Stop";
        }
    }

    auto default_value_type = node_jot_type(node->get_default_case_value()->accept(this));
    if (!is_jot_types_equals(expected_type, default_value_type)) {
        context->diagnostics.add_diagnostic_error(
            node->get_position().position,
            "Switch case default values must be the same type of other cases expect " +
                jot_type_literal(expected_type) + " but got " +
                jot_type_literal(default_value_type));
        throw "Stop";
    }

    node->set_type_node(expected_type);
    return expected_type;
}

std::any JotTypeChecker::visit(GroupExpression* node)
{
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(AssignExpression* node)
{
    auto left_node = node->get_left();
    auto left_type = node_jot_type(left_node->accept(this));

    // Make sure to report error if use want to modify string literal using index expression
    if (left_node->get_ast_node_type() == AstNodeType::IndexExpr) {
        auto index_expression = std::dynamic_pointer_cast<IndexExpression>(left_node);
        auto value_type = index_expression->get_value()->get_type_node();
        if (jot_type_literal(value_type) == "*Int8") {
            context->diagnostics.add_diagnostic_error(
                index_expression->get_position().position,
                "String literal are readonly can't modify it using [i]");
            throw "Stop";
        }
    }

    auto right_type = node_jot_type(node->get_right()->accept(this));

    // if Variable type is pointer and rvalue is null, change null base type to lvalue type
    if (is_pointer_type(left_type) and is_null_type(right_type)) {
        auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->get_right());
        null_expr->null_base_type = left_type;
        return left_type;
    }

    // RValue type and LValue Type don't matchs
    if (!is_jot_types_equals(left_type, right_type)) {
        context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                                  "Type missmatch expect " +
                                                      jot_type_literal(left_type) + " but got " +
                                                      jot_type_literal(right_type));
        throw "Stop";
    }

    return right_type;
}

std::any JotTypeChecker::visit(BinaryExpression* node)
{
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    // Assert that right and left values are both numbers
    bool is_left_number = is_number_type(left_type);
    bool is_right_number = is_number_type(right_type);
    bool is_the_same = is_jot_types_equals(left_type, right_type);
    if (not is_left_number || not is_right_number) {

        if (not is_left_number) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                                      "Expected binary left to be number but got " +
                                                          jot_type_literal(left_type));
        }

        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Expected binary right to be number but got " + jot_type_literal(left_type));
        }

        if (not is_the_same) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                                      "Binary Expression type missmatch " +
                                                          jot_type_literal(left_type) + " and " +
                                                          jot_type_literal(right_type));
        }

        throw "Stop";
    }

    return left_type;
}

std::any JotTypeChecker::visit(ShiftExpression* node)
{
    const auto& lhs = node->get_left();
    const auto& rhs = node->get_right();

    auto left_type = node_jot_type(lhs->accept(this));
    auto right_type = node_jot_type(rhs->accept(this));

    bool is_left_number = is_integer_type(left_type);
    bool is_right_number = is_integer_type(right_type);
    bool is_the_same = is_jot_types_equals(left_type, right_type);

    // Check that they are both the integers and with the same size
    if (not is_left_number || not is_right_number || not is_the_same) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Shift Expressions Expected left to be integers but got " +
                    jot_type_literal(left_type));
        }

        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Shift Expressions Expected right to be integers but got " +
                    jot_type_literal(left_type));
        }

        if (not is_the_same) {
            context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                                      "Shift Expression type missmatch " +
                                                          jot_type_literal(left_type) + " and " +
                                                          jot_type_literal(right_type));
        }

        throw "Stop";
    }

    // Check for compile time integer overflow if possiable
    if (rhs->get_ast_node_type() == AstNodeType::NumberExpr) {
        auto crhs = std::dynamic_pointer_cast<NumberExpression>(rhs);
        auto str_value = crhs->get_value().literal;
        auto num = str_to_int(str_value.c_str());
        auto number_kind = std::static_pointer_cast<JotNumberType>(left_type)->number_kind;
        auto first_operand_width = number_kind_width[number_kind];
        if (num >= first_operand_width) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Shift Expressions second operand can't be bigger than or equal first operand bit "
                "width (" +
                    std::to_string(first_operand_width) + ")");
            throw "Stop";
        }
    }

    // Check that scond operand is a positive number
    if (rhs->get_ast_node_type() == AstNodeType::PrefixUnaryExpr) {
        auto unary = std::dynamic_pointer_cast<PrefixUnaryExpression>(rhs);
        if (unary->get_operator_token().kind == TokenKind::Minus &&
            unary->get_right()->get_ast_node_type() == AstNodeType::NumberExpr) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Shift Expressions second operand can't be a negative number");
            throw "Stop";
        }
    }

    return node_jot_type(node->get_type_node());
}

std::any JotTypeChecker::visit(ComparisonExpression* node)
{
    const auto left_type = node_jot_type(node->get_left()->accept(this));
    const auto right_type = node_jot_type(node->get_right()->accept(this));
    const auto are_the_same = is_jot_types_equals(left_type, right_type);

    // Numbers comparasions
    if (is_number_type(left_type) and is_number_type(right_type)) {
        if (are_the_same) {
            return node_jot_type(node->get_type_node());
        }

        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().position,
            "You can't compare numbers with different size or types " +
                jot_type_literal(left_type) + " and " + jot_type_literal(right_type));
        throw "Stop";
    }

    // Enumerations elements comparasions
    if (is_enum_element_type(left_type) and is_enum_element_type(right_type)) {
        if (are_the_same) {
            return node_jot_type(node->get_type_node());
        }

        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().position,
            "You can't compare elements from different enums " + jot_type_literal(left_type) +
                " and " + jot_type_literal(right_type));
        throw "Stop";
    }

    // Pointers comparasions
    if (is_pointer_type(left_type) and is_pointer_type(right_type)) {
        if (are_the_same) {
            return node_jot_type(node->get_type_node());
        }

        context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                                  "You can't compare pointers to different types " +
                                                      jot_type_literal(left_type) + " and " +
                                                      jot_type_literal(right_type));
        throw "Stop";
    }

    // Pointer vs null comparaisons and set null pointer base type
    if (is_pointer_type(left_type) and is_null_type(right_type)) {
        auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->get_right());
        null_expr->null_base_type = left_type;
        return node_jot_type(node->get_type_node());
    }

    // Null vs Pointer comparaisons and set null pointer base type
    if (is_null_type(left_type) and is_pointer_type(right_type)) {
        auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->get_left());
        null_expr->null_base_type = right_type;
        return node_jot_type(node->get_type_node());
    }

    // Null vs Null comparaisons, no need to set pointer base, use the default base
    if (is_null_type(left_type) and is_null_type(right_type)) {
        return node_jot_type(node->get_type_node());
    }

    // Comparing different types together is invalid
    context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                              "Can't compare thoese types together " +
                                                  jot_type_literal(left_type) + " and " +
                                                  jot_type_literal(right_type));
    throw "Stop";
}

std::any JotTypeChecker::visit(LogicalExpression* node)
{
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    // TODO: Assert that they are int1 (bool)
    bool is_left_number = is_number_type(left_type);
    bool is_right_number = is_number_type(right_type);
    if (not is_left_number || not is_right_number) {
        if (not is_left_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Expected Logical left to be number but got " + jot_type_literal(left_type));
        }
        if (not is_right_number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Expected Logical right to be number but got " + jot_type_literal(left_type));
        }
        throw "Stop";
    }

    return node_jot_type(node->get_type_node());
}

std::any JotTypeChecker::visit(PrefixUnaryExpression* node)
{
    auto operand_type = node_jot_type(node->get_right()->accept(this));
    auto unary_operator = node->get_operator_token().kind;

    if (unary_operator == TokenKind::Minus) {
        if (is_number_type(operand_type)) {
            node->set_type_node(operand_type);
            return operand_type;
        }
        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().position,
            "Unary - operator require number as an right operand but got " +
                jot_type_literal(operand_type));
        throw "Stop";
    }

    if (unary_operator == TokenKind::Bang) {
        if (is_number_type(operand_type)) {
            node->set_type_node(operand_type);
            return operand_type;
        }
        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().position,
            "Unary - operator require boolean as an right operand but got " +
                jot_type_literal(operand_type));
        throw "Stop";
    }

    if (unary_operator == TokenKind::Not) {
        if (is_number_type(operand_type)) {
            node->set_type_node(operand_type);
            return operand_type;
        }
        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().position,
            "Unary ~ operator require number as an right operand but got " +
                jot_type_literal(operand_type));
        throw "Stop";
    }

    if (unary_operator == TokenKind::Star) {
        if (operand_type->type_kind == TypeKind::Pointer) {
            auto pointer_type = std::static_pointer_cast<JotPointerType>(operand_type);
            auto type = pointer_type->base_type;
            node->set_type_node(type);
            return type;
        }

        context->diagnostics.add_diagnostic_error(
            node->get_operator_token().position,
            "Derefernse operator require pointer as an right operand but got " +
                jot_type_literal(operand_type));
        throw "Stop";
    }

    if (unary_operator == TokenKind::And) {
        auto pointer_type = std::make_shared<JotPointerType>(operand_type);
        node->set_type_node(pointer_type);
        return pointer_type;
    }

    if (unary_operator == TokenKind::PlusPlus || unary_operator == TokenKind::MinusMinus) {
        if (operand_type->type_kind != TypeKind::Number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Unary ++ or -- expression expect variable to be number ttype but got " +
                    jot_type_literal(operand_type));
            throw "Stop";
        }
        node->set_type_node(operand_type);
        return operand_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                              "Unsupported unary expression " +
                                                  jot_type_literal(operand_type));
    throw "Stop";
}

std::any JotTypeChecker::visit(PostfixUnaryExpression* node)
{
    auto operand_type = node_jot_type(node->get_right()->accept(this));
    auto unary_operator = node->get_operator_token().kind;

    if (unary_operator == TokenKind::PlusPlus or unary_operator == TokenKind::MinusMinus) {
        if (operand_type->type_kind != TypeKind::Number) {
            context->diagnostics.add_diagnostic_error(
                node->get_operator_token().position,
                "Unary ++ or -- expression expect variable to be number ttype but got " +
                    jot_type_literal(operand_type));
            throw "Stop";
        }
        node->set_type_node(operand_type);
        return operand_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_operator_token().position,
                                              "Unsupported unary expression " +
                                                  jot_type_literal(operand_type));
    throw "Stop";
}

std::any JotTypeChecker::visit(CallExpression* node)
{
    auto callee = node->get_callee();
    auto callee_ast_node_type = node->get_callee()->get_ast_node_type();

    // Call function by name for example function();
    if (callee_ast_node_type == AstNodeType::LiteralExpr) {
        auto literal = std::dynamic_pointer_cast<LiteralExpression>(callee);
        auto name = literal->get_name().literal;
        if (types_table.is_defined(name)) {
            auto lookup = types_table.lookup(name);
            auto value = node_jot_type(types_table.lookup(name));

            if (value->type_kind == TypeKind::Pointer) {
                auto pointer_type = std::static_pointer_cast<JotPointerType>(value);
                value = pointer_type->base_type;
            }

            if (value->type_kind == TypeKind::Function) {
                auto type = std::static_pointer_cast<JotFunctionType>(value);
                node->set_type_node(type);
                auto parameters = type->parameters;
                auto arguments = node->get_arguments();
                for (auto& argument : arguments) {
                    argument->set_type_node(node_jot_type(argument->accept(this)));
                }

                check_parameters_types(node->get_position().position, arguments, parameters,
                                       type->has_varargs, type->varargs_type,
                                       type->implicit_parameters_count);

                return type->return_type;
            }
            else {
                context->diagnostics.add_diagnostic_error(
                    node->get_position().position, "Call expression work only with function");
                throw "Stop";
            }
        }
        else {
            context->diagnostics.add_diagnostic_error(
                node->get_position().position, "Can't resolve function call with name " + name);
            throw "Stop";
        }
    }

    // Call function pointer returned from call expression for example function()();
    if (callee_ast_node_type == AstNodeType::CallExpr) {
        auto call = std::dynamic_pointer_cast<CallExpression>(callee);
        auto call_result = node_jot_type(call->accept(this));
        auto function_pointer_type = std::static_pointer_cast<JotPointerType>(call_result);
        auto function_type =
            std::static_pointer_cast<JotFunctionType>(function_pointer_type->base_type);
        auto parameters = function_type->parameters;
        auto arguments = node->get_arguments();
        check_parameters_types(node->get_position().position, arguments, parameters,
                               function_type->has_varargs, function_type->varargs_type,
                               function_type->implicit_parameters_count);
        node->set_type_node(function_type);
        return function_type->return_type;
    }

    // Call lambda expression for example { () void -> return; } ()
    if (callee_ast_node_type == AstNodeType::LambdaExpr) {
        auto lambda = std::dynamic_pointer_cast<LambdaExpression>(node->get_callee());
        auto lambda_function_type = node_jot_type(lambda->accept(this));
        auto function_ptr_type = std::static_pointer_cast<JotPointerType>(lambda_function_type);

        auto function_type =
            std::static_pointer_cast<JotFunctionType>(function_ptr_type->base_type);

        auto parameters = function_type->parameters;
        auto arguments = node->get_arguments();
        for (auto& argument : arguments) {
            argument->set_type_node(node_jot_type(argument->accept(this)));
        }

        check_parameters_types(node->get_position().position, arguments, parameters,
                               function_type->has_varargs, function_type->varargs_type,
                               function_type->implicit_parameters_count);

        node->set_type_node(function_type);
        return function_type->return_type;
    }

    // Call struct field with function pointer for example type struct.field()
    if (callee_ast_node_type == AstNodeType::DotExpr) {
        auto dot_expression = std::dynamic_pointer_cast<DotExpression>(node->get_callee());
        auto dot_function_type = node_jot_type(dot_expression->accept(this));
        auto function_ptr_type = std::static_pointer_cast<JotPointerType>(dot_function_type);

        auto function_type =
            std::static_pointer_cast<JotFunctionType>(function_ptr_type->base_type);

        auto parameters = function_type->parameters;
        auto arguments = node->get_arguments();
        for (auto& argument : arguments) {
            argument->set_type_node(node_jot_type(argument->accept(this)));
        }

        check_parameters_types(node->get_position().position, arguments, parameters,
                               function_type->has_varargs, function_type->varargs_type,
                               function_type->implicit_parameters_count);

        node->set_type_node(function_type);
        return function_type->return_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_position().position,
                                              "Unexpected callee type for Call Expression");
    throw "Stop";
}

std::any JotTypeChecker::visit(InitializeExpression* node)
{
    auto type = node->type;

    if (type->type_kind == TypeKind::Structure) {
        auto struct_type = std::static_pointer_cast<JotStructType>(type);
        auto parameters = struct_type->fields_types;
        auto arguments = node->arguments;

        check_parameters_types(node->position.position, arguments, parameters, false, nullptr, 0);
        return struct_type;
    }

    context->diagnostics.add_diagnostic_error(node->position.position,
                                              "InitializeExpression work only with structures");
    throw "Stop";
}

std::any JotTypeChecker::visit(LambdaExpression* node)
{
    auto function_ptr_type = std::static_pointer_cast<JotPointerType>(node->get_type_node());
    auto function_type = std::static_pointer_cast<JotFunctionType>(function_ptr_type->base_type);
    return_types_stack.push(function_type->return_type);

    is_inside_lambda_body = true;
    lambda_implicit_parameters.push({});

    push_new_scope();

    // Define Explicit parameter inside lambda body scope
    for (auto& parameter : node->explicit_parameters) {
        types_table.define(parameter->get_name().literal, parameter->get_type());
    }

    node->body->accept(this);
    pop_current_scope();

    is_inside_lambda_body = false;

    auto extra_parameter_pairs = lambda_implicit_parameters.top();

    // Append Implicit Parameter at the start of function parameters
    for (auto& parameter_pair : extra_parameter_pairs) {
        node->implict_parameters_names.push_back(parameter_pair.first);
        node->implict_parameters_types.push_back(parameter_pair.second);
        function_type->implicit_parameters_count++;
    }

    function_type->parameters.insert(function_type->parameters.begin(),
                                     node->implict_parameters_types.begin(),
                                     node->implict_parameters_types.end());

    // Modify function pointer type after appending implicit paramaters
    function_ptr_type->base_type = function_type;
    node->set_type_node(function_ptr_type);

    lambda_implicit_parameters.pop();
    return_types_stack.pop();

    return function_ptr_type;
}

std::any JotTypeChecker::visit(DotExpression* node)
{
    auto callee = node->get_callee()->accept(this);
    auto callee_type = node_jot_type(callee);
    auto callee_type_kind = callee_type->type_kind;

    if (callee_type_kind == TypeKind::Structure) {
        auto struct_type = std::static_pointer_cast<JotStructType>(callee_type);
        auto field_name = node->get_field_name().literal;
        auto fields_names = struct_type->fields_names;
        if (fields_names.contains(field_name)) {
            int  member_index = fields_names[field_name];
            auto field_type = struct_type->fields_types[member_index];
            node->set_type_node(field_type);
            node->field_index = member_index;
            return field_type;
        }

        context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                  "Can't find a field with name " + field_name +
                                                      " in struct " + struct_type->name);
        throw "Stop";
    }

    if (callee_type_kind == TypeKind::Pointer) {
        auto pointer_type = std::static_pointer_cast<JotPointerType>(callee_type);
        auto pointer_to_type = pointer_type->base_type;
        if (pointer_to_type->type_kind == TypeKind::Structure) {
            auto struct_type = std::static_pointer_cast<JotStructType>(pointer_to_type);
            auto field_name = node->get_field_name().literal;
            auto fields_names = struct_type->fields_names;
            if (fields_names.contains(field_name)) {
                int  member_index = fields_names[field_name];
                auto field_type = struct_type->fields_types[member_index];
                node->set_type_node(field_type);
                node->field_index = member_index;
                return field_type;
            }
            context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                      "Can't find a field with name " + field_name +
                                                          " in struct " + struct_type->name);
            throw "Stop";
        }

        context->diagnostics.add_diagnostic_error(
            node->get_position().position,
            "Dot expression expect calling member from struct or pointer to struct");
        throw "Stop";
    }

    if (callee_type_kind == TypeKind::Array) {
        auto attribute_token = node->get_field_name();
        auto literal = attribute_token.literal;

        if (literal == "count") {
            node->is_constants_ = true;
            node->set_type_node(jot_int64_ty);
            return jot_int64_ty;
        }

        context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                  "Unkown Array attribute with name " + literal);
        throw "Stop";
    }

    context->diagnostics.add_diagnostic_error(
        node->get_position().position, "Dot expression expect struct or enum type as lvalue");
    throw "Stop";
}

std::any JotTypeChecker::visit(CastExpression* node)
{
    auto value = node->get_value();
    auto value_type = node_jot_type(value->accept(this));
    auto cast_result_type = node->get_type_node();

    // No need for castring if both has the same type
    if (is_jot_types_equals(value_type, cast_result_type)) {
        // TODO: Fire warning 'Unrequired castring because both has same type'
        return cast_result_type;
    }

    if (!can_jot_types_casted(value_type, cast_result_type)) {
        context->diagnostics.add_diagnostic_error(
            node->get_position().position, "Can't cast from " + jot_type_literal(value_type) +
                                               " to " + jot_type_literal(cast_result_type));
        throw "Stop";
    }
    return cast_result_type;
}

std::any JotTypeChecker::visit(TypeSizeExpression* node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(ValueSizeExpression* node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(IndexExpression* node)
{
    auto index_expression = node->get_index();
    auto index_type = node_jot_type(index_expression->accept(this));

    // Make sure index is integer type with any size
    if (!is_integer_type(index_type)) {
        context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                  "Index must be an integer but got " +
                                                      jot_type_literal(index_type));
        throw "Stop";
    }

    // Check if index is number expression to perform compile time checks
    bool   has_constant_index = index_expression->get_ast_node_type() == AstNodeType::NumberExpr;
    size_t constant_index = -1;

    if (has_constant_index) {
        auto number_expr = std::dynamic_pointer_cast<NumberExpression>(index_expression);
        auto number_literal = number_expr->get_value().literal;
        constant_index = str_to_int(number_literal.c_str());

        // Check that index is not negative
        if (constant_index < 0) {
            context->diagnostics.add_diagnostic_error(node->get_position().position,
                                                      "Index can't be negative number");
            throw "Stop";
        }
    }

    auto callee_expression = node->get_value();
    auto callee_type = node_jot_type(callee_expression->accept(this));

    if (callee_type->type_kind == TypeKind::Array) {
        auto array_type = std::static_pointer_cast<JotArrayType>(callee_type);
        node->set_type_node(array_type->element_type);

        // Check that index is not larger or equal array size
        if (has_constant_index && constant_index >= array_type->size) {
            context->diagnostics.add_diagnostic_error(
                node->get_position().position, "Index can't be bigger than or equal array size");
            throw "Stop";
        }

        return array_type->element_type;
    }

    if (callee_type->type_kind == TypeKind::Pointer) {
        auto pointer_type = std::static_pointer_cast<JotPointerType>(callee_type);
        node->set_type_node(pointer_type->base_type);
        return pointer_type->base_type;
    }

    context->diagnostics.add_diagnostic_error(node->get_position().position,
                                              "Index expression require array but got " +
                                                  jot_type_literal(callee_type));
    throw "Stop";
}

std::any JotTypeChecker::visit(EnumAccessExpression* node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(LiteralExpression* node)
{
    const auto name = node->get_name().literal;
    if (!types_table.is_defined(name)) {
        context->diagnostics.add_diagnostic_error(node->get_name().position,
                                                  "Can't resolve variable with name " +
                                                      node->get_name().literal);
        throw "Stop";
    }

    std::any value = nullptr;
    if (is_inside_lambda_body) {
        auto local_variable = types_table.lookup_on_current(name);

        // If not explicit parameter or local declared variable
        std::string type_literal = local_variable.type().name();
        if (type_literal == "Dn") {
            // Check if it declared in any outer scope
            auto outer_variable_pair = types_table.lookup_with_level(name);
            auto declared_scope_level = outer_variable_pair.second;

            value = outer_variable_pair.first;

            // Check if it not global variable, need to perform implicit capture for this variable
            if (declared_scope_level != 0 && declared_scope_level < types_table.size() - 2) {
                auto type = node_jot_type(value);
                types_table.define(name, type);
                lambda_implicit_parameters.top().push_back({name, type});
            }
        }
        // No need for implicit capture, it already local varaible
        else {
            value = local_variable;
        }
    }
    else {
        value = types_table.lookup(name);
    }

    auto type = node_jot_type(value);
    node->set_type(type);

    if (type->type_kind == TypeKind::Number || type->type_kind == TypeKind::EnumerationElement) {
        node->set_constant(true);
    }

    return type;
}

std::any JotTypeChecker::visit(NumberExpression* node)
{
    auto number_type = std::static_pointer_cast<JotNumberType>(node->get_type_node());
    auto number_kind = number_type->number_kind;
    auto number_literal = node->get_value().literal;

    bool is_valid_range = check_number_limits(number_literal.c_str(), number_kind);
    if (not is_valid_range) {
        // TODO: Diagnostic message can be improved and provide more information
        // for example `value x must be in range s .. e or you should change the type to y`
        context->diagnostics.add_diagnostic_error(node->get_value().position,
                                                  "Number Value " + number_literal +
                                                      " Can't be represented using type " +
                                                      jot_type_literal(number_type));
        throw "Stop";
    }

    return number_type;
}

std::any JotTypeChecker::visit(ArrayExpression* node)
{
    const auto values = node->get_values();
    const auto values_size = values.size();
    if (values_size == 0) {
        return node->get_type_node();
    }

    auto last_element_type = node_jot_type(values[0]->accept(this));
    for (size_t i = 1; i < values_size; i++) {
        auto current_element_type = node_jot_type(values[i]->accept(this));
        if (is_jot_types_equals(current_element_type, last_element_type)) {
            last_element_type = current_element_type;
            continue;
        }

        context->diagnostics.add_diagnostic_error(
            node->get_position().position, "Array elements with index " + std::to_string(i - 1) +
                                               " and " + std::to_string(i) +
                                               " are not the same types");
        throw "Stop";
    }

    // Modify element_type with the type of first elements
    auto array_type = std::static_pointer_cast<JotArrayType>(node->get_type_node());
    array_type->element_type = last_element_type;
    node->set_type_node(array_type);
    return array_type;
}

std::any JotTypeChecker::visit(StringExpression* node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(CharacterExpression* node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(BooleanExpression* node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(NullExpression* node) { return node->get_type_node(); }

std::shared_ptr<JotType> JotTypeChecker::node_jot_type(std::any any_type)
{
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

void JotTypeChecker::check_parameters_types(TokenSpan                                 location,
                                            std::vector<std::shared_ptr<Expression>>& arguments,
                                            std::vector<std::shared_ptr<JotType>>&    parameters,
                                            bool has_varargs, std::shared_ptr<JotType> varargs_type,
                                            int implicit_parameters_count)
{

    const auto arguments_size = arguments.size();
    const auto all_arguments_size = arguments_size + implicit_parameters_count;
    const auto parameters_size = parameters.size();

    // If hasent varargs, parameters and arguments must be the same size
    if (not has_varargs && all_arguments_size != parameters_size) {
        context->diagnostics.add_diagnostic_error(
            location, "Invalid number of arguments, expect " + std::to_string(parameters_size) +
                          " but got " + std::to_string(all_arguments_size));
        throw "Stop";
    }

    // If it has varargs, number of parameters must be bigger than arguments
    if (has_varargs && parameters_size > all_arguments_size) {
        context->diagnostics.add_diagnostic_error(
            location, "Invalid number of arguments, expect at last" +
                          std::to_string(parameters_size) + " but got " +
                          std::to_string(all_arguments_size));
        throw "Stop";
    }

    // Resolve Arguments types
    std::vector<std::shared_ptr<JotType>> arguments_types;
    for (auto& argument : arguments) {
        arguments_types.push_back(node_jot_type(argument->accept(this)));
    }

    auto coun = parameters_size > arguments_size ? arguments_size : parameters_size;

    // Check non varargs parameters vs arguments
    for (size_t i = 0; i < coun; i++) {
        size_t p = i + implicit_parameters_count;

        if (!is_jot_types_equals(parameters[p], arguments_types[i])) {

            // if Parameter is pointer type and null pointer passed as argument
            // Change null pointer base type to parameter type
            if (is_pointer_type(parameters[p]) and is_null_type(arguments_types[i])) {
                auto null_expr = std::dynamic_pointer_cast<NullExpression>(arguments[i]);
                null_expr->null_base_type = parameters[p];
                continue;
            }

            context->diagnostics.add_diagnostic_error(
                location, "Argument type didn't match parameter type expect " +
                              jot_type_literal(parameters[p]) + " got " +
                              jot_type_literal(arguments_types[i]));
            throw "Stop";
        }
    }

    // If varargs type is Any, no need for type checking
    if (varargs_type == nullptr) {
        return;
    }

    // Check extra varargs types
    for (size_t i = parameters_size; i < arguments_size; i++) {
        if (!is_jot_types_equals(arguments_types[i], varargs_type)) {
            context->diagnostics.add_diagnostic_error(
                location, "Argument type didn't match varargs type expect " +
                              jot_type_literal(varargs_type) + " got " +
                              jot_type_literal(arguments_types[i]));
            throw "Stop";
        }
    }
}

bool JotTypeChecker::is_same_type(const std::shared_ptr<JotType>& left,
                                  const std::shared_ptr<JotType>& right)
{
    return left->type_kind == right->type_kind;
}

bool JotTypeChecker::check_number_limits(const char* literal, NumberKind kind)
{
    switch (kind) {
    case NumberKind::Integer1: {
        auto value = str_to_int(literal);
        return value == 0 or value == 1;
    }
    case NumberKind::Integer8: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int8_t>::min() and
               value <= std::numeric_limits<int8_t>::max();
    }
    case NumberKind::UInteger8: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint8_t>::min() and
               value <= std::numeric_limits<uint8_t>::max();
    }
    case NumberKind::Integer16: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int16_t>::min() and
               value <= std::numeric_limits<int16_t>::max();
    }
    case NumberKind::UInteger16: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint16_t>::min() and
               value <= std::numeric_limits<uint16_t>::max();
    }
    case NumberKind::Integer32: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int32_t>::min() and
               value <= std::numeric_limits<int32_t>::max();
    }
    case NumberKind::UInteger32: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint32_t>::min() and
               value <= std::numeric_limits<uint32_t>::max();
    }
    case NumberKind::Integer64: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int64_t>::min() and
               value <= std::numeric_limits<int64_t>::max();
    }
    case NumberKind::UInteger64: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint64_t>::min() and
               value <= std::numeric_limits<uint64_t>::max();
    }
    case NumberKind::Float32: {
        auto value = str_to_float(literal);
        return value >= -std::numeric_limits<float>::max() and
               value <= std::numeric_limits<float>::max();
    }
    case NumberKind::Float64: {
        auto value = str_to_float(literal);
        return value >= -std::numeric_limits<double>::max() and
               value <= std::numeric_limits<double>::max();
    }
    }
}

inline void JotTypeChecker::push_new_scope() { types_table.push_new_scope(); }

inline void JotTypeChecker::pop_current_scope() { types_table.pop_current_scope(); }