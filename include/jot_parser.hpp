#pragma once

#include "jot_parser.hpp"
#include "jot_token.hpp"
#include "jot_tokenizer.hpp"
#include "jot_ast.hpp"

#include <memory>
#include <optional>

class JotParser {
public:
    explicit JotParser(std::unique_ptr<JotTokenizer> tokenizer): tokenizer(std::move(tokenizer)) {
        advanced_token();
        advanced_token();
    }

    std::shared_ptr<CompilationUnit> parse_compilation_unit();

private:

    std::shared_ptr<Statement> parse_declaration_statement();

    std::shared_ptr<Statement> parse_statement();

    std::shared_ptr<FieldDeclaration> parse_field_declaration();

    std::shared_ptr<FunctionPrototype> parse_function_prototype();

    std::shared_ptr<FunctionDeclaration> parse_function_declaration();

    std::shared_ptr<Parameter> parse_parameter();

    std::shared_ptr<ReturnStatement> parse_return_statement();

    std::shared_ptr<WhileStatement> parse_while_statement();

    std::shared_ptr<BlockStatement> parse_block_statement();

    std::shared_ptr<ExpressionStatement> parse_expression_statement();

    std::shared_ptr<Expression> parse_expression();

    std::shared_ptr<Expression> parse_equality_expression();

    std::shared_ptr<Expression> parse_comparison_expression();

    std::shared_ptr<Expression> parse_term_expression();

    std::shared_ptr<Expression> parse_factor_expression();

    std::shared_ptr<Expression> parse_prefix_expression();

    std::shared_ptr<Expression> parse_postfix_expression();

    std::shared_ptr<Expression> parse_primary_expression();

    std::shared_ptr<JotType> parse_type();

    std::shared_ptr<JotType> parse_type_with_prefix();

    std::shared_ptr<JotType> parse_type_with_postfix();

    std::shared_ptr<JotType> parse_primary_type();

    std::shared_ptr<JotType> parse_identifier_type();

    void advanced_token();

    Token peek_previous();

    Token peek_current();

    Token peek_next();

    bool is_previous_kind(TokenKind);
    
    bool is_current_kind(TokenKind);
    
    bool is_next_kind(TokenKind);

    Token consume_kind(TokenKind, const char*);

    void assert_kind(TokenKind, const char*);

    bool is_source_available();

    std::unique_ptr<JotTokenizer> tokenizer;
    std::optional<Token> previous_token;
    std::optional<Token> current_token;
    std::optional<Token> next_token;
};

