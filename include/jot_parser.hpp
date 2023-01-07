#pragma once

#include "jot_ast.hpp"
#include "jot_context.hpp"
#include "jot_files.hpp"
#include "jot_parser.hpp"
#include "jot_primitives.hpp"
#include "jot_token.hpp"
#include "jot_tokenizer.hpp"

#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

enum class AstNodeScope {
    GlobalScope,
    FunctionScope,
    ConditionalScope,
};

class JotParser {
  public:
    JotParser(std::shared_ptr<JotContext> context, JotTokenizer& tokenizer)
        : context(context), tokenizer(tokenizer)
    {
        advanced_token();
        advanced_token();
        auto file_path = context->source_manager.resolve_source_path(tokenizer.source_file_id);
        file_parent_path = find_parent_path(file_path) + "/";
    }

    std::shared_ptr<CompilationUnit> parse_compilation_unit();

  private:
    std::vector<std::shared_ptr<Statement>> parse_import_declaration();

    std::vector<std::shared_ptr<Statement>> parse_load_declaration();

    std::vector<std::shared_ptr<Statement>> parse_single_source_file(std::string& path);

    void merge_tree_nodes(std::vector<std::shared_ptr<Statement>>& distany,
                          std::vector<std::shared_ptr<Statement>>& source);

    std::shared_ptr<Statement> parse_declaration_statement();

    std::shared_ptr<Statement> parse_statement();

    std::shared_ptr<FieldDeclaration> parse_field_declaration(bool is_global);

    std::shared_ptr<FunctionPrototype> parse_function_prototype(FunctionCallKind kind,
                                                                bool             is_external);

    std::shared_ptr<FunctionDeclaration> parse_function_declaration(FunctionCallKind kind);

    std::shared_ptr<StructDeclaration> parse_structure_declaration(bool is_packed);

    std::shared_ptr<EnumDeclaration> parse_enum_declaration();

    std::shared_ptr<Parameter> parse_parameter();

    std::shared_ptr<ReturnStatement> parse_return_statement();

    std::shared_ptr<DeferStatement> parse_defer_statement();

    std::shared_ptr<BreakStatement> parse_break_statement();

    std::shared_ptr<ContinueStatement> parse_continue_statement();

    std::shared_ptr<IfStatement> parse_if_statement();

    std::shared_ptr<Statement> parse_for_statement();

    std::shared_ptr<WhileStatement> parse_while_statement();

    std::shared_ptr<SwitchStatement> parse_switch_statement();

    std::shared_ptr<BlockStatement> parse_block_statement();

    std::shared_ptr<ExpressionStatement> parse_expression_statement();

    std::shared_ptr<Expression> parse_expression();

    std::shared_ptr<Expression> parse_assignment_expression();

    std::shared_ptr<Expression> parse_logical_or_expression();

    std::shared_ptr<Expression> parse_logical_and_expression();

    std::shared_ptr<Expression> parse_equality_expression();

    std::shared_ptr<Expression> parse_comparison_expression();

    std::shared_ptr<Expression> parse_shift_expression();

    std::shared_ptr<Expression> parse_term_expression();

    std::shared_ptr<Expression> parse_factor_expression();

    std::shared_ptr<Expression> parse_enum_access_expression();

    std::shared_ptr<Expression> parse_infix_call_expression();

    std::shared_ptr<Expression> parse_prefix_expression();

    std::shared_ptr<Expression> parse_prefix_call_expression();

    std::shared_ptr<Expression> parse_postfix_increment_or_decrement();

    std::shared_ptr<Expression> parse_enumeration_attribute_expression();

    std::shared_ptr<Expression> parse_structure_access_expression();

    std::shared_ptr<Expression> parse_postfix_index_expression();

    std::shared_ptr<Expression> parse_function_call_expression();

    std::shared_ptr<Expression> parse_postfix_call_expression();

    std::shared_ptr<Expression> parse_primary_expression();

    std::shared_ptr<NumberExpression> parse_number_expression();

    std::shared_ptr<LiteralExpression> parse_literal_expression();

    std::shared_ptr<IfExpression> parse_if_expression();

    std::shared_ptr<SwitchExpression> parse_switch_expression();

    std::shared_ptr<GroupExpression> parse_group_expression();

    std::shared_ptr<ArrayExpression> parse_array_expression();

    std::shared_ptr<CastExpression> parse_cast_expression();

    std::shared_ptr<TypeSizeExpression> parse_type_size_expression();

    std::shared_ptr<ValueSizeExpression> parse_value_size_expression();

    std::shared_ptr<JotType> parse_type();

    std::shared_ptr<JotType> parse_type_with_prefix();

    std::shared_ptr<JotType> parse_type_with_postfix();

    std::shared_ptr<JotType> parse_primary_type();

    std::shared_ptr<JotType> parse_identifier_type();

    NumberKind get_number_kind(TokenKind token);

    void register_function_call(FunctionCallKind kind, std::string& name);

    void advanced_token();

    Token peek_and_advance_token();

    Token peek_previous();

    Token peek_current();

    Token peek_next();

    bool is_previous_kind(TokenKind);

    bool is_current_kind(TokenKind);

    bool is_next_kind(TokenKind);

    Token consume_kind(TokenKind, const char*);

    void assert_kind(TokenKind, const char*);

    bool is_source_available();

    std::string                 file_parent_path;
    std::shared_ptr<JotContext> context;
    JotTokenizer&               tokenizer;
    std::optional<Token>        previous_token;
    std::optional<Token>        current_token;
    std::optional<Token>        next_token;
    AstNodeScope                current_ast_scope = AstNodeScope::GlobalScope;
    std::string_view            current_struct_name = "";
    int                         current_struct_unknown_fields = 0;
    int                         loop_stack_size = 0;
};
