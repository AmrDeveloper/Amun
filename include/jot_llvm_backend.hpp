#pragma once

#include "jot_ast.hpp"
#include "jot_ast_visitor.hpp"
#include "jot_llvm_builder.hpp"
#include "jot_llvm_defer.hpp"
#include "jot_llvm_type.hpp"
#include "jot_scoped_list.hpp"
#include "jot_scoped_map.hpp"
#include "jot_type.hpp"

#include <any>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

class JotLLVMBackend : public TreeVisitor {
  public:
    JotLLVMBackend() { alloca_inst_table.push_new_scope(); }

    std::unique_ptr<llvm::Module> compile(std::string                      module_name,
                                          std::shared_ptr<CompilationUnit> compilation_unit);

    std::any visit(BlockStatement* node) override;

    std::any visit(FieldDeclaration* node) override;

    std::any visit(FunctionPrototype* node) override;

    std::any visit(FunctionDeclaration* node) override;

    std::any visit(StructDeclaration* node) override;

    std::any visit(EnumDeclaration* node) override;

    std::any visit(IfStatement* node) override;

    std::any visit(SwitchExpression* node) override;

    std::any visit(ForRangeStatement* node) override;

    std::any visit(ForEachStatement* node) override;

    std::any visit(ForeverStatement* node) override;

    std::any visit(WhileStatement* node) override;

    std::any visit(SwitchStatement* node) override;

    std::any visit(ReturnStatement* node) override;

    std::any visit(DeferStatement* node) override;

    std::any visit(BreakStatement* node) override;

    std::any visit(ContinueStatement* node) override;

    std::any visit(ExpressionStatement* node) override;

    std::any visit(IfExpression* node) override;

    std::any visit(GroupExpression* node) override;

    std::any visit(AssignExpression* node) override;

    std::any visit(BinaryExpression* node) override;

    std::any visit(ShiftExpression* node) override;

    std::any visit(ComparisonExpression* node) override;

    std::any visit(LogicalExpression* node) override;

    std::any visit(PrefixUnaryExpression* node) override;

    std::any visit(PostfixUnaryExpression* node) override;

    std::any visit(CallExpression* node) override;

    std::any visit(InitializeExpression* node) override;

    std::any visit(LambdaExpression* node) override;

    std::any visit(DotExpression* node) override;

    std::any visit(CastExpression* node) override;

    std::any visit(TypeSizeExpression* node) override;

    std::any visit(ValueSizeExpression* node) override;

    std::any visit(IndexExpression* node) override;

    std::any visit(EnumAccessExpression* node) override;

    std::any visit(LiteralExpression* node) override;

    std::any visit(NumberExpression* node) override;

    std::any visit(ArrayExpression* node) override;

    std::any visit(StringExpression* node) override;

    std::any visit(CharacterExpression* node) override;

    std::any visit(BooleanExpression* node) override;

    std::any visit(NullExpression* node) override;

  private:
    llvm::Value* llvm_node_value(std::any any_value);

    llvm::Value* llvm_resolve_value(std::any any_value);

    llvm::Value* llvm_resolve_variable(const std::string& name);

    llvm::Value* llvm_number_value(const std::string& value_litearl, NumberKind size);

    llvm::Type* llvm_type_from_jot_type(std::shared_ptr<JotType> type);

    llvm::Value* create_llvm_numbers_bianry(TokenKind op, llvm::Value* left, llvm::Value* right);

    llvm::Value* create_llvm_integers_bianry(TokenKind op, llvm::Value* left, llvm::Value* right);

    llvm::Value* create_llvm_floats_bianry(TokenKind op, llvm::Value* left, llvm::Value* right);

    llvm::Value* create_llvm_numbers_comparison(TokenKind op, llvm::Value* left,
                                                llvm::Value* right);

    llvm::Value* create_llvm_integers_comparison(TokenKind op, llvm::Value* left,
                                                 llvm::Value* right);

    llvm::Value* create_llvm_floats_comparison(TokenKind op, llvm::Value* left, llvm::Value* right);

    llvm::Value* create_llvm_value_increment(std::shared_ptr<Expression> right, bool is_prefix);

    llvm::Value* create_llvm_value_decrement(std::shared_ptr<Expression> right, bool is_prefix);

    llvm::Value* access_struct_member_pointer(DotExpression* expression);

    llvm::Value* access_array_element(std::shared_ptr<Expression> array, llvm::Value* index);

    llvm::Constant* resolve_constant_expression(std::shared_ptr<Expression> value);

    llvm::Constant* resolve_constant_index_expression(std::shared_ptr<IndexExpression> expression);

    llvm::Constant* resolve_constant_if_expression(std::shared_ptr<IfExpression> expression);

    llvm::Constant*
    resolve_constant_switch_expression(std::shared_ptr<SwitchExpression> expression);

    llvm::Constant* resolve_constant_string_expression(const std::string& literal);

    llvm::AllocaInst* create_entry_block_alloca(llvm::Function*   function,
                                                const std::string var_name, llvm::Type* type);

    void create_switch_case_branch(llvm::SwitchInst* switch_inst, llvm::Function* current_function,
                                   llvm::BasicBlock*           basic_block,
                                   std::shared_ptr<SwitchCase> switch_case);

    llvm::Function* lookup_function(std::string& name);

    bool is_lambda_function_name(const std::string& name);

    bool is_global_block();

    void execute_defer_call(std::shared_ptr<DeferCall>& defer_call);

    void execute_scope_defer_calls();

    void execute_all_defer_calls();

    void push_alloca_inst_scope();

    void pop_alloca_inst_scope();

    void internal_compiler_error(const char* message);

    std::unique_ptr<llvm::Module> llvm_module;

    std::unordered_map<std::string, std::shared_ptr<FunctionPrototype>> functions_table;
    std::unordered_map<std::string, llvm::Function*>                    llvm_functions;
    std::unordered_map<std::string, llvm::Constant*>                    constants_string_pool;
    std::unordered_map<std::string, llvm::Type*>                        structures_types_map;

    std::stack<JotScopedList<std::shared_ptr<DeferCall>>> defer_calls_stack;

    JotScopedMap<std::string, std::any> alloca_inst_table;
    std::stack<llvm::BasicBlock*>       break_blocks_stack;
    std::stack<llvm::BasicBlock*>       continue_blocks_stack;

    bool has_return_statement = false;
    bool has_break_or_continue_statement = false;

    // counter to generate unquie lambda names
    size_t lambda_unique_id = 0;
    // map lambda generated name to implicit parameters
    std::unordered_map<std::string, std::vector<std::string>> lambda_extra_parameters;
};