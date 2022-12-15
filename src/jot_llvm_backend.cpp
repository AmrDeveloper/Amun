#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_ast_visitor.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"

#include <llvm/ADT/ArrayRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/Casting.h>

#include <any>
#include <memory>
#include <string>

std::unique_ptr<llvm::Module>
JotLLVMBackend::compile(std::string module_name, std::shared_ptr<CompilationUnit> compilation_unit)
{
    llvm_module = std::make_unique<llvm::Module>(module_name, llvm_context);
    try {
        for (auto& statement : compilation_unit->get_tree_nodes()) {
            statement->accept(this);
        }
    }
    catch (...) {
        jot::loge << "LLVM Backend Exception \n";
    }
    return std::move(llvm_module);
}

std::any JotLLVMBackend::visit(BlockStatement* node)
{
    for (auto& statement : node->get_nodes()) {
        statement->accept(this);

        // In the same block there are no needs to generate code for unreachable code
        if (statement->get_ast_node_type() == AstNodeType::Return or
            statement->get_ast_node_type() == AstNodeType::Break or
            statement->get_ast_node_type() == AstNodeType::Continue) {
            break;
        }
    }
    return 0;
}

std::any JotLLVMBackend::visit(FieldDeclaration* node)
{
    auto var_name = node->get_name().literal;
    auto field_type = node->get_type();
    auto llvm_type = llvm_type_from_jot_type(field_type);

    // Globals code generation block can be moved into other function to be clear and handle more
    // cases and to handle also soem compile time evaluations
    if (node->is_global()) {
        auto constants_value = resolve_constant_expression(node);
        auto global_variable = new llvm::GlobalVariable(*llvm_module, llvm_type, false,
                                                        llvm::GlobalValue::ExternalLinkage,
                                                        constants_value, var_name.c_str());

        global_variable->setAlignment(llvm::MaybeAlign(0));
        return 0;
    }

    // if field has initalizer evaluate it, else initalize it with default value
    std::any value;
    if (node->get_value() == nullptr) {
        value = llvm_type_null_value(field_type);
    }
    else {
        value = node->get_value()->accept(this);
    }

    auto current_function = Builder.GetInsertBlock()->getParent();
    if (value.type() == typeid(llvm::Value*)) {
        auto init_value = std::any_cast<llvm::Value*>(value);
        // auto init_value_type = init_value->getType();

        // This case if you assign derefernced variable for example
        // Case in C Language
        // int* ptr = (int*) malloc(sizeof(int));
        // int value = *ptr;
        // Clang compiler emit load instruction twice to resolve this problem
        //  if (init_value_type != llvm_type && init_value_type->getPointerElementType() ==
        //  llvm_type) {
        //    init_value = derefernecs_llvm_pointer(init_value);
        // }

        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::CallInst*)) {
        auto init_value = std::any_cast<llvm::CallInst*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::AllocaInst*)) {
        auto init_value = std::any_cast<llvm::AllocaInst*>(value);
        Builder.CreateLoad(init_value->getAllocatedType(), init_value, var_name);
        alloca_inst_scope->define(var_name, init_value);
    }
    else if (value.type() == typeid(llvm::Constant*)) {
        auto constant = std::any_cast<llvm::Constant*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(constant, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::LoadInst*)) {
        auto load_inst = std::any_cast<llvm::LoadInst*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(load_inst, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::PHINode*)) {
        auto node = std::any_cast<llvm::PHINode*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(node, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::Function*)) {
        auto node = std::any_cast<llvm::Function*>(value);
        llvm_functions[var_name] = node;
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, node->getType());
        Builder.CreateStore(node, alloc_inst);
        alloca_inst_scope->define(var_name, alloc_inst);
    }
    else {
        internal_compiler_error("Un supported rvalue for field declaration");
    }
    return 0;
}

std::any JotLLVMBackend::visit(FunctionPrototype* node)
{
    auto                     parameters = node->get_parameters();
    size_t                   parameters_size = parameters.size();
    std::vector<llvm::Type*> arguments(parameters_size);
    for (size_t i = 0; i < parameters_size; i++) {
        arguments[i] = llvm_type_from_jot_type(parameters[i]->get_type());
    }

    auto return_type = llvm_type_from_jot_type(node->get_return_type());
    auto function_type = llvm::FunctionType::get(return_type, arguments, node->has_varargs());
    auto function_name = node->get_name().literal;
    auto linkage = node->is_external() || function_name == "main" ? llvm::Function::ExternalLinkage
                                                                  : llvm::Function::InternalLinkage;
    auto function =
        llvm::Function::Create(function_type, linkage, function_name, llvm_module.get());

    unsigned index = 0;
    for (auto& argument : function->args()) {
        if (index >= parameters_size) {
            // Varargs case
            break;
        }
        argument.setName(parameters[index++]->get_name().literal);
    }

    return function;
}

std::any JotLLVMBackend::visit(FunctionDeclaration* node)
{
    auto prototype = node->get_prototype();
    auto name = prototype->get_name().literal;
    functions_table[name] = prototype;

    auto function = std::any_cast<llvm::Function*>(prototype->accept(this));
    auto entry_block = llvm::BasicBlock::Create(llvm_context, "entry", function);
    Builder.SetInsertPoint(entry_block);

    push_alloca_inst_scope();
    for (auto& arg : function->args()) {
        const std::string arg_name_str = std::string(arg.getName());
        auto* alloca_inst = create_entry_block_alloca(function, arg_name_str, arg.getType());
        alloca_inst_scope->define(arg_name_str, alloca_inst);
        Builder.CreateStore(&arg, alloca_inst);
    }

    node->get_body()->accept(this);

    pop_alloca_inst_scope();

    alloca_inst_scope->define(name, function);

    verifyFunction(*function);

    clear_defer_calls_stack();

    has_return_statement = false;
    return function;
}

std::any JotLLVMBackend::visit(StructDeclaration* node)
{
    const auto struct_type = node->get_struct_type();
    const auto struct_name = struct_type->name;
    const auto struct_llvm_type = llvm::StructType::create(llvm_context);
    struct_llvm_type->setName(struct_name);
    const auto               fields = struct_type->fields_types;
    std::vector<llvm::Type*> struct_fields;
    struct_fields.reserve(fields.size());
    for (auto& field : fields) {
        if (field->type_kind == TypeKind::Pointer) {
            auto pointer = std::static_pointer_cast<JotPointerType>(field);
            if (pointer->base_type->type_kind == TypeKind::Structure) {
                auto struct_ty = std::static_pointer_cast<JotStructType>(pointer->base_type);
                if (struct_ty->name == struct_name) {
                    struct_fields.push_back(struct_llvm_type->getPointerTo());
                    continue;
                }
            }
        }
        struct_fields.push_back(llvm_type_from_jot_type(field));
    }
    auto declare_with_padding = false;
    struct_llvm_type->setBody(struct_fields, declare_with_padding);
    structures_types_map[struct_name] = struct_llvm_type;
    return 0;
}

std::any JotLLVMBackend::visit([[maybe_unused]] EnumDeclaration* node)
{
    // Enumeration type is only compile time type, no need to generate any IR for it
    return 0;
}

std::any JotLLVMBackend::visit(IfStatement* node)
{
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

        auto condition = llvm_resolve_value(conditional_blocks[i]->get_condition()->accept(this));
        Builder.CreateCondBr(condition, true_block, false_branch);
        Builder.SetInsertPoint(true_block);

        push_alloca_inst_scope();
        conditional_blocks[i]->get_body()->accept(this);
        pop_alloca_inst_scope();

        // If there are not return, break or continue statement, must branch end block
        if (not has_break_or_continue_statement && not has_return_statement)
            Builder.CreateBr(end_block);
        else
            has_return_statement = false;

        Builder.SetInsertPoint(false_branch);
    }

    current_function->getBasicBlockList().push_back(end_block);

    if (has_break_or_continue_statement)
        has_break_or_continue_statement = false;
    else
        Builder.SetInsertPoint(end_block);

    return 0;
}

std::any JotLLVMBackend::visit(WhileStatement* node)
{
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto condition_branch = llvm::BasicBlock::Create(llvm_context, "while.condition");
    auto loop_branch = llvm::BasicBlock::Create(llvm_context, "while.loop");
    auto end_branch = llvm::BasicBlock::Create(llvm_context, "while.end");

    break_blocks_stack.push(end_branch);
    continue_blocks_stack.push(condition_branch);

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

    if (has_break_or_continue_statement)
        has_break_or_continue_statement = false;
    else
        Builder.CreateBr(condition_branch);

    current_function->getBasicBlockList().push_back(end_branch);
    Builder.SetInsertPoint(end_branch);

    break_blocks_stack.pop();
    continue_blocks_stack.pop();

    return 0;
}

std::any JotLLVMBackend::visit(SwitchStatement* node)
{
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto argument = node->get_argument();
    auto llvm_value = llvm_resolve_value(argument->accept(this));
    auto basic_block = llvm::BasicBlock::Create(llvm_context, "", current_function);
    auto switch_inst = Builder.CreateSwitch(llvm_value, basic_block);

    auto switch_cases = node->get_cases();
    auto switch_cases_size = switch_cases.size();

    // Generate code for each switch case
    for (size_t i = 0; i < switch_cases_size; i++) {
        auto switch_case = switch_cases[i];
        create_switch_case_branch(switch_inst, current_function, basic_block, switch_case);
    }

    // Generate code for default cases is exists
    auto default_branch = node->get_default_case();
    if (default_branch) {
        create_switch_case_branch(switch_inst, current_function, basic_block, default_branch);
    }

    Builder.SetInsertPoint(basic_block);
    return 0;
}

std::any JotLLVMBackend::visit(ReturnStatement* node)
{
    // Generate code for defer calls if there are any
    execute_defer_calls();

    has_return_statement = true;

    if (not node->has_value()) {
        return Builder.CreateRetVoid();
    }

    auto value = node->return_value()->accept(this);

    // This code must be cleard with branches in Field Dec to be more clear and easy to extend
    if (value.type() == typeid(llvm::Value*)) {
        auto return_value = std::any_cast<llvm::Value*>(value);
        return Builder.CreateRet(return_value);
    }
    else if (value.type() == typeid(llvm::CallInst*)) {
        auto init_value = std::any_cast<llvm::CallInst*>(value);
        return Builder.CreateRet(init_value);
    }
    else if (value.type() == typeid(llvm::AllocaInst*)) {
        auto init_value = std::any_cast<llvm::AllocaInst*>(value);
        auto value_litearl = Builder.CreateLoad(init_value->getAllocatedType(), init_value);
        return Builder.CreateRet(value_litearl);
    }
    else if (value.type() == typeid(llvm::Function*)) {
        auto node = std::any_cast<llvm::Function*>(value);
        return Builder.CreateRet(node);
    }
    else if (value.type() == typeid(llvm::Constant*)) {
        auto init_value = std::any_cast<llvm::Constant*>(value);
        return Builder.CreateRet(init_value);
    }
    else if (value.type() == typeid(llvm::LoadInst*)) {
        auto load_inst = std::any_cast<llvm::LoadInst*>(value);
        return Builder.CreateRet(load_inst);
    }
    else if (value.type() == typeid(llvm::GlobalVariable*)) {
        auto variable = std::any_cast<llvm::GlobalVariable*>(value);
        auto value = Builder.CreateLoad(variable->getValueType(), variable);
        return Builder.CreateRet(value);
    }
    else if (value.type() == typeid(llvm::PHINode*)) {
        auto phi = std::any_cast<llvm::PHINode*>(value);
        return Builder.CreateRet(phi);
    }

    internal_compiler_error("Un expected return type");
}

std::any JotLLVMBackend::visit(DeferStatement* node)
{
    auto call_expression = node->get_call_expression();
    auto callee = std::dynamic_pointer_cast<LiteralExpression>(call_expression->get_callee());
    auto callee_literal = callee->get_name().literal;
    auto function = lookup_function(callee_literal);
    if (not function) {
        auto value = llvm_node_value(alloca_inst_scope->lookup(callee_literal));
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
            auto loaded = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
            auto function_type = llvm_type_from_jot_type(call_expression->get_type_node());
            if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                auto                      arguments = call_expression->get_arguments();
                size_t                    arguments_size = arguments.size();
                std::vector<llvm::Value*> arguments_values;
                arguments_values.reserve(arguments_size);
                for (size_t i = 0; i < arguments_size; i++) {
                    auto value = llvm_node_value(arguments[i]->accept(this));
                    if (function_pointer->getParamType(i) == value->getType()) {
                        arguments_values.push_back(value);
                    }
                    else {
                        auto expected_type = function_pointer->getParamType(i);
                        auto loaded_value = Builder.CreateLoad(expected_type, value);
                        arguments_values.push_back(loaded_value);
                    }
                }
                auto defer_function_call = std::make_shared<DeferFunctionPtrCall>(
                    function_pointer, loaded, arguments_values);
                defer_calls_stack.insert(defer_calls_stack.begin(), defer_function_call);
            }
        }
        return 0;
    }

    auto                      arguments = call_expression->get_arguments();
    auto                      arguments_size = arguments.size();
    auto                      parameter_size = function->arg_size();
    std::vector<llvm::Value*> arguments_values;
    arguments_values.reserve(arguments_size);
    for (size_t i = 0; i < arguments_size; i++) {
        auto argument = arguments[i];
        auto value = llvm_node_value(argument->accept(this));

        if (i >= parameter_size) {
            if (argument->get_ast_node_type() == AstNodeType::LiteralExpr) {
                auto argument_type = llvm_type_from_jot_type(argument->get_type_node());
                auto loaded_value = Builder.CreateLoad(argument_type, value);
                arguments_values.push_back(loaded_value);
                continue;
            }
            arguments_values.push_back(value);
            continue;
        }

        if (function->getArg(i)->getType() == value->getType()) {
            arguments_values.push_back(value);
            continue;
        }

        auto expected_type = function->getArg(i)->getType();
        auto loaded_value = Builder.CreateLoad(expected_type, value);
        arguments_values.push_back(loaded_value);
    }

    auto defer_function_call = std::make_shared<DeferFunctionCall>(function, arguments_values);

    // Inser must be at the begin to simulate stack but in vector to easy traverse and clear
    defer_calls_stack.insert(defer_calls_stack.begin(), defer_function_call);
    return 0;
}

std::any JotLLVMBackend::visit(BreakStatement* node)
{
    has_break_or_continue_statement = true;

    for (int i = 1; i < node->get_times(); i++) {
        break_blocks_stack.pop();
    }

    Builder.CreateBr(break_blocks_stack.top());
    return 0;
}

std::any JotLLVMBackend::visit(ContinueStatement* node)
{
    has_break_or_continue_statement = true;

    for (int i = 1; i < node->get_times(); i++) {
        continue_blocks_stack.pop();
    }

    Builder.CreateBr(continue_blocks_stack.top());
    return 0;
}

std::any JotLLVMBackend::visit(ExpressionStatement* node)
{
    node->get_expression()->accept(this);
    return 0;
}

std::any JotLLVMBackend::visit(IfExpression* node)
{
    // If it constant, we can resolve it at Compile time
    if (is_global_block() && node->is_constant()) {
        return resolve_constant_if_expression(std::make_shared<IfExpression>(*node));
    }

    auto function = Builder.GetInsertBlock()->getParent();

    auto condition = llvm_resolve_value(node->get_condition()->accept(this));

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(llvm_context, "then", function);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(llvm_context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(llvm_context, "ifcont");
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

    llvm::PHINode* pn = Builder.CreatePHI(then_value->getType(), 2, "iftmp");
    pn->addIncoming(then_value, thenBB);
    pn->addIncoming(else_value, elseBB);

    return pn;
}

std::any JotLLVMBackend::visit(SwitchExpression* node)
{
    // If it constant, we can resolve it at Compile time
    if (is_global_block() && node->is_constant()) {
        return resolve_constant_switch_expression(std::make_shared<SwitchExpression>(*node));
    }

    // Resolve the argument condition
    // In each branch check the equlity between argument and case
    // If they are equal conditional jump to the final branch, else jump to the next branch
    // If the current branch is default case branch, perform un conditional jump to final branch

    auto cases = node->get_switch_cases();
    auto values = node->get_switch_cases_values();

    // The number of cases that has a value (not default case)
    auto cases_size = cases.size();

    // The number of all values (cases and default case values)
    auto values_size = cases_size + 1;

    // The value type for all cases values
    auto value_type = llvm_type_from_jot_type(node->get_type_node());

    auto function = Builder.GetInsertBlock()->getParent();

    auto argument = llvm_resolve_value(node->get_argument()->accept(this));

    // Create basic blocks that match the number of cases even the default case
    std::vector<llvm::BasicBlock*> llvm_branches(values_size);
    for (size_t i = 0; i < values_size; i++) {
        llvm_branches[i] = llvm::BasicBlock::Create(llvm_context);
    }

    // Resolve all values before creating any branch or jumps
    std::vector<llvm::Value*> llvm_values(values_size);
    for (size_t i = 0; i < cases_size; i++) {
        llvm_values[i] = llvm_resolve_value(values[i]->accept(this));
    }

    // Resolve the default value
    llvm_values[cases_size] = llvm_resolve_value(node->get_default_case_value()->accept(this));

    // Merge branch is the branch which contains the phi node used as a destination
    // if current case has the same value as argument valeu
    // or no case match the argument value so default branch will perfrom jump to it
    auto merge_branch = llvm::BasicBlock::Create(llvm_context);

    // Un conditional jump to the first block and make it the current insert point
    auto first_branch = llvm_branches[0];
    Builder.CreateBr(first_branch);
    function->getBasicBlockList().push_back(first_branch);
    Builder.SetInsertPoint(first_branch);

    for (size_t i = 1; i < values_size; i++) {
        auto current_branch = llvm_branches[i];

        // Compare the argument value with current case
        auto case_value = llvm_node_value(cases[i - 1]->accept(this));
        auto condition = Builder.CreateICmpEQ(argument, case_value);

        // Jump to the merge block if current case equal to argument,
        // else jump to next branch (case)
        Builder.CreateCondBr(condition, merge_branch, current_branch);

        // condition first then branch
        function->getBasicBlockList().push_back(current_branch);
        Builder.SetInsertPoint(current_branch);
    }

    // Un conditional jump from the default branch to the merge branch
    Builder.CreateBr(merge_branch);

    // Insert the merge branch and make it the insert point to generate the phi node
    function->getBasicBlockList().push_back(merge_branch);
    Builder.SetInsertPoint(merge_branch);

    // Create A phi nodes with all resolved values and their basic blocks
    auto phi_node = Builder.CreatePHI(value_type, values_size);
    for (size_t i = 0; i < values_size; i++) {
        phi_node->addIncoming(llvm_values[i], llvm_branches[i]);
    }

    return phi_node;
}

std::any JotLLVMBackend::visit(GroupExpression* node)
{
    return node->get_expression()->accept(this);
}

std::any JotLLVMBackend::visit(AssignExpression* node)
{
    auto left_node = node->get_left();
    // Assign value to variable
    // variable = value
    if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(left_node)) {
        auto name = literal->get_name().literal;
        auto value = node->get_right()->accept(this);

        auto right_value = llvm_resolve_value(value);
        auto left_value = node->get_left()->accept(this);
        if (left_value.type() == typeid(llvm::AllocaInst*)) {
            auto alloca = std::any_cast<llvm::AllocaInst*>(left_value);

            // Alloca type must be a pointer to rvalue type
            // this case solve assiging dereferneced value into variable
            // Case
            // var x = 0;
            // x = *ptr;
            // Check samples/memory/AssignPtrValueToVar.jot
            // Check samples/general/Linkedlist.jot
            if (alloca->getType() == right_value->getType()) {
                right_value = derefernecs_llvm_pointer(right_value);
            }

            alloca_inst_scope->update(name, alloca);
            return Builder.CreateStore(right_value, alloca);
        }

        if (left_value.type() == typeid(llvm::GlobalVariable*)) {
            auto global_variable = std::any_cast<llvm::GlobalVariable*>(left_value);
            return Builder.CreateStore(right_value, global_variable);
        }
    }

    // Assign value to n dimentions array position
    // array []? = value
    if (auto index_expression = std::dynamic_pointer_cast<IndexExpression>(left_node)) {
        auto node_value = index_expression->get_value();
        auto index = llvm_resolve_value(index_expression->get_index()->accept(this));
        auto right_value = llvm_node_value(node->get_right()->accept(this));

        // Update element value in Single dimention Array
        if (auto array_literal = std::dynamic_pointer_cast<LiteralExpression>(node_value)) {
            auto array = array_literal->accept(this);
            if (array.type() == typeid(llvm::AllocaInst*)) {
                auto alloca = llvm::dyn_cast<llvm::AllocaInst>(
                    llvm_node_value(alloca_inst_scope->lookup(array_literal->get_name().literal)));
                auto ptr = Builder.CreateGEP(alloca->getAllocatedType(), alloca,
                                             {zero_int32_value, index});
                return Builder.CreateStore(right_value, ptr);
            }

            if (array.type() == typeid(llvm::GlobalVariable*)) {
                auto global_variable_array =
                    llvm::dyn_cast<llvm::GlobalVariable>(llvm_node_value(array));
                auto ptr = Builder.CreateGEP(global_variable_array->getValueType(),
                                             global_variable_array, {zero_int32_value, index});
                return Builder.CreateStore(right_value, ptr);
            }

            internal_compiler_error("Assign value index expression");
        }

        // Update element value in Multi dimentions Array
        if (auto sub_index_expression = std::dynamic_pointer_cast<IndexExpression>(node_value)) {
            auto array = llvm_node_value(node_value->accept(this));
            auto load_inst = dyn_cast<llvm::LoadInst>(array);
            auto ptr = Builder.CreateGEP(array->getType(), load_inst->getPointerOperand(),
                                         {zero_int32_value, index});
            return Builder.CreateStore(right_value, ptr);
        }
    }

    // Assign value to structure field
    if (auto dot_expression = std::dynamic_pointer_cast<DotExpression>(left_node)) {
        auto member_ptr = access_struct_member_pointer(dot_expression.get());
        auto rvalue = llvm_resolve_value(node->get_right()->accept(this));
        return Builder.CreateStore(rvalue, member_ptr);
    }

    // Assign value to pointer address
    // *ptr = value;
    if (auto unary_expression = std::dynamic_pointer_cast<PrefixUnaryExpression>(left_node)) {
        auto opt = unary_expression->get_operator_token().kind;
        if (opt == TokenKind::Star) {
            auto rvalue = llvm_node_value(node->get_right()->accept(this));
            auto pointer = llvm_node_value(unary_expression->get_right()->accept(this));
            auto load = Builder.CreateLoad(pointer->getType()->getPointerElementType(), pointer);
            Builder.CreateStore(rvalue, load);
            return rvalue;
        }
    }

    internal_compiler_error("Invalid assignments expression with unexpected lvalue type");
}

std::any JotLLVMBackend::visit(BinaryExpression* node)
{
    auto left = llvm_resolve_value(node->get_left()->accept(this));
    auto right = llvm_resolve_value(node->get_right()->accept(this));
    auto op = node->get_operator_token().kind;

    // Binary Operations for integer types
    if (left->getType()->isIntegerTy() and right->getType()->isIntegerTy()) {
        return create_llvm_integers_bianry(op, left, right);
    }

    // Binary Operations for floating point types
    if (left->getType()->isFloatingPointTy() and right->getType()->isFloatingPointTy()) {
        return create_llvm_floats_bianry(op, left, right);
    }

    internal_compiler_error("Binary Operators work only for Numeric types");
}

std::any JotLLVMBackend::visit(ShiftExpression* node)
{
    auto left = llvm_resolve_value(node->get_left()->accept(this));
    auto right = llvm_resolve_value(node->get_right()->accept(this));
    auto operator_kind = node->get_operator_token().kind;

    if (operator_kind == TokenKind::LeftShift) {
        return Builder.CreateShl(left, right);
    }

    if (operator_kind == TokenKind::RightShift) {
        return Builder.CreateAShr(left, right);
    }

    internal_compiler_error("Invalid Shift expression operator");
}

std::any JotLLVMBackend::visit(ComparisonExpression* node)
{
    const auto left = llvm_resolve_value(node->get_left()->accept(this));
    const auto right = llvm_resolve_value(node->get_right()->accept(this));
    const auto op = node->get_operator_token().kind;

    const auto left_type = left->getType();
    const auto right_type = right->getType();

    // Comparison Operations for integers types
    if (left_type->isIntegerTy() and right_type->isIntegerTy()) {
        return create_llvm_integers_comparison(op, left, right);
    }

    // Comparison Operations for floating point types
    if (left_type->isFloatingPointTy() and right_type->isFloatingPointTy()) {
        return create_llvm_floats_comparison(op, left, right);
    }

    // Comparison Operations for pointers types thay points to the same type, no need for casting
    if (left_type->isPointerTy() and right_type->isPointerTy()) {
        return create_llvm_integers_comparison(op, left, right);
    }

    // This line must be unreacable and any type error must handled in type checker pass
    internal_compiler_error("Binary Operators work only for Numeric types");
}

std::any JotLLVMBackend::visit(LogicalExpression* node)
{
    auto left = llvm_node_value(node->get_left()->accept(this));
    auto right = llvm_node_value(node->get_right()->accept(this));
    if (not left || not right) {
        return nullptr;
    }

    switch (node->get_operator_token().kind) {
    case TokenKind::LogicalAnd: {
        return Builder.CreateLogicalAnd(left, right);
    }
    case TokenKind::LogicalOr: {
        return Builder.CreateLogicalOr(left, right);
    }
    default: {
        internal_compiler_error("Invalid Logical operator");
    }
    }
}

std::any JotLLVMBackend::visit(PrefixUnaryExpression* node)
{
    auto operand = node->get_right();
    auto operator_kind = node->get_operator_token().kind;

    // Unary - minus operator
    if (operator_kind == TokenKind::Minus) {
        auto right = llvm_resolve_value(operand->accept(this));
        return Builder.CreateNeg(right);
    }

    // Bang can be implemented as (value == false)
    if (operator_kind == TokenKind::Bang) {
        auto right = llvm_resolve_value(operand->accept(this));
        return Builder.CreateICmpEQ(right, false_value);
    }

    // Pointer * Dereference operator
    if (operator_kind == TokenKind::Star) {
        auto right = llvm_node_value(operand->accept(this));
        auto is_expect_struct_type = node->get_type_node()->type_kind == TypeKind::Structure;
        // No need to emit 2 load inst if the current type is pointer to struct
        if (is_expect_struct_type) {
            return derefernecs_llvm_pointer(right);
        }
        auto derefernce_right = derefernecs_llvm_pointer(right);
        return derefernecs_llvm_pointer(derefernce_right);
    }

    // Address of operator (&) to return pointer of operand
    if (operator_kind == TokenKind::And) {
        auto right = llvm_node_value(operand->accept(this));
        auto ptr = Builder.CreateAlloca(right->getType(), nullptr);
        Builder.CreateStore(right, ptr);
        return ptr;
    }

    // Unary ~ not operator
    if (operator_kind == TokenKind::Not) {
        auto right = llvm_resolve_value(operand->accept(this));
        return Builder.CreateNot(right);
    }

    // Unary prefix ++ operator, example (++x)
    if (operator_kind == TokenKind::PlusPlus) {
        return create_llvm_value_increment(operand, true);
    }

    // Unary prefix -- operator, example (--x)
    if (operator_kind == TokenKind::MinusMinus) {
        return create_llvm_value_decrement(operand, true);
    }

    internal_compiler_error("Invalid Prefix Unary operator");
}

std::any JotLLVMBackend::visit(PostfixUnaryExpression* node)
{
    auto operand = node->get_right();
    auto operator_kind = node->get_operator_token().kind;

    // Unary postfix ++ operator, example (x++)
    if (operator_kind == TokenKind::PlusPlus) {
        return create_llvm_value_increment(operand, false);
    }

    // Unary postfix -- operator, example (x--)
    if (operator_kind == TokenKind::MinusMinus) {
        return create_llvm_value_decrement(operand, false);
    }

    internal_compiler_error("Invalid Postfix Unary operator");
}

std::any JotLLVMBackend::visit(CallExpression* node)
{
    // If callee is also a CallExpression this case when you have a function that return a
    // function pointer and you call it for example function()();
    if (node->get_callee()->get_ast_node_type() == AstNodeType::CallExpr) {
        auto callee_function = llvm_node_value(node->get_callee()->accept(this));
        auto call_instruction = llvm::dyn_cast<llvm::CallInst>(callee_function);
        auto function = call_instruction->getCalledFunction();
        auto callee_function_type = function->getFunctionType();
        auto return_ptr_type = callee_function_type->getReturnType()->getPointerElementType();
        auto function_pointer_type = llvm::dyn_cast<llvm::FunctionType>(return_ptr_type);

        auto                      arguments = node->get_arguments();
        size_t                    arguments_size = arguments.size();
        std::vector<llvm::Value*> arguments_values;
        arguments_values.reserve(arguments_size);
        for (size_t i = 0; i < arguments_size; i++) {
            auto value = llvm_node_value(arguments[i]->accept(this));
            if (function_pointer_type->getParamType(i) == value->getType()) {
                arguments_values.push_back(value);
            }
            else {
                auto expected_type = function_pointer_type->getParamType(i);
                auto loaded_value = Builder.CreateLoad(expected_type, value);
                arguments_values.push_back(loaded_value);
            }
        }
        return Builder.CreateCall(function_pointer_type, call_instruction, arguments_values);
    }

    auto callee = std::dynamic_pointer_cast<LiteralExpression>(node->get_callee());
    auto callee_literal = callee->get_name().literal;
    auto function = lookup_function(callee_literal);
    if (not function) {
        auto value = llvm_node_value(alloca_inst_scope->lookup(callee_literal));
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
            auto loaded = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
            auto function_type = llvm_type_from_jot_type(node->get_type_node());
            if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                auto                      arguments = node->get_arguments();
                size_t                    arguments_size = arguments.size();
                std::vector<llvm::Value*> arguments_values;
                arguments_values.reserve(arguments_size);
                for (size_t i = 0; i < arguments_size; i++) {
                    auto value = llvm_node_value(arguments[i]->accept(this));
                    if (function_pointer->getParamType(i) == value->getType()) {
                        arguments_values.push_back(value);
                    }
                    else {
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

    auto                      arguments = node->get_arguments();
    auto                      arguments_size = arguments.size();
    auto                      parameter_size = function->arg_size();
    std::vector<llvm::Value*> arguments_values;
    arguments_values.reserve(arguments_size);
    for (size_t i = 0; i < arguments_size; i++) {
        auto argument = arguments[i];
        auto value = llvm_node_value(argument->accept(this));

        // This condition works only if this function has varargs flag
        if (i >= parameter_size) {
            if (argument->get_ast_node_type() == AstNodeType::LiteralExpr) {
                auto argument_type = llvm_type_from_jot_type(argument->get_type_node());
                auto loaded_value = Builder.CreateLoad(argument_type, value);
                arguments_values.push_back(loaded_value);
                continue;
            }

            arguments_values.push_back(value);
            continue;
        }

        // If argument type is the same parameter type just pass it directly
        if (function->getArg(i)->getType() == value->getType()) {
            arguments_values.push_back(value);
            continue;
        }

        // Load the constants first and then pass it to the arguments values
        auto expected_type = function->getArg(i)->getType();
        auto loaded_value = Builder.CreateLoad(expected_type, value);
        arguments_values.push_back(loaded_value);
    }
    return Builder.CreateCall(function, arguments_values);
}

std::any JotLLVMBackend::visit(DotExpression* node)
{
    auto expected_llvm_type = llvm_type_from_jot_type(node->get_type_node());
    auto member_ptr = access_struct_member_pointer(node);

    // If expected type is pointer no need for loading it for example node.next.data
    if (expected_llvm_type->isPointerTy()) {
        return member_ptr;
    }

    return Builder.CreateLoad(expected_llvm_type, member_ptr);
}

std::any JotLLVMBackend::visit(CastExpression* node)
{
    auto value = llvm_resolve_value(node->get_value()->accept(this));
    auto value_type = llvm_type_from_jot_type(node->get_value()->get_type_node());
    auto target_type = llvm_type_from_jot_type(node->get_type_node());

    // No need for castring if both part has the same type
    if (value_type == target_type) {
        return value;
    }

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

    // Array of type T to pointer or type T, return pointer to the first element
    if (target_type->isPointerTy() and value_type->isArrayTy()) {
        auto load_inst = dyn_cast<llvm::LoadInst>(value);
        return Builder.CreateGEP(value->getType(), load_inst->getPointerOperand(),
                                 {zero_int32_value, zero_int32_value});
    }

    // Bit casting
    return Builder.CreateBitCast(value, target_type);
}

std::any JotLLVMBackend::visit(TypeSizeExpression* node)
{
    auto llvm_type = llvm_type_from_jot_type(node->get_type());
    auto type_alloc_size = llvm_module->getDataLayout().getTypeAllocSize(llvm_type);
    auto type_size = llvm::ConstantInt::get(llvm_int64_type, type_alloc_size);
    return type_size;
}

std::any JotLLVMBackend::visit(ValueSizeExpression* node)
{
    auto llvm_type = llvm_type_from_jot_type(node->get_value()->get_type_node());
    auto type_alloc_size = llvm_module->getDataLayout().getTypeAllocSize(llvm_type);
    auto type_size = llvm::ConstantInt::get(llvm_int64_type, type_alloc_size);
    return type_size;
}

std::any JotLLVMBackend::visit(IndexExpression* node)
{
    auto index = llvm_resolve_value(node->get_index()->accept(this));
    auto node_value = node->get_value();
    auto values = node_value->get_type_node();

    if (values->type_kind == TypeKind::Pointer) {
        auto element_type = llvm_type_from_jot_type(node->get_type_node());
        auto value = llvm_resolve_value(node_value->accept(this));
        auto ptr = Builder.CreateGEP(element_type, value, index);
        return Builder.CreateLoad(element_type, ptr);
    }

    // One dimension Array Index Expression
    if (auto array_literal = std::dynamic_pointer_cast<LiteralExpression>(node_value)) {
        auto array = array_literal->accept(this);

        if (array.type() == typeid(llvm::AllocaInst*)) {
            auto alloca = llvm::dyn_cast<llvm::AllocaInst>(llvm_node_value(array));
            auto alloca_type = alloca->getAllocatedType();
            auto ptr = Builder.CreateGEP(alloca_type, alloca, {zero_int32_value, index});
            return Builder.CreateLoad(llvm_type_from_jot_type(node->get_type_node()), ptr);
        }

        if (array.type() == typeid(llvm::GlobalVariable*)) {
            auto global_variable_array =
                llvm::dyn_cast<llvm::GlobalVariable>(llvm_node_value(array));

            auto local_insert_block = Builder.GetInsertBlock();

            // Its local
            if (local_insert_block) {
                auto ptr = Builder.CreateGEP(global_variable_array->getValueType(),
                                             global_variable_array, {zero_int32_value, index});
                return Builder.CreateLoad(llvm_type_from_jot_type(node->get_type_node()), ptr);
            }

            // Resolve index expression for constants array
            auto initalizer = global_variable_array->getInitializer();
            auto constants_index = llvm::dyn_cast<llvm::ConstantInt>(index);

            // Index expression for array with constants data types such as integers, floats
            if (auto data_array = llvm::dyn_cast<llvm::ConstantDataArray>(initalizer)) {
                return data_array->getElementAsConstant(constants_index->getLimitedValue());
            }

            // Index expression for array with non constants data types
            if (auto constants_array = llvm::dyn_cast<llvm::ConstantArray>(initalizer)) {
                return constants_array->getAggregateElement(constants_index);
            }

            // Index Expression for array that initalized with default values
            if (auto constants_array = llvm::dyn_cast<llvm::Constant>(initalizer)) {
                return constants_array->getAggregateElement(constants_index);
            }
        }

        internal_compiler_error("Index expr with literal must have alloca or global variable");
    }

    // Multidimensional Array Index Expression
    if (auto index_expression = std::dynamic_pointer_cast<IndexExpression>(node_value)) {
        auto array = llvm_node_value(node_value->accept(this));
        if (auto load_inst = dyn_cast<llvm::LoadInst>(array)) {
            auto ptr = Builder.CreateGEP(array->getType(), load_inst->getPointerOperand(),
                                         {zero_int32_value, index});
            return Builder.CreateLoad(llvm_type_from_jot_type(node->get_type_node()), ptr);
        }

        if (auto constants_array = llvm::dyn_cast<llvm::Constant>(array)) {
            auto constants_index = llvm::dyn_cast<llvm::ConstantInt>(index);
            return constants_array->getAggregateElement(constants_index);
        }
    }

    internal_compiler_error("Invalid Index expression");
}

std::any JotLLVMBackend::visit(EnumAccessExpression* node)
{
    auto element_type = llvm_type_from_jot_type(node->get_type_node());
    auto element_index = llvm::ConstantInt::get(element_type, node->get_enum_element_index());
    return llvm::dyn_cast<llvm::Value>(element_index);
}

std::any JotLLVMBackend::visit(LiteralExpression* node)
{
    auto name = node->get_name().literal;
    auto alloca_inst = alloca_inst_scope->lookup(name);
    if (alloca_inst.type() != typeid(nullptr))
        return alloca_inst;
    // If it not in alloca inst table,a maybe it global variable
    return llvm_module->getNamedGlobal(name);
}

std::any JotLLVMBackend::visit(NumberExpression* node)
{
    auto number_type = std::static_pointer_cast<JotNumberType>(node->get_type_node());
    return llvm_number_value(node->get_value().literal, number_type->number_kind);
}

std::any JotLLVMBackend::visit(ArrayExpression* node)
{
    auto node_values = node->get_values();
    auto size = node_values.size();
    if (node->is_constant()) {
        auto arrayType =
            llvm::dyn_cast<llvm::ArrayType>(llvm_type_from_jot_type(node->get_type_node()));
        std::vector<llvm::Constant*> values;
        values.reserve(size);
        for (auto& value : node_values) {
            values.push_back(
                llvm::dyn_cast<llvm::Constant>(llvm_resolve_value(value->accept(this))));
        }
        return llvm::ConstantArray::get(arrayType, values);
    }

    auto                      arrayType = llvm_type_from_jot_type(node->get_type_node());
    std::vector<llvm::Value*> values;
    values.reserve(size);
    for (auto& value : node_values) {
        values.push_back(llvm_resolve_value(value->accept(this)));
    }
    auto alloca = Builder.CreateAlloca(arrayType);
    for (size_t i = 0; i < size; i++) {
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

std::any JotLLVMBackend::visit(StringExpression* node)
{
    std::string literal = node->get_value().literal;
    return resolve_constant_string_expression(literal);
}

std::any JotLLVMBackend::visit(CharacterExpression* node)
{
    char char_asci_value = node->get_value().literal[0];
    return llvm_character_value(char_asci_value);
}

std::any JotLLVMBackend::visit(BooleanExpression* node)
{
    return llvm_boolean_value(node->get_value().kind == TokenKind::TrueKeyword);
}

std::any JotLLVMBackend::visit([[maybe_unused]] NullExpression* node)
{
    return llvm_type_null_value(node->null_base_type);
}

llvm::Value* JotLLVMBackend::llvm_node_value(std::any any_value)
{
    if (any_value.type() == typeid(llvm::Value*)) {
        return std::any_cast<llvm::Value*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::CallInst*)) {
        return std::any_cast<llvm::CallInst*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::AllocaInst*)) {
        return std::any_cast<llvm::AllocaInst*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::Constant*)) {
        return std::any_cast<llvm::Constant*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::ConstantInt*)) {
        return std::any_cast<llvm::ConstantInt*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::LoadInst*)) {
        return std::any_cast<llvm::LoadInst*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::PHINode*)) {
        return std::any_cast<llvm::PHINode*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::Function*)) {
        return std::any_cast<llvm::Function*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::GlobalVariable*)) {
        return std::any_cast<llvm::GlobalVariable*>(any_value);
    }
    else if (any_value.type() == typeid(llvm::ConstantPointerNull*)) {
        return std::any_cast<llvm::ConstantPointerNull*>(any_value);
    }

    internal_compiler_error("Unknown type llvm node type ");
}

llvm::Value* JotLLVMBackend::llvm_resolve_value(std::any any_value)
{
    auto llvm_value = llvm_node_value(any_value);

    if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(llvm_value)) {
        return Builder.CreateLoad(alloca->getAllocatedType(), alloca);
    }

    if (auto variable = llvm::dyn_cast<llvm::GlobalVariable>(llvm_value)) {
        return variable->getInitializer();
    }

    return llvm_value;
}

inline llvm::Value* JotLLVMBackend::llvm_number_value(const std::string& value_litearl,
                                                      NumberKind         size)
{
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
        auto value = std::strtoll(value_litearl.c_str(), NULL, 0);
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

inline llvm::Value* JotLLVMBackend::llvm_boolean_value(bool value)
{
    return llvm::ConstantInt::get(llvm_int1_type, value);
}

inline llvm::Value* JotLLVMBackend::llvm_character_value(char character)
{
    return llvm::ConstantInt::get(llvm_int8_type, character);
}

inline llvm::Value* JotLLVMBackend::llvm_type_null_value(std::shared_ptr<JotType>& type)
{
    // Return null value for the base type
    return llvm::Constant::getNullValue(llvm_type_from_jot_type(type));
}

llvm::Type* JotLLVMBackend::llvm_type_from_jot_type(std::shared_ptr<JotType> type)
{
    TypeKind type_kind = type->type_kind;
    if (type_kind == TypeKind::Number) {
        auto       jot_number_type = std::static_pointer_cast<JotNumberType>(type);
        NumberKind number_kind = jot_number_type->number_kind;
        switch (number_kind) {
        case NumberKind::Integer1: return llvm_int1_type;
        case NumberKind::Integer8: return llvm_int8_type;
        case NumberKind::Integer16: return llvm_int16_type;
        case NumberKind::Integer32: return llvm_int32_type;
        case NumberKind::Integer64: return llvm_int64_type;
        case NumberKind::Float32: return llvm_float32_type;
        case NumberKind::Float64: return llvm_float64_type;
        }
    }

    if (type_kind == TypeKind::Array) {
        auto jot_array_type = std::static_pointer_cast<JotArrayType>(type);
        auto element_type = llvm_type_from_jot_type(jot_array_type->element_type);
        auto size = jot_array_type->size;
        return llvm::ArrayType::get(element_type, size);
    }

    if (type_kind == TypeKind::Pointer) {
        auto jot_pointer_type = std::static_pointer_cast<JotPointerType>(type);
        // In llvm *void should be generated as *i8
        if (jot_pointer_type->base_type->type_kind == TypeKind::Void) {
            return llvm_void_ptr_type;
        }
        auto point_to_type = llvm_type_from_jot_type(jot_pointer_type->base_type);
        return llvm::PointerType::get(point_to_type, 0);
    }

    if (type_kind == TypeKind::Function) {
        auto jot_function_type = std::static_pointer_cast<JotFunctionType>(type);
        auto parameters = jot_function_type->parameters;
        int  parameters_size = parameters.size();
        std::vector<llvm::Type*> arguments(parameters_size);
        for (int i = 0; i < parameters_size; i++) {
            arguments[i] = llvm_type_from_jot_type(parameters[i]);
        }
        auto return_type = llvm_type_from_jot_type(jot_function_type->return_type);
        auto function_type = llvm::FunctionType::get(return_type, arguments, false);
        return function_type;
    }

    if (type_kind == TypeKind::Structure) {
        auto struct_type = std::static_pointer_cast<JotStructType>(type);
        auto struct_name = struct_type->name;
        return structures_types_map[struct_name];
    }

    if (type_kind == TypeKind::EnumerationElement) {
        auto enum_element_type = std::static_pointer_cast<JotEnumElementType>(type);
        return llvm_type_from_jot_type(enum_element_type->element_type);
    }

    if (type_kind == TypeKind::Void) {
        return llvm_void_type;
    }

    internal_compiler_error("Can't find LLVM Type for this Jot Type");
}

inline llvm::Value* JotLLVMBackend::create_llvm_integers_bianry(TokenKind op, llvm::Value* left,
                                                                llvm::Value* right)
{
    switch (op) {
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
        internal_compiler_error("Invalid binary operator for integers types");
    }
    }
}

inline llvm::Value* JotLLVMBackend::create_llvm_floats_bianry(TokenKind op, llvm::Value* left,
                                                              llvm::Value* right)
{
    switch (op) {
    case TokenKind::Plus: {
        return Builder.CreateFAdd(left, right, "addtemp");
    }
    case TokenKind::Minus: {
        return Builder.CreateFSub(left, right, "subtmp");
    }
    case TokenKind::Star: {
        return Builder.CreateFMul(left, right, "multmp");
    }
    case TokenKind::Slash: {
        return Builder.CreateFDiv(left, right, "divtmp");
    }
    case TokenKind::Percent: {
        return Builder.CreateFRem(left, right, "remtemp");
    }
    default: {
        internal_compiler_error("Invalid binary operator for floating point types");
    }
    }
}

inline llvm::Value* JotLLVMBackend::create_llvm_integers_comparison(TokenKind op, llvm::Value* left,
                                                                    llvm::Value* right)
{
    switch (op) {
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
        internal_compiler_error("Invalid Integers Comparison operator");
    }
    }
}

inline llvm::Value* JotLLVMBackend::create_llvm_floats_comparison(TokenKind op, llvm::Value* left,
                                                                  llvm::Value* right)
{
    switch (op) {
    case TokenKind::EqualEqual: {
        return Builder.CreateFCmpOEQ(left, right);
    }
    case TokenKind::BangEqual: {
        return Builder.CreateFCmpONE(left, right);
    }
    case TokenKind::Greater: {
        return Builder.CreateFCmpOGT(left, right);
    }
    case TokenKind::GreaterEqual: {
        return Builder.CreateFCmpOGE(left, right);
    }
    case TokenKind::Smaller: {
        return Builder.CreateFCmpOLT(left, right);
    }
    case TokenKind::SmallerEqual: {
        return Builder.CreateFCmpOLE(left, right);
    }
    default: {
        internal_compiler_error("Invalid floats Comparison operator");
    }
    }
}

llvm::Value* JotLLVMBackend::create_llvm_value_increment(std::shared_ptr<Expression> operand,
                                                         bool                        is_prefix)
{
    auto number_type = std::static_pointer_cast<JotNumberType>(operand->get_type_node());
    auto constants_one = llvm_number_value("1", number_type->number_kind);

    std::any right = nullptr;
    if (operand->get_ast_node_type() == AstNodeType::DotExpr) {
        auto dot_expression = std::dynamic_pointer_cast<DotExpression>(operand);
        right = access_struct_member_pointer(dot_expression.get());
    }
    else {
        right = operand->accept(this);
    }

    if (right.type() == typeid(llvm::LoadInst*)) {
        auto current_value = std::any_cast<llvm::LoadInst*>(right);
        auto new_value = create_llvm_integers_bianry(TokenKind::Plus, current_value, constants_one);
        Builder.CreateStore(new_value, current_value->getPointerOperand());
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::AllocaInst*)) {
        auto alloca = std::any_cast<llvm::AllocaInst*>(right);
        auto current_value = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
        auto new_value = create_llvm_integers_bianry(TokenKind::Plus, current_value, constants_one);
        Builder.CreateStore(new_value, alloca);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::GlobalVariable*)) {
        auto global_variable = std::any_cast<llvm::GlobalVariable*>(right);
        auto current_value = Builder.CreateLoad(global_variable->getValueType(), global_variable);
        auto new_value = create_llvm_integers_bianry(TokenKind::Plus, current_value, constants_one);
        Builder.CreateStore(new_value, global_variable);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::Value*)) {
        auto current_value_ptr = std::any_cast<llvm::Value*>(right);
        auto number_llvm_type = llvm_type_from_jot_type(number_type);
        auto current_value = Builder.CreateLoad(number_llvm_type, current_value_ptr);
        auto new_value = create_llvm_integers_bianry(TokenKind::Plus, current_value, constants_one);
        Builder.CreateStore(new_value, current_value_ptr);
        return is_prefix ? new_value : current_value;
    }

    internal_compiler_error("Unary expression with non global or alloca type");
}

llvm::Value* JotLLVMBackend::create_llvm_value_decrement(std::shared_ptr<Expression> operand,
                                                         bool                        is_prefix)
{
    auto number_type = std::static_pointer_cast<JotNumberType>(operand->get_type_node());
    auto constants_one = llvm_number_value("1", number_type->number_kind);

    std::any right = nullptr;
    if (operand->get_ast_node_type() == AstNodeType::DotExpr) {
        auto dot_expression = std::dynamic_pointer_cast<DotExpression>(operand);
        right = access_struct_member_pointer(dot_expression.get());
    }
    else {
        right = operand->accept(this);
    }

    if (right.type() == typeid(llvm::LoadInst*)) {
        auto current_value = std::any_cast<llvm::LoadInst*>(right);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::Minus, current_value, constants_one);
        Builder.CreateStore(new_value, current_value->getPointerOperand());
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::AllocaInst*)) {
        auto alloca = std::any_cast<llvm::AllocaInst*>(right);
        auto current_value = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::Minus, current_value, constants_one);
        Builder.CreateStore(new_value, alloca);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::GlobalVariable*)) {
        auto global_variable = std::any_cast<llvm::GlobalVariable*>(right);
        auto current_value = Builder.CreateLoad(global_variable->getValueType(), global_variable);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::Minus, current_value, constants_one);
        Builder.CreateStore(new_value, global_variable);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::Value*)) {
        auto current_value_ptr = std::any_cast<llvm::Value*>(right);
        auto number_llvm_type = llvm_type_from_jot_type(number_type);
        auto current_value = Builder.CreateLoad(number_llvm_type, current_value_ptr);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::Minus, current_value, constants_one);
        Builder.CreateStore(new_value, current_value_ptr);
        return is_prefix ? new_value : current_value;
    }

    internal_compiler_error("Unary expression with non global or alloca type");
}

llvm::Value* JotLLVMBackend::access_struct_member_pointer(DotExpression* expression)
{
    auto callee = expression->get_callee();
    auto callee_value = llvm_node_value(callee->accept(this));
    auto callee_llvm_type = llvm_type_from_jot_type(callee->get_type_node());

    // Indices to access structure member
    std::vector<llvm::Value*> indices(2);
    indices[0] = zero_int32_value;
    indices[1] = llvm_number_value(std::to_string(expression->field_index), NumberKind::Integer32);

    // Access struct member allocaed on the stack or derefernecs from pointer
    // struct.member or (*struct).member
    if (callee_llvm_type->isStructTy()) {
        // Return a pointer to struct member
        return Builder.CreateGEP(callee_llvm_type, callee_value, indices);
    }

    // Syntax sugger for accessing struct member from pointer to struct, like -> operator in c
    if (callee_llvm_type->isPointerTy()) {
        // Struct type used it to access member from it
        auto struct_type = callee_llvm_type->getPointerElementType();
        assert(struct_type->isStructTy());
        // Auto Dereferencing the struct pointer
        auto struct_value = derefernecs_llvm_pointer(callee_value);
        // Return a pointer to struct member
        return Builder.CreateGEP(struct_type, struct_value, indices);
    }

    internal_compiler_error("Invalid callee type in access_struct_member_pointer");
}

inline llvm::Value* JotLLVMBackend::derefernecs_llvm_pointer(llvm::Value* pointer)
{
    assert(pointer->getType()->isPointerTy());
    auto pointer_element_type = pointer->getType()->getPointerElementType();
    return Builder.CreateLoad(pointer_element_type, pointer);
}

llvm::Constant* JotLLVMBackend::resolve_constant_expression(FieldDeclaration* node)
{
    auto value = node->get_value();
    auto field_type = node->get_type();

    // If there are no value, return default value
    if (value == nullptr) {
        return llvm::dyn_cast<llvm::Constant>(llvm_type_null_value(field_type));
    }

    // If right value is index expression resolve it and return constant value
    if (value->get_ast_node_type() == AstNodeType::IndexExpr) {
        auto index_expression = std::dynamic_pointer_cast<IndexExpression>(value);
        return resolve_constant_index_expression(index_expression);
    }

    // If right value is if expression, resolve it to constant value
    if (value->get_ast_node_type() == AstNodeType::IfExpr) {
        auto if_expression = std::dynamic_pointer_cast<IfExpression>(value);
        return resolve_constant_if_expression(if_expression);
    }

    // Resolve non index constants value
    auto llvm_value = llvm_resolve_value(node->get_value()->accept(this));
    return llvm::dyn_cast<llvm::Constant>(llvm_value);
}

llvm::Constant*
JotLLVMBackend::resolve_constant_index_expression(std::shared_ptr<IndexExpression> expression)
{
    auto llvm_array = llvm_node_value(expression->get_value()->accept(this));
    auto index_value = expression->get_index()->accept(this);
    auto constants_index = llvm::dyn_cast<llvm::ConstantInt>(llvm_node_value(index_value));

    if (auto global_variable_array = llvm::dyn_cast<llvm::GlobalVariable>(llvm_array)) {
        auto initalizer = global_variable_array->getInitializer();

        // Index expression for array with constants data types such as integers, floats
        if (auto data_array = llvm::dyn_cast<llvm::ConstantDataArray>(initalizer)) {
            return data_array->getElementAsConstant(constants_index->getLimitedValue());
        }

        // Index expression for array with non constants data types
        if (auto constants_array = llvm::dyn_cast<llvm::ConstantArray>(initalizer)) {
            return constants_array->getAggregateElement(constants_index);
        }

        if (auto constants_array = llvm::dyn_cast<llvm::Constant>(initalizer)) {
            return constants_array->getAggregateElement(constants_index);
        }
    }

    if (auto data_array = llvm::dyn_cast<llvm::ConstantDataArray>(llvm_array)) {
        return data_array->getElementAsConstant(constants_index->getLimitedValue());
    }

    if (auto constants_array = llvm::dyn_cast<llvm::ConstantArray>(llvm_array)) {
        return constants_array->getAggregateElement(constants_index);
    }

    if (auto constants_array = llvm::dyn_cast<llvm::Constant>(llvm_array)) {
        return constants_array->getAggregateElement(constants_index);
    }

    internal_compiler_error("Invalid type in resolve_global_index_expression");
}

llvm::Constant*
JotLLVMBackend::resolve_constant_if_expression(std::shared_ptr<IfExpression> expression)
{
    auto condition = llvm_resolve_value(expression->get_condition()->accept(this));
    auto constant_condition = llvm::dyn_cast<llvm::Constant>(condition);
    if (constant_condition->isZeroValue()) {
        return llvm::dyn_cast<llvm::Constant>(
            llvm_resolve_value(expression->get_else_value()->accept(this)));
    }
    return llvm::dyn_cast<llvm::Constant>(
        llvm_resolve_value(expression->get_if_value()->accept(this)));
}

llvm::Constant*
JotLLVMBackend::resolve_constant_switch_expression(std::shared_ptr<SwitchExpression> expression)
{
    auto argument = llvm_resolve_value(expression->get_argument()->accept(this));
    auto constant_argument = llvm::dyn_cast<llvm::Constant>(argument);
    auto switch_cases = expression->get_switch_cases();
    auto cases_size = switch_cases.size();
    for (size_t i = 0; i < cases_size; i++) {
        auto switch_case = switch_cases[i];
        auto case_value = llvm_resolve_value(switch_case->accept(this));
        auto constant_case = llvm::dyn_cast<llvm::Constant>(case_value);
        if (constant_argument == constant_case) {
            auto value = llvm_resolve_value(expression->get_switch_cases_values()[i]->accept(this));
            return llvm::dyn_cast<llvm::Constant>(value);
        }
    }
    auto default_value = llvm_resolve_value(expression->get_default_case_value()->accept(this));
    return llvm::dyn_cast<llvm::Constant>(default_value);
}

llvm::Constant* JotLLVMBackend::resolve_constant_string_expression(const std::string& literal)
{
    // Resolve constants string from string pool if it generated before
    if (constants_string_pool.count(literal)) {
        return constants_string_pool[literal];
    }

    auto                         size = literal.size();
    auto                         length = size + 1;
    std::vector<llvm::Constant*> characters(length);
    for (unsigned int i = 0; i < size; i++) {
        characters[i] = llvm::ConstantInt::get(llvm_int8_type, literal[i]);
    }
    // Add '\0' at the end of string literal
    characters[size] = llvm::ConstantInt::get(llvm_int8_type, 0);
    auto array_type = llvm::ArrayType::get(llvm_int8_type, length);
    auto init = llvm::ConstantArray::get(array_type, characters);
    auto variable = new llvm::GlobalVariable(*llvm_module, init->getType(), true,
                                             llvm::GlobalVariable::ExternalLinkage, init, literal);
    auto string = llvm::ConstantExpr::getBitCast(variable, llvm_int8_type->getPointerTo());
    // define the constants string in the constants pool
    constants_string_pool[literal] = string;
    return string;
}

inline llvm::AllocaInst* JotLLVMBackend::create_entry_block_alloca(llvm::Function*   function,
                                                                   const std::string var_name,
                                                                   llvm::Type*       type)
{
    llvm::IRBuilder<> builder_object(&function->getEntryBlock(), function->getEntryBlock().begin());
    return builder_object.CreateAlloca(type, 0, var_name.c_str());
}

void JotLLVMBackend::create_switch_case_branch(llvm::SwitchInst*           switch_inst,
                                               llvm::Function*             current_function,
                                               llvm::BasicBlock*           basic_block,
                                               std::shared_ptr<SwitchCase> switch_case)
{
    auto branch_block = llvm::BasicBlock::Create(llvm_context, "", current_function);
    Builder.SetInsertPoint(branch_block);

    bool body_has_return_statement = false;

    auto branch_body = switch_case->get_body();

    // If switch body is block, check if the last node is return statement or not,
    // if it not block, check if it return statement or not
    if (branch_body->get_ast_node_type() == AstNodeType::Block) {
        auto block = std::dynamic_pointer_cast<BlockStatement>(branch_body);
        auto nodes = block->get_nodes();
        if (not nodes.empty()) {
            body_has_return_statement = nodes.back()->get_ast_node_type() == AstNodeType::Return;
        }
    }
    else {
        body_has_return_statement = branch_body->get_ast_node_type() == AstNodeType::Return;
    }

    // Visit the case branch in sub scope
    push_alloca_inst_scope();
    branch_body->accept(this);
    pop_alloca_inst_scope();

    // Create branch only if current block hasn't return node
    if (not body_has_return_statement) {
        Builder.CreateBr(basic_block);
    }

    // Normal switch case branch with value and body
    auto switch_case_values = switch_case->get_values();
    if (not switch_case_values.empty()) {
        // Map all values for this case with single branch block
        for (auto& switch_case_value : switch_case_values) {
            auto value = llvm_node_value(switch_case_value->accept(this));
            auto integer_value = llvm::dyn_cast<llvm::ConstantInt>(value);
            switch_inst->addCase(integer_value, branch_block);
        }
        return;
    }

    // Default switch case branch with body and no value
    switch_inst->setDefaultDest(branch_block);
}

llvm::Function* JotLLVMBackend::lookup_function(std::string name)
{
    if (auto function = llvm_module->getFunction(name)) {
        return function;
    }

    auto function_prototype = functions_table.find(name);
    if (function_prototype != functions_table.end()) {
        return std::any_cast<llvm::Function*>(function_prototype->second->accept(this));
    }

    return llvm_functions[name];
}

inline bool JotLLVMBackend::is_global_block() { return Builder.GetInsertBlock() == nullptr; }

inline void JotLLVMBackend::execute_defer_calls()
{
    for (auto& defer_call : defer_calls_stack) {
        defer_call->generate_call();
    }
}

inline void JotLLVMBackend::clear_defer_calls_stack() { defer_calls_stack.clear(); }

inline void JotLLVMBackend::push_alloca_inst_scope()
{
    alloca_inst_scope = std::make_shared<JotSymbolTable>(alloca_inst_scope);
}

inline void JotLLVMBackend::pop_alloca_inst_scope()
{
    alloca_inst_scope = alloca_inst_scope->get_parent_symbol_table();
}

inline void JotLLVMBackend::internal_compiler_error(const char* message)
{
    jot::loge << "Internal Compiler Error: " << message << '\n';
    exit(EXIT_FAILURE);
}