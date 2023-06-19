#include "../include/amun_typechecker.hpp"
#include "../include/amun_ast_visitor.hpp"
#include "../include/amun_basic.hpp"
#include "../include/amun_logger.hpp"
#include "../include/amun_name_mangle.hpp"
#include "../include/amun_type.hpp"

#include <any>
#include <cassert>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

auto amun::TypeChecker::check_compilation_unit(Shared<CompilationUnit> compilation_unit) -> void
{
    auto statements = compilation_unit->tree_nodes;
    try {
        // Iterate over top level nodes
        for (auto& statement : statements) {
            statement->accept(this);
        }
    }
    catch (...) {
    }
}

auto amun::TypeChecker::visit(BlockStatement* node) -> std::any
{
    push_new_scope();
    for (const auto& statement : node->statements) {
        statement->accept(this);
        // Here we can report warn for unreachable code after return, continue
        // or break keyword We can make it error after return, must modify the
        // diagnostics engine first
    }
    pop_current_scope();
    return 0;
}

auto amun::TypeChecker::visit(FieldDeclaration* node) -> std::any
{
    auto left_type = resolve_generic_type(node->type);
    auto right_value = node->value;
    auto name = node->name.literal;

    bool should_update_node_type = true;

    // If field has initalizer, check that initalizer type is matching the
    // declaration type If declaration type is null, infier it using the rvalue
    // type
    if (right_value != nullptr) {
        auto origin_right_value_type = right_value->get_type_node();
        auto right_type = node_amun_type(right_value->accept(this));

        // Prevent declaring field with void type
        if (amun::is_void_type(right_type)) {
            context->diagnostics.report_error(node->name.position,
                                              "Can't declare field with void type");
            throw "Stop";
        }

        bool is_type_updated = false;
        if (origin_right_value_type != nullptr) {
            is_type_updated = origin_right_value_type->type_kind == amun::TypeKind::GENERIC_STRUCT;
            if (is_type_updated) {
                node->type = origin_right_value_type;
                right_type = resolve_generic_type(right_type);
                should_update_node_type = false;
                bool is_first_defined = types_table.define(name, right_type);
                if (!is_first_defined) {
                    context->diagnostics.report_error(node->name.position,
                                                      "Field " + name +
                                                          " is defined twice in the same scope");
                    throw "Stop";
                }
            }
        }

        // Resolve left hand side if it generic struct
        if (amun::is_generic_struct_type(left_type)) {
            left_type = resolve_generic_type(left_type);
        }

        if (node->is_global and !right_value->is_constant()) {
            context->diagnostics.report_error(node->name.position,
                                              "Initializer element is not a compile-time constant");
            throw "Stop";
        }

        bool is_left_none_type = amun::is_none_type(left_type);
        bool is_left_ptr_type = amun::is_pointer_type(left_type);
        bool is_right_none_type = amun::is_none_type(right_type);
        bool is_right_null_type = amun::is_null_type(right_type);

        if (is_left_none_type and is_right_none_type) {
            context->diagnostics.report_error(node->name.position,
                                              "Can't resolve field type when both "
                                              "rvalue and lvalue are unkown");
            throw "Stop";
        }

        if (is_left_none_type and is_right_null_type) {
            context->diagnostics.report_error(
                node->name.position, "Can't resolve field type rvalue is null, please add type to "
                                     "the varaible");
            throw "Stop";
        }

        if (!is_left_ptr_type and is_right_null_type) {
            context->diagnostics.report_error(node->name.position,
                                              "Can't declare non pointer variable with null value");
            throw "Stop";
        }

        if (!is_type_updated && is_left_none_type) {
            node->type = right_type;
            left_type = right_type;
            is_type_updated = true;
        }

        if (!is_type_updated && is_right_none_type) {
            node->value->set_type_node(left_type);
            right_type = left_type;
            is_type_updated = true;
        }

        if (is_left_ptr_type and is_right_null_type) {
            auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->value);
            null_expr->null_base_type = left_type;
            is_type_updated = true;
        }

        if (!is_type_updated && !amun::is_types_equals(left_type, right_type)) {
            context->diagnostics.report_error(
                node->name.position, "Type missmatch expect " + amun::get_type_literal(left_type) +
                                         " but got " + amun::get_type_literal(right_type));
            throw "Stop";
        }
    }

    if (should_update_node_type) {
        bool is_first_defined = true;
        if (left_type->type_kind == amun::TypeKind::GENERIC_STRUCT) {
            node->type = left_type;
            is_first_defined = types_table.define(name, resolve_generic_type(left_type));
        }
        else {
            is_first_defined = types_table.define(name, left_type);
        }

        if (!is_first_defined) {
            context->diagnostics.report_error(
                node->name.position, "Field " + name + " is defined twice in the same scope");
            throw "Stop";
        }
    }

    return 0;
}

auto amun::TypeChecker::visit(DestructuringDeclaraion* node) -> std::any
{
    auto position = node->equal_token.position;

    if (node->is_global) {
        context->diagnostics.report_error(position, "Can't used it in global scope");
        throw "Stop";
    }

    auto value = node_amun_type(node->value->accept(this));
    if (!amun::is_tuple_type(value)) {
        context->diagnostics.report_error(position, "value type must be a tuple");
        throw "Stop";
    }

    auto tuple_value = std::static_pointer_cast<amun::TupleType>(value);
    auto tuple_field_types = tuple_value->fields_types;
    auto tuple_size = tuple_field_types.size();

    if (tuple_field_types.size() != node->names.size()) {
        context->diagnostics.report_error(position,
                                          "number of fields must be equal to tuple size expect " +
                                              std::to_string(tuple_field_types.size()) +
                                              " but got " + std::to_string(node->names.size()));
        throw "Stop";
    }

    for (size_t i = 0; i < tuple_size; i++) {
        if (amun::is_none_type(node->types[i])) {
            node->types[i] = resolve_generic_type(tuple_field_types[i]);
        }
        else if (!amun::is_types_equals(node->types[i], tuple_field_types[i])) {
            context->diagnostics.report_error(node->names[i].position,
                                              "field type must be equal to tuple element type");
            throw "Stop";
        }

        bool is_first_defined = types_table.define(node->names[i].literal, node->types[i]);
        if (!is_first_defined) {
            context->diagnostics.report_error(node->names[i].position,
                                              "Field " + node->names[i].literal +
                                                  " is defined twice in the same scope");
            throw "Stop";
        }
    }

    return 0;
}

auto amun::TypeChecker::visit(ConstDeclaration* node) -> std::any
{
    auto name = node->name.literal;
    auto type = node_amun_type(node->value->accept(this));
    bool is_first_defined = types_table.define(name, type);
    if (!is_first_defined) {
        context->diagnostics.report_error(node->name.position,
                                          "Field " + name + " is defined twice in the same scope");
        throw "Stop";
    }
    return 0;
}

auto amun::TypeChecker::visit(FunctionPrototype* node) -> std::any
{
    auto name = node->name;
    std::vector<Shared<amun::Type>> parameters;
    parameters.reserve(node->parameters.size());
    for (const auto& parameter : node->parameters) {
        parameters.push_back(parameter->type);
    }
    auto return_type = node->return_type;
    auto function_type = std::make_shared<amun::FunctionType>(
        name, parameters, return_type, node->has_varargs, node->varargs_type);

    bool is_first_defined = types_table.define(name.literal, function_type);
    if (not is_first_defined) {
        context->diagnostics.report_error(name.position, "function " + name.literal +
                                                             " is defined twice in the same scope");
        throw "Stop";
    }

    return function_type;
}

auto amun::TypeChecker::visit(IntrinsicPrototype* node) -> std::any
{
    auto name = node->name;

    std::vector<Shared<amun::Type>> parameters;
    parameters.reserve(node->parameters.size());
    for (auto& parameter : node->parameters) {
        parameters.push_back(parameter->type);
    }

    auto return_type = node->return_type;
    auto function_type = std::make_shared<amun::FunctionType>(
        name, parameters, return_type, node->varargs, node->varargs_type, true);
    bool is_first_defined = types_table.define(name.literal, function_type);
    if (not is_first_defined) {
        context->diagnostics.report_error(name.position, "function " + name.literal +
                                                             " is defined twice in the same scope");
        throw "Stop";
    }
    return function_type;
}

auto amun::TypeChecker::visit(FunctionDeclaration* node) -> std::any
{
    auto prototype = node->prototype;
    if (prototype->is_generic) {
        generic_functions_declaraions[prototype->name.literal] = node;
        return 0;
    }

    auto function_type = node_amun_type(node->prototype->accept(this));
    auto function = std::static_pointer_cast<amun::FunctionType>(function_type);
    return_types_stack.push(function->return_type);

    push_new_scope();
    for (auto& parameter : prototype->parameters) {
        types_table.define(parameter->name.literal, parameter->type);
    }

    auto function_body = node->body;
    function_body->accept(this);
    pop_current_scope();

    return_types_stack.pop();

    // If Function return type is not void, should check for missing return
    // statement
    if (!amun::is_void_type(function->return_type) &&
        !check_missing_return_statement(function_body)) {
        const auto& span = node->prototype->name.position;
        context->diagnostics.report_error(
            span, "A 'return' statement required in a function with a block "
                  "body ('{...}')");
        throw "Stop";
    }

    return function_type;
}

auto amun::TypeChecker::visit(OperatorFunctionDeclaraion* node) -> std::any
{
    auto prototype = node->function->prototype;
    auto paramters = prototype->parameters;

    // Check that there is at least one parameter of struct, tuple, array, enum
    bool has_non_primitive_parameter = false;
    for (const auto& parameter : paramters) {
        const auto& type = parameter->type;
        if (!(amun::is_number_type(type) || amun::is_enum_element_type(type))) {
            has_non_primitive_parameter = true;
            break;
        }
    }

    if (!has_non_primitive_parameter) {
        auto position = node->op.position;
        context->diagnostics.report_error(position,
                                          "overloaded operator must have at least one parameter of "
                                          "struct, tuple, array, enum");
        throw "Stop";
    }

    // One of paramters must be non primitives
    return node->function->accept(this);
}

auto amun::TypeChecker::visit(StructDeclaration* node) -> std::any
{
    auto struct_type = node->struct_type;
    // Generic struct are a template and should defined
    if (!struct_type->is_generic) {
        auto struct_name = struct_type->name;
        types_table.define(struct_name, struct_type);
    }
    return nullptr;
}

auto amun::TypeChecker::visit(EnumDeclaration* node) -> std::any
{
    auto name = node->name.literal;
    auto enum_type = std::static_pointer_cast<amun::EnumType>(node->enum_type);
    auto enum_element_type = enum_type->element_type;
    if (!amun::is_integer_type(enum_element_type)) {
        context->diagnostics.report_error(node->name.position,
                                          "Enum element type must be aa integer type");
        throw "Stop";
    }

    auto element_size = enum_type->values.size();
    if (element_size > 2 && amun::is_boolean_type(enum_element_type)) {
        context->diagnostics.report_error(
            node->name.position, "Enum with bool (int1) type can't has more than 2 elements");
        throw "Stop";
    }

    bool is_first_defined = types_table.define(name, enum_type);
    if (!is_first_defined) {
        context->diagnostics.report_error(
            node->name.position, "enumeration " + name + " is defined twice in the same scope");
        throw "Stop";
    }
    return is_first_defined;
}

auto amun::TypeChecker::visit(IfStatement* node) -> std::any
{
    for (auto& conditional_block : node->conditional_blocks) {
        auto condition = node_amun_type(conditional_block->condition->accept(this));
        if (!amun::is_number_type(condition)) {
            context->diagnostics.report_error(conditional_block->position.position,
                                              "if condition mush be a number but got " +
                                                  amun::get_type_literal(condition));
            throw "Stop";
        }
        push_new_scope();
        conditional_block->body->accept(this);
        pop_current_scope();
    }
    return 0;
}

auto amun::TypeChecker::visit(ForRangeStatement* node) -> std::any
{
    const auto start_type = node_amun_type(node->range_start->accept(this));
    const auto end_type = node_amun_type(node->range_end->accept(this));

    // For range start and end must be number type and has the same exacily type
    if (amun::is_number_type(start_type) && amun::is_types_equals(start_type, end_type)) {
        // User declared step must be the same type as range start and end
        if (node->step) {
            const auto step_type = node_amun_type(node->step->accept(this));
            if (!amun::is_types_equals(step_type, start_type)) {
                context->diagnostics.report_error(
                    node->position.position,
                    "For range declared step must be the same type as range "
                    "start and end");
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

    context->diagnostics.report_error(node->position.position,
                                      "For range start and end must be integers");
    throw "Stop";
}

auto amun::TypeChecker::visit(ForEachStatement* node) -> std::any
{
    auto collection_type = node_amun_type(node->collection->accept(this));
    auto is_array_type = collection_type->type_kind == amun::TypeKind::STATIC_ARRAY;
    auto is_string_type = amun::is_pointer_of_type(collection_type, amun::i8_type);

    if (!is_array_type && !is_string_type &&
        collection_type->type_kind != TypeKind::STATIC_VECTOR) {
        context->diagnostics.report_error(node->position.position,
                                          "For each expect array or string as paramter");
        throw "Stop";
    }

    push_new_scope();

    // If name is equal _ that mean don't create implicit variable for element name
    if (node->element_name != "_") {
        // Define element name only inside loop scope
        if (is_array_type) {
            // If paramter is array, set element type to array elmenet type
            auto array_type = std::static_pointer_cast<amun::StaticArrayType>(collection_type);
            types_table.define(node->element_name, array_type->element_type);
        }
        else {
            // If parameter is string, set element type to char (*int8) type
            types_table.define(node->element_name, amun::i8_type);
        }
    }

    // If name is equal _ that mean don't create implicit variable for index
    if (node->index_name != "_") {
        // Define element name inside loop scope
        types_table.define(node->index_name, amun::i64_type);
    }

    node->body->accept(this);

    pop_current_scope();
    return 0;
}

auto amun::TypeChecker::visit(ForeverStatement* node) -> std::any
{
    push_new_scope();
    node->body->accept(this);
    pop_current_scope();
    return 0;
}

auto amun::TypeChecker::visit(WhileStatement* node) -> std::any
{
    auto left_type = node_amun_type(node->condition->accept(this));
    if (!amun::is_number_type(left_type)) {
        context->diagnostics.report_error(node->keyword.position,
                                          "While condition mush be a number but got " +
                                              amun::get_type_literal(left_type));
        throw "Stop";
    }
    push_new_scope();
    node->body->accept(this);
    pop_current_scope();
    return 0;
}

auto amun::TypeChecker::visit(SwitchStatement* node) -> std::any
{
    // Check that switch argument is integer type
    auto argument = node_amun_type(node->argument->accept(this));
    auto position = node->keyword.position;

    bool is_argment_enum_type = amun::is_enum_element_type(argument);
    bool is_argument_num_type = amun::is_integer_type(argument);
    if (!is_argument_num_type && !is_argment_enum_type) {
        context->diagnostics.report_error(
            position, "Switch argument type must be integer or enum element but found " +
                          amun::get_type_literal(argument));
        throw "Stop";
    }

    // Check that all cases values are integers or enum element, and no
    // duplication
    std::unordered_set<std::string> cases_values;
    for (auto& branch : node->cases) {
        auto values = branch->values;
        auto branch_position = branch->position.position;

        // Check each value of this case
        for (auto& value : values) {
            auto value_node_type = value->get_ast_node_type();
            if (value_node_type == AstNodeType::AST_ENUM_ELEMENT) {
                if (is_argment_enum_type) {
                    auto enum_access = std::dynamic_pointer_cast<EnumAccessExpression>(value);
                    auto enum_element = std::static_pointer_cast<amun::EnumElementType>(argument);
                    if (enum_access->enum_name.literal != enum_element->enum_name) {
                        context->diagnostics.report_error(
                            branch_position, "Switch argument and case are elements of "
                                             "different enums " +
                                                 enum_element->enum_name + " and " +
                                                 enum_access->enum_name.literal);
                        throw "Stop";
                    }

                    auto enum_index_string = std::to_string(enum_access->enum_element_index);
                    if (!cases_values.insert(enum_index_string).second) {
                        context->diagnostics.report_error(branch_position,
                                                          "Switch can't has more than case "
                                                          "with the same constants value");
                        throw "Stop";
                    }

                    continue;
                }

                context->diagnostics.report_error(
                    branch_position, "Switch argument is enum type and expect all cases to "
                                     "be the same type");
                throw "Stop";
            }

            if (value_node_type == AstNodeType::AST_NUMBER) {
                if (is_argument_num_type) {
                    // If it a number type we must check that it integer
                    auto value_type = node_amun_type(value->accept(this));
                    if (!amun::is_number_type(value_type)) {
                        context->diagnostics.report_error(
                            branch_position, "Switch case value must be an integer but found " +
                                                 amun::get_type_literal(value_type));
                        throw "Stop";
                    }

                    auto number = std::dynamic_pointer_cast<NumberExpression>(value);
                    if (!cases_values.insert(number->value.literal).second) {
                        context->diagnostics.report_error(branch_position,
                                                          "Switch can't has more than case "
                                                          "with the same constants value");
                        throw "Stop";
                    }

                    continue;
                }

                context->diagnostics.report_error(
                    branch_position, "Switch argument is integer type and expect all cases "
                                     "to be the same type");
                throw "Stop";
            }

            context->diagnostics.report_error(branch_position,
                                              "Switch case type must be integer or enum element");
            throw "Stop";
        }

        // Check the branch body once inside new scope
        push_new_scope();
        branch->body->accept(this);
        pop_current_scope();
    }

    // Check default branch body if exists inside new scope
    bool has_else_branch = false;
    auto else_branch = node->default_case;
    if (else_branch) {
        push_new_scope();
        else_branch->body->accept(this);
        pop_current_scope();
        has_else_branch = true;
    }

    // If argument and cases types are enum elements and switch has @complete
    // note Should provide complete check to make sure all cases are covered or
    // there is else branch
    if (node->should_perform_complete_check && amun::is_enum_element_type(argument)) {
        auto enum_element = std::static_pointer_cast<amun::EnumElementType>(argument);
        auto enum_name = enum_element->enum_name;
        auto enum_type = context->enumerations[enum_element->enum_name];
        check_complete_switch_cases(enum_type, cases_values, has_else_branch, position);
    }

    return 0;
}

auto amun::TypeChecker::visit(ReturnStatement* node) -> std::any
{
    if (!node->has_value) {
        if (return_types_stack.top()->type_kind != amun::TypeKind::VOID) {
            context->diagnostics.report_error(node->keyword.position,
                                              "Expect return value to be " +
                                                  amun::get_type_literal(return_types_stack.top()) +
                                                  " but got void");
            throw "Stop";
        }
        return 0;
    }

    auto return_type = node_amun_type(node->value->accept(this));
    auto function_return_type = resolve_generic_type(return_types_stack.top());

    if (!amun::is_types_equals(function_return_type, return_type)) {
        // If Function return type is pointer and return value is null
        // set null pointer base type to function return type
        if (amun::is_pointer_type(function_return_type) and amun::is_null_type(return_type)) {
            auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->value);
            null_expr->null_base_type = function_return_type;
            return 0;
        }

        // If function return type is not pointer, you can't return null
        if (!amun::is_pointer_type(function_return_type) and amun::is_null_type(return_type)) {
            context->diagnostics.report_error(
                node->keyword.position,
                "Can't return null from function that return non pointer type");
            throw "Stop";
        }

        // Prevent returning function with implicit capture from other functions
        if (amun::is_function_pointer_type(function_return_type) and
            amun::is_function_pointer_type(return_type)) {
            auto expected_fun_ptr_type =
                std::static_pointer_cast<amun::PointerType>(function_return_type);
            auto expected_fun_type =
                std::static_pointer_cast<amun::FunctionType>(expected_fun_ptr_type->base_type);

            auto return_fun_ptr = std::static_pointer_cast<amun::PointerType>(return_type);
            auto return_fun =
                std::static_pointer_cast<amun::FunctionType>(return_fun_ptr->base_type);

            if (expected_fun_type->implicit_parameters_count !=
                return_fun->implicit_parameters_count) {
                context->diagnostics.report_error(
                    node->keyword.position, "Can't return lambda that implicit capture values from "
                                            "function");
                throw "Stop";
            }
        }

        context->diagnostics.report_error(node->keyword.position,
                                          "Expect return value to be " +
                                              amun::get_type_literal(function_return_type) +
                                              " but got " + amun::get_type_literal(return_type));
        throw "Stop";
    }

    return 0;
}

auto amun::TypeChecker::visit(DeferStatement* node) -> std::any
{
    node->call_expression->accept(this);
    return 0;
}

auto amun::TypeChecker::visit(BreakStatement* node) -> std::any
{
    if (node->has_times and node->times == 1) {
        context->diagnostics.report_warning(node->keyword.position,
                                            "`break 1;` can implicity written as `break;`");
    }
    return 0;
}

auto amun::TypeChecker::visit(ContinueStatement* node) -> std::any
{
    if (node->has_times and node->times == 1) {
        context->diagnostics.report_warning(node->keyword.position,
                                            "`continue 1;` can implicity written as `continue;`");
    }
    return 0;
}

auto amun::TypeChecker::visit(ExpressionStatement* node) -> std::any
{
    return node->expression->accept(this);
}

auto amun::TypeChecker::visit(IfExpression* node) -> std::any
{
    auto condition = node_amun_type(node->condition->accept(this));
    if (not amun::is_number_type(condition)) {
        context->diagnostics.report_error(node->if_token.position,
                                          "If Expression condition mush be a number but got " +
                                              amun::get_type_literal(condition));
        throw "Stop";
    }

    auto if_value = node_amun_type(node->if_expression->accept(this));
    auto else_value = node_amun_type(node->else_expression->accept(this));
    if (!amun::is_types_equals(if_value, else_value)) {
        context->diagnostics.report_error(node->if_token.position,
                                          "If Expression Type missmatch expect " +
                                              amun::get_type_literal(if_value) + " but got " +
                                              amun::get_type_literal(else_value));
        throw "Stop";
    }
    node->set_type_node(if_value);
    return if_value;
}

auto amun::TypeChecker::visit(SwitchExpression* node) -> std::any
{
    auto argument = node_amun_type(node->argument->accept(this));
    auto position = node->keyword.position;

    auto cases = node->switch_cases;
    auto cases_size = cases.size();

    for (size_t i = 0; i < cases_size; i++) {
        auto case_expression = cases[i];
        auto case_type = node_amun_type(case_expression->accept(this));
        if (!amun::is_types_equals(argument, case_type)) {
            context->diagnostics.report_error(
                position, "Switch case type must be the same type of argument type " +
                              amun::get_type_literal(argument) + " but got " +
                              amun::get_type_literal(case_type) + " in case number " +
                              std::to_string(i + 1));
            throw "Stop";
        }
    }

    auto values = node->switch_cases_values;
    auto expected_type = node_amun_type(values[0]->accept(this));
    for (size_t i = 1; i < cases_size; i++) {
        auto case_value = node_amun_type(values[i]->accept(this));
        if (!amun::is_types_equals(expected_type, case_value)) {
            context->diagnostics.report_error(position,
                                              "Switch cases must be the same time but got " +
                                                  amun::get_type_literal(expected_type) + " and " +
                                                  amun::get_type_literal(case_value));
            throw "Stop";
        }
    }

    bool has_else_branch = false;
    auto else_value = node->default_value;
    if (else_value) {
        auto default_value_type = node_amun_type(else_value->accept(this));
        has_else_branch = true;
        if (!amun::is_types_equals(expected_type, default_value_type)) {
            context->diagnostics.report_error(
                position,
                "Switch case default values must be the same type of other cases expect " +
                    amun::get_type_literal(expected_type) + " but got " +
                    amun::get_type_literal(default_value_type));
            throw "Stop";
        }
    }

    // Check that switch expression are complete
    if (!has_else_branch) {
        if (amun::is_enum_element_type(argument)) {
            auto enum_element = std::static_pointer_cast<amun::EnumElementType>(argument);
            auto enum_name = enum_element->enum_name;
            auto enum_type = context->enumerations[enum_name];
            auto enum_values = enum_type->values;
            auto cases_count = cases.size();
            if (enum_values.size() > cases_count) {
                context->diagnostics.report_error(position,
                                                  "Switch is incomplete and must has else branch");
                throw "Stop";
            }
        }
        else {
            // If argument type is not enum element, it must has else branch
            context->diagnostics.report_error(position,
                                              "Switch is incomplete and must has else branch");
            throw "Stop";
        }
    }

    node->set_type_node(expected_type);
    return expected_type;
}

auto amun::TypeChecker::visit(GroupExpression* node) -> std::any
{
    return node->expression->accept(this);
}

auto amun::TypeChecker::visit(TupleExpression* node) -> std::any
{
    std::vector<Shared<amun::Type>> field_types;
    field_types.reserve(node->values.size());
    for (const auto& value : node->values) {
        field_types.push_back(node_amun_type(value->accept(this)));
    }
    auto tuple_type = std::make_shared<amun::TupleType>("", field_types);
    tuple_type->name = mangle_tuple_type(tuple_type);
    node->set_type_node(tuple_type);
    return tuple_type;
}

auto amun::TypeChecker::visit(AssignExpression* node) -> std::any
{
    auto left_node = node->left;
    auto left_type = node_amun_type(left_node->accept(this));

    // Check that right hand side is a valid type for assignements
    check_valid_assignment_right_side(left_node, node->operator_token.position);

    auto right_type = node_amun_type(node->right->accept(this));

    // if Variable type is pointer and rvalue is null, change null base type to
    // lvalue type
    if (amun::is_pointer_type(left_type) and amun::is_null_type(right_type)) {
        auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->right);
        null_expr->null_base_type = left_type;
        return left_type;
    }

    // RValue type and LValue Type don't matchs
    if (!amun::is_types_equals(left_type, right_type)) {
        context->diagnostics.report_error(node->operator_token.position,
                                          "Type missmatch expect " +
                                              amun::get_type_literal(left_type) + " but got " +
                                              amun::get_type_literal(right_type));
        throw "Stop";
    }

    return right_type;
}

auto amun::TypeChecker::visit(BinaryExpression* node) -> std::any
{
    auto lhs = node_amun_type(node->left->accept(this));
    auto rhs = node_amun_type(node->right->accept(this));
    auto op = node->operator_token;
    auto position = op.position;

    // Check that types are numbers and no need for operator overloading
    if (amun::is_number_type(lhs) && amun::is_number_type(rhs)) {
        if (amun::is_types_equals(lhs, rhs)) {
            return lhs;
        }

        context->diagnostics.report_error(
            position, "Expect numbers types to be the same size but got " +
                          amun::get_type_literal(lhs) + " and " + amun::get_type_literal(rhs));
        throw "Stop";
    }

    // Check if those types has an operator overloading function
    auto function_name = mangle_operator_function(op.kind, {lhs, rhs});
    if (types_table.is_defined(function_name)) {
        auto function = types_table.lookup(function_name);
        auto type = node_amun_type(function);
        assert(type->type_kind == amun::TypeKind::FUNCTION);
        auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
        return function_type->return_type;
    }

    // Report error that no operator overloading function to handle those types
    auto op_literal = overloading_operator_literal[op.kind];
    auto lhs_str = amun::get_type_literal(lhs);
    auto rhs_str = amun::get_type_literal(rhs);
    auto prototype = "operator " + op_literal + "(" + lhs_str + ", " + rhs_str + ")";
    context->diagnostics.report_error(position, "Can't found operator overloading " + prototype);
    throw "Stop";
}

auto amun::TypeChecker::visit(BitwiseExpression* node) -> std::any
{
    auto lhs = node_amun_type(node->left->accept(this));
    auto rhs = node_amun_type(node->right->accept(this));
    auto op = node->operator_token;
    auto position = op.position;

    // Check that types are numbers and no need for operator overloading
    if (amun::is_number_type(lhs) && amun::is_number_type(rhs)) {
        if (amun::is_types_equals(lhs, rhs)) {
            auto right = node->right;
            auto right_node_type = right->get_ast_node_type();

            // Check for compile time integer overflow if possiable
            if (op.kind == TokenKind::TOKEN_RIGHT_SHIFT || op.kind == TokenKind::TOKEN_LEFT_SHIFT) {
                if (right_node_type == AstNodeType::AST_NUMBER) {
                    auto crhs = std::dynamic_pointer_cast<NumberExpression>(right);
                    auto str_value = crhs->value.literal;
                    auto num = str_to_int(str_value.c_str());
                    auto number_kind = std::static_pointer_cast<amun::NumberType>(lhs)->number_kind;
                    auto first_operand_width = number_kind_width[number_kind];
                    if (num >= first_operand_width) {
                        context->diagnostics.report_error(
                            node->operator_token.position,
                            "Shift Expressions second operand can't be "
                            "bigger than or equal first operand bit "
                            "width (" +
                                std::to_string(first_operand_width) + ")");
                        throw "Stop";
                    }
                }

                // Check that scond operand is a positive number
                if (right_node_type == AstNodeType::AST_PREFIX_UNARY) {
                    auto unary = std::dynamic_pointer_cast<PrefixUnaryExpression>(right);
                    if (unary->operator_token.kind == TokenKind::TOKEN_MINUS &&
                        unary->right->get_ast_node_type() == AstNodeType::AST_NUMBER) {
                        context->diagnostics.report_error(
                            node->operator_token.position,
                            "Shift Expressions second operand can't be a negative "
                            "number");
                        throw "Stop";
                    }
                }
            }
            return lhs;
        }

        context->diagnostics.report_error(
            position, "Expect numbers types to be the same size but got " +
                          amun::get_type_literal(lhs) + " and " + amun::get_type_literal(rhs));
        throw "Stop";
    }

    // Check if those types has an operator overloading function
    auto function_name = mangle_operator_function(op.kind, {lhs, rhs});
    if (types_table.is_defined(function_name)) {
        auto function = types_table.lookup(function_name);
        auto type = node_amun_type(function);
        assert(type->type_kind == amun::TypeKind::FUNCTION);
        auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
        return function_type->return_type;
    }

    // Report error that no operator overloading function to handle those types
    auto op_literal = overloading_operator_literal[op.kind];
    auto lhs_str = amun::get_type_literal(lhs);
    auto rhs_str = amun::get_type_literal(rhs);
    auto prototype = "operator " + op_literal + "(" + lhs_str + ", " + rhs_str + ")";
    context->diagnostics.report_error(position, "Can't found operator overloading " + prototype);
    throw "Stop";
}

auto amun::TypeChecker::visit(ComparisonExpression* node) -> std::any
{
    auto lhs = node_amun_type(node->left->accept(this));
    auto rhs = node_amun_type(node->right->accept(this));
    auto are_types_equals = amun::is_types_equals(lhs, rhs);
    auto op = node->operator_token;
    auto position = op.position;

    // Numbers comparasions
    if (amun::is_number_type(lhs) && amun::is_number_type(rhs)) {
        if (are_types_equals) {
            return amun::i1_type;
        }

        context->diagnostics.report_error(
            position, "Expect numbers types to be the same size but got " +
                          amun::get_type_literal(lhs) + " and " + amun::get_type_literal(rhs));
        throw "Stop";
    }

    // Enumerations elements comparasions
    if (amun::is_enum_element_type(lhs) && amun::is_enum_element_type(rhs)) {
        if (are_types_equals) {
            return amun::i1_type;
        }

        context->diagnostics.report_error(
            position, "You can't compare elements from different enums " +
                          amun::get_type_literal(lhs) + " and " + amun::get_type_literal(rhs));
        throw "Stop";
    }

    // Pointers comparasions
    if (amun::is_pointer_type(lhs) && amun::is_pointer_type(rhs)) {
        if (are_types_equals) {
            return amun::i1_type;
        }

        context->diagnostics.report_error(node->operator_token.position,
                                          "You can't compare pointers to different types " +
                                              amun::get_type_literal(lhs) + " and " +
                                              amun::get_type_literal(rhs));
        throw "Stop";
    }

    // Pointer vs null comparaisons and set null pointer base type
    if (amun::is_pointer_type(lhs) && amun::is_null_type(rhs)) {
        auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->right);
        null_expr->null_base_type = lhs;
        return amun::i1_type;
    }

    // Null vs Pointer comparaisons and set null pointer base type
    if (amun::is_null_type(lhs) && amun::is_pointer_type(rhs)) {
        auto null_expr = std::dynamic_pointer_cast<NullExpression>(node->left);
        null_expr->null_base_type = rhs;
        return amun::i1_type;
    }

    // Null vs Null comparaisons, no need to set pointer base, use the default
    // base
    if (amun::is_null_type(lhs) && amun::is_null_type(rhs)) {
        return amun::i1_type;
    }

    // Can't compare null value with non pointer value
    if (amun::is_null_type(lhs) || amun::is_null_type(rhs)) {
        context->diagnostics.report_error(node->operator_token.position,
                                          "Can't compare non pointer type with null value");
        throw "Stop";
    }

    // Check if those types has an operator overloading function
    auto function_name = mangle_operator_function(op.kind, {lhs, rhs});
    if (types_table.is_defined(function_name)) {
        auto function = types_table.lookup(function_name);
        auto type = node_amun_type(function);
        assert(type->type_kind == amun::TypeKind::FUNCTION);
        auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
        return function_type->return_type;
    }

    // Report error that no operator overloading function to handle those types
    auto op_literal = overloading_operator_literal[op.kind];
    auto lhs_str = amun::get_type_literal(lhs);
    auto rhs_str = amun::get_type_literal(rhs);
    auto prototype = "operator " + op_literal + "(" + lhs_str + ", " + rhs_str + ")";
    context->diagnostics.report_error(position, "Can't found operator overloading " + prototype);
    throw "Stop";
}

auto amun::TypeChecker::visit(LogicalExpression* node) -> std::any
{
    auto lhs = node_amun_type(node->left->accept(this));
    auto rhs = node_amun_type(node->right->accept(this));

    if (amun::is_integer1_type(lhs) && amun::is_integer1_type(rhs)) {
        return lhs;
    }

    auto op = node->operator_token;

    // Check if those types has an operator overloading function
    auto function_name = mangle_operator_function(op.kind, {lhs, rhs});
    if (types_table.is_defined(function_name)) {
        auto function = types_table.lookup(function_name);
        auto type = node_amun_type(function);
        assert(type->type_kind == amun::TypeKind::FUNCTION);
        auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
        return function_type->return_type;
    }

    // Report error that no operator overloading function to handle those types
    auto position = op.position;
    auto op_literal = overloading_operator_literal[op.kind];
    auto lhs_str = amun::get_type_literal(lhs);
    auto rhs_str = amun::get_type_literal(rhs);
    auto prototype = "operator " + op_literal + "(" + lhs_str + ", " + rhs_str + ")";
    context->diagnostics.report_error(position, "Can't found operator overloading " + prototype);
    throw "Stop";
}

auto amun::TypeChecker::visit(PrefixUnaryExpression* node) -> std::any
{
    auto rhs = node_amun_type(node->right->accept(this));
    auto op_kind = node->operator_token.kind;

    if (op_kind == TokenKind::TOKEN_MINUS) {
        if (amun::is_number_type(rhs)) {
            node->set_type_node(rhs);
            return rhs;
        }

        // Check if those types has an operator overloading function
        auto function_name = "_prefix" + mangle_operator_function(op_kind, {rhs});
        if (types_table.is_defined(function_name)) {
            auto function = types_table.lookup(function_name);
            auto type = node_amun_type(function);
            assert(type->type_kind == amun::TypeKind::FUNCTION);
            auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
            return function_type->return_type;
        }

        context->diagnostics.report_error(
            node->operator_token.position,
            "Unary Minus `-` expect numbers or to override operators " +
                amun::get_type_literal(rhs));
        throw "Stop";
    }

    if (op_kind == TokenKind::TOKEN_BANG) {
        if (amun::is_number_type(rhs)) {
            node->set_type_node(rhs);
            return rhs;
        }

        // Check if those types has an operator overloading function
        auto function_name = "_prefix" + mangle_operator_function(op_kind, {rhs});
        if (types_table.is_defined(function_name)) {
            auto function = types_table.lookup(function_name);
            auto type = node_amun_type(function);
            assert(type->type_kind == amun::TypeKind::FUNCTION);
            auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
            return function_type->return_type;
        }

        context->diagnostics.report_error(node->operator_token.position,
                                          "Bang `!` expect numbers or to override operators " +
                                              amun::get_type_literal(rhs));
        throw "Stop";
    }

    if (op_kind == TokenKind::TOKEN_NOT) {
        if (amun::is_number_type(rhs)) {
            node->set_type_node(rhs);
            return rhs;
        }

        // Check if those types has an operator overloading function
        auto function_name = "_prefix" + mangle_operator_function(op_kind, {rhs});
        if (types_table.is_defined(function_name)) {
            auto function = types_table.lookup(function_name);
            auto type = node_amun_type(function);
            assert(type->type_kind == amun::TypeKind::FUNCTION);
            auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
            return function_type->return_type;
        }

        context->diagnostics.report_error(node->operator_token.position,
                                          "Not `~` expect numbers or to override operators " +
                                              amun::get_type_literal(rhs));
        throw "Stop";
    }

    if (op_kind == TokenKind::TOKEN_STAR) {
        if (rhs->type_kind == amun::TypeKind::POINTER) {
            auto pointer_type = std::static_pointer_cast<amun::PointerType>(rhs);
            auto type = pointer_type->base_type;
            node->set_type_node(type);
            return type;
        }

        context->diagnostics.report_error(
            node->operator_token.position,
            "Derefernse operator require pointer as an right operand but got " +
                amun::get_type_literal(rhs));
        throw "Stop";
    }

    if (op_kind == TokenKind::TOKEN_AND) {
        auto pointer_type = std::make_shared<amun::PointerType>(rhs);
        if (amun::is_function_pointer_type(pointer_type)) {
            auto function_type =
                std::static_pointer_cast<amun::FunctionType>(pointer_type->base_type);
            if (function_type->is_intrinsic) {
                context->diagnostics.report_error(function_type->name.position,
                                                  "Can't take address of an intrinsic function");
                throw "Stop";
            }
        }
        node->set_type_node(pointer_type);
        return pointer_type;
    }

    if (op_kind == TokenKind::TOKEN_PLUS_PLUS || op_kind == TokenKind::TOKEN_MINUS_MINUS) {
        if (rhs->type_kind == amun::TypeKind::NUMBER) {
            node->set_type_node(rhs);
            return rhs;
        }

        // Check if those types has an operator overloading function
        auto function_name = "_prefix" + mangle_operator_function(op_kind, {rhs});
        if (types_table.is_defined(function_name)) {
            auto function = types_table.lookup(function_name);
            auto type = node_amun_type(function);
            assert(type->type_kind == amun::TypeKind::FUNCTION);
            auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
            return function_type->return_type;
        }

        context->diagnostics.report_error(
            node->operator_token.position,
            "Unary ++ or -- expect numbers or to override operators " +
                amun::get_type_literal(rhs));
        throw "Stop";
    }

    context->diagnostics.report_error(node->operator_token.position,
                                      "Unsupported unary expression " +
                                          amun::get_type_literal(rhs));
    throw "Stop";
}

auto amun::TypeChecker::visit(PostfixUnaryExpression* node) -> std::any
{
    auto rhs = node_amun_type(node->right->accept(this));
    auto op_kind = node->operator_token.kind;
    auto position = node->operator_token.position;

    if (op_kind == TokenKind::TOKEN_PLUS_PLUS or op_kind == TokenKind::TOKEN_MINUS_MINUS) {
        if (rhs->type_kind == amun::TypeKind::NUMBER) {
            node->set_type_node(rhs);
            return rhs;
        }

        // Check if those types has an operator overloading function
        auto function_name = "_postfix" + mangle_operator_function(op_kind, {rhs});
        if (types_table.is_defined(function_name)) {
            auto function = types_table.lookup(function_name);
            auto type = node_amun_type(function);
            assert(type->type_kind == amun::TypeKind::FUNCTION);
            auto function_type = std::static_pointer_cast<amun::FunctionType>(type);
            return function_type->return_type;
        }

        context->diagnostics.report_error(
            position, "Unary ++ or -- expect numbers or to override operators " +
                          amun::get_type_literal(rhs));
        throw "Stop";
    }

    context->diagnostics.report_error(position, "Unsupported unary expression " +
                                                    amun::get_type_literal(rhs));
    throw "Stop";
}

auto amun::TypeChecker::visit(CallExpression* node) -> std::any
{
    auto callee = node->callee;
    auto callee_ast_node_type = node->callee->get_ast_node_type();
    auto node_span = node->position.position;

    // Call function by name for example function();
    if (callee_ast_node_type == AstNodeType::AST_LITERAL) {
        auto literal = std::dynamic_pointer_cast<LiteralExpression>(callee);
        auto name = literal->name.literal;
        if (types_table.is_defined(name)) {
            auto lookup = types_table.lookup(name);
            auto value = node_amun_type(types_table.lookup(name));

            if (value->type_kind == amun::TypeKind::POINTER) {
                auto pointer_type = std::static_pointer_cast<amun::PointerType>(value);
                value = pointer_type->base_type;
            }

            if (value->type_kind == amun::TypeKind::FUNCTION) {
                auto type = std::static_pointer_cast<amun::FunctionType>(value);
                node->set_type_node(type);
                auto parameters = type->parameters;
                auto arguments = node->arguments;
                for (auto& argument : arguments) {
                    argument->set_type_node(node_amun_type(argument->accept(this)));
                }

                check_parameters_types(node_span, arguments, parameters, type->has_varargs,
                                       type->varargs_type, type->implicit_parameters_count);

                return type->return_type;
            }
            else {
                context->diagnostics.report_error(node_span,
                                                  "Call expression work only with function");
                throw "Stop";
            }
        }

        else if (generic_functions_declaraions.contains(name)) {
            auto function_declaraion = generic_functions_declaraions[name];
            auto function_prototype = function_declaraion->prototype;
            auto prototype_parameters = function_prototype->parameters;
            auto prototype_generic_names = function_prototype->generic_parameters;

            auto call_arguments = node->arguments;
            auto call_generic_arguments = node->generic_arguments;

            if (prototype_parameters.size() != call_arguments.size()) {
                context->diagnostics.report_error(
                    node_span, "Invalid number of arguments, expect " +
                                   std::to_string(prototype_parameters.size()) + " but got " +
                                   std::to_string(call_arguments.size()));
                throw "Stop";
            }

            auto call_has_generic_arguments = call_generic_arguments.empty();
            if (call_has_generic_arguments) {
                if (prototype_parameters.empty()) {
                    context->diagnostics.report_error(
                        node_span, "Not enough information to infer generic types variables");
                    throw "Stop";
                }

                node->generic_arguments.resize(prototype_generic_names.size());

                int parameter_index = 0;
                std::set<int> generic_arguments_indeces;
                for (const auto& parameter : prototype_parameters) {
                    const auto parameter_type = parameter->type;
                    const auto resolved_argument = call_arguments[parameter_index]->accept(this);
                    const auto argument_type = node_amun_type(resolved_argument);

                    if (!amun::is_types_equals(parameter_type, argument_type)) {

                        // Report error when use pass `null` as argment
                        if (amun::is_null_type(argument_type)) {
                            context->diagnostics.report_error(node_span,
                                                              "Not enough information to infer "
                                                              "generic parameter from null value");
                            throw "Stop";
                        }

                        // Report error when use pass `void` as argment
                        if (amun::is_void_type(argument_type)) {
                            context->diagnostics.report_error(
                                node_span, "Can't pass `void` value as argument");
                            throw "Stop";
                        }

                        auto result_map = infier_type_by_other_type(parameter_type, argument_type);
                        for (const auto& type_pair : result_map) {
                            auto index = index_of(prototype_generic_names, type_pair.first);
                            if (generic_arguments_indeces.insert(index).second) {
                                node->generic_arguments[index] = type_pair.second;
                            }
                        }
                    }

                    parameter_index++;
                }

                if (generic_arguments_indeces.size() != prototype_generic_names.size()) {
                    context->diagnostics.report_error(
                        node_span, "Not enough information to infer all generic types variables");
                    throw "Stop";
                }

                call_generic_arguments = node->generic_arguments;
            }

            auto generic_arguments_count = prototype_generic_names.size();
            auto generic_parameters_count = call_generic_arguments.size();
            if (generic_parameters_count != generic_arguments_count) {
                context->diagnostics.report_error(
                    node_span, "Not enough information to infer all generic types variables");
                throw "Stop";
            }

            for (size_t i = 0; i < generic_parameters_count; i++) {
                generic_types[prototype_generic_names[i]] = call_generic_arguments[i];
            }

            auto return_type = resolve_generic_type(
                function_prototype->return_type, prototype_generic_names, call_generic_arguments);
            return_types_stack.push(return_type);

            std::vector<Shared<amun::Type>> resolved_parameters;
            resolved_parameters.reserve(prototype_parameters.size());
            for (const auto& parameter : prototype_parameters) {
                resolved_parameters.push_back(resolve_generic_type(
                    parameter->type, prototype_generic_names, call_generic_arguments));
            }

            push_new_scope();

            int index = 0;
            for (auto& parameter : prototype_parameters) {
                types_table.define(parameter->name.literal, resolved_parameters[index]);
                index++;
            }

            function_declaraion->body->accept(this);
            pop_current_scope();

            return_types_stack.pop();

            auto arguments = call_arguments;
            for (auto& argument : arguments) {
                auto argument_type = node_amun_type(argument->accept(this));
                argument_type = resolve_generic_type(argument_type);
                argument->set_type_node(argument_type);
            }

            check_parameters_types(node_span, arguments, resolved_parameters,
                                   function_prototype->has_varargs,
                                   function_prototype->varargs_type, 0);

            generic_types.clear();
            return return_type;
        }

        else {
            context->diagnostics.report_error(node_span,
                                              "Can't resolve function call with name " + name);
            throw "Stop";
        }
    }

    // Call function pointer returned from call expression for example
    // function()();
    if (callee_ast_node_type == AstNodeType::AST_CALL) {
        auto call = std::dynamic_pointer_cast<CallExpression>(callee);
        auto call_result = node_amun_type(call->accept(this));
        auto function_pointer_type = std::static_pointer_cast<amun::PointerType>(call_result);
        auto function_type =
            std::static_pointer_cast<amun::FunctionType>(function_pointer_type->base_type);
        auto parameters = function_type->parameters;
        auto arguments = node->arguments;
        check_parameters_types(node_span, arguments, parameters, function_type->has_varargs,
                               function_type->varargs_type,
                               function_type->implicit_parameters_count);
        node->set_type_node(function_type);
        return function_type->return_type;
    }

    // Call lambda expression for example { () void -> return; } ()
    if (callee_ast_node_type == AstNodeType::AST_LAMBDA) {
        auto lambda = std::dynamic_pointer_cast<LambdaExpression>(node->callee);
        auto lambda_function_type = node_amun_type(lambda->accept(this));
        auto function_ptr_type = std::static_pointer_cast<amun::PointerType>(lambda_function_type);

        auto function_type =
            std::static_pointer_cast<amun::FunctionType>(function_ptr_type->base_type);

        auto parameters = function_type->parameters;
        auto arguments = node->arguments;
        for (auto& argument : arguments) {
            argument->set_type_node(node_amun_type(argument->accept(this)));
        }

        check_parameters_types(node_span, arguments, parameters, function_type->has_varargs,
                               function_type->varargs_type,
                               function_type->implicit_parameters_count);

        node->set_type_node(function_type);
        return function_type->return_type;
    }

    // Call struct field with function pointer for example type struct.field()
    if (callee_ast_node_type == AstNodeType::AST_DOT) {
        auto dot_expression = std::dynamic_pointer_cast<DotExpression>(node->callee);
        auto dot_function_type = node_amun_type(dot_expression->accept(this));
        auto function_ptr_type = std::static_pointer_cast<amun::PointerType>(dot_function_type);

        auto function_type =
            std::static_pointer_cast<amun::FunctionType>(function_ptr_type->base_type);

        auto parameters = function_type->parameters;
        auto arguments = node->arguments;
        for (auto& argument : arguments) {
            argument->set_type_node(node_amun_type(argument->accept(this)));
        }

        check_parameters_types(node_span, arguments, parameters, function_type->has_varargs,
                               function_type->varargs_type,
                               function_type->implicit_parameters_count);

        node->set_type_node(function_type);
        return function_type->return_type;
    }

    context->diagnostics.report_error(node_span, "Unexpected callee type for Call Expression");
    throw "Stop";
}

auto amun::TypeChecker::visit(InitializeExpression* node) -> std::any
{
    auto type = resolve_generic_type(node->type);
    node->set_type_node(type);

    if (type->type_kind == amun::TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<amun::StructType>(type);
        auto parameters = struct_type->fields_types;
        auto arguments = node->arguments;

        check_parameters_types(node->position.position, arguments, parameters, false, nullptr, 0);
        return struct_type;
    }

    context->diagnostics.report_error(node->position.position,
                                      "InitializeExpression work only with structures");
    throw "Stop";
}

auto amun::TypeChecker::visit(LambdaExpression* node) -> std::any
{
    auto function_ptr_type = std::static_pointer_cast<amun::PointerType>(node->get_type_node());
    auto function_type = std::static_pointer_cast<amun::FunctionType>(function_ptr_type->base_type);

    // Resolving return type
    function_type->return_type = resolve_generic_type(function_type->return_type);
    return_types_stack.push(function_type->return_type);

    is_inside_lambda_body = true;
    lambda_implicit_parameters.push({});

    push_new_scope();

    function_type->parameters.clear();

    // Define Explicit parameter inside lambda body scope
    for (auto& parameter : node->explicit_parameters) {
        // Resolve only if lambda is inside generic function
        parameter->type = resolve_generic_type(parameter->type);
        types_table.define(parameter->name.literal, parameter->type);
        function_type->parameters.push_back(parameter->type);
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

auto amun::TypeChecker::visit(DotExpression* node) -> std::any
{
    auto callee = node->callee->accept(this);
    auto callee_type = node_amun_type(callee);
    auto callee_type_kind = callee_type->type_kind;
    auto node_position = node->dot_token.position;

    if (callee_type_kind == amun::TypeKind::STRUCT) {

        // Assert that use access struct member only using field name
        if (node->field_name.kind != TokenKind::TOKEN_IDENTIFIER) {
            context->diagnostics.report_error(node_position,
                                              "Can't access struct member using index, only "
                                              "tuples can do this");
            throw "Stop";
        }

        auto struct_type = std::static_pointer_cast<amun::StructType>(callee_type);
        auto field_name = node->field_name.literal;
        auto fields_names = struct_type->fields_names;
        if (is_contains(fields_names, field_name)) {
            int member_index = index_of(fields_names, field_name);
            auto field_type = struct_type->fields_types[member_index];
            node->set_type_node(field_type);
            node->field_index = member_index;
            return field_type;
        }

        context->diagnostics.report_error(node_position, "Can't find a field with name " +
                                                             field_name + " in struct " +
                                                             struct_type->name);
        throw "Stop";
    }

    if (callee_type_kind == amun::TypeKind::TUPLE) {
        // Assert that use access tuple using integer position only
        if (node->field_name.kind != TokenKind::TOKEN_INT) {
            context->diagnostics.report_error(node_position,
                                              "Tuple must be accessed using position only");
            throw "Stop";
        }

        auto tuple_type = std::static_pointer_cast<amun::TupleType>(callee_type);
        auto field_index = node->field_index;

        if (field_index >= tuple_type->fields_types.size()) {
            context->diagnostics.report_error(node_position, "No tuple field with index " +
                                                                 std::to_string(field_index));
            throw "Stop";
        }
        auto field_type = tuple_type->fields_types[field_index];
        node->set_type_node(field_type);
        return field_type;
    }

    if (callee_type_kind == amun::TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<amun::PointerType>(callee_type);
        auto pointer_to_type = pointer_type->base_type;
        if (pointer_to_type->type_kind == amun::TypeKind::STRUCT) {
            auto struct_type = std::static_pointer_cast<amun::StructType>(pointer_to_type);
            auto field_name = node->field_name.literal;
            auto fields_names = struct_type->fields_names;
            if (is_contains(fields_names, field_name)) {
                int member_index = index_of(fields_names, field_name);
                auto field_type = struct_type->fields_types[member_index];
                node->set_type_node(field_type);
                node->field_index = member_index;
                return field_type;
            }
            context->diagnostics.report_error(node_position, "Can't find a field with name " +
                                                                 field_name + " in struct " +
                                                                 struct_type->name);
            throw "Stop";
        }

        if (amun::is_types_equals(pointer_to_type, amun::i8_type)) {
            auto attribute_token = node->field_name;
            auto literal = attribute_token.literal;

            if (literal == "count") {
                node->is_constants_ = node->callee->get_ast_node_type() == AstNodeType::AST_STRING;
                node->set_type_node(amun::i64_type);
                return amun::i64_type;
            }

            context->diagnostics.report_error(node_position,
                                              "Unkown String attribute with name " + literal);
            throw "Stop";
        }

        context->diagnostics.report_error(node_position,
                                          "Dot expression expect calling member from struct "
                                          "or pointer to struct");
        throw "Stop";
    }

    if (callee_type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto attribute_token = node->field_name;
        auto literal = attribute_token.literal;

        if (literal == "count") {
            node->is_constants_ = true;
            node->set_type_node(amun::i64_type);
            return amun::i64_type;
        }

        context->diagnostics.report_error(node_position,
                                          "Unkown Array attribute with name " + literal);
        throw "Stop";
    }

    if (callee_type_kind == amun::TypeKind::STATIC_VECTOR) {
        auto attribute_token = node->field_name;
        auto literal = attribute_token.literal;

        if (literal == "count") {
            node->is_constants_ = true;
            node->set_type_node(amun::i64_type);
            return amun::i64_type;
        }

        context->diagnostics.report_error(node_position,
                                          "Unkown Array attribute with name " + literal);
        throw "Stop";
    }

    if (callee_type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto generic_type = std::static_pointer_cast<amun::GenericStructType>(callee_type);
        auto resolved_type = resolve_generic_type(generic_type);
        auto struct_type = std::static_pointer_cast<amun::StructType>(resolved_type);
        auto fields_names = struct_type->fields_names;
        auto field_name = node->field_name.literal;
        if (is_contains(fields_names, field_name)) {
            int member_index = index_of(fields_names, field_name);
            auto field_type = struct_type->fields_types[member_index];
            node->set_type_node(field_type);
            node->field_index = member_index;
            return field_type;
        }
        context->diagnostics.report_error(node_position, "Can't find a field with name " +
                                                             field_name + " in struct " +
                                                             struct_type->name);
        throw "Stop";
    }

    context->diagnostics.report_error(node_position,
                                      "Dot expression expect struct or enum type as lvalue");
    throw "Stop";
}

auto amun::TypeChecker::visit(CastExpression* node) -> std::any
{
    auto value = node->value;
    auto value_type = node_amun_type(value->accept(this));
    auto target_type = node->get_type_node();
    target_type = resolve_generic_type(target_type);
    auto node_position = node->position.position;

    // No need for castring if both has the same type
    if (amun::is_types_equals(value_type, target_type)) {
        context->diagnostics.report_warning(node_position, "unnecessary cast to the same type");
        return target_type;
    }

    if (!amun::can_types_casted(value_type, target_type)) {
        context->diagnostics.report_error(node_position,
                                          "Can't cast from " + amun::get_type_literal(value_type) +
                                              " to " + amun::get_type_literal(target_type));
        throw "Stop";
    }

    return target_type;
}

auto amun::TypeChecker::visit(TypeSizeExpression* node) -> std::any
{
    auto type = node->type;
    auto resolved_type = resolve_generic_type(type);
    node->type = resolved_type;
    return amun::i64_type;
}

auto amun::TypeChecker::visit(TypeAlignExpression* node) -> std::any
{
    auto type = node->type;
    auto resolved_type = resolve_generic_type(type);
    node->type = resolved_type;
    return amun::i64_type;
}

auto amun::TypeChecker::visit(ValueSizeExpression* node) -> std::any
{
    node->value->accept(this);
    return amun::i64_type;
}

auto amun::TypeChecker::visit(IndexExpression* node) -> std::any
{
    auto index_expression = node->index;
    auto index_type = node_amun_type(index_expression->accept(this));
    auto position = node->position.position;

    // Make sure index is integer type with any size
    if (!amun::is_integer_type(index_type)) {
        context->diagnostics.report_error(position, "Index must be an integer but got " +
                                                        amun::get_type_literal(index_type));
        throw "Stop";
    }

    // Check if index is number expression to perform compile time checks
    bool has_constant_index = index_expression->get_ast_node_type() == AstNodeType::AST_NUMBER;
    size_t constant_index = -1;

    if (has_constant_index) {
        auto number_expr = std::dynamic_pointer_cast<NumberExpression>(index_expression);
        auto number_literal = number_expr->value.literal;
        constant_index = str_to_int(number_literal.c_str());

        // Check that index is not negative
        if (constant_index < 0) {
            context->diagnostics.report_error(position, "Index can't be negative number");
            throw "Stop";
        }
    }

    auto callee_expression = node->value;
    auto callee_type = node_amun_type(callee_expression->accept(this));

    if (callee_type->type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto array_type = std::static_pointer_cast<amun::StaticArrayType>(callee_type);
        node->set_type_node(array_type->element_type);

        // Compile time bounds check
        if (has_constant_index && constant_index >= array_type->size) {
            context->diagnostics.report_error(position,
                                              "Index can't be bigger than or equal array size");
            throw "Stop";
        }

        return array_type->element_type;
    }

    if (callee_type->type_kind == amun::TypeKind::STATIC_VECTOR) {
        auto vector_type = std::static_pointer_cast<amun::StaticVectorType>(callee_type);
        auto array_type = vector_type->array;
        node->set_type_node(array_type->element_type);

        // Compile time bounds check
        if (has_constant_index && constant_index >= array_type->size) {
            context->diagnostics.report_error(position,
                                              "Index can't be bigger than or equal array size");
            throw "Stop";
        }

        return array_type->element_type;
    }

    if (callee_type->type_kind == amun::TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<amun::PointerType>(callee_type);
        node->set_type_node(pointer_type->base_type);
        return pointer_type->base_type;
    }

    context->diagnostics.report_error(position, "Index expression require array but got " +
                                                    amun::get_type_literal(callee_type));
    throw "Stop";
}

auto amun::TypeChecker::visit(EnumAccessExpression* node) -> std::any
{
    return node->get_type_node();
}

auto amun::TypeChecker::visit(LiteralExpression* node) -> std::any
{
    const auto name = node->name.literal;
    if (!types_table.is_defined(name)) {
        context->diagnostics.report_error(node->name.position,
                                          "Can't resolve variable with name " + node->name.literal);
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

            // Check if it not global variable, need to perform implicit capture
            // for this variable
            if (declared_scope_level != 0 && declared_scope_level < types_table.size() - 2) {
                auto type = node_amun_type(value);
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

    auto type = node_amun_type(value);
    node->type = type;

    if (type->type_kind == amun::TypeKind::NUMBER ||
        type->type_kind == amun::TypeKind::ENUM_ELEMENT) {
        node->set_constant(true);
    }

    return type;
}

auto amun::TypeChecker::visit(NumberExpression* node) -> std::any
{
    auto number_type = std::static_pointer_cast<amun::NumberType>(node->get_type_node());
    auto number_kind = number_type->number_kind;
    auto number_literal = node->value.literal;

    bool is_valid_range = check_number_limits(number_literal.c_str(), number_kind);
    if (not is_valid_range) {
        // TODO: Diagnostic message can be improved and provide more information
        // for example `value x must be in range s .. e or you should change the
        // type to y`
        context->diagnostics.report_error(node->value.position,
                                          "Number Value " + number_literal +
                                              " Can't be represented using type " +
                                              amun::get_type_literal(number_type));
        throw "Stop";
    }

    return number_type;
}

auto amun::TypeChecker::visit(ArrayExpression* node) -> std::any
{
    const auto values = node->values;
    const auto values_size = values.size();
    if (values_size == 0) {
        return node->get_type_node();
    }

    auto last_element_type = node_amun_type(values[0]->accept(this));
    for (size_t i = 1; i < values_size; i++) {
        auto current_element_type = node_amun_type(values[i]->accept(this));
        if (amun::is_types_equals(current_element_type, last_element_type)) {
            last_element_type = current_element_type;
            continue;
        }

        context->diagnostics.report_error(
            node->position.position, "Array elements with index " + std::to_string(i - 1) +
                                         " and " + std::to_string(i) + " are not the same types");
        throw "Stop";
    }

    // Modify element_type with the type of first elements
    auto array_type = std::static_pointer_cast<amun::StaticArrayType>(node->get_type_node());
    array_type->element_type = last_element_type;
    node->set_type_node(array_type);
    return array_type;
}

auto amun::TypeChecker::visit(VectorExpression* node) -> std::any
{
    auto array = node->array;
    auto array_type = std::static_pointer_cast<amun::StaticArrayType>(array->type);
    auto element_type = array_type->element_type;

    if (element_type->type_kind != TypeKind::NUMBER || amun::is_signed_integer_type(element_type)) {
        auto position = node->array->position.position;
        context->diagnostics.report_error(position,
                                          "vector type accept only unsinged number or float types");
        throw "Stop";
    }

    return node->get_type_node();
}

auto amun::TypeChecker::visit(StringExpression* node) -> std::any { return node->get_type_node(); }

auto amun::TypeChecker::visit(CharacterExpression* node) -> std::any
{
    return node->get_type_node();
}

auto amun::TypeChecker::visit(BooleanExpression* node) -> std::any { return node->get_type_node(); }

auto amun::TypeChecker::visit(NullExpression* node) -> std::any { return node->get_type_node(); }

auto amun::TypeChecker::visit(UndefinedExpression* node) -> std::any
{
    return node->get_type_node();
}

auto amun::TypeChecker::node_amun_type(std::any any_type) -> Shared<amun::Type>
{
    if (any_type.type() == typeid(Shared<amun::FunctionType>)) {
        return std::any_cast<Shared<amun::FunctionType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::PointerType>)) {
        return std::any_cast<Shared<amun::PointerType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::NumberType>)) {
        return std::any_cast<Shared<amun::NumberType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::StaticArrayType>)) {
        return std::any_cast<Shared<amun::StaticArrayType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::StructType>)) {
        return std::any_cast<Shared<amun::StructType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::TupleType>)) {
        return std::any_cast<Shared<amun::TupleType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::EnumType>)) {
        return std::any_cast<Shared<amun::EnumType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::EnumElementType>)) {
        return std::any_cast<Shared<amun::EnumElementType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::NoneType>)) {
        return std::any_cast<Shared<amun::NoneType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::VoidType>)) {
        return std::any_cast<Shared<amun::VoidType>>(any_type);
    }
    if (any_type.type() == typeid(Shared<amun::NullType>)) {
        return std::any_cast<Shared<amun::NullType>>(any_type);
    }

    return std::any_cast<Shared<amun::Type>>(any_type);
}

auto amun::TypeChecker::is_same_type(const Shared<amun::Type>& left,
                                     const Shared<amun::Type>& right) -> bool
{
    return left->type_kind == right->type_kind;
}

auto amun::TypeChecker::resolve_generic_type(Shared<amun::Type> type,
                                             std::vector<std::string> generic_names,
                                             std::vector<Shared<amun::Type>> generic_parameters)
    -> Shared<amun::Type>
{
    const auto type_kind = type->type_kind;

    // Generic parameter such as T, E, ...etc
    if (type_kind == amun::TypeKind::GENERIC_PARAMETER) {
        auto generic = std::static_pointer_cast<amun::GenericParameterType>(type);
        auto position = index_of(generic_names, generic->name);

        if (position >= 0) {
            auto resolved_type = generic_parameters[position];
            this->generic_types[generic->name] = resolved_type;
            return resolved_type;
        }

        return this->generic_types[generic->name];
    }

    // Resolve the base of pointer type
    if (type_kind == amun::TypeKind::POINTER) {
        auto pointer = std::static_pointer_cast<amun::PointerType>(type);
        pointer->base_type =
            resolve_generic_type(pointer->base_type, generic_names, generic_parameters);
        return pointer;
    }

    // Resolve the element type of array type
    if (type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto array = std::static_pointer_cast<amun::StaticArrayType>(type);
        array->element_type =
            resolve_generic_type(array->element_type, generic_names, generic_parameters);
        return array;
    }

    // Resolve function pointer with generic parameters or return type
    if (type_kind == amun::TypeKind::FUNCTION) {
        auto function = std::static_pointer_cast<amun::FunctionType>(type);

        function->return_type =
            resolve_generic_type(function->return_type, generic_names, generic_parameters);

        auto parameters = function->parameters;
        auto parameters_count = parameters.size();

        for (uint64 i = 0; i < parameters_count; i++) {
            function->parameters[i] =
                resolve_generic_type(function->parameters[i], generic_names, generic_parameters);
        }

        return function;
    }

    // Resolve the fields types of structure type
    if (type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto generic_struct = std::static_pointer_cast<amun::GenericStructType>(type);
        auto structure = generic_struct->struct_type;

        int i = 0;
        auto generic_struct_param = generic_struct->parameters;
        for (auto& parameter : generic_struct_param) {
            if (parameter->type_kind == amun::TypeKind::GENERIC_PARAMETER) {
                auto generic_type = std::static_pointer_cast<amun::GenericParameterType>(parameter);
                if (generic_types.contains(generic_type->name)) {
                    generic_struct->parameters[i] = generic_types[generic_type->name];
                }
            }
            i++;
        }

        auto mangled_name = structure->name + mangle_types(generic_struct->parameters);
        if (types_table.is_defined(mangled_name)) {
            return std::any_cast<Shared<amun::StructType>>(types_table.lookup(mangled_name));
        }

        std::vector<std::string> fields_names;
        for (const auto& name : structure->fields_names) {
            fields_names.push_back(name);
        }

        std::vector<Shared<amun::Type>> types;
        for (const auto& type : structure->fields_types) {
            types.push_back(resolve_generic_type(type, structure->generic_parameters,
                                                 generic_struct->parameters));
        }

        auto new_struct = std::make_shared<amun::StructType>(
            mangled_name, fields_names, types, structure->generic_parameters, true, true);
        new_struct->generic_parameters_types = generic_struct->parameters;
        types_table.define(mangled_name, new_struct);
        return new_struct;
    }

    // Resolve the inner types of tuple type
    if (type_kind == amun::TypeKind::TUPLE) {
        auto tuple = std::static_pointer_cast<amun::TupleType>(type);
        for (auto& field : tuple->fields_types) {
            field = resolve_generic_type(field, generic_names, generic_parameters);
        }
        tuple->name = mangle_tuple_type(tuple);
        return tuple;
    }

    return type;
}

auto amun::TypeChecker::infier_type_by_other_type(Shared<amun::Type> type, Shared<amun::Type> other)
    -> std::unordered_map<std::string, Shared<amun::Type>>
{
    if (type->type_kind == amun::TypeKind::GENERIC_PARAMETER) {
        std::unordered_map<std::string, Shared<amun::Type>> resolved_types;
        auto geneic_parameter = std::static_pointer_cast<amun::GenericParameterType>(type);
        resolved_types[geneic_parameter->name] = other;
        return resolved_types;
    }

    if (amun::is_pointer_type(type) && amun::is_pointer_type(other)) {
        auto type_ptr = std::static_pointer_cast<amun::PointerType>(type);
        auto other_ptr = std::static_pointer_cast<amun::PointerType>(other);
        return infier_type_by_other_type(type_ptr->base_type, other_ptr->base_type);
    }

    if (amun::is_array_type(type) && amun::is_array_type(other)) {
        auto type_arr = std::static_pointer_cast<amun::StaticArrayType>(type);
        auto other_arr = std::static_pointer_cast<amun::StaticArrayType>(other);
        return infier_type_by_other_type(type_arr->element_type, other_arr->element_type);
    }

    if (amun::is_generic_struct_type(type) && amun::is_struct_type(other)) {
        auto type_generic_struct = std::static_pointer_cast<amun::GenericStructType>(type);

        auto type_struct = type_generic_struct->struct_type;
        auto other_struct = std::static_pointer_cast<amun::StructType>(other);

        int index = 0;
        std::unordered_map<std::string, Shared<amun::Type>> resolved_types;
        for (const auto& t : other_struct->generic_parameters_types) {
            auto result = infier_type_by_other_type(type_generic_struct->parameters[index], t);
            for (const auto& element : result) {
                if (!resolved_types.contains(element.first)) {
                    resolved_types[element.first] = element.second;
                }
            }
            index++;
        }

        return resolved_types;
    }

    if (amun::is_generic_struct_type(type) && amun::is_generic_struct_type(other)) {
        auto type_generic_struct = std::static_pointer_cast<amun::GenericStructType>(type);
        auto other_generic_struct = std::static_pointer_cast<amun::GenericStructType>(other);

        auto type_struct = type_generic_struct->struct_type;
        auto other_struct = other_generic_struct->struct_type;

        if (type_struct->name == other_struct->name &&
            type_struct->fields_types.size() == other_struct->fields_types.size()) {
            std::unordered_map<std::string, Shared<amun::Type>> resolved_types;
            size_t index = 0;
            for (const auto& field_type : type_generic_struct->parameters) {
                auto other_type = other_generic_struct->parameters[index];
                auto result = infier_type_by_other_type(field_type, other_type);
                for (const auto& element : result) {
                    if (!resolved_types.contains(element.first)) {
                        resolved_types[element.first] = element.second;
                    }
                }
                index++;
            }
            return resolved_types;
        }

        return {};
    }

    if (amun::is_tuple_type(type) && amun::is_tuple_type(other)) {
        auto type_tuple = std::static_pointer_cast<amun::TupleType>(type);
        auto other_tuple = std::static_pointer_cast<amun::TupleType>(other);
        if (type_tuple->fields_types.size() == other_tuple->fields_types.size()) {
            std::unordered_map<std::string, Shared<amun::Type>> resolved_types;
            size_t index = 0;
            for (const auto& field_type : type_tuple->fields_types) {
                auto other_type = other_tuple->fields_types[index];
                auto result = infier_type_by_other_type(field_type, other_type);
                for (const auto& element : result) {
                    if (!resolved_types.contains(element.first)) {
                        resolved_types[element.first] = element.second;
                    }
                }
                index++;
            }
            return resolved_types;
        }

        return {};
    }

    if (amun::is_function_pointer_type(type) && amun::is_function_pointer_type(other)) {
        auto type_ptr = std::static_pointer_cast<amun::PointerType>(type);
        auto other_ptr = std::static_pointer_cast<amun::PointerType>(other);

        auto type_fptr = std::static_pointer_cast<amun::FunctionType>(type_ptr->base_type);
        auto other_fptr = std::static_pointer_cast<amun::FunctionType>(other_ptr->base_type);

        if (type_fptr->parameters.size() == other_fptr->parameters.size()) {
            auto return_type =
                infier_type_by_other_type(type_fptr->return_type, other_fptr->return_type);
            std::unordered_map<std::string, Shared<amun::Type>> resolved_types;
            for (const auto& type : return_type) {
                resolved_types[type.first] = type.second;
            }

            int index = 0;
            for (const auto& parameter : type_fptr->parameters) {
                auto parameter_result =
                    infier_type_by_other_type(parameter, other_fptr->parameters[index]);
                for (const auto& type : parameter_result) {
                    if (resolved_types.contains(type.first)) {
                        resolved_types[type.first] = type.second;
                    }
                }
                index++;
            }

            return resolved_types;
        }

        return {};
    }

    return {};
}

auto amun::TypeChecker::check_complete_switch_cases(Shared<amun::EnumType> enum_type,
                                                    std::unordered_set<std::string> cases_values,
                                                    bool has_else_branch, TokenSpan span) -> void
{
    auto enum_name = enum_type->name;
    auto enum_members_map = enum_type->values;
    auto enum_members_size = enum_members_map.size();

    auto switch_cases_size = cases_values.size();
    auto missing_cases_count = enum_members_size - switch_cases_size;

    // Complete or has else branch
    if (has_else_branch || missing_cases_count == 0) {
        return;
    }

    // Build helpful error message for missing cases
    std::stringstream string_stream;
    string_stream << "Incomplete switch, missing ";
    string_stream << missing_cases_count << " cases\n\n";
    string_stream << "You forget to cover the following cases:\n";

    for (const auto& member : enum_members_map) {
        if (!cases_values.contains(std::to_string(member.second))) {
            auto enum_element_name = enum_name.literal + "::" + member.first;
            string_stream << "- " << enum_element_name << "\n";
        }
    }

    context->diagnostics.report_error(span, string_stream.str());
    throw "Stop";
}

auto amun::TypeChecker::check_parameters_types(TokenSpan location,
                                               std::vector<Shared<Expression>>& arguments,
                                               std::vector<Shared<amun::Type>>& parameters,
                                               bool has_varargs, Shared<amun::Type> varargs_type,
                                               int implicit_parameters_count) -> void
{

    const auto arguments_size = arguments.size();
    const auto all_arguments_size = arguments_size + implicit_parameters_count;
    const auto parameters_size = parameters.size();

    // If hasent varargs, parameters and arguments must be the same size
    if (!has_varargs && all_arguments_size != parameters_size) {
        context->diagnostics.report_error(
            location, "Invalid number of arguments, expect " + std::to_string(parameters_size) +
                          " but got " + std::to_string(all_arguments_size));
        throw "Stop";
    }

    // If it has varargs, number of parameters must be bigger than arguments
    if (has_varargs && parameters_size > all_arguments_size) {
        context->diagnostics.report_error(location, "Invalid number of arguments, expect at last" +
                                                        std::to_string(parameters_size) +
                                                        " but got " +
                                                        std::to_string(all_arguments_size));
        throw "Stop";
    }

    // Resolve Arguments types
    std::vector<Shared<amun::Type>> arguments_types;
    arguments_types.reserve(arguments.size());

    // Resolve Arguments
    for (auto& argument : arguments) {
        // Check if any argument is an lambda expression with implicit capturing
        check_lambda_has_invalid_capturing(argument);

        auto argument_type = node_amun_type(argument->accept(this));
        if (argument_type->type_kind == amun::TypeKind::GENERIC_STRUCT) {
            arguments_types.push_back(resolve_generic_type(argument_type));
        }
        else {
            arguments_types.push_back(argument_type);
        }
    }

    // Resolve Generic parameters
    for (size_t i = 0; i < parameters_size; i++) {
        if (parameters[i]->type_kind == amun::TypeKind::GENERIC_STRUCT) {
            parameters[i] = resolve_generic_type(parameters[i]);
        }
    }

    auto count = parameters_size > arguments_size ? arguments_size : parameters_size;

    // Check non varargs parameters vs arguments
    for (size_t i = 0; i < count; i++) {
        size_t p = i + implicit_parameters_count;

        if (!amun::is_types_equals(parameters[p], arguments_types[i])) {
            // if Parameter is pointer type and null pointer passed as argument
            // Change null pointer base type to parameter type
            if (amun::is_pointer_type(parameters[p]) && amun::is_null_type(arguments_types[i])) {
                auto null_expr = std::dynamic_pointer_cast<NullExpression>(arguments[i]);
                null_expr->null_base_type = parameters[p];
                continue;
            }

            // If parameter is array type and argument is an empty array literal
            // Infier the element type of empty array literal to be the same element type of
            // parameter element type
            if (amun::is_array_type(parameters[p]) &&
                arguments[i]->get_ast_node_type() == AstNodeType::AST_ARRAY) {
                auto array_expr = std::dynamic_pointer_cast<ArrayExpression>(arguments[i]);
                auto array_type = std::static_pointer_cast<amun::StaticArrayType>(array_expr->type);
                auto param_type = std::static_pointer_cast<amun::StaticArrayType>(parameters[p]);
                if (array_type->size == 0) {
                    array_type->element_type = param_type->element_type;
                }
                continue;
            }

            context->diagnostics.report_error(location,
                                              "Argument type didn't match parameter type expect " +
                                                  amun::get_type_literal(parameters[p]) + " got " +
                                                  amun::get_type_literal(arguments_types[i]));
            throw "Stop";
        }
    }

    // If varargs type is Any, no need for type checking
    if (varargs_type == nullptr) {
        return;
    }

    // Check extra varargs types
    for (size_t i = parameters_size; i < arguments_size; i++) {
        if (!amun::is_types_equals(arguments_types[i], varargs_type)) {
            context->diagnostics.report_error(location,
                                              "Argument type didn't match varargs type expect " +
                                                  amun::get_type_literal(varargs_type) + " got " +
                                                  amun::get_type_literal(arguments_types[i]));
            throw "Stop";
        }
    }
}

auto amun::TypeChecker::check_lambda_has_invalid_capturing(Shared<Expression> expression) -> void
{
    if (expression->get_ast_node_type() == AstNodeType::AST_LAMBDA) {
        auto lambda = std::dynamic_pointer_cast<LambdaExpression>(expression);
        auto location = lambda->position.position;
        if (!lambda->implict_parameters_names.empty()) {
            std::stringstream error_message;
            error_message << "function argument lambda expression can't capture variables ";
            error_message << "from non global scopes\n\n";
            error_message << "Captured variables:\n";
            for (const auto& name : lambda->implict_parameters_names) {
                error_message << "-> " + name + "\n";
            }
            context->diagnostics.report_error(location, error_message.str());
            throw "Stop";
        }
    }
}

auto amun::TypeChecker::check_number_limits(const char* literal, amun::NumberKind kind) -> bool
{
    switch (kind) {
    case amun::NumberKind::INTEGER_1: {
        auto value = str_to_int(literal);
        return value == 0 or value == 1;
    }
    case amun::NumberKind::INTEGER_8: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int8_t>::min() and
               value <= std::numeric_limits<int8_t>::max();
    }
    case amun::NumberKind::U_INTEGER_8: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint8_t>::min() and
               value <= std::numeric_limits<uint8_t>::max();
    }
    case amun::NumberKind::INTEGER_16: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int16_t>::min() and
               value <= std::numeric_limits<int16_t>::max();
    }
    case amun::NumberKind::U_INTEGER_16: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint16_t>::min() and
               value <= std::numeric_limits<uint16_t>::max();
    }
    case amun::NumberKind::INTEGER_32: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int32_t>::min() and
               value <= std::numeric_limits<int32_t>::max();
    }
    case amun::NumberKind::U_INTEGER_32: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint32_t>::min() and
               value <= std::numeric_limits<uint32_t>::max();
    }
    case amun::NumberKind::INTEGER_64: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<int64_t>::min() and
               value <= std::numeric_limits<int64_t>::max();
    }
    case amun::NumberKind::U_INTEGER_64: {
        auto value = str_to_int(literal);
        ;
        return value >= std::numeric_limits<uint64_t>::min() and
               value <= std::numeric_limits<uint64_t>::max();
    }
    case amun::NumberKind::FLOAT_32: {
        auto value = str_to_float(literal);
        return value >= -std::numeric_limits<float>::max() and
               value <= std::numeric_limits<float>::max();
    }
    case amun::NumberKind::FLOAT_64: {
        auto value = str_to_float(literal);
        return value >= -std::numeric_limits<double>::max() and
               value <= std::numeric_limits<double>::max();
    }
    }
}

auto amun::TypeChecker::check_missing_return_statement(Shared<Statement> node) -> bool
{
    // This case for single node function declaration
    if (node->get_ast_node_type() == AstNodeType::AST_RETURN) {
        return true;
    }

    const auto& body = std::dynamic_pointer_cast<BlockStatement>(node);
    const auto& statements = body->statements;

    // This check called only for non void function so it must have return in
    // empty body
    if (statements.empty()) {
        return false;
    }

    // Check that last node is return statement
    if (statements.back()->get_ast_node_type() == AstNodeType::AST_RETURN) {
        return true;
    }

    // Iterate down to up to find a fully covered node that will always return
    for (auto it = std::rbegin(statements); it != std::rend(statements); ++it) {
        const auto& statement = *it;

        auto node_kind = statement->get_ast_node_type();
        if (node_kind == AstNodeType::AST_BLOCK && check_missing_return_statement(statement)) {
            return true;
        }

        else if (node_kind == AstNodeType::AST_IF_STATEMENT) {
            bool is_covered = false;
            auto if_statement = std::dynamic_pointer_cast<IfStatement>(statement);
            for (const auto& branch : if_statement->conditional_blocks) {
                is_covered = check_missing_return_statement(branch->body);
                if (!is_covered) {
                    break;
                }
            }

            if (is_covered && if_statement->has_else) {
                return true;
            }
        }

        else if (node_kind == AstNodeType::AST_SWITCH_STATEMENT) {
            auto switch_statement = std::dynamic_pointer_cast<SwitchStatement>(statement);
            auto default_body = switch_statement->default_case;
            if (default_body == nullptr) {
                return false;
            }

            if (!check_missing_return_statement(default_body->body)) {
                continue;
            }

            bool is_cases_covered = false;
            for (const auto& switch_case : switch_statement->cases) {
                is_cases_covered = check_missing_return_statement(switch_case->body);
                if (!is_cases_covered) {
                    break;
                }
            }

            if (is_cases_covered) {
                return true;
            }
        }
    }

    return false;
}

auto amun::TypeChecker::check_valid_assignment_right_side(Shared<Expression> node,
                                                          TokenSpan position) -> void
{
    const auto left_node_type = node->get_ast_node_type();

    // Literal expression is a valid right hand side
    if (left_node_type == AstNodeType::AST_LITERAL) {
        return;
    }

    // Index expression is a valid right hand side but
    // Make sure to report error if use want to modify string literal using
    // index expression
    if (left_node_type == AstNodeType::AST_INDEX) {
        auto index_expression = std::dynamic_pointer_cast<IndexExpression>(node);
        auto value_type = index_expression->value->get_type_node();
        if (amun::get_type_literal(value_type) == "*Int8") {
            auto index_position = index_expression->position.position;
            context->diagnostics.report_error(
                index_position, "String literal are readonly can't modify it using [i]");
            throw "Stop";
        }
        else {
            return;
        }
    }

    // Prefix unary expression is a valid right hand side but only if token is *
    if (left_node_type == AstNodeType::AST_PREFIX_UNARY) {
        auto prefix_unary = std::static_pointer_cast<PrefixUnaryExpression>(node);
        if (prefix_unary->operator_token.kind == TokenKind::TOKEN_STAR) {
            return;
        }

        context->diagnostics.report_error(position, "Invalid right hand side for");
        throw "Stop";
    }

    // Character literal can't be used as right hand side for assignment
    // expression
    if (left_node_type == AstNodeType::AST_CHARACTER) {
        context->diagnostics.report_error(position, "char literal is invalid right hand "
                                                    "side for assignment expression");
        throw "Stop";
    }

    // Boolean value can't be used as right hand side for assignment expression
    if (left_node_type == AstNodeType::AST_BOOL) {
        context->diagnostics.report_error(position, "boolean value is invalid right hand "
                                                    "side for assignment expression");
        throw "Stop";
    }

    // Number value can't be used as right hand side for assignment expression
    if (left_node_type == AstNodeType::AST_NUMBER) {
        context->diagnostics.report_error(position, "number value is invalid right hand "
                                                    "side for assignment expression");
        throw "Stop";
    }

    // String value can't be used as right hand side for assignment expression
    if (left_node_type == AstNodeType::AST_STRING) {
        context->diagnostics.report_error(position, "string literal is invalid right hand side for "
                                                    "assignment expression");
        throw "Stop";
    }

    // Enum element can't be used as right hand side for assignment expression
    if (left_node_type == AstNodeType::AST_ENUM_ELEMENT) {
        context->diagnostics.report_error(position, "Enum element is invalid right hand "
                                                    "side for assignment expression");
        throw "Stop";
    }

    // Null literal can't be used as right hand side for assignment expression
    if (left_node_type == AstNodeType::AST_NULL) {
        context->diagnostics.report_error(position, "Null literal is invalid right hand "
                                                    "side for assignment expression");
        throw "Stop";
    }
}

inline auto amun::TypeChecker::push_new_scope() -> void { types_table.push_new_scope(); }

inline auto amun::TypeChecker::pop_current_scope() -> void { types_table.pop_current_scope(); }