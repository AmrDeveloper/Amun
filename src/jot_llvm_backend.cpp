#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"
#include "jot_ast.hpp"

#include <llvm-14/llvm/IR/Instructions.h>
#include <llvm-14/llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>

#include <any>
#include <cstdlib>
#include <memory>

std::unique_ptr<llvm::Module>
JotLLVMBackend::compile(std::string module_name,
                        std::shared_ptr<CompilationUnit> compilation_unit) {
    llvm_module = std::make_unique<llvm::Module>(module_name, llvm_context);
    try {
        for (auto &statement : compilation_unit->get_tree_nodes()) {
            statement->accept(this);
        }
    } catch (const std::bad_any_cast &e) {
        jot::loge << e.what() << '\n';
    }
    return std::move(llvm_module);
}

std::any JotLLVMBackend::visit(BlockStatement *node) {
    for (auto &statement : node->get_nodes()) {
        statement->accept(this);
    }
    return 0;
}

std::any JotLLVMBackend::visit(FieldDeclaration *node) {
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto var_name = node->get_name().get_literal();

    auto value = node->get_value()->accept(this);
    if (value.type() == typeid(llvm::Value *)) {
        auto init_value = std::any_cast<llvm::Value *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_int64_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_table[var_name] = alloc_inst;
    } else if (value.type() == typeid(llvm::CallInst *)) {
        auto init_value = std::any_cast<llvm::CallInst *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_int64_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_table[var_name] = alloc_inst;
    } else if (value.type() == typeid(llvm::AllocaInst *)) {
        auto init_value = std::any_cast<llvm::AllocaInst *>(value);
        Builder.CreateLoad(init_value->getAllocatedType(), init_value, var_name);
        alloca_inst_table[var_name] = init_value;
    }
    return 0;
}

std::any JotLLVMBackend::visit(FunctionPrototype *node) {
    auto parameters = node->get_parameters();
    std::vector<llvm::Type *> arguments(parameters.size(), llvm_int64_type);
    auto return_type = llvm_int64_type;
    auto function_type = llvm::FunctionType::get(return_type, arguments, false);
    auto function = llvm::Function::Create(function_type, llvm::Function::ExternalLinkage,
                                           node->get_name().get_literal(), llvm_module.get());

    unsigned index = 0;
    for (auto &argument : function->args()) {
        argument.setName(parameters[index++]->get_name().get_literal());
    }

    return function;
}

std::any JotLLVMBackend::visit(FunctionDeclaration *node) {
    auto prototype = node->get_prototype();
    auto name = prototype->get_name().get_literal();
    functions_table[name] = prototype;

    auto function = std::any_cast<llvm::Function *>(prototype->accept(this));
    auto entry_block = llvm::BasicBlock::Create(llvm_context, "entry", function);
    Builder.SetInsertPoint(entry_block);

    for (auto &arg : function->args()) {
        const std::string arg_name_str = std::string(arg.getName());
        auto *alloca_inst = create_entry_block_alloca(function, arg_name_str, llvm_int64_type);
        Builder.CreateStore(&arg, alloca_inst);
    }

    node->get_body()->accept(this);
    verifyFunction(*function);
    return function;
}

std::any JotLLVMBackend::visit(WhileStatement *node) { return 0; }

std::any JotLLVMBackend::visit(ReturnStatement *node) {
    if (node->return_value()->get_type_node()->get_type_kind() == TypeKind::Void) {
        return Builder.CreateRetVoid();
    }
    auto value = node->return_value()->accept(this);
    if (value.type() == typeid(llvm::Value *)) {
        auto return_value = std::any_cast<llvm::Value *>(value);
        return Builder.CreateRet(return_value);
    } else if (value.type() == typeid(llvm::CallInst *)) {
        auto init_value = std::any_cast<llvm::CallInst *>(value);
        return Builder.CreateRet(init_value);
    } else if (value.type() == typeid(llvm::AllocaInst *)) {
        auto init_value = std::any_cast<llvm::AllocaInst *>(value);
        auto value_litearl = Builder.CreateLoad(init_value->getAllocatedType(), init_value, "x");
        return Builder.CreateRet(value_litearl);
    }
    return 0;
}

std::any JotLLVMBackend::visit(ExpressionStatement *node) {
    node->get_expression()->accept(this);
    return 0;
}

std::any JotLLVMBackend::visit(GroupExpression *node) {
    return node->get_expression()->accept(this);
}

std::any JotLLVMBackend::visit(AssignExpression *node) {
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto literal = std::dynamic_pointer_cast<LiteralExpression>(node->get_left());
    auto name = literal->get_name().get_literal();
    auto value = node->get_right()->accept(this);
    if (value.type() == typeid(llvm::AllocaInst *)) {
        auto init_value = std::any_cast<llvm::AllocaInst *>(value);
        return Builder.CreateLoad(init_value->getAllocatedType(), init_value, name.c_str());
    } else {
        llvm::Value *right_value = llvm_node_value(value);
        auto alloc_inst = create_entry_block_alloca(current_function, name, llvm_int64_type);
        alloca_inst_table[name] = alloc_inst;
        return Builder.CreateStore(right_value, alloc_inst);
    }
}

std::any JotLLVMBackend::visit(BinaryExpression *node) {
    llvm::Value *left = llvm_node_value(node->get_left()->accept(this));
    llvm::Value *right = llvm_node_value(node->get_right()->accept(this));
    if (!left || !right) {
        return nullptr;
    }
    switch (node->get_operator_token().get_kind()) {
    case TokenKind::Plus: {
        return Builder.CreateAdd(left, right, "addtemp");
    }
    case TokenKind::Minus: {
        return Builder.CreateSub(left, right, "subtmp");
    }
    case TokenKind::Star: {
        return Builder.CreateMul(left, right, "multmp");
    }
    case TokenKind::Slash: {
        return Builder.CreateUDiv(left, right, "divtmp");
    }
    case TokenKind::Percent: {
        return Builder.CreateURem(left, right, "remtemp");
    }
    default: {
        jot::loge << "Invalid binary operator\n";
        exit(1);
    }
    }
}

std::any JotLLVMBackend::visit(UnaryExpression *node) { return 0; }

std::any JotLLVMBackend::visit(CallExpression *node) {
    auto callee = std::dynamic_pointer_cast<LiteralExpression>(node->get_callee());
    auto callee_literal = callee->get_name().get_literal();
    auto function = lookup_function(callee_literal);

    auto arguments = node->get_arguments();
    size_t arguments_size = function->arg_size();
    std::vector<llvm::Value *> arguments_values;
    for (size_t i = 0; i < arguments_size; i++) {
        auto value = std::any_cast<llvm::Value *>(arguments[i]->accept(this));
        arguments_values.push_back(value);
    }

    return Builder.CreateCall(function, arguments_values, "calltmp");
}

std::any JotLLVMBackend::visit(LiteralExpression *node) {
    auto name = node->get_name().get_literal();
    llvm::AllocaInst *variable = alloca_inst_table[name];
    return variable;
}

std::any JotLLVMBackend::visit(NumberExpression *node) {
    auto number_type = std::dynamic_pointer_cast<JotNumber>(node->get_type_node());
    return llvm_number_value(node->get_value().get_literal(), number_type->get_kind());
}

std::any JotLLVMBackend::visit(CharacterExpression *node) {
    return llvm_number_value(node->get_value().get_literal(), NumberKind::Integer8);
}

std::any JotLLVMBackend::visit(BooleanExpression *node) {
    return llvm_number_value(node->get_value().get_literal(), NumberKind::Integer1);
}

std::any JotLLVMBackend::visit(NullExpression *node) {
    return llvm::PointerType::get(llvm_int64_type, 0);
}

llvm::Value *JotLLVMBackend::llvm_node_value(std::any any_value) {
    if (any_value.type() == typeid(llvm::Value *)) {
        return std::any_cast<llvm::Value *>(any_value);
    } else if (any_value.type() == typeid(llvm::CallInst *)) {
        return std::any_cast<llvm::CallInst *>(any_value);
    } else if (any_value.type() == typeid(llvm::AllocaInst *)) {
        return std::any_cast<llvm::AllocaInst *>(any_value);
    }
    return nullptr;
}

llvm::Value *JotLLVMBackend::llvm_number_value(std::string value_litearl, NumberKind size) {
    switch (size) {
    case NumberKind::Integer1: {
        auto value = std::stoi(value_litearl.c_str());
        return llvm::ConstantInt::get(llvm_int1_type, value);
    }
    case NumberKind::Integer8: {
        auto value = std::stoi(value_litearl.c_str());
        return llvm::ConstantInt::get(llvm_int8_type, value);
    }
    case NumberKind::Integer16: {
        auto value = std::stoi(value_litearl.c_str());
        return llvm::ConstantInt::get(llvm_int16_type, value);
    }
    case NumberKind::Integer32: {
        auto value = std::stoi(value_litearl.c_str());
        return llvm::ConstantInt::get(llvm_int32_type, value);
    }
    case NumberKind::Integer64: {
        auto value = std::stoi(value_litearl.c_str());
        return llvm::ConstantInt::get(llvm_int64_type, value);
    }
    case NumberKind::Float32: {
        auto value = std::stod(value_litearl.c_str());
        return llvm::ConstantFP::get(llvm_float32_type, value);
    }
    case NumberKind::Float64: {
        auto value = std::stod(value_litearl.c_str());
        return llvm::ConstantFP::get(llvm_float64_type, value);
    }
    }
}

llvm::AllocaInst *JotLLVMBackend::create_entry_block_alloca(llvm::Function *function,
                                                            const std::string var_name,
                                                            llvm::Type *type) {
    llvm::IRBuilder<> builder_object(&function->getEntryBlock(), function->getEntryBlock().begin());
    return builder_object.CreateAlloca(type, 0, var_name.c_str());
}

llvm::Function *JotLLVMBackend::lookup_function(std::string name) {
    if (auto function = llvm_module->getFunction(name)) {
        return function;
    }

    auto function_prototype = functions_table.find(name);
    if (function_prototype != functions_table.end()) {
        return std::any_cast<llvm::Function *>(function_prototype->second->accept(this));
    }

    return nullptr;
}