#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_ast_visitor.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Casting.h>

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
        jot::loge << "LLVM Backend : " << e.what() << '\n';
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

    auto llvm_type = llvm_type_from_jot_type(node->get_type());

    auto value = node->get_value()->accept(this);

    if (value.type() == typeid(llvm::Value *)) {
        auto init_value = std::any_cast<llvm::Value *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    } else if (value.type() == typeid(llvm::CallInst *)) {
        auto init_value = std::any_cast<llvm::CallInst *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    } else if (value.type() == typeid(llvm::AllocaInst *)) {
        auto init_value = std::any_cast<llvm::AllocaInst *>(value);
        Builder.CreateLoad(init_value->getAllocatedType(), init_value, var_name);
        alloca_inst_scope->define(var_name, init_value);
    } else if (value.type() == typeid(llvm::Constant *)) {
        auto constants_string = std::any_cast<llvm::Constant *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(constants_string, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    } else if (value.type() == typeid(llvm::LoadInst *)) {
        auto constants_string = std::any_cast<llvm::LoadInst *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(constants_string, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    } else if (value.type() == typeid(llvm::PHINode *)) {
        auto node = std::any_cast<llvm::PHINode *>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(node, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    } else if (value.type() == typeid(llvm::Function *)) {
        auto node = std::any_cast<llvm::Function *>(value);
        llvm_functions[var_name] = node;
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, node->getType());
        Builder.CreateStore(node, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    } else {
        jot::loge << "Un supported rvalue for field declaration " << value.type().name() << '\n';
    }
    return 0;
}

std::any JotLLVMBackend::visit(FunctionPrototype *node) {
    auto parameters = node->get_parameters();
    int parameters_size = parameters.size();
    std::vector<llvm::Type *> arguments(parameters.size());
    for (int i = 0; i < parameters_size; i++) {
        arguments[i] = llvm_type_from_jot_type(parameters[i]->get_type());
    }

    auto return_type = llvm_type_from_jot_type(node->get_return_type());
    auto function_type = llvm::FunctionType::get(return_type, arguments, false);
    auto function_name = node->get_name().get_literal();
    auto linkage = node->is_external() || function_name == "main" ? llvm::Function::ExternalLinkage
                                                                  : llvm::Function::InternalLinkage;
    auto function =
        llvm::Function::Create(function_type, linkage, function_name, llvm_module.get());

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

    push_alloca_inst_scope();
    for (auto &arg : function->args()) {
        const std::string arg_name_str = std::string(arg.getName());
        auto *alloca_inst = create_entry_block_alloca(function, arg_name_str, arg.getType());
        alloca_inst_scope->define(arg_name_str, alloca_inst);
        Builder.CreateStore(&arg, alloca_inst);
    }

    node->get_body()->accept(this);

    pop_alloca_inst_scope();

    alloca_inst_scope->define(name, function);

    verifyFunction(*function);

    return function;
}

std::any JotLLVMBackend::visit(EnumDeclaration *node) {
    // TODO: Implement it later
    return 0;
}

std::any JotLLVMBackend::visit(IfStatement *node) {
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto start_block = llvm::BasicBlock::Create(llvm_context, "if.start");
    auto end_block = llvm::BasicBlock::Create(llvm_context, "if.end");

    Builder.CreateBr(start_block);
    current_function->getBasicBlockList().push_back(start_block);
    Builder.SetInsertPoint(start_block);

    auto conditional_blocks = node->get_conditional_blocks();
    auto conditional_blocks_size = conditional_blocks.size();
    for (unsigned long i = 0; i < conditional_blocks_size; i++) {
        auto true_block = llvm::BasicBlock::Create(llvm_context, "if.true");
        current_function->getBasicBlockList().push_back(true_block);

        auto false_branch = end_block;
        if (i + 1 < conditional_blocks_size) {
            false_branch = llvm::BasicBlock::Create(llvm_context, "if.false");
            current_function->getBasicBlockList().push_back(false_branch);
        }

        auto condition = llvm_node_value(conditional_blocks[i]->get_condition()->accept(this));
        Builder.CreateCondBr(condition, true_block, false_branch);
        Builder.SetInsertPoint(true_block);

        push_alloca_inst_scope();
        conditional_blocks[i]->get_body()->accept(this);
        pop_alloca_inst_scope();

        bool has_return_statement = false;
        auto body = conditional_blocks[i]->get_body();
        if (std::dynamic_pointer_cast<ReturnStatement>(body)) {
            has_return_statement = true;
        } else if (auto block = std::dynamic_pointer_cast<BlockStatement>(body)) {
            if (std::dynamic_pointer_cast<ReturnStatement>(block->get_nodes().back())) {
                has_return_statement = true;
            }
        }

        if (not has_return_statement)
            Builder.CreateBr(end_block);
        Builder.SetInsertPoint(false_branch);
    }

    current_function->getBasicBlockList().push_back(end_block);
    Builder.SetInsertPoint(end_block);

    return 0;
}

std::any JotLLVMBackend::visit(WhileStatement *node) {
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto condition_branch = llvm::BasicBlock::Create(llvm_context, "while.condition");
    auto loop_branch = llvm::BasicBlock::Create(llvm_context, "while.loop");
    auto end_branch = llvm::BasicBlock::Create(llvm_context, "while.end");

    Builder.CreateBr(condition_branch);
    current_function->getBasicBlockList().push_back(condition_branch);
    Builder.SetInsertPoint(condition_branch);

    auto condition = llvm_node_value(node->get_condition()->accept(this));
    Builder.CreateCondBr(condition, loop_branch, end_branch);

    current_function->getBasicBlockList().push_back(loop_branch);
    Builder.SetInsertPoint(loop_branch);
    push_alloca_inst_scope();
    node->get_body()->accept(this);
    pop_alloca_inst_scope();
    Builder.CreateBr(condition_branch);

    current_function->getBasicBlockList().push_back(end_branch);
    Builder.SetInsertPoint(end_branch);

    return 0;
}

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
        auto value_litearl = Builder.CreateLoad(init_value->getAllocatedType(), init_value);
        return Builder.CreateRet(value_litearl);
    } else if (value.type() == typeid(llvm::Function *)) {
        auto node = std::any_cast<llvm::Function *>(value);
        return Builder.CreateRet(node);
    } else if (value.type() == typeid(llvm::Constant *)) {
        auto init_value = std::any_cast<llvm::Constant *>(value);
        return Builder.CreateRet(init_value);
    } else if (value.type() == typeid(llvm::LoadInst *)) {
        auto load_inst = std::any_cast<llvm::LoadInst *>(value);
        return Builder.CreateRet(load_inst);
    } else {
        jot::loge << "Un expected return type " << value.type().name() << '\n';
    }
    return 0;
}

std::any JotLLVMBackend::visit(ExpressionStatement *node) {
    node->get_expression()->accept(this);
    return 0;
}

std::any JotLLVMBackend::visit(IfExpression *node) {
    auto function = Builder.GetInsertBlock()->getParent();

    auto condition = llvm_node_value(node->get_condition()->accept(this));

    llvm::BasicBlock *thenBB = llvm::BasicBlock::Create(llvm_context, "then", function);
    llvm::BasicBlock *elseBB = llvm::BasicBlock::Create(llvm_context, "else");
    llvm::BasicBlock *mergeBB = llvm::BasicBlock::Create(llvm_context, "ifcont");
    Builder.CreateCondBr(condition, thenBB, elseBB);

    Builder.SetInsertPoint(thenBB);
    auto then_value = llvm_node_value(node->get_if_value()->accept(this));

    Builder.CreateBr(mergeBB);
    thenBB = Builder.GetInsertBlock();

    function->getBasicBlockList().push_back(elseBB);
    Builder.SetInsertPoint(elseBB);

    auto else_value = llvm_node_value(node->get_else_value()->accept(this));

    Builder.CreateBr(mergeBB);
    elseBB = Builder.GetInsertBlock();

    function->getBasicBlockList().push_back(mergeBB);
    Builder.SetInsertPoint(mergeBB);

    llvm::PHINode *pn = Builder.CreatePHI(then_value->getType(), 2, "iftmp");
    pn->addIncoming(then_value, thenBB);
    pn->addIncoming(else_value, elseBB);

    return pn;
}

std::any JotLLVMBackend::visit(GroupExpression *node) {
    return node->get_expression()->accept(this);
}

std::any JotLLVMBackend::visit(AssignExpression *node) {
    // Assign value to variable
    // variable = value
    if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(node->get_left())) {
        auto name = literal->get_name().get_literal();
        auto value = node->get_right()->accept(this);

        if (value.type() == typeid(llvm::AllocaInst *)) {
            auto init_value = std::any_cast<llvm::AllocaInst *>(value);
            alloca_inst_scope->update(name, init_value);
            return Builder.CreateLoad(init_value->getAllocatedType(), init_value, name.c_str());
        }

        llvm::Value *right_value = llvm_node_value(value);
        auto alloc_inst = std::any_cast<llvm::AllocaInst *>(alloca_inst_scope->lookup(name));
        alloca_inst_scope->update(name, alloc_inst);
        return Builder.CreateStore(right_value, alloc_inst);
    }

    // Assign value to n dimentions array position
    // array []? = value
    if (auto index_expression = std::dynamic_pointer_cast<IndexExpression>(node->get_left())) {
        auto node_value = index_expression->get_value();
        auto index = llvm_node_value(index_expression->get_index()->accept(this));
        auto right_value = llvm_node_value(node->get_right()->accept(this));

        // Update element value in Single dimention Array
        if (auto array_literal = std::dynamic_pointer_cast<LiteralExpression>(node_value)) {
            auto alloca = llvm::dyn_cast<llvm::AllocaInst>(llvm_node_value(
                alloca_inst_scope->lookup(array_literal->get_name().get_literal())));
            auto ptr =
                Builder.CreateGEP(alloca->getAllocatedType(), alloca, {zero_int32_value, index});
            return Builder.CreateStore(right_value, ptr);
        }

        // Update element value in Multi dimentions Array
        if (auto sub_index_expression = std::dynamic_pointer_cast<IndexExpression>(node_value)) {
            auto array = llvm_node_value(node_value->accept(this));
            auto element_type = llvm_type_from_jot_type(node->get_type_node());
            auto load_inst = dyn_cast<llvm::LoadInst>(array);
            auto ptr = Builder.CreateGEP(array->getType(), load_inst->getPointerOperand(),
                                         {zero_int32_value, index});
            return Builder.CreateStore(right_value, ptr);
        }
    }

    jot::loge << "Internal compiler error: Invalid assignments with multi d array \n";
    exit(EXIT_FAILURE);
}

std::any JotLLVMBackend::visit(BinaryExpression *node) {
    auto left = llvm_node_value(node->get_left()->accept(this));
    auto right = llvm_node_value(node->get_right()->accept(this));
    if (not left || not right) {
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

std::any JotLLVMBackend::visit(ComparisonExpression *node) {
    auto left = llvm_node_value(node->get_left()->accept(this));
    auto right = llvm_node_value(node->get_right()->accept(this));
    if (not left || not right) {
        return nullptr;
    }

    switch (node->get_operator_token().get_kind()) {
    case TokenKind::EqualEqual: {
        return Builder.CreateICmpEQ(left, right);
    }
    case TokenKind::BangEqual: {
        return Builder.CreateICmpNE(left, right);
    }
    case TokenKind::Greater: {
        return Builder.CreateICmpUGT(left, right);
    }
    case TokenKind::GreaterEqual: {
        return Builder.CreateICmpUGE(left, right);
    }
    case TokenKind::Smaller: {
        return Builder.CreateICmpSLT(left, right);
    }
    case TokenKind::SmallerEqual: {
        return Builder.CreateICmpSLE(left, right);
    }
    default: {
        jot::loge << "Invalid Comparison operator\n";
        exit(1);
    }
    }
}

std::any JotLLVMBackend::visit(LogicalExpression *node) {
    auto left = llvm_node_value(node->get_left()->accept(this));
    auto right = llvm_node_value(node->get_right()->accept(this));
    if (not left || not right) {
        return nullptr;
    }

    switch (node->get_operator_token().get_kind()) {
    case TokenKind::LogicalAnd: {
        return Builder.CreateLogicalAnd(left, right);
    }
    case TokenKind::LogicalOr: {
        return Builder.CreateLogicalOr(left, right);
    }
    default: {
        jot::loge << "Invalid Logical operator\n";
        exit(1);
    }
    }
}

std::any JotLLVMBackend::visit(UnaryExpression *node) {
    auto right = llvm_node_value(node->get_right()->accept(this));
    switch (node->get_operator_token().get_kind()) {
    case TokenKind::Minus: {
        return Builder.CreateNeg(right);
    }
    case TokenKind::Bang: {
        jot::loge << "Unary ! operator not implemented yet\n";
        exit(1);
    }
    case TokenKind::Star: {
        jot::loge << "Unary * operator not implemented yet\n";
        exit(1);
    }
    case TokenKind::And: {
        return right;
    }
    case TokenKind::Not: {
        return Builder.CreateNot(right);
    }
    default: {
        jot::loge << "Invalid Unary operator\n";
        exit(1);
    }
    }
}

std::any JotLLVMBackend::visit(CallExpression *node) {
    auto callee = std::dynamic_pointer_cast<LiteralExpression>(node->get_callee());
    auto callee_literal = callee->get_name().get_literal();
    auto function = lookup_function(callee_literal);
    if (not function) {
        auto value = llvm_node_value(alloca_inst_scope->lookup(callee_literal));
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
            auto loaded = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
            auto function_type = llvm_type_from_jot_type(node->get_type_node());
            if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                std::vector<llvm::Value *> arguments_values;
                auto arguments = node->get_arguments();
                size_t arguments_size = arguments.size();
                for (size_t i = 0; i < arguments_size; i++) {
                    auto value = llvm_node_value(arguments[i]->accept(this));
                    if (function_pointer->getParamType(i) == value->getType()) {
                        arguments_values.push_back(value);
                    } else {
                        auto expected_type = function_pointer->getParamType(i);
                        auto loaded_value = Builder.CreateLoad(expected_type, value);
                        arguments_values.push_back(loaded_value);
                    }
                }
                return Builder.CreateCall(function_pointer, loaded, arguments_values);
            }
            return loaded;
        }
    }

    auto arguments = node->get_arguments();
    size_t arguments_size = function->arg_size();
    std::vector<llvm::Value *> arguments_values;
    for (size_t i = 0; i < arguments_size; i++) {
        auto value = llvm_node_value(arguments[i]->accept(this));
        if (function->getArg(i)->getType() == value->getType()) {
            arguments_values.push_back(value);
        } else {
            // Load the constants first and then pass it to the arguments values
            auto expected_type = function->getArg(i)->getType();
            auto loaded_value = Builder.CreateLoad(expected_type, value);
            arguments_values.push_back(loaded_value);
        }
    }
    return Builder.CreateCall(function, arguments_values, "calltmp");
}

std::any JotLLVMBackend::visit(CastExpression *node) {
    auto value = llvm_node_value(node->get_value()->accept(this));
    auto value_type = llvm_type_from_jot_type(node->get_value()->get_type_node());
    auto target_type = llvm_type_from_jot_type(node->get_type_node());

    // Integer to Integer with different size
    if (target_type->isIntegerTy() and value_type->isIntegerTy()) {
        return Builder.CreateIntCast(value, target_type, true);
    }

    // Floating to Floating with differnt size
    if (target_type->isFloatingPointTy() and value_type->isFloatingPointTy()) {
        return Builder.CreateFPCast(value, target_type);
    }

    // Floating point to Integer
    if (target_type->isIntegerTy() and value_type->isFloatingPointTy()) {
        return Builder.CreateFPToSI(value, target_type);
    }

    // Integer to Floating point
    if (target_type->isFloatingPointTy() and value_type->isIntegerTy()) {
        return Builder.CreateSIToFP(value, target_type);
    }

    // Pointer to Integer
    if (target_type->isIntegerTy() and value_type->isPointerTy()) {
        return Builder.CreatePtrToInt(value, target_type);
    }

    // Integer to Pointer
    if (target_type->isPointerTy() and value_type->isIntegerTy()) {
        return Builder.CreateIntToPtr(value, target_type);
    }

    // Bit casting
    return Builder.CreateBitCast(value, target_type);
}

std::any JotLLVMBackend::visit(IndexExpression *node) {
    auto index = llvm_node_value(node->get_index()->accept(this));
    auto node_value = node->get_value();
    auto values = node_value->get_type_node();

    if (values->get_type_kind() == TypeKind::Pointer) {
        auto element_type = llvm_type_from_jot_type(node->get_type_node());
        auto value = llvm_node_value(node_value->accept(this));
        auto ptr = Builder.CreateGEP(element_type, value, index);
        return Builder.CreateLoad(element_type, ptr);
    }

    // One dimension Array Index Expression
    if (auto array_literal = std::dynamic_pointer_cast<LiteralExpression>(node_value)) {
        auto alloca = llvm::dyn_cast<llvm::AllocaInst>(
            llvm_node_value(alloca_inst_scope->lookup(array_literal->get_name().get_literal())));
        auto ptr = Builder.CreateGEP(alloca->getAllocatedType(), alloca, {zero_int32_value, index});
        return Builder.CreateLoad(llvm_type_from_jot_type(node->get_type_node()), ptr);
    }

    // Multidimensional Array Index Expression
    if (auto index_expression = std::dynamic_pointer_cast<IndexExpression>(node_value)) {
        auto array = llvm_node_value(node_value->accept(this));
        auto element_type = llvm_type_from_jot_type(node->get_type_node());
        auto load_inst = dyn_cast<llvm::LoadInst>(array);
        auto ptr = Builder.CreateGEP(array->getType(), load_inst->getPointerOperand(),
                                     {zero_int32_value, index});
        return Builder.CreateLoad(llvm_type_from_jot_type(node->get_type_node()), ptr);
    }

    jot::loge << "Internal compiler error: Invalid Index expression \n";
    exit(EXIT_FAILURE);
}

std::any JotLLVMBackend::visit(EnumAccessExpression *node) {
    auto element_type = llvm_type_from_jot_type(node->get_type_node());
    auto element_index = llvm::ConstantInt::get(element_type, node->get_enum_element_index());
    return llvm::dyn_cast<llvm::Value>(element_index);
}

std::any JotLLVMBackend::visit(LiteralExpression *node) {
    auto name = node->get_name().get_literal();
    auto alloca_inst = alloca_inst_scope->lookup(name);

    if (alloca_inst.type() == typeid(llvm::AllocaInst *)) {
        llvm::AllocaInst *variable = std::any_cast<llvm::AllocaInst *>(alloca_inst);
        return Builder.CreateLoad(variable->getAllocatedType(), variable);
    }

    return alloca_inst;
}

std::any JotLLVMBackend::visit(NumberExpression *node) {
    auto number_type = std::dynamic_pointer_cast<JotNumberType>(node->get_type_node());
    return llvm_number_value(node->get_value().get_literal(), number_type->get_kind());
}

std::any JotLLVMBackend::visit(ArrayExpression *node) {
    auto arrayType = llvm_type_from_jot_type(node->get_type_node());
    std::vector<llvm::Value *> values;
    for (auto &value : node->get_values()) {
        values.push_back(llvm_node_value(value->accept(this)));
    }
    auto alloca = Builder.CreateAlloca(arrayType);
    for (size_t i = 0; i < values.size(); i++) {
        auto index = llvm::ConstantInt::get(llvm_context, llvm::APInt(32, i, true));
        auto ptr = Builder.CreateGEP(alloca->getAllocatedType(), alloca, {zero_int32_value, index});

        auto value = values[i];
        if (value->getType() == ptr->getType()) {
            value = Builder.CreateLoad(value->getType()->getPointerElementType(), value);
        }
        Builder.CreateStore(value, ptr);
    }
    return alloca;
}

std::any JotLLVMBackend::visit(StringExpression *node) {
    std::string literal = node->get_value().get_literal();
    return Builder.CreateGlobalStringPtr(literal);
}

std::any JotLLVMBackend::visit(CharacterExpression *node) {
    char char_asci_value = node->get_value().get_literal()[0];
    return llvm_character_value(char_asci_value);
}

std::any JotLLVMBackend::visit(BooleanExpression *node) {
    return llvm_boolean_value(node->get_value().get_kind() == TokenKind::TrueKeyword);
}

std::any JotLLVMBackend::visit([[maybe_unused]] NullExpression *node) {
    return llvm::PointerType::get(llvm_int64_type, 0);
}

llvm::Value *JotLLVMBackend::llvm_node_value(std::any any_value) {
    if (any_value.type() == typeid(llvm::Value *)) {
        return std::any_cast<llvm::Value *>(any_value);
    } else if (any_value.type() == typeid(llvm::CallInst *)) {
        return std::any_cast<llvm::CallInst *>(any_value);
    } else if (any_value.type() == typeid(llvm::AllocaInst *)) {
        return std::any_cast<llvm::AllocaInst *>(any_value);
    } else if (any_value.type() == typeid(llvm::Constant *)) {
        return std::any_cast<llvm::Constant *>(any_value);
    } else if (any_value.type() == typeid(llvm::LoadInst *)) {
        return std::any_cast<llvm::LoadInst *>(any_value);
    } else if (any_value.type() == typeid(llvm::PHINode *)) {
        return std::any_cast<llvm::PHINode *>(any_value);
    } else if (any_value.type() == typeid(llvm::Function *)) {
        return std::any_cast<llvm::Function *>(any_value);
    } else {
        jot::loge << "Unknown type " << any_value.type().name() << '\n';
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

llvm::Value *JotLLVMBackend::llvm_boolean_value(bool value) {
    return llvm::ConstantInt::get(llvm_int1_type, value);
}

llvm::Value *JotLLVMBackend::llvm_character_value(char character) {
    return llvm::ConstantInt::get(llvm_int8_type, character);
}

llvm::Type *JotLLVMBackend::llvm_type_from_jot_type(std::shared_ptr<JotType> type) {
    TypeKind type_kind = type->get_type_kind();
    if (type_kind == TypeKind::Number) {
        auto jot_number_type = std::dynamic_pointer_cast<JotNumberType>(type);
        NumberKind number_kind = jot_number_type->get_kind();
        switch (number_kind) {
        case NumberKind::Integer1: return llvm_int1_type;
        case NumberKind::Integer8: return llvm_int8_type;
        case NumberKind::Integer16: return llvm_int16_type;
        case NumberKind::Integer32: return llvm_int32_type;
        case NumberKind::Integer64: return llvm_int64_type;
        case NumberKind::Float32: return llvm_float32_type;
        case NumberKind::Float64: return llvm_float64_type;
        }
    } else if (type_kind == TypeKind::Array) {
        auto jot_array_type = std::dynamic_pointer_cast<JotArrayType>(type);
        auto element_type = llvm_type_from_jot_type(jot_array_type->get_element_type());
        auto size = jot_array_type->get_size();
        return llvm::ArrayType::get(element_type, size);
    } else if (type_kind == TypeKind::Pointer) {
        auto jot_pointer_type = std::dynamic_pointer_cast<JotPointerType>(type);
        auto point_to_type = llvm_type_from_jot_type(jot_pointer_type->get_point_to());
        return llvm::PointerType::get(point_to_type, 0);
    } else if (type_kind == TypeKind::Function) {
        auto jot_function_type = std::dynamic_pointer_cast<JotFunctionType>(type);
        auto parameters = jot_function_type->get_parameters();
        int parameters_size = parameters.size();
        std::vector<llvm::Type *> arguments(parameters.size());
        for (int i = 0; i < parameters_size; i++) {
            arguments[i] = llvm_type_from_jot_type(parameters[i]);
        }
        auto return_type = llvm_type_from_jot_type(jot_function_type->get_return_type());
        auto function_type = llvm::FunctionType::get(return_type, arguments, false);
        return function_type;
    } else if (type_kind == TypeKind::EnumerationElement) {
        auto enum_element_type = std::dynamic_pointer_cast<JotEnumElementType>(type);
        return llvm_type_from_jot_type(enum_element_type->get_element_type());
    }
    jot::loge << "Unkown element type\n";
    return llvm_int64_type;
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

    return llvm_functions[name];
}

void JotLLVMBackend::push_alloca_inst_scope() {
    alloca_inst_scope = std::make_shared<JotSymbolTable>(alloca_inst_scope);
}

void JotLLVMBackend::pop_alloca_inst_scope() {
    alloca_inst_scope = alloca_inst_scope->get_parent_symbol_table();
}