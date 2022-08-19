#pragma once

#include "jot_ast.hpp"
#include "jot_ast_visitor.hpp"
#include "jot_symboltable.hpp"
#include "jot_type.hpp"

#include <llvm-14/llvm/IR/Constants.h>
#include <llvm-14/llvm/IR/Function.h>
#include <llvm-14/llvm/IR/Type.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>

#include <any>
#include <map>
#include <memory>

// LLVM Context, Builder and Current Module
static llvm::LLVMContext llvm_context;
static llvm::IRBuilder<> Builder(llvm_context);
static std::unique_ptr<llvm::Module> llvm_module;
static std::map<std::string, std::shared_ptr<FunctionPrototype>> functions_table;
static std::map<std::string, llvm::Function *> llvm_functions;

// LLVM Integer types
static auto llvm_int1_type = llvm::Type::getInt1Ty(llvm_context);
static auto llvm_int8_type = llvm::Type::getInt8Ty(llvm_context);
static auto llvm_int16_type = llvm::Type::getInt16Ty(llvm_context);
static auto llvm_int32_type = llvm::Type::getInt32Ty(llvm_context);
static auto llvm_int64_type = llvm::Type::getInt64Ty(llvm_context);

static auto llvm_int64_ptr_type = llvm::Type::getInt64PtrTy(llvm_context);

// LLVM Floating pointer types
static auto llvm_float32_type = llvm::Type::getFloatTy(llvm_context);
static auto llvm_float64_type = llvm::Type::getDoubleTy(llvm_context);

// LLVM Void type
static auto llvm_void_type = llvm::Type::getVoidTy(llvm_context);

class JotLLVMBackend : public TreeVisitor {
  public:
    JotLLVMBackend() {
        alloca_inst_global_scope = std::make_shared<JotSymbolTable>();
        alloca_inst_scope = alloca_inst_global_scope;
    }

    std::unique_ptr<llvm::Module> compile(std::string module_name,
                                          std::shared_ptr<CompilationUnit> compilation_unit);

    std::any visit(BlockStatement *node) override;

    std::any visit(FieldDeclaration *node) override;

    std::any visit(FunctionPrototype *node) override;

    std::any visit(FunctionDeclaration *node) override;

    std::any visit(EnumDeclaration *node) override;

    std::any visit(IfStatement *node) override;

    std::any visit(WhileStatement *node) override;

    std::any visit(ReturnStatement *node) override;

    std::any visit(ExpressionStatement *node) override;

    std::any visit(IfExpression *node) override;

    std::any visit(GroupExpression *node) override;

    std::any visit(AssignExpression *node) override;

    std::any visit(BinaryExpression *node) override;

    std::any visit(ComparisonExpression *node) override;

    std::any visit(LogicalExpression *node) override;

    std::any visit(UnaryExpression *node) override;

    std::any visit(CallExpression *node) override;

    std::any visit(IndexExpression *node) override;

    std::any visit(EnumAccessExpression *node) override;

    std::any visit(LiteralExpression *node) override;

    std::any visit(NumberExpression *node) override;

    std::any visit(ArrayExpression *node) override;

    std::any visit(StringExpression *node) override;

    std::any visit(CharacterExpression *node) override;

    std::any visit(BooleanExpression *node) override;

    std::any visit(NullExpression *node) override;

  private:
    llvm::Value *llvm_node_value(std::any any_value);

    llvm::Value *llvm_number_value(std::string value_litearl, NumberKind size);

    llvm::Value *llvm_boolean_value(bool value);

    llvm::Value *llvm_character_value(char character);

    llvm::Type *llvm_type_from_jot_type(std::shared_ptr<JotType> type);

    llvm::AllocaInst *create_entry_block_alloca(llvm::Function *function,
                                                const std::string var_name, llvm::Type *type);

    llvm::Function *lookup_function(std::string name);

    void push_alloca_inst_scope();

    void pop_alloca_inst_scope();

    std::shared_ptr<JotSymbolTable> alloca_inst_global_scope;
    std::shared_ptr<JotSymbolTable> alloca_inst_scope;
};