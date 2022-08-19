#pragma once

#include "jot_ast.hpp"
#include "jot_context.hpp"
#include "jot_parser.hpp"
#include "jot_token.hpp"
#include "jot_tokenizer.hpp"
#include "jot_type.hpp"

#include <memory>
#include <optional>
#include <unordered_map>

class JotParser {
  public:
    JotParser(std::shared_ptr<JotContext> context, std::unique_ptr<JotTokenizer> tokenizer)
        : context(context), tokenizer(std::move(tokenizer)) {
        advanced_token();
        advanced_token();
    }

    std::shared_ptr<CompilationUnit> parse_compilation_unit();

  private:
    std::vector<std::shared_ptr<Statement>> parse_import_declaration();

    std::vector<std::shared_ptr<Statement>> parse_single_source_file(std::string &path);

    std::shared_ptr<Statement> parse_declaration_statement();

    std::shared_ptr<Statement> parse_statement();

    std::shared_ptr<FieldDeclaration> parse_field_declaration();

    std::shared_ptr<FunctionPrototype> parse_function_prototype(FunctionCallKind kind,
                                                                bool is_external);

    std::shared_ptr<FunctionDeclaration> parse_function_declaration(FunctionCallKind kind);

    std::shared_ptr<EnumDeclaration> parse_enum_declaration();

    std::shared_ptr<Parameter> parse_parameter();

    std::shared_ptr<ReturnStatement> parse_return_statement();

    std::shared_ptr<IfStatement> parse_if_statement();

    std::shared_ptr<WhileStatement> parse_while_statement();

    std::shared_ptr<BlockStatement> parse_block_statement();

    std::shared_ptr<ExpressionStatement> parse_expression_statement();

    std::shared_ptr<Expression> parse_expression();

    std::shared_ptr<Expression> parse_assignment_expression();

    std::shared_ptr<Expression> parse_logical_or_expression();

    std::shared_ptr<Expression> parse_logical_and_expression();

    std::shared_ptr<Expression> parse_equality_expression();

    std::shared_ptr<Expression> parse_comparison_expression();

    std::shared_ptr<Expression> parse_term_expression();

    std::shared_ptr<Expression> parse_factor_expression();

    std::shared_ptr<Expression> parse_enum_access_expression();

    std::shared_ptr<Expression> parse_infix_call_expression();

    std::shared_ptr<Expression> parse_prefix_expression();

    std::shared_ptr<Expression> parse_prefix_call_expression();

    std::shared_ptr<Expression> parse_postfix_expression();

    std::shared_ptr<Expression> parse_postfix_call_expression();

    std::shared_ptr<Expression> parse_primary_expression();

    std::shared_ptr<ArrayExpression> parse_array_expression();

    std::shared_ptr<NumberExpression> parse_number_expression();

    std::shared_ptr<LiteralExpression> parse_literal_expression();

    std::shared_ptr<IfExpression> parse_if_expression();

    std::shared_ptr<GroupExpression> parse_group_expression();

    std::shared_ptr<JotType> parse_type();

    std::shared_ptr<JotType> parse_type_with_prefix();

    std::shared_ptr<JotType> parse_type_with_postfix();

    std::shared_ptr<JotType> parse_primary_type();

    std::shared_ptr<JotType> parse_identifier_type();

    NumberKind get_number_kind(TokenKind token);

    void register_function_call(FunctionCallKind kind, std::string &name);

    void advanced_token();

    Token peek_and_advance_token();

    Token peek_previous();

    Token peek_current();

    Token peek_next();

    bool is_previous_kind(TokenKind);

    bool is_current_kind(TokenKind);

    bool is_next_kind(TokenKind);

    Token consume_kind(TokenKind, const char *);

    void assert_kind(TokenKind, const char *);

    bool is_source_available();

    std::shared_ptr<JotContext> context;
    std::unique_ptr<JotTokenizer> tokenizer;
    std::unordered_map<std::string, std::shared_ptr<JotEnumType>> enumerations;
    std::optional<Token> previous_token;
    std::optional<Token> current_token;
    std::optional<Token> next_token;
};
