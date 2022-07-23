#include "../include/jot_typechecker.hpp"
#include "../include/jot_logger.hpp"
#include "jot_ast_visitor.hpp"

#include <memory>

void JotTypeChecker::check_compilation_unit(std::shared_ptr<CompilationUnit> compilation_unit) {
    auto statements = compilation_unit->get_tree_nodes();
    try {
        for (auto &statement : statements) {
            statement->accept(this);
        }
    } catch (const std::bad_any_cast &e) {
        std::cout << e.what() << '\n';
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

    if (!left_type->equals(right_type)) {
        jot::loge << "Type missmatch expect " << left_type->type_literal() << " but got "
                  << right_type->type_literal() << '\n';
        exit(1);
    }

    auto name = node->get_name().get_literal();
    bool is_first_defined = symbol_table.define(name, left_type);
    if (!is_first_defined) {
        jot::loge << "Field " << name << " is defined twice in the same scope\n";
        exit(1);
    }

    return 0;
}

std::any JotTypeChecker::visit(ExternalPrototype *node) {
    return node->get_prototype()->accept(this);
}

std::any JotTypeChecker::visit(FunctionPrototype *node) {
    auto name = node->get_name();
    std::vector<std::shared_ptr<JotType>> parameters;
    for (auto &parameter : node->get_parameters()) {
        parameters.push_back(parameter->get_type());
    }
    auto return_type = node->get_return_type();
    auto function_type = std::make_shared<JotFunctionType>(name, parameters, return_type);
    bool is_first_defined = symbol_table.define(name.get_literal(), function_type);
    if (!is_first_defined) {
        jot::loge << "function " << name.get_literal() << " is defined twice in the same scope\n";
        exit(1);
    }

    return function_type;
}

std::any JotTypeChecker::visit(FunctionDeclaration *node) {
    auto prototype = node->get_prototype()->accept(this);
    node->get_body()->accept(this);
    return prototype;
}

std::any JotTypeChecker::visit(EnumDeclaration *node) {
    auto name = node->get_name().get_literal();
    auto enum_type = std::make_shared<JotEnumType>(node->get_name(), node->get_values());
    bool is_first_defined = symbol_table.define(name, enum_type);
    if (!is_first_defined) {
        jot::loge << "enumeration " << name << " is defined twice in the same scope\n";
        exit(1);
    }
    return is_first_defined;
}

std::any JotTypeChecker::visit(WhileStatement *node) {
    auto left_type = node_jot_type(node->get_condition()->accept(this));
    if (!is_number_type(left_type)) {
        jot::loge << "While condition mush be a number but got " << left_type->type_literal()
                  << '\n';
        exit(1);
    }
    node->get_body()->accept(this);
    return 0;
}

std::any JotTypeChecker::visit(ReturnStatement *node) {
    // TODO: should improved and use accept once finish symbol table implementation
    return node->return_value()->get_type_node();
}

std::any JotTypeChecker::visit(ExpressionStatement *node) {
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(GroupExpression *node) {
    return node->get_expression()->accept(this);
}

std::any JotTypeChecker::visit(AssignExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    if (!left_type->equals(right_type)) {
        jot::loge << "Type missmatch expect " << left_type->type_literal() << " but got "
                  << right_type->type_literal() << '\n';
        exit(1);
    }

    return 0;
}

std::any JotTypeChecker::visit(BinaryExpression *node) {
    auto left_type = node_jot_type(node->get_left()->accept(this));
    auto right_type = node_jot_type(node->get_right()->accept(this));

    bool is_left_number = is_number_type(left_type);
    bool is_right_number = is_number_type(right_type);
    if (!is_left_number || !is_right_number) {
        if (!is_left_number) {
            jot::loge << "Expected binary left to be number but got " << left_type->type_literal()
                      << '\n';
        }
        if (!is_right_number) {
            jot::loge << "Expected binary right to be number but got " << right_type->type_literal()
                      << '\n';
        }
        exit(1);
    }

    return left_type;
}

std::any JotTypeChecker::visit(UnaryExpression *node) {
    auto left_type = node_jot_type(node->get_right()->accept(this));
    auto unary_operator = node->get_operator_token().get_kind();

    if (unary_operator == TokenKind::Star) {
        if (is_pointer_type(left_type)) {
            return std::make_shared<JotPointerType>(left_type->get_type_token(), left_type);
            ;
        } else {
            jot::loge << "expect unary operator `*` right node to be pointer but got "
                      << left_type->type_literal() << '\n';
            exit(1);
        }
    } else {
        if (is_number_type(left_type)) {
            return left_type;
        } else {
            jot::loge << "expect unary expression to be number but got "
                      << left_type->type_literal() << '\n';
            exit(1);
        }
    }
}

std::any JotTypeChecker::visit(CallExpression *node) {
    if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(node->get_callee())) {
        auto name = literal->get_name().get_literal();
        if (symbol_table.is_defined(name)) {
            auto value = symbol_table.lookup(name);
            if (auto type = std::any_cast<std::shared_ptr<JotFunctionType>>(&value)) {
                auto parameters = type->get()->get_parameters();
                auto arguments = node->get_arguments();
                if (parameters.size() != arguments.size()) {
                    jot::loge << "Invalid number of arguments " << name << " expect "
                              << parameters.size() << " got " << arguments.size() << '\n';
                    exit(1);
                }

                std::vector<std::shared_ptr<JotType>> arguments_types;
                for (auto &argument : arguments) {
                    arguments_types.push_back(
                        std::any_cast<std::shared_ptr<JotType>>(argument->accept(this)));
                }

                size_t arguments_size = arguments_types.size();
                for (size_t i = 0; i < arguments_size; i++) {
                    if (!is_same_type(parameters[i], arguments_types[i])) {
                        jot::loge << "Argument type didn't match parameter type expect "
                                  << parameters[i]->type_literal() << " got "
                                  << arguments_types[i]->type_literal() << '\n';
                        exit(1);
                    }
                }

                return type->get()->get_return_type();
            } else {
                jot::loge << "Call expression work only with function\n";
                exit(1);
            }
        } else {
            jot::loge << "Can't resolve function call with name " << name << '\n';
            exit(1);
        }
    } else {
        jot::loge << "Call expression must be a literal \n";
        exit(1);
    }
}

std::any JotTypeChecker::visit(LiteralExpression *node) {
    auto name = node->get_name().get_literal();
    if (symbol_table.is_defined(name)) {
        auto value = symbol_table.lookup(name);
        auto type = node_jot_type(value);
        return type;
    } else {
        jot::loge << "Can't resolve variable with name " << node->get_name().get_literal() << '\n';
        exit(1);
    }
}

std::any JotTypeChecker::visit(NumberExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(CharacterExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(BooleanExpression *node) { return node->get_type_node(); }

std::any JotTypeChecker::visit(NullExpression *node) { return node->get_type_node(); }

std::shared_ptr<JotType> JotTypeChecker::node_jot_type(std::any any_type) {
    if (any_type.type() == typeid(std::shared_ptr<JotNumber>)) {
        return std::any_cast<std::shared_ptr<JotNumber>>(any_type);
    }

    if (any_type.type() == typeid(std::shared_ptr<JotPointerType>)) {
        return std::any_cast<std::shared_ptr<JotPointerType>>(any_type);
    }

    if (any_type.type() == typeid(std::shared_ptr<JotFunctionType>)) {
        return std::any_cast<std::shared_ptr<JotFunctionType>>(any_type);
    }

    if (any_type.type() == typeid(std::shared_ptr<JotEnumType>)) {
        return std::any_cast<std::shared_ptr<JotEnumType>>(any_type);
    }

    if (any_type.type() == typeid(std::shared_ptr<JotNamedType>)) {
        return std::any_cast<std::shared_ptr<JotNamedType>>(any_type);
    }

    if (any_type.type() == typeid(std::shared_ptr<JotVoid>)) {
        return std::any_cast<std::shared_ptr<JotVoid>>(any_type);
    }

    if (any_type.type() == typeid(std::shared_ptr<JotNull>)) {
        return std::any_cast<std::shared_ptr<JotNull>>(any_type);
    }

    return std::any_cast<std::shared_ptr<JotType>>(any_type);
}

bool JotTypeChecker::is_number_type(const std::shared_ptr<JotType> &type) {
    return type->get_type_kind() == TypeKind::Number;
}

bool JotTypeChecker::is_pointer_type(const std::shared_ptr<JotType> &type) {
    return type->get_type_kind() == TypeKind::Pointer;
}

bool JotTypeChecker::is_same_type(const std::shared_ptr<JotType> &left,
                                  const std::shared_ptr<JotType> &right) {
    return left->get_type_kind() == right->get_type_kind();
}