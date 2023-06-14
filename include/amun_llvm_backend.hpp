#pragma once

#include "amun_ast.hpp"
#include "amun_ast_visitor.hpp"
#include "amun_basic.hpp"
#include "amun_llvm_builder.hpp"
#include "amun_llvm_defer.hpp"
#include "amun_llvm_type.hpp"
#include "amun_scoped_list.hpp"
#include "amun_scoped_map.hpp"
#include "amun_type.hpp"

#include <any>
#include <memory>
#include <stack>
#include <unordered_map>
#include <vector>

namespace amun {

class LLVMBackend : public TreeVisitor {
  public:
    LLVMBackend() { alloca_inst_table.push_new_scope(); }

    auto compile(std::string module_name, Shared<CompilationUnit> compilation_unit)
        -> Unique<llvm::Module>;

    auto visit(BlockStatement* node) -> std::any override;

    auto visit(FieldDeclaration* node) -> std::any override;

    auto visit(DestructuringDeclaraion* node) -> std::any override;

    auto visit(ConstDeclaration* node) -> std::any override;

    auto visit(FunctionPrototype* node) -> std::any override;

    auto visit(OperatorFunctionDeclaraion* node) -> std::any override;

    auto visit(IntrinsicPrototype* node) -> std::any override;

    auto visit(FunctionDeclaration* node) -> std::any override;

    auto visit(StructDeclaration* node) -> std::any override;

    auto visit(EnumDeclaration* node) -> std::any override;

    auto visit(IfStatement* node) -> std::any override;

    auto visit(SwitchExpression* node) -> std::any override;

    auto visit(ForRangeStatement* node) -> std::any override;

    auto visit(ForEachStatement* node) -> std::any override;

    auto visit(ForeverStatement* node) -> std::any override;

    auto visit(WhileStatement* node) -> std::any override;

    auto visit(SwitchStatement* node) -> std::any override;

    auto visit(ReturnStatement* node) -> std::any override;

    auto visit(DeferStatement* node) -> std::any override;

    auto visit(BreakStatement* node) -> std::any override;

    auto visit(ContinueStatement* node) -> std::any override;

    auto visit(ExpressionStatement* node) -> std::any override;

    auto visit(IfExpression* node) -> std::any override;

    auto visit(GroupExpression* node) -> std::any override;

    auto visit(TupleExpression* node) -> std::any override;

    auto visit(AssignExpression* node) -> std::any override;

    auto visit(BinaryExpression* node) -> std::any override;

    auto visit(BitwiseExpression* node) -> std::any override;

    auto visit(ComparisonExpression* node) -> std::any override;

    auto visit(LogicalExpression* node) -> std::any override;

    auto visit(PrefixUnaryExpression* node) -> std::any override;

    auto visit(PostfixUnaryExpression* node) -> std::any override;

    auto visit(CallExpression* node) -> std::any override;

    auto visit(InitializeExpression* node) -> std::any override;

    auto visit(LambdaExpression* node) -> std::any override;

    auto visit(DotExpression* node) -> std::any override;

    auto visit(CastExpression* node) -> std::any override;

    auto visit(TypeSizeExpression* node) -> std::any override;

    auto visit(TypeAlignExpression* node) -> std::any override;

    auto visit(ValueSizeExpression* node) -> std::any override;

    auto visit(IndexExpression* node) -> std::any override;

    auto visit(EnumAccessExpression* node) -> std::any override;

    auto visit(LiteralExpression* node) -> std::any override;

    auto visit(NumberExpression* node) -> std::any override;

    auto visit(ArrayExpression* node) -> std::any override;

    auto visit(VectorExpression* node) -> std::any override;

    auto visit(StringExpression* node) -> std::any override;

    auto visit(CharacterExpression* node) -> std::any override;

    auto visit(BooleanExpression* node) -> std::any override;

    auto visit(NullExpression* node) -> std::any override;

    auto visit(UndefinedExpression* node) -> std::any override;

  private:
    auto llvm_node_value(std::any any_value) -> llvm::Value*;

    auto llvm_resolve_value(std::any any_value) -> llvm::Value*;

    auto llvm_resolve_variable(const std::string& name) -> llvm::Value*;

    auto llvm_number_value(const std::string& value_litearl, amun::NumberKind size) -> llvm::Value*;

    auto llvm_type_from_amun_type(Shared<amun::Type> type) -> llvm::Type*;

    auto create_global_field_declaration(std::string name, Shared<Expression> value,
                                         Shared<amun::Type> type) -> void;

    auto create_llvm_numbers_bianry(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_integers_bianry(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_floats_bianry(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_numbers_comparison(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_integers_comparison(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_unsigned_integers_comparison(TokenKind op, llvm::Value* left,
                                                  llvm::Value* right) -> llvm::Value*;

    auto create_llvm_floats_comparison(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_strings_comparison(TokenKind op, llvm::Value* left, llvm::Value* right)
        -> llvm::Value*;

    auto create_llvm_value_increment(Shared<Expression> right, bool is_prefix) -> llvm::Value*;

    auto create_llvm_value_decrement(Shared<Expression> right, bool is_prefix) -> llvm::Value*;

    auto create_llvm_string_length(llvm::Value* string) -> llvm::Value*;

    auto create_llvm_struct_type(std::string name, std::vector<Shared<amun::Type>> members,
                                 bool is_packed, bool is_extern) -> llvm::StructType*;

    auto create_overloading_function_call(std::string& name, std::vector<llvm::Value*> args)
        -> llvm::Value*;

    auto access_struct_member_pointer(llvm::Value* callee, llvm::Type* type, int field_index)
        -> llvm::Value*;

    auto access_struct_member_pointer(Expression* callee, int field_index) -> llvm::Value*;

    auto access_struct_member_pointer(DotExpression* expression) -> llvm::Value*;

    auto access_array_element(std::shared_ptr<Expression> node_value, llvm::Value* index)
        -> llvm::Value*;

    auto resolve_generic_function(FunctionDeclaration* node,
                                  std::vector<Shared<amun::Type>> generic_parameters)
        -> llvm::Function*;

    auto resolve_constant_expression(Shared<Expression> value) -> llvm::Constant*;

    auto resolve_constant_index_expression(Shared<IndexExpression> expression) -> llvm::Constant*;

    auto resolve_constant_if_expression(Shared<IfExpression> expression) -> llvm::Constant*;

    auto resolve_constant_switch_expression(Shared<SwitchExpression> expression) -> llvm::Constant*;

    auto resolve_constant_string_expression(const std::string& literal) -> llvm::Constant*;

    auto resolve_generic_struct(Shared<amun::GenericStructType> generic) -> llvm::StructType*;

    auto create_entry_block_alloca(llvm::Function* function, std::string var_name, llvm::Type* type)
        -> llvm::AllocaInst*;

    auto create_switch_case_branch(llvm::SwitchInst* switch_inst, llvm::Function* current_function,
                                   llvm::BasicBlock* basic_block, Shared<SwitchCase> switch_case)
        -> void;

    auto lookup_function(std::string& name) -> llvm::Function*;

    auto is_lambda_function_name(const std::string& name) -> bool;

    auto is_global_block() -> bool;

    auto execute_defer_call(Shared<amun::DeferCall>& defer_call) -> void;

    auto execute_scope_defer_calls() -> void;

    auto execute_all_defer_calls() -> void;

    auto push_alloca_inst_scope() -> void;

    auto pop_alloca_inst_scope() -> void;

    auto internal_compiler_error(const char* message) -> void;

    Unique<llvm::Module> llvm_module;

    std::unordered_map<std::string, Shared<FunctionPrototype>> functions_table;
    std::unordered_map<std::string, llvm::Function*> llvm_functions;
    std::unordered_map<std::string, llvm::Constant*> constants_string_pool;
    std::unordered_map<std::string, llvm::Type*> structures_types_map;

    std::unordered_map<std::string, llvm::StructType*> generated_tuples;

    // Generic function declaraions and parameters
    std::unordered_map<std::string, FunctionDeclaration*> functions_declaraions;
    std::unordered_map<std::string, Shared<amun::Type>> generic_types;

    std::stack<amun::ScopedList<Shared<amun::DeferCall>>> defer_calls_stack;

    amun::ScopedMap<std::string, std::any> alloca_inst_table;
    std::stack<llvm::BasicBlock*> break_blocks_stack;
    std::stack<llvm::BasicBlock*> continue_blocks_stack;

    bool has_return_statement = false;
    bool has_break_or_continue_statement = false;

    bool is_on_global_scope = true;

    // counter to generate unquie lambda names
    size_t lambda_unique_id = 0;
    // map lambda generated name to implicit parameters
    std::unordered_map<std::string, std::vector<std::string>> lambda_extra_parameters;
};

} // namespace amun