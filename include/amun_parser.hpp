#pragma once

#include "amun_ast.hpp"
#include "amun_context.hpp"
#include "amun_files.hpp"
#include "amun_parser.hpp"
#include "amun_primitives.hpp"
#include "amun_token.hpp"
#include "amun_tokenizer.hpp"

#include <memory>
#include <optional>
#include <stack>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace amun {

enum class AstNodeScope {
    GLOBAL_SCOPE,
    FUNCTION_SCOPE,
    CONDITION_SCOPE,
};

static std::unordered_map<std::string, Shared<amun::Type>> primitive_types = {
    {"int1", amun::i1_type},     {"bool", amun::i1_type},
    {"char", amun::i8_type},     {"uchar", amun::u8_type},

    {"int8", amun::i8_type},     {"int16", amun::i6_type},
    {"int32", amun::i32_type},   {"int64", amun::i64_type},

    {"uint8", amun::u8_type},    {"uint16", amun::u16_type},
    {"uint32", amun::u32_type},  {"uint64", amun::u64_type},

    {"float32", amun::f32_type}, {"float64", amun::f64_type},

    {"void", amun::void_type},
};

class Parser {
  public:
    Parser(Shared<amun::Context> context, amun::Tokenizer& tokenizer)
        : context(context), tokenizer(tokenizer)
    {
        auto file_path = context->source_manager.resolve_source_path(tokenizer.source_file_id);
        file_parent_path = amun::find_parent_path(file_path) + "/";
    }

    auto parse_compilation_unit() -> Shared<CompilationUnit>;

  private:
    auto parse_import_declaration() -> std::vector<Shared<Statement>>;

    auto parse_load_declaration() -> std::vector<Shared<Statement>>;

    auto parse_compiletime_constants_declaraion() -> Shared<ConstDeclaration>;

    auto parse_type_alias_declaration() -> void;

    auto parse_single_source_file(std::string& path) -> std::vector<Shared<Statement>>;

    auto parse_declaration_statement() -> Shared<Statement>;

    auto parse_declaraions_directive() -> Shared<Statement>;

    auto parse_statement() -> Shared<Statement>;

    auto parse_field_declaration(bool is_global) -> Shared<FieldDeclaration>;

    auto parse_intrinsic_prototype() -> Shared<IntrinsicPrototype>;

    auto parse_function_prototype(amun::FunctionKind kind, bool is_external)
        -> Shared<FunctionPrototype>;

    auto parse_function_declaration(amun::FunctionKind kind) -> Shared<FunctionDeclaration>;

    auto parse_operator_function_declaraion(amun::FunctionKind kind)
        -> Shared<OperatorFunctionDeclaraion>;

    auto parse_operator_function_operator(amun::FunctionKind kind) -> Token;

    auto parse_structure_declaration(bool is_packed, bool is_extern) -> Shared<StructDeclaration>;

    auto parse_enum_declaration() -> Shared<EnumDeclaration>;

    auto parse_parameter() -> Shared<Parameter>;

    auto parse_return_statement() -> Shared<ReturnStatement>;

    auto parse_defer_statement() -> Shared<DeferStatement>;

    auto parse_break_statement() -> Shared<BreakStatement>;

    auto parse_continue_statement() -> Shared<ContinueStatement>;

    auto parse_if_statement() -> Shared<IfStatement>;

    auto parse_for_statement() -> Shared<Statement>;

    auto parse_while_statement() -> Shared<WhileStatement>;

    auto parse_switch_statement() -> Shared<SwitchStatement>;

    auto parse_block_statement() -> Shared<BlockStatement>;

    auto parse_expression_statement() -> Shared<ExpressionStatement>;

    auto parse_statements_directive() -> Shared<Statement>;

    auto parse_expression() -> Shared<Expression>;

    auto parse_assignment_expression() -> Shared<Expression>;

    auto parse_logical_or_expression() -> Shared<Expression>;

    auto parse_logical_and_expression() -> Shared<Expression>;

    auto parse_bitwise_and_expression() -> Shared<Expression>;

    auto parse_bitwise_xor_expression() -> Shared<Expression>;

    auto parse_bitwise_or_expression() -> Shared<Expression>;

    auto parse_equality_expression() -> Shared<Expression>;

    auto parse_comparison_expression() -> Shared<Expression>;

    auto parse_bitwise_shift_expression() -> Shared<Expression>;

    auto parse_term_expression() -> Shared<Expression>;

    auto parse_factor_expression() -> Shared<Expression>;

    auto parse_enum_access_expression() -> Shared<Expression>;

    auto parse_infix_call_expression() -> Shared<Expression>;

    auto parse_prefix_expression() -> Shared<Expression>;

    auto parse_prefix_call_expression() -> Shared<Expression>;

    auto parse_postfix_increment_or_decrement() -> Shared<Expression>;

    auto parse_enumeration_attribute_expression() -> Shared<Expression>;

    auto parse_call_or_access_expression() -> Shared<Expression>;

    auto parse_postfix_call_expression() -> Shared<Expression>;

    auto parse_initializer_expression() -> Shared<Expression>;

    auto parse_function_call_with_lambda_argument() -> Shared<Expression>;

    auto parse_primary_expression() -> Shared<Expression>;

    auto parse_lambda_expression() -> Shared<LambdaExpression>;

    auto parse_number_expression() -> Shared<NumberExpression>;

    auto parse_literal_expression() -> Shared<LiteralExpression>;

    auto parse_if_expression() -> Shared<IfExpression>;

    auto parse_switch_expression() -> Shared<SwitchExpression>;

    auto parse_group_or_tuple_expression() -> Shared<Expression>;

    auto parse_array_expression() -> Shared<ArrayExpression>;

    auto parse_cast_expression() -> Shared<CastExpression>;

    auto parse_type_size_expression() -> Shared<TypeSizeExpression>;

    auto parse_type_allign_expression() -> Shared<TypeAlignExpression>;

    auto parse_value_size_expression() -> Shared<ValueSizeExpression>;

    auto parse_expressions_directive() -> Shared<Expression>;

    auto parse_type() -> Shared<amun::Type>;

    auto parse_type_with_prefix() -> Shared<amun::Type>;

    auto parse_pointer_to_type() -> Shared<amun::Type>;

    auto parse_function_type() -> Shared<amun::Type>;

    auto parse_tuple_type() -> Shared<amun::Type>;

    auto parse_fixed_size_array_type() -> Shared<amun::Type>;

    auto parse_type_with_postfix() -> Shared<amun::Type>;

    auto parse_generic_struct_type() -> Shared<amun::Type>;

    auto parse_primary_type() -> Shared<amun::Type>;

    auto parse_identifier_type() -> Shared<amun::Type>;

    auto check_function_kind_paramters_count(FunctionKind kind, int count, TokenSpan span) -> void;

    auto check_generic_parameter_name(Token name) -> void;

    auto check_compiletime_constants_expression(Shared<Expression> expression, TokenSpan position)
        -> void;

    auto unexpected_token_error() -> void;

    auto check_unnecessary_semicolon_warning() -> void;

    auto get_number_kind(TokenKind token) -> amun::NumberKind;

    auto is_function_declaration_kind(std::string& fun_name, amun::FunctionKind kind) -> bool;

    auto is_valid_intrinsic_name(std::string& name) -> bool;

    auto advanced_token() -> void;

    auto peek_and_advance_token() -> Token;

    auto peek_previous() -> Token;

    auto peek_current() -> Token;

    auto peek_next() -> Token;

    auto is_previous_kind(TokenKind) -> bool;

    auto is_current_kind(TokenKind) -> bool;

    auto is_next_kind(TokenKind) -> bool;

    auto consume_kind(TokenKind, const char*) -> Token;

    auto assert_kind(TokenKind, const char*) -> void;

    auto is_source_available() -> bool;

    std::string file_parent_path;
    Shared<amun::Context> context;

    amun::Tokenizer& tokenizer;
    std::optional<Token> previous_token;
    std::optional<Token> current_token;
    std::optional<Token> next_token;

    std::unordered_set<std::string> generic_parameters_names;

    AstNodeScope current_ast_scope = AstNodeScope::GLOBAL_SCOPE;
    std::stack<int> loop_levels_stack;

    std::string_view current_struct_name;
    int current_struct_unknown_fields = 0;
};

} // namespace amun