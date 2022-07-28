#include "../include/jot_parser.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_logger.hpp"

#include <memory>

std::shared_ptr<CompilationUnit> JotParser::parse_compilation_unit() {
    std::vector<std::shared_ptr<Statement>> tree_nodes;
    while (is_source_available()) {
        auto current_token = peek_current().get_kind();
        switch (current_token) {
        case TokenKind::ImportKeyword: {
            auto module_tree_node = parse_import_declaration();
            tree_nodes.insert(std::end(tree_nodes), std::begin(module_tree_node),
                              std::end(module_tree_node));
            break;
        }
        default: {
            tree_nodes.push_back(parse_declaration_statement());
        }
        }
    }
    return std::make_shared<CompilationUnit>(tree_nodes);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_import_declaration() {
    advanced_token();
    auto library_name =
        consume_kind(TokenKind::String, "Expect string as library name after import statement");
    std::string library_path = "../lib/" + library_name.get_literal() + ".jot";
    const char *file_name = library_path.c_str();
    std::string source_content = read_file_content(file_name);
    auto tokenizer = std::make_unique<JotTokenizer>(file_name, source_content);
    JotParser parser(std::move(tokenizer));
    auto compilation_unit = parser.parse_compilation_unit();
    return compilation_unit->get_tree_nodes();
}

std::shared_ptr<Statement> JotParser::parse_declaration_statement() {
    jot::logi << "Parse Declaration statement Current " << peek_current().get_literal() << "\n";
    switch (peek_current().get_kind()) {
    case TokenKind::ExternKeyword: {
        return parse_external_prototype();
    }
    case TokenKind::FunKeyword: {
        return parse_function_declaration();
    }
    case TokenKind::VarKeyword: {
        jot::logi << "Parse global Var statement\n";
        return parse_field_declaration();
    }
    case TokenKind::EnumKeyword: {
        return parse_enum_declaration();
    }
    default: {
        jot::loge << "Can't find declaration statement that start with "
                  << peek_current().get_kind_literal() << '\n';
        exit(EXIT_FAILURE);
    }
    }
}

std::shared_ptr<Statement> JotParser::parse_statement() {
    switch (peek_current().get_kind()) {
    case TokenKind::VarKeyword: {
        jot::logi << "Parse Var statement\n";
        return parse_field_declaration();
    }
    case TokenKind::WhileKeyword: {
        jot::logi << "Parse While statement\n";
        return parse_while_statement();
    }
    case TokenKind::ReturnKeyword: {
        jot::logi << "Parse Return statement\n";
        return parse_return_statement();
    }
    case TokenKind::OpenBrace: {
        jot::logi << "Parse block statement\n";
        return parse_block_statement();
    }
    default: {
        return parse_expression_statement();
    }
    }
}

std::shared_ptr<FieldDeclaration> JotParser::parse_field_declaration() {
    jot::logi << "Parse Field statement\n";
    assert_kind(TokenKind::VarKeyword, "Expect var keyword.");
    auto name = consume_kind(TokenKind::Symbol, "Expect identifier as variable name.");
    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        auto type = parse_type();
        assert_kind(TokenKind::Equal, "Expect = after variable name.");
        auto value = parse_expression();
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
        return std::make_shared<FieldDeclaration>(name, type, value);
    }
    assert_kind(TokenKind::Equal, "Expect = after variable name.");
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<FieldDeclaration>(name, value->get_type_node(), value);
}

std::shared_ptr<ExternalPrototype> JotParser::parse_external_prototype() {
    auto external_token = peek_current();
    advanced_token();
    auto prototype = parse_function_prototype();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after external declaration");
    return std::make_shared<ExternalPrototype>(external_token, prototype);
}

std::shared_ptr<FunctionPrototype> JotParser::parse_function_prototype() {
    jot::logi << "Parse Function Prototype\n";
    assert_kind(TokenKind::FunKeyword, "Expect function keyword.");
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as function name.");

    std::vector<std::shared_ptr<Parameter>> parameters;
    if (is_current_kind(TokenKind::OpenParen)) {
        advanced_token();
        while (is_source_available() && !is_current_kind(TokenKind::CloseParen)) {
            parameters.push_back(parse_parameter());
            if (is_current_kind(TokenKind::Comma)) {
                advanced_token();
            } else {
                break;
            }
        }
        assert_kind(TokenKind::CloseParen, "Expect ) after function parameters.");
    }
    auto return_type = parse_type();
    return std::make_shared<FunctionPrototype>(name, parameters, return_type);
}

std::shared_ptr<FunctionDeclaration> JotParser::parse_function_declaration() {
    jot::logi << "Parse Function Declaration\n";
    auto prototype = parse_function_prototype();

    if (is_current_kind(TokenKind::Equal)) {
        advanced_token();
        auto value = parse_expression_statement();
        return std::make_shared<FunctionDeclaration>(prototype, value);
    }

    if (is_current_kind(TokenKind::OpenBrace)) {
        auto block = parse_block_statement();
        return std::make_shared<FunctionDeclaration>(prototype, block);
    }

    jot::loge << "Invalid function declaration body\n";
    exit(EXIT_FAILURE);
}

std::shared_ptr<EnumDeclaration> JotParser::parse_enum_declaration() {
    jot::logi << "Parse enum declaration statement\n";
    auto enum_token = consume_kind(TokenKind::EnumKeyword, "Expect enum keyword");
    auto enum_name = consume_kind(TokenKind::Symbol, "Expect Symbol as enum name");
    assert_kind(TokenKind::OpenBrace, "Expect { after enum name");
    std::vector<Token> enum_values;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        auto enum_value = consume_kind(TokenKind::Symbol, "Expect Symbol as enum value");
        enum_values.push_back(enum_value);

        if (is_current_kind(TokenKind::Comma))
            advanced_token();
        else
            break;
    }
    assert_kind(TokenKind::CloseBrace, "Expect } in the end of enum declaration");
    return std::make_shared<EnumDeclaration>(enum_name, enum_values);
}

std::shared_ptr<Parameter> JotParser::parse_parameter() {
    jot::logi << "Parse Function Parameter\n";
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as parameter name.");
    auto type = parse_type();
    return std::make_shared<Parameter>(name, type);
}

std::shared_ptr<BlockStatement> JotParser::parse_block_statement() {
    jot::logi << "Parse Block Statement\n";
    consume_kind(TokenKind::OpenBrace, "Expect { on the start of block.");
    std::vector<std::shared_ptr<Statement>> statements;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        jot::logi << "Parse Block Statement\n";
        statements.push_back(parse_statement());
    }
    consume_kind(TokenKind::CloseBrace, "Expect } on the end of block.");
    return std::make_shared<BlockStatement>(statements);
}

std::shared_ptr<ReturnStatement> JotParser::parse_return_statement() {
    jot::logi << "Parse Return Statement\n";
    auto keyword = consume_kind(TokenKind::ReturnKeyword, "Expect return keyword.");
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after return statement");
    return std::make_shared<ReturnStatement>(keyword, value);
}

std::shared_ptr<WhileStatement> JotParser::parse_while_statement() {
    jot::logi << "Parse While Statement\n";
    auto keyword = consume_kind(TokenKind::WhileKeyword, "Expect while keyword.");
    auto condition = parse_expression();
    auto body = parse_statement();
    return std::make_shared<WhileStatement>(keyword, condition, body);
}

std::shared_ptr<ExpressionStatement> JotParser::parse_expression_statement() {
    jot::logi << "Parse Expression Statement\n";
    auto expression = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<ExpressionStatement>(expression);
}

std::shared_ptr<Expression> JotParser::parse_expression() { return parse_assignment_expression(); }

std::shared_ptr<Expression> JotParser::parse_assignment_expression() {
    auto expression = parse_equality_expression();
    if (is_current_kind(TokenKind::Equal)) {
        auto equal_token = peek_current();
        advanced_token();
        auto value = parse_assignment_expression();
        return std::make_shared<AssignExpression>(expression, equal_token, value);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_equality_expression() {
    auto expression = parse_comparison_expression();
    while (is_current_kind(TokenKind::EqualEqual) || is_current_kind(TokenKind::BangEqual)) {
        jot::logi << "Parse Equality Expression\n";
        Token operator_token = peek_current();
        advanced_token();
        auto right = parse_comparison_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_comparison_expression() {
    auto expression = parse_term_expression();
    while (is_current_kind(TokenKind::Greater) || is_current_kind(TokenKind::GreaterEqual) ||
           is_current_kind(TokenKind::Smaller) || is_current_kind(TokenKind::SmallerEqual)) {
        jot::logi << "Parse Comparison Expression\n";
        Token operator_token = peek_current();
        advanced_token();
        auto right = parse_term_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_term_expression() {
    auto expression = parse_factor_expression();
    while (is_current_kind(TokenKind::Plus) || is_current_kind(TokenKind::Minus)) {
        jot::logi << "Parse Term Expression\n";
        Token operator_token = peek_current();
        advanced_token();
        auto right = parse_factor_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_factor_expression() {
    auto expression = parse_prefix_expression();
    while (is_current_kind(TokenKind::Star) || is_current_kind(TokenKind::Slash) ||
           is_current_kind(TokenKind::Percent)) {
        jot::logi << "Parse Factor Expression\n";
        Token operator_token = peek_current();
        advanced_token();
        auto right = parse_prefix_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_prefix_expression() {
    if (peek_current().is_unary_operator()) {
        jot::logi << "Parse Unary Expression\n";
        Token token = peek_current();
        advanced_token();
        auto right = parse_prefix_expression();
        return std::make_shared<UnaryExpression>(token, right);
    }
    return parse_postfix_expression();
}

std::shared_ptr<Expression> JotParser::parse_postfix_expression() {
    auto expression = parse_primary_expression();

    while (true) {
        if (is_current_kind(TokenKind::OpenParen)) {
            jot::logi << "Parse Call Expression\n";
            advanced_token();
            Token position = peek_previous();
            std::vector<std::shared_ptr<Expression>> arguments;
            while (is_source_available() && !is_current_kind(TokenKind::CloseParen)) {
                arguments.push_back(parse_expression());
                if (is_current_kind(TokenKind::Comma))
                    advanced_token();
            }
            assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");
            expression = std::make_shared<CallExpression>(position, expression, arguments);
        } else {
            break;
        }
    }

    return expression;
}

std::shared_ptr<Expression> JotParser::parse_primary_expression() {
    auto current_token_kind = peek_current().get_kind();
    switch (current_token_kind) {
    case TokenKind::Float:
    case TokenKind::Integer: {
        jot::logi << "Parse Primary Number Expression\n";
        advanced_token();
        return std::make_shared<NumberExpression>(peek_previous());
    }
    case TokenKind::Character: {
        jot::logi << "Parse Primary Character Expression\n";
        advanced_token();
        return std::make_shared<CharacterExpression>(peek_previous());
    }
    case TokenKind::String: {
        jot::logi << "Parse Primary String Expression\n";
        advanced_token();
        return std::make_shared<StringExpression>(peek_previous());
    }
    case TokenKind::TrueKeyword:
    case TokenKind::FalseKeyword: {
        jot::logi << "ParsePrimary  Boolean Expression\n";
        advanced_token();
        return std::make_shared<BooleanExpression>(peek_previous());
    }
    case TokenKind::NullKeyword: {
        jot::logi << "Parse Primary Null Expression\n";
        advanced_token();
        return std::make_shared<NullExpression>(peek_previous());
    }
    case TokenKind::Symbol: {
        jot::logi << "Parse Primary Literal Expression\n";
        Token symbol_token = peek_current();
        advanced_token();
        return std::make_shared<LiteralExpression>(symbol_token,
                                                   std::make_shared<JotNamedType>(symbol_token));
    }
    case TokenKind::OpenParen: {
        jot::logi << "Parse Primary Grouping Expression\n";
        Token position = peek_current();
        advanced_token();
        auto expression = parse_expression();
        assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");
        return std::make_shared<GroupExpression>(position, expression);
    }
    default: {
        jot::loge << "Unexpected or unsupported expression :" << peek_current().get_kind_literal()
                  << '\n';
        exit(EXIT_FAILURE);
    }
    }
}

std::shared_ptr<JotType> JotParser::parse_type() { return parse_type_with_prefix(); }

std::shared_ptr<JotType> JotParser::parse_type_with_prefix() {
    if (is_current_kind(TokenKind::Star)) {
        advanced_token();
        auto operand = parse_type_with_prefix();
        return std::make_shared<JotPointerType>(operand->get_type_token(), operand);
    }

    if (is_current_kind(TokenKind::Address)) {
        advanced_token();
        auto operand = parse_type_with_prefix();
        return std::make_shared<JotNumber>(operand->get_type_token(), NumberKind::Integer64);
    }

    return parse_type_with_postfix();
}

std::shared_ptr<JotType> JotParser::parse_type_with_postfix() {
    auto primary_type = parse_primary_type();
    // TODO: will used later to support [] and maybe ?
    return primary_type;
}

std::shared_ptr<JotType> JotParser::parse_primary_type() {
    if (is_current_kind(TokenKind::Symbol)) {
        return parse_identifier_type();
    }
    jot::loge << "Expected symbol as type but got " << peek_current().get_kind_literal() << '\n';
    exit(EXIT_FAILURE);
}

std::shared_ptr<JotType> JotParser::parse_identifier_type() {
    Token symbol_token = consume_kind(TokenKind::Symbol, "Expect identifier as type");
    std::string type_literal = symbol_token.get_literal();

    if (type_literal == "int16") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer16);
    }

    if (type_literal == "int32") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer32);
    }

    if (type_literal == "int64") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer64);
    }

    if (type_literal == "float32") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Float32);
    }

    if (type_literal == "float64") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Float64);
    }

    if (type_literal == "char" || type_literal == "int8") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer8);
    }

    if (type_literal == "bool") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer1);
    }

    if (type_literal == "void") {
        return std::make_shared<JotVoid>(symbol_token);
    }

    jot::loge << "Unexpected identifier type " << type_literal << '\n';
    exit(EXIT_FAILURE);

    /**
     *  TODO: should return a named type node but for now just allow primitive types
     *  return std::make_shared<NamedTypeNode>(symbol_token);
     */
}

void JotParser::advanced_token() {
    previous_token = current_token;
    current_token = next_token;
    next_token = tokenizer->scan_next_token();
}

Token JotParser::peek_previous() { return previous_token.value(); }

Token JotParser::peek_current() { return current_token.value(); }

Token JotParser::peek_next() { return next_token.value(); }

bool JotParser::is_previous_kind(TokenKind kind) { return peek_previous().get_kind() == kind; }

bool JotParser::is_current_kind(TokenKind kind) { return peek_current().get_kind() == kind; }

bool JotParser::is_next_kind(TokenKind kind) { return peek_next().get_kind() == kind; }

Token JotParser::consume_kind(TokenKind kind, const char *message) {
    if (is_current_kind(kind)) {
        advanced_token();
        return previous_token.value();
    }
    jot::loge << message << '\n';
    exit(EXIT_FAILURE);
}

void JotParser::assert_kind(TokenKind kind, const char *message) {
    if (is_current_kind(kind)) {
        advanced_token();
        return;
    }
    jot::loge << message << '\n';
    exit(EXIT_FAILURE);
}

bool JotParser::is_source_available() { return !peek_current().is_end_of_file(); }
