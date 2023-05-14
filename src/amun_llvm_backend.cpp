#include "../include/amun_llvm_backend.hpp"
#include "../include/amun_ast_visitor.hpp"
#include "../include/amun_llvm_builder.hpp"
#include "../include/amun_llvm_intrinsic.hpp"
#include "../include/amun_logger.hpp"
#include "../include/amun_name_mangle.hpp"
#include "../include/amun_type.hpp"

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
#include <vector>

auto amun::LLVMBackend::compile(std::string module_name,
                                std::shared_ptr<CompilationUnit> compilation_unit)
    -> std::unique_ptr<llvm::Module>
{
    llvm_module = std::make_unique<llvm::Module>(module_name, llvm_context);
    try {
        for (const auto& statement : compilation_unit->tree_nodes) {
            statement->accept(this);
        }
    }
    catch (...) {
        amun::loge << "LLVM Backend Exception \n";
    }
    return std::move(llvm_module);
}

auto amun::LLVMBackend::visit(BlockStatement* node) -> std::any
{
    push_alloca_inst_scope();
    defer_calls_stack.top().push_new_scope();
    bool should_execute_defers = true;
    for (const auto& statement : node->statements) {
        const auto ast_node_type = statement->get_ast_node_type();
        if (ast_node_type == AstNodeType::AST_RETURN) {
            should_execute_defers = false;
            execute_all_defer_calls();
        }

        statement->accept(this);

        // In the same block there are no needs to generate code for unreachable code
        if (ast_node_type == AstNodeType::AST_RETURN or ast_node_type == AstNodeType::AST_BREAK or
            ast_node_type == AstNodeType::AST_CONTINUE) {
            break;
        }
    }

    if (should_execute_defers) {
        execute_scope_defer_calls();
    }

    defer_calls_stack.top().pop_current_scope();
    pop_alloca_inst_scope();
    return 0;
}

auto amun::LLVMBackend::visit(FieldDeclaration* node) -> std::any
{
    auto var_name = node->name.literal;
    auto field_type = node->type;
    if (field_type->type_kind == amun::TypeKind::GENERIC_PARAMETER) {
        auto generic = std::static_pointer_cast<amun::GenericParameterType>(field_type);
        field_type = generic_types[generic->name];
    }

    llvm::Type* llvm_type;
    if (field_type->type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto generic_type = std::static_pointer_cast<amun::GenericStructType>(field_type);
        llvm_type = resolve_generic_struct(generic_type);
    }
    else {
        llvm_type = llvm_type_from_amun_type(field_type);
    }

    // Globals code generation block can be moved into other function to be clear and handle more
    // cases and to handle also soem compile time evaluations
    if (node->is_global) {
        // if field has initalizer evaluate it, else initalize it with default value
        llvm::Constant* constants_value;
        if (node->value == nullptr) {
            constants_value = create_llvm_null(llvm_type_from_amun_type(field_type));
        }
        else {
            constants_value = resolve_constant_expression(node->value);
        }

        auto global_variable =
            new llvm::GlobalVariable(*llvm_module, llvm_type, false,
                                     llvm::GlobalValue::ExternalLinkage, constants_value, var_name);
        global_variable->setAlignment(llvm::MaybeAlign(0));
        return 0;
    }

    // if field has initalizer evaluate it, else initalize it with default value
    std::any value;
    if (node->value == nullptr) {
        value = create_llvm_null(llvm_type_from_amun_type(field_type));
    }
    else {
        value = node->value->accept(this);
    }

    auto current_function = Builder.GetInsertBlock()->getParent();
    if (value.type() == typeid(llvm::Value*)) {
        auto init_value = std::any_cast<llvm::Value*>(value);
        auto init_value_type = init_value->getType();

        // This case if you assign derefernced variable for example
        // Case in C Language
        // int* ptr = (int*) malloc(sizeof(int));
        // int value = *ptr;
        // Clang compiler emit load instruction twice to resolve this problem
        if (init_value_type != llvm_type && init_value_type->getPointerElementType() == llvm_type) {
            init_value = derefernecs_llvm_pointer(init_value);
        }

        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::CallInst*)) {
        auto init_value = std::any_cast<llvm::CallInst*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(init_value, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::AllocaInst*)) {
        auto init_value = std::any_cast<llvm::AllocaInst*>(value);
        Builder.CreateLoad(init_value->getAllocatedType(), init_value, var_name);
        alloca_inst_table.define(var_name, init_value);
    }
    else if (value.type() == typeid(llvm::Constant*)) {
        auto constant = std::any_cast<llvm::Constant*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(constant, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::ConstantInt*)) {
        auto constant = std::any_cast<llvm::ConstantInt*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(constant, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::LoadInst*)) {
        auto load_inst = std::any_cast<llvm::LoadInst*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(load_inst, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::PHINode*)) {
        auto node = std::any_cast<llvm::PHINode*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(node, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::Function*)) {
        auto node = std::any_cast<llvm::Function*>(value);
        llvm_functions[var_name] = node;
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, node->getType());
        Builder.CreateStore(node, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else if (value.type() == typeid(llvm::UndefValue*)) {
        auto undefined = std::any_cast<llvm::UndefValue*>(value);
        auto alloc_inst = create_entry_block_alloca(current_function, var_name, llvm_type);
        Builder.CreateStore(undefined, alloc_inst);
        alloca_inst_table.define(var_name, alloc_inst);
    }
    else {
        internal_compiler_error("Un supported rvalue for field declaration");
    }
    return 0;
}

auto amun::LLVMBackend::visit(ConstDeclaration* node) -> std::any { return 0; }

auto amun::LLVMBackend::visit(FunctionPrototype* node) -> std::any
{
    auto parameters = node->parameters;
    size_t parameters_size = parameters.size();
    std::vector<llvm::Type*> arguments(parameters_size);
    for (size_t i = 0; i < parameters_size; i++) {
        arguments[i] = llvm_type_from_amun_type(parameters[i]->type);
    }

    auto return_type = llvm_type_from_amun_type(node->return_type);
    auto function_type = llvm::FunctionType::get(return_type, arguments, node->has_varargs);
    auto function_name = node->name.literal;
    auto linkage = node->is_external || function_name == "main" ? llvm::Function::ExternalLinkage
                                                                : llvm::Function::InternalLinkage;

    auto function = llvm::Function::Create(function_type, linkage, function_name, nullptr);
    llvm_module->getFunctionList().push_back(function);

    unsigned index = 0;
    for (auto& argument : function->args()) {
        if (index >= parameters_size) {
            // Varargs case
            break;
        }
        argument.setName(parameters[index++]->name.literal);
    }

    return function;
}

auto amun::LLVMBackend::visit(IntrinsicPrototype* node) -> std::any
{
    auto name = node->name.literal;
    auto prototype_parameters = node->parameters;

    std::vector<llvm::Type*> parameters_types;
    parameters_types.reserve(prototype_parameters.size());
    for (const auto& parameters : prototype_parameters) {
        parameters_types.push_back(llvm_type_from_amun_type(parameters->type));
    }

    auto native_name = node->native_name;

    if (!llvm_intrinsics_map.contains(native_name)) {
        internal_compiler_error("Trying to call unkown intrinsic function");
    }

    auto intrinsic_id = llvm_intrinsics_map[native_name];

    auto* function =
        llvm::Intrinsic::getDeclaration(llvm_module.get(), intrinsic_id, parameters_types);

    llvm_functions[name] = function;

    return function;
}

auto amun::LLVMBackend::resolve_generic_function(FunctionDeclaration* node,
                                                 std::vector<Shared<amun::Type>> generic_parameters)
    -> llvm::Function*
{

    is_on_global_scope = false;
    auto prototype = node->prototype;
    auto name = prototype->name.literal;
    auto mangled_name = name + mangle_types(generic_parameters);

    if (alloca_inst_table.is_defined(mangled_name)) {
        auto value = alloca_inst_table.lookup(mangled_name);
        return std::any_cast<llvm::Function*>(value);
    }

    // Resolve Generic Parameters
    int generic_parameter_index = 0;
    for (const auto& parameter : prototype->generic_parameters) {
        generic_types[parameter] = generic_parameters[generic_parameter_index++];
    }

    auto return_type = prototype->return_type;
    if (prototype->return_type->type_kind == amun::TypeKind::GENERIC_PARAMETER) {
        auto generic_type = std::static_pointer_cast<amun::GenericParameterType>(return_type);
        return_type = generic_types[generic_type->name];
    }

    std::vector<llvm::Type*> arguments;
    for (const auto& parameter : prototype->parameters) {
        if (parameter->type->type_kind == amun::TypeKind::GENERIC_PARAMETER) {
            auto generic_type =
                std::static_pointer_cast<amun::GenericParameterType>(parameter->type);
            arguments.push_back(llvm_type_from_amun_type(generic_types[generic_type->name]));
        }
        else {
            arguments.push_back(llvm_type_from_amun_type(parameter->type));
        }
    }

    functions_table[mangled_name] = prototype;

    auto linkage =
        name == "main" ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;

    auto function_type =
        llvm::FunctionType::get(llvm_type_from_amun_type(return_type), arguments, false);

    auto previous_insert_block = Builder.GetInsertBlock();

    auto function = llvm::Function::Create(function_type, linkage, mangled_name, nullptr);
    llvm_module->getFunctionList().push_back(function);

    unsigned index = 0;
    for (auto& argument : function->args()) {
        if (index >= prototype->parameters.size()) {
            // Varargs case
            break;
        }
        argument.setName(prototype->parameters[index++]->name.literal);
    }

    auto entry_block = llvm::BasicBlock::Create(llvm_context, "entry", function);
    Builder.SetInsertPoint(entry_block);

    defer_calls_stack.push({});
    push_alloca_inst_scope();

    for (auto& arg : function->args()) {
        const std::string arg_name_str = std::string(arg.getName());
        auto* alloca_inst = create_entry_block_alloca(function, arg_name_str, arg.getType());
        alloca_inst_table.define(arg_name_str, alloca_inst);
        Builder.CreateStore(&arg, alloca_inst);
    }

    const auto& body = node->body;
    body->accept(this);

    pop_alloca_inst_scope();
    defer_calls_stack.pop();

    alloca_inst_table.define(mangled_name, function);

    // Assert that this block end with return statement or unreachable
    if (body->get_ast_node_type() == AstNodeType::AST_BLOCK) {
        const auto& body_statement = std::dynamic_pointer_cast<BlockStatement>(body);
        const auto& statements = body_statement->statements;
        if (statements.empty() ||
            statements.back()->get_ast_node_type() != AstNodeType::AST_RETURN) {
            Builder.CreateUnreachable();
        }
    }

    verifyFunction(*function);

    has_return_statement = false;
    is_on_global_scope = true;

    Builder.SetInsertPoint(previous_insert_block);

    generic_types.clear();
    return function;
}

auto amun::LLVMBackend::visit(FunctionDeclaration* node) -> std::any
{
    is_on_global_scope = false;
    auto prototype = node->prototype;
    auto name = prototype->name.literal;
    if (prototype->is_generic) {
        functions_declaraions[name] = node;
        return 0;
    }

    functions_table[name] = prototype;

    auto function = std::any_cast<llvm::Function*>(prototype->accept(this));
    auto entry_block = llvm::BasicBlock::Create(llvm_context, "entry", function);
    Builder.SetInsertPoint(entry_block);

    defer_calls_stack.push({});
    push_alloca_inst_scope();
    for (auto& arg : function->args()) {
        const std::string arg_name_str = std::string(arg.getName());
        auto* alloca_inst = create_entry_block_alloca(function, arg_name_str, arg.getType());
        alloca_inst_table.define(arg_name_str, alloca_inst);
        Builder.CreateStore(&arg, alloca_inst);
    }

    const auto& body = node->body;
    body->accept(this);

    pop_alloca_inst_scope();
    defer_calls_stack.pop();

    alloca_inst_table.define(name, function);

    // Assert that this block end with return statement or unreachable
    if (body->get_ast_node_type() == AstNodeType::AST_BLOCK) {
        const auto& body_statement = std::dynamic_pointer_cast<BlockStatement>(body);
        const auto& statements = body_statement->statements;
        if (statements.empty() ||
            statements.back()->get_ast_node_type() != AstNodeType::AST_RETURN) {
            Builder.CreateUnreachable();
        }
    }

    verifyFunction(*function);

    has_return_statement = false;
    is_on_global_scope = true;
    return function;
}

auto amun::LLVMBackend::visit(OperatorFunctionDeclaraion* node) -> std::any
{
    return node->function->accept(this);
}

auto amun::LLVMBackend::visit(StructDeclaration* node) -> std::any
{
    const auto struct_type = node->struct_type;

    // Generic Struct is a template and should be defined only when used
    if (struct_type->is_generic) {
        return 0;
    }

    const auto struct_name = struct_type->name;
    return create_llvm_struct_type(struct_name, struct_type->fields_types, struct_type->is_packed,
                                   struct_type->is_extern);
    return 0;
}

auto amun::LLVMBackend::visit([[maybe_unused]] EnumDeclaration* node) -> std::any
{
    // Enumeration type is only compile time type, no need to generate any IR for it
    return 0;
}

auto amun::LLVMBackend::visit(IfStatement* node) -> std::any
{
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto start_block = llvm::BasicBlock::Create(llvm_context, "if.start");
    auto end_block = llvm::BasicBlock::Create(llvm_context, "if.end");

    Builder.CreateBr(start_block);
    current_function->getBasicBlockList().push_back(start_block);
    Builder.SetInsertPoint(start_block);

    auto conditional_blocks = node->conditional_blocks;
    auto conditional_blocks_size = conditional_blocks.size();
    for (unsigned long i = 0; i < conditional_blocks_size; i++) {
        auto true_block = llvm::BasicBlock::Create(llvm_context, "if.true");
        current_function->getBasicBlockList().push_back(true_block);

        auto false_branch = end_block;
        if (i + 1 < conditional_blocks_size) {
            false_branch = llvm::BasicBlock::Create(llvm_context, "if.false");
            current_function->getBasicBlockList().push_back(false_branch);
        }

        auto condition = llvm_resolve_value(conditional_blocks[i]->condition->accept(this));
        Builder.CreateCondBr(condition, true_block, false_branch);
        Builder.SetInsertPoint(true_block);

        push_alloca_inst_scope();
        conditional_blocks[i]->body->accept(this);
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

auto amun::LLVMBackend::visit(ForRangeStatement* node) -> std::any
{
    auto start = llvm_resolve_value(node->range_start->accept(this));
    auto end = llvm_resolve_value(node->range_end->accept(this));

    llvm::Value* step = nullptr;
    if (node->step) {
        // Resolve user declared step
        step = llvm_resolve_value(node->step->accept(this));
    }
    else {
        // Default step is 1 with number type as the same as range start
        auto node_type = node->range_start->get_type_node();
        auto number_type = std::static_pointer_cast<amun::NumberType>(node_type);
        step = llvm_number_value("1", number_type->number_kind);
    }

    start = create_llvm_numbers_bianry(TokenKind::TOKEN_MINUS, start, step);
    end = create_llvm_numbers_bianry(TokenKind::TOKEN_MINUS, end, step);

    const auto element_llvm_type = start->getType();

    auto condition_block = llvm::BasicBlock::Create(llvm_context, "for.cond");
    auto body_block = llvm::BasicBlock::Create(llvm_context, "for");
    auto end_block = llvm::BasicBlock::Create(llvm_context, "for.end");

    break_blocks_stack.push(end_block);
    continue_blocks_stack.push(condition_block);

    push_alloca_inst_scope();

    // Declare iterator name variable
    const auto var_name = node->element_name;
    const auto current_function = Builder.GetInsertBlock()->getParent();
    auto alloc_inst = create_entry_block_alloca(current_function, var_name, element_llvm_type);
    Builder.CreateStore(start, alloc_inst);
    alloca_inst_table.define(var_name, alloc_inst);
    Builder.CreateBr(condition_block);

    // Generate condition block
    current_function->getBasicBlockList().push_back(condition_block);
    Builder.SetInsertPoint(condition_block);
    auto variable = derefernecs_llvm_pointer(alloc_inst);
    auto condition = create_llvm_numbers_comparison(TokenKind::TOKEN_SMALLER_EQUAL, variable, end);
    Builder.CreateCondBr(condition, body_block, end_block);

    // Generate For body IR Code
    current_function->getBasicBlockList().push_back(body_block);
    Builder.SetInsertPoint(body_block);

    // Increment loop variable
    auto value_ptr = Builder.CreateLoad(alloc_inst->getAllocatedType(), alloc_inst);
    auto new_value = create_llvm_numbers_bianry(TokenKind::TOKEN_PLUS, value_ptr, step);
    Builder.CreateStore(new_value, alloc_inst);

    node->body->accept(this);
    pop_alloca_inst_scope();

    if (has_break_or_continue_statement) {
        has_break_or_continue_statement = false;
    }
    else {
        Builder.CreateBr(condition_block);
    }

    // Set the insertion point to the end block
    current_function->getBasicBlockList().push_back(end_block);
    Builder.SetInsertPoint(end_block);

    break_blocks_stack.pop();
    continue_blocks_stack.pop();

    return 0;
}

auto amun::LLVMBackend::visit(ForEachStatement* node) -> std::any
{
    auto collection_expression = node->collection;
    auto collection_exp_type = collection_expression->get_type_node();
    auto collection_value = llvm_node_value(collection_expression->accept(this));
    auto collection = llvm_resolve_value(collection_value);
    auto collection_type = collection->getType();

    // If collection type is fixed size array with zero length, no need to generate for each
    if (collection_type->isArrayTy() && collection_type->getArrayNumElements() == 0) {
        return 0;
    }

    auto is_foreach_string = amun::is_types_equals(collection_exp_type, amun::i8_ptr_type);

    auto zero_value = create_llvm_int64(-1, true);
    auto step = create_llvm_int64(1, true);

    // Get length of collections depending on type
    auto length = is_foreach_string
                      ? create_llvm_string_length(collection)
                      : create_llvm_int64(collection_type->getArrayNumElements(), true);

    auto end = create_llvm_integers_bianry(TokenKind::TOKEN_MINUS, length, step);
    auto condition_block = llvm::BasicBlock::Create(llvm_context, "for.cond");
    auto body_block = llvm::BasicBlock::Create(llvm_context, "for");
    auto end_block = llvm::BasicBlock::Create(llvm_context, "for.end");

    break_blocks_stack.push(end_block);
    continue_blocks_stack.push(condition_block);

    push_alloca_inst_scope();

    // Resolve it_index
    const auto index_name = node->index_name;
    const auto current_function = Builder.GetInsertBlock()->getParent();
    auto index_alloca = create_entry_block_alloca(current_function, index_name, llvm_int64_type);
    Builder.CreateStore(zero_value, index_alloca);
    alloca_inst_table.define(index_name, index_alloca);

    // Resolve element name to be collection[it_index]
    llvm::AllocaInst* element_alloca;
    const auto element_name = node->element_name;
    auto element_type = is_foreach_string ? llvm_int8_type : collection_type->getArrayElementType();
    if (node->element_name != "_") {
        element_alloca = create_entry_block_alloca(current_function, element_name, element_type);
    }

    Builder.CreateBr(condition_block);

    // Generate condition block
    current_function->getBasicBlockList().push_back(condition_block);
    Builder.SetInsertPoint(condition_block);
    auto variable = derefernecs_llvm_pointer(index_alloca);
    auto condition = create_llvm_numbers_comparison(TokenKind::TOKEN_SMALLER, variable, end);
    Builder.CreateCondBr(condition, body_block, end_block);

    // Generate For body IR Code
    current_function->getBasicBlockList().push_back(body_block);
    Builder.SetInsertPoint(body_block);

    // Increment loop variable
    auto value_ptr = Builder.CreateLoad(index_alloca->getAllocatedType(), index_alloca);
    auto new_value = create_llvm_numbers_bianry(TokenKind::TOKEN_PLUS, value_ptr, step);
    Builder.CreateStore(new_value, index_alloca);

    // If array expression is passed directly we should first save it on temp variable
    if (node->collection->get_ast_node_type() == AstNodeType::AST_ARRAY) {
        auto temp_name = "_temp";
        auto temp_alloca = create_entry_block_alloca(current_function, temp_name, collection_type);
        Builder.CreateStore(collection, temp_alloca);
        alloca_inst_table.define(temp_name, temp_alloca);

        auto location = TokenSpan();
        auto token = Token{TokenKind::TOKEN_IDENTIFIER, location, temp_name};
        collection_expression = std::make_shared<LiteralExpression>(token);
        collection_expression->set_type_node(node->collection->get_type_node());
    }

    // Update it variable with the element in the current index
    if (element_name != "_") {
        auto current_index = derefernecs_llvm_pointer(index_alloca);
        auto value = access_array_element(collection_expression, current_index);
        Builder.CreateStore(value, element_alloca);
        alloca_inst_table.define(element_name, element_alloca);
    }

    node->body->accept(this);
    pop_alloca_inst_scope();

    if (has_break_or_continue_statement) {
        has_break_or_continue_statement = false;
    }
    else {
        Builder.CreateBr(condition_block);
    }

    // Set the insertion point to the end block
    current_function->getBasicBlockList().push_back(end_block);
    Builder.SetInsertPoint(end_block);

    break_blocks_stack.pop();
    continue_blocks_stack.pop();

    return 0;
}

auto amun::LLVMBackend::visit(ForeverStatement* node) -> std::any
{
    auto body_block = llvm::BasicBlock::Create(llvm_context, "forever");
    auto end_block = llvm::BasicBlock::Create(llvm_context, "forever.end");

    break_blocks_stack.push(end_block);
    continue_blocks_stack.push(body_block);

    push_alloca_inst_scope();

    const auto current_function = Builder.GetInsertBlock()->getParent();

    Builder.CreateBr(body_block);

    // Generate For body IR Code
    current_function->getBasicBlockList().push_back(body_block);
    Builder.SetInsertPoint(body_block);

    node->body->accept(this);
    pop_alloca_inst_scope();

    if (has_break_or_continue_statement) {
        has_break_or_continue_statement = false;
    }
    else {
        Builder.CreateBr(body_block);
    }

    // Set the insertion point to the end block
    current_function->getBasicBlockList().push_back(end_block);
    Builder.SetInsertPoint(end_block);

    break_blocks_stack.pop();
    continue_blocks_stack.pop();

    return 0;
}

auto amun::LLVMBackend::visit(WhileStatement* node) -> std::any
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

    auto condition = llvm_node_value(node->condition->accept(this));
    Builder.CreateCondBr(condition, loop_branch, end_branch);

    current_function->getBasicBlockList().push_back(loop_branch);
    Builder.SetInsertPoint(loop_branch);

    push_alloca_inst_scope();
    node->body->accept(this);
    pop_alloca_inst_scope();

    if (has_break_or_continue_statement) {
        has_break_or_continue_statement = false;
    }
    else {
        Builder.CreateBr(condition_branch);
    }

    current_function->getBasicBlockList().push_back(end_branch);
    Builder.SetInsertPoint(end_branch);

    break_blocks_stack.pop();
    continue_blocks_stack.pop();

    return 0;
}

auto amun::LLVMBackend::visit(SwitchStatement* node) -> std::any
{
    auto current_function = Builder.GetInsertBlock()->getParent();
    auto argument = node->argument;
    auto llvm_value = llvm_resolve_value(argument->accept(this));
    auto basic_block = llvm::BasicBlock::Create(llvm_context, "", current_function);
    auto switch_inst = Builder.CreateSwitch(llvm_value, basic_block);

    auto switch_cases = node->cases;
    auto switch_cases_size = switch_cases.size();

    // Generate code for each switch case
    for (size_t i = 0; i < switch_cases_size; i++) {
        auto switch_case = switch_cases[i];
        create_switch_case_branch(switch_inst, current_function, basic_block, switch_case);
    }

    // Generate code for default cases is exists
    auto default_branch = node->default_case;
    if (default_branch) {
        create_switch_case_branch(switch_inst, current_function, basic_block, default_branch);
    }

    Builder.SetInsertPoint(basic_block);
    return 0;
}

auto amun::LLVMBackend::visit(ReturnStatement* node) -> std::any
{
    has_return_statement = true;

    // If node has no value that mean it will return void
    if (!node->has_value) {
        return Builder.CreateRetVoid();
    }

    auto value = node->value->accept(this);

    if (value.type() == typeid(llvm::Value*)) {
        auto return_value = std::any_cast<llvm::Value*>(value);
        return Builder.CreateRet(return_value);
    }

    if (value.type() == typeid(llvm::CallInst*)) {
        auto init_value = std::any_cast<llvm::CallInst*>(value);
        return Builder.CreateRet(init_value);
    }

    if (value.type() == typeid(llvm::AllocaInst*)) {
        auto init_value = std::any_cast<llvm::AllocaInst*>(value);
        auto value_litearl = Builder.CreateLoad(init_value->getAllocatedType(), init_value);
        return Builder.CreateRet(value_litearl);
    }

    if (value.type() == typeid(llvm::Function*)) {
        auto node = std::any_cast<llvm::Function*>(value);
        return Builder.CreateRet(node);
    }

    if (value.type() == typeid(llvm::Constant*)) {
        auto init_value = std::any_cast<llvm::Constant*>(value);
        return Builder.CreateRet(init_value);
    }

    if (value.type() == typeid(llvm::ConstantInt*)) {
        auto init_value = std::any_cast<llvm::ConstantInt*>(value);
        return Builder.CreateRet(init_value);
    }

    if (value.type() == typeid(llvm::LoadInst*)) {
        auto load_inst = std::any_cast<llvm::LoadInst*>(value);
        return Builder.CreateRet(load_inst);
    }

    if (value.type() == typeid(llvm::GlobalVariable*)) {
        auto variable = std::any_cast<llvm::GlobalVariable*>(value);
        auto value = Builder.CreateLoad(variable->getValueType(), variable);
        return Builder.CreateRet(value);
    }

    // Used when use return node is if or switch expression
    if (value.type() == typeid(llvm::PHINode*)) {
        auto phi = std::any_cast<llvm::PHINode*>(value);
        auto expected_type = node->value->get_type_node();
        auto expected_llvm_type = llvm_type_from_amun_type(expected_type);

        // Return type from PHI node is primitives and no need for derefernece
        if (phi->getType() == expected_llvm_type) {
            return Builder.CreateRet(phi);
        }
        auto phi_value = derefernecs_llvm_pointer(phi);
        return Builder.CreateRet(phi_value);
    }

    internal_compiler_error("Un expected return type");
}

auto amun::LLVMBackend::visit(DeferStatement* node) -> std::any
{
    auto call_expression = node->call_expression;
    auto callee = std::dynamic_pointer_cast<LiteralExpression>(call_expression->callee);
    auto callee_literal = callee->name.literal;
    auto function = lookup_function(callee_literal);
    if (not function) {
        auto value = llvm_node_value(alloca_inst_table.lookup(callee_literal));
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
            auto loaded = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
            auto function_type = llvm_type_from_amun_type(call_expression->get_type_node());
            if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                auto arguments = call_expression->arguments;
                size_t arguments_size = arguments.size();
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
                auto defer_function_call = std::make_shared<amun::DeferFunctionPtrCall>(
                    function_pointer, loaded, arguments_values);
                defer_calls_stack.top().push_front(defer_function_call);
            }
        }
        return 0;
    }

    auto arguments = call_expression->arguments;
    auto arguments_size = arguments.size();
    auto parameter_size = function->arg_size();
    std::vector<llvm::Value*> arguments_values;
    arguments_values.reserve(arguments_size);
    for (size_t i = 0; i < arguments_size; i++) {
        auto argument = arguments[i];
        auto value = llvm_node_value(argument->accept(this));

        if (i >= parameter_size) {
            if (argument->get_ast_node_type() == AstNodeType::AST_LITERAL) {
                auto argument_type = llvm_type_from_amun_type(argument->get_type_node());
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

    auto defer_function_call =
        std::make_shared<amun::DeferFunctionCall>(function, arguments_values);

    // Inser must be at the begin to simulate stack but in vector to easy traverse and clear
    defer_calls_stack.top().push_front(defer_function_call);
    return 0;
}

auto amun::LLVMBackend::visit(BreakStatement* node) -> std::any
{
    has_break_or_continue_statement = true;

    for (int i = 1; i < node->times; i++) {
        break_blocks_stack.pop();
    }

    Builder.CreateBr(break_blocks_stack.top());
    return 0;
}

auto amun::LLVMBackend::visit(ContinueStatement* node) -> std::any
{
    has_break_or_continue_statement = true;

    for (int i = 1; i < node->times; i++) {
        continue_blocks_stack.pop();
    }

    Builder.CreateBr(continue_blocks_stack.top());
    return 0;
}

auto amun::LLVMBackend::visit(ExpressionStatement* node) -> std::any
{
    node->expression->accept(this);
    return 0;
}

auto amun::LLVMBackend::visit(IfExpression* node) -> std::any
{
    // If it constant, we can resolve it at Compile time
    if (is_global_block() && node->is_constant()) {
        return resolve_constant_if_expression(std::make_shared<IfExpression>(*node));
    }

    auto function = Builder.GetInsertBlock()->getParent();

    auto condition = llvm_resolve_value(node->condition->accept(this));

    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(llvm_context, "then", function);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(llvm_context, "else");
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(llvm_context, "ifcont");
    Builder.CreateCondBr(condition, thenBB, elseBB);

    Builder.SetInsertPoint(thenBB);
    auto then_value = llvm_node_value(node->if_expression->accept(this));

    Builder.CreateBr(mergeBB);
    thenBB = Builder.GetInsertBlock();

    function->getBasicBlockList().push_back(elseBB);
    Builder.SetInsertPoint(elseBB);

    auto else_value = llvm_node_value(node->else_expression->accept(this));

    Builder.CreateBr(mergeBB);
    elseBB = Builder.GetInsertBlock();

    function->getBasicBlockList().push_back(mergeBB);
    Builder.SetInsertPoint(mergeBB);

    llvm::PHINode* pn = Builder.CreatePHI(then_value->getType(), 2, "iftmp");
    pn->addIncoming(then_value, thenBB);
    pn->addIncoming(else_value, elseBB);

    return pn;
}

auto amun::LLVMBackend::visit(SwitchExpression* node) -> std::any
{
    // If it constant, we can resolve it at Compile time
    if (is_global_block() && node->is_constant()) {
        return resolve_constant_switch_expression(std::make_shared<SwitchExpression>(*node));
    }

    // Resolve the argument condition
    // In each branch check the equlity between argument and case
    // If they are equal conditional jump to the final branch, else jump to the next branch
    // If the current branch is default case branch, perform un conditional jump to final branch
    auto cases = node->switch_cases;
    auto values = node->switch_cases_values;
    auto else_branch = node->default_value;

    // The number of cases that has a value (not default case)
    auto cases_size = cases.size();

    // The number of all values (cases and default case values)
    auto values_size = cases_size;

    if (else_branch) {
        values_size += 1;
    }

    // The value type for all cases values
    auto value_type = llvm_type_from_amun_type(node->get_type_node());
    auto function = Builder.GetInsertBlock()->getParent();
    auto argument = llvm_resolve_value(node->argument->accept(this));

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

    // Resolve the else value if declared
    if (else_branch) {
        llvm_values[cases_size] = llvm_resolve_value(else_branch->accept(this));
    }

    // Merge branch is the branch which contains the phi node used as a destination
    // if current case has the same value as argument value
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

auto amun::LLVMBackend::visit(GroupExpression* node) -> std::any
{
    return node->expression->accept(this);
}

auto amun::LLVMBackend::visit(TupleExpression* node) -> std::any
{
    auto tuple_type = llvm_type_from_amun_type(node->type);
    auto alloc_inst = Builder.CreateAlloca(tuple_type);

    size_t argument_index = 0;
    for (auto& argument : node->values) {
        auto argument_value = llvm_resolve_value(argument->accept(this));
        auto index = llvm::ConstantInt::get(llvm_context, llvm::APInt(32, argument_index, true));
        auto member_ptr = Builder.CreateGEP(tuple_type, alloc_inst, {zero_int32_value, index});
        Builder.CreateStore(argument_value, member_ptr);
        argument_index++;
    }

    return alloc_inst;
}

auto amun::LLVMBackend::visit(AssignExpression* node) -> std::any
{
    auto left_node = node->left;
    // Assign value to variable
    // variable = value
    if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(left_node)) {
        auto name = literal->name.literal;
        auto value = node->right->accept(this);

        auto right_value = llvm_resolve_value(value);
        auto left_value = node->left->accept(this);
        if (left_value.type() == typeid(llvm::AllocaInst*)) {
            auto alloca = std::any_cast<llvm::AllocaInst*>(left_value);

            // Alloca type must be a pointer to rvalue type
            // this case solve assiging dereferneced value into variable
            // Case
            // var x = 0;
            // x = *ptr;
            // Check samples/memory/AssignPtrValueToVar.amun
            // Check samples/general/Linkedlist.amun
            if (alloca->getType() == right_value->getType()) {
                right_value = derefernecs_llvm_pointer(right_value);
            }

            alloca_inst_table.update(name, alloca);
            Builder.CreateStore(right_value, alloca);
            return right_value;
        }

        if (left_value.type() == typeid(llvm::GlobalVariable*)) {
            auto global_variable = std::any_cast<llvm::GlobalVariable*>(left_value);
            Builder.CreateStore(right_value, global_variable);
            return right_value;
        }
    }

    // Assign value to n dimentions array position
    // array []? = value
    if (auto index_expression = std::dynamic_pointer_cast<IndexExpression>(left_node)) {
        auto node_value = index_expression->value;
        auto index = llvm_resolve_value(index_expression->index->accept(this));
        auto right_value = llvm_node_value(node->right->accept(this));

        // Update element value in Single dimention Array
        if (auto array_literal = std::dynamic_pointer_cast<LiteralExpression>(node_value)) {
            auto array = array_literal->accept(this);
            if (array.type() == typeid(llvm::AllocaInst*)) {
                auto alloca = llvm::dyn_cast<llvm::AllocaInst>(
                    llvm_node_value(alloca_inst_table.lookup(array_literal->name.literal)));
                auto ptr = Builder.CreateGEP(alloca->getAllocatedType(), alloca,
                                             {zero_int32_value, index});
                Builder.CreateStore(right_value, ptr);
                return right_value;
            }

            if (array.type() == typeid(llvm::GlobalVariable*)) {
                auto global_variable_array =
                    llvm::dyn_cast<llvm::GlobalVariable>(llvm_node_value(array));
                auto ptr = Builder.CreateGEP(global_variable_array->getValueType(),
                                             global_variable_array, {zero_int32_value, index});
                Builder.CreateStore(right_value, ptr);
                return right_value;
            }

            internal_compiler_error("Assign value index expression");
        }

        // Update element value in Multi dimentions Array
        if (auto sub_index_expression = std::dynamic_pointer_cast<IndexExpression>(node_value)) {
            auto array = llvm_node_value(node_value->accept(this));
            auto load_inst = dyn_cast<llvm::LoadInst>(array);
            auto ptr = Builder.CreateGEP(array->getType(), load_inst->getPointerOperand(),
                                         {zero_int32_value, index});
            Builder.CreateStore(right_value, ptr);
            return right_value;
        }

        // Update element value in struct field array
        if (auto struct_acess = std::dynamic_pointer_cast<DotExpression>(node_value)) {
            auto struct_field = struct_acess->accept(this);
            if (auto load_inst = std::any_cast<llvm::LoadInst*>(struct_field)) {
                auto ptr =
                    Builder.CreateGEP(llvm_node_value(struct_field)->getType(),
                                      load_inst->getPointerOperand(), {zero_int32_value, index});
                if (right_value->getType() == ptr->getType()) {
                    right_value = derefernecs_llvm_pointer(right_value);
                }
                Builder.CreateStore(right_value, ptr);
                return right_value;
            }
        }
    }

    // Assign value to structure field
    if (auto dot_expression = std::dynamic_pointer_cast<DotExpression>(left_node)) {
        auto member_ptr = access_struct_member_pointer(dot_expression.get());
        auto rvalue = llvm_resolve_value(node->right->accept(this));
        Builder.CreateStore(rvalue, member_ptr);
        return rvalue;
    }

    // Assign value to pointer address
    // *ptr = value;
    if (auto unary_expression = std::dynamic_pointer_cast<PrefixUnaryExpression>(left_node)) {
        auto opt = unary_expression->operator_token.kind;
        if (opt == TokenKind::TOKEN_STAR) {
            auto rvalue = llvm_node_value(node->right->accept(this));
            auto pointer = llvm_node_value(unary_expression->right->accept(this));
            auto load = Builder.CreateLoad(pointer->getType()->getPointerElementType(), pointer);
            Builder.CreateStore(rvalue, load);
            return rvalue;
        }
    }

    internal_compiler_error("Invalid assignments expression with unexpected lvalue type");
}

auto amun::LLVMBackend::visit(BinaryExpression* node) -> std::any
{
    auto lhs = llvm_resolve_value(node->left->accept(this));
    auto rhs = llvm_resolve_value(node->right->accept(this));
    auto op = node->operator_token.kind;

    // Binary Operations for integer types
    if (lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy()) {
        return create_llvm_integers_bianry(op, lhs, rhs);
    }

    // Binary Operations for floating point types
    if (lhs->getType()->isFloatingPointTy() && rhs->getType()->isFloatingPointTy()) {
        return create_llvm_floats_bianry(op, lhs, rhs);
    }

    // Perform overloading function call
    // No need for extra checks after type checker pass
    auto lhs_type = node->left->get_type_node();
    auto rhs_type = node->right->get_type_node();
    auto name = mangle_operator_function(op, {lhs_type, rhs_type});
    return create_overloading_function_call(name, {lhs, rhs});
}

auto amun::LLVMBackend::visit(BitwiseExpression* node) -> std::any
{
    auto lhs = llvm_resolve_value(node->left->accept(this));
    auto rhs = llvm_resolve_value(node->right->accept(this));

    auto lhs_type = node->left->get_type_node();
    auto rhs_type = node->right->get_type_node();
    auto op = node->operator_token.kind;

    if (amun::is_integer_type(lhs_type) && amun::is_integer_type(rhs_type)) {
        if (op == TokenKind::TOKEN_OR) {
            return Builder.CreateOr(lhs, rhs);
        }

        if (op == TokenKind::TOKEN_AND) {
            return Builder.CreateAnd(lhs, rhs);
        }

        if (op == TokenKind::TOKEN_XOR) {
            return Builder.CreateXor(lhs, rhs);
        }

        if (op == TokenKind::TOKEN_LEFT_SHIFT) {
            return Builder.CreateShl(lhs, rhs);
        }

        if (op == TokenKind::TOKEN_RIGHT_SHIFT) {
            return Builder.CreateAShr(lhs, rhs);
        }
    }

    // Perform overloading function call
    // No need for extra checks after type checker pass
    auto name = mangle_operator_function(op, {lhs_type, rhs_type});
    return create_overloading_function_call(name, {lhs, rhs});
}

auto amun::LLVMBackend::visit(ComparisonExpression* node) -> std::any
{
    auto lhs = llvm_resolve_value(node->left->accept(this));
    auto rhs = llvm_resolve_value(node->right->accept(this));
    const auto op = node->operator_token.kind;

    // Comparison Operations for integers types
    if (lhs->getType()->isIntegerTy() && rhs->getType()->isIntegerTy()) {
        if (amun::is_unsigned_integer_type(node->left->get_type_node())) {
            return create_llvm_unsigned_integers_comparison(op, lhs, rhs);
        }
        return create_llvm_integers_comparison(op, lhs, rhs);
    }

    // Comparison Operations for floating point types
    if (lhs->getType()->isFloatingPointTy() && rhs->getType()->isFloatingPointTy()) {
        return create_llvm_floats_comparison(op, lhs, rhs);
    }

    // Comparison Operations for pointers types thay points to the same type, no need for casting
    if (lhs->getType()->isPointerTy() && rhs->getType()->isPointerTy()) {

        // Can be optimized by checking if both sides are String literal expression
        if (node->left->get_ast_node_type() == AstNodeType::AST_STRING &&
            node->right->get_ast_node_type() == AstNodeType::AST_STRING) {
            auto lhs_str = std::dynamic_pointer_cast<StringExpression>(node->left)->value.literal;
            auto rlhs_str = std::dynamic_pointer_cast<StringExpression>(node->right)->value.literal;
            auto compare = std::strcmp(lhs_str.c_str(), rlhs_str.c_str());
            auto result_llvm = create_llvm_int32(compare, true);
            return create_llvm_integers_comparison(op, result_llvm, zero_int32_value);
        }

        // If both sides are strings use strcmp function
        if (amun::is_types_equals(node->left->get_type_node(), amun::i8_ptr_type) &&
            amun::is_types_equals(node->right->get_type_node(), amun::i8_ptr_type)) {
            return create_llvm_strings_comparison(op, lhs, rhs);
        }

        // Compare address of both pointers
        return create_llvm_integers_comparison(op, lhs, rhs);
    }

    // Perform overloading function call
    // No need for extra checks after type checker pass
    auto lhs_type = node->left->get_type_node();
    auto rhs_type = node->right->get_type_node();
    auto name = mangle_operator_function(op, {lhs_type, rhs_type});
    return create_overloading_function_call(name, {lhs, rhs});
}

auto amun::LLVMBackend::visit(LogicalExpression* node) -> std::any
{
    auto lhs = llvm_resolve_value(node->left->accept(this));
    auto rhs = llvm_resolve_value(node->right->accept(this));

    auto lhs_type = node->left->get_type_node();
    auto rhs_type = node->right->get_type_node();
    auto op = node->operator_token.kind;

    if (amun::is_integer1_type(lhs_type) && amun::is_integer1_type(rhs_type)) {
        if (op == TokenKind::TOKEN_AND_AND) {
            return Builder.CreateLogicalAnd(lhs, rhs);
        }

        if (op == TokenKind::TOKEN_OR_OR) {
            return Builder.CreateLogicalOr(lhs, rhs);
        }
    }

    // Perform overloading function call
    // No need for extra checks after type checker pass
    auto name = mangle_operator_function(op, {lhs_type, rhs_type});
    return create_overloading_function_call(name, {lhs, rhs});
}

auto amun::LLVMBackend::visit(PrefixUnaryExpression* node) -> std::any
{
    auto operand = node->right;
    auto operator_kind = node->operator_token.kind;

    // Unary - minus operator
    if (operator_kind == TokenKind::TOKEN_MINUS) {
        auto rhs = llvm_resolve_value(operand->accept(this));
        if (rhs->getType()->isFloatingPointTy()) {
            return Builder.CreateFNeg(rhs);
        }
        if (rhs->getType()->isIntegerTy()) {
            return Builder.CreateNeg(rhs);
        }

        auto name = "_prefix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        return create_overloading_function_call(name, {rhs});
    }

    // Bang can be implemented as (value == false)
    if (operator_kind == TokenKind::TOKEN_BANG) {
        auto rhs = llvm_resolve_value(operand->accept(this));
        if (rhs->getType() == llvm_int1_type) {
            return Builder.CreateICmpEQ(rhs, false_value);
        }

        auto name = "_prefix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        return create_overloading_function_call(name, {rhs});
    }

    // Pointer * Dereference operator
    if (operator_kind == TokenKind::TOKEN_STAR) {
        auto right = llvm_node_value(operand->accept(this));
        auto is_expect_struct_type = node->get_type_node()->type_kind == amun::TypeKind::STRUCT;
        // No need to emit 2 load inst if the current type is pointer to struct
        if (is_expect_struct_type) {
            return derefernecs_llvm_pointer(right);
        }
        auto derefernce_right = derefernecs_llvm_pointer(right);
        return derefernecs_llvm_pointer(derefernce_right);
    }

    // Address of operator (&) to return pointer of operand
    if (operator_kind == TokenKind::TOKEN_AND) {
        auto right = llvm_node_value(operand->accept(this));
        auto ptr = Builder.CreateAlloca(right->getType(), nullptr);
        Builder.CreateStore(right, ptr);
        return ptr;
    }

    // Unary ~ not operator
    if (operator_kind == TokenKind::TOKEN_NOT) {
        auto rhs = llvm_resolve_value(operand->accept(this));

        if (operand->get_type_node()->type_kind == TypeKind::NUMBER) {
            return Builder.CreateNot(rhs);
        }

        auto name = "_prefix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        return create_overloading_function_call(name, {rhs});
    }

    // Unary prefix ++ operator, example (++x)
    if (operator_kind == TokenKind::TOKEN_PLUS_PLUS) {
        if (operand->get_type_node()->type_kind == TypeKind::NUMBER) {
            return create_llvm_value_increment(operand, true);
        }
        auto name = "_prefix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        auto llvm_rhs = llvm_resolve_value(operand->accept(this));
        return create_overloading_function_call(name, {llvm_rhs});
    }

    // Unary prefix -- operator, example (--x)
    if (operator_kind == TokenKind::TOKEN_MINUS_MINUS) {
        if (operand->get_type_node()->type_kind == TypeKind::NUMBER) {
            return create_llvm_value_decrement(operand, true);
        }
        auto name = "_prefix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        auto llvm_rhs = llvm_resolve_value(operand->accept(this));
        return create_overloading_function_call(name, {llvm_rhs});
    }

    internal_compiler_error("Invalid Prefix Unary operator");
}

auto amun::LLVMBackend::visit(PostfixUnaryExpression* node) -> std::any
{
    auto operand = node->right;
    auto operator_kind = node->operator_token.kind;

    // Unary postfix ++ operator, example (x++)
    if (operator_kind == TokenKind::TOKEN_PLUS_PLUS) {
        if (operand->get_type_node()->type_kind == TypeKind::NUMBER) {
            return create_llvm_value_increment(operand, false);
        }
        auto name =
            "_postfix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        auto llvm_rhs = llvm_resolve_value(operand->accept(this));
        return create_overloading_function_call(name, {llvm_rhs});
    }

    // Unary postfix -- operator, example (x--)
    if (operator_kind == TokenKind::TOKEN_MINUS_MINUS) {
        if (operand->get_type_node()->type_kind == TypeKind::NUMBER) {
            return create_llvm_value_decrement(operand, false);
        }
        auto name =
            "_postfix" + mangle_operator_function(operator_kind, {operand->get_type_node()});
        auto llvm_rhs = llvm_resolve_value(operand->accept(this));
        return create_overloading_function_call(name, {llvm_rhs});
    }

    internal_compiler_error("Invalid Postfix Unary operator");
}

auto amun::LLVMBackend::visit(CallExpression* node) -> std::any
{
    auto callee_ast_node_type = node->callee->get_ast_node_type();

    // If callee is also a CallExpression this case when you have a function that return a
    // function pointer and you call it for example function()();
    if (callee_ast_node_type == AstNodeType::AST_CALL) {
        auto callee_function = llvm_node_value(node->callee->accept(this));
        auto call_instruction = llvm::dyn_cast<llvm::CallInst>(callee_function);
        auto function = call_instruction->getCalledFunction();
        auto callee_function_type = function->getFunctionType();
        auto return_ptr_type = callee_function_type->getReturnType()->getPointerElementType();
        auto function_pointer_type = llvm::dyn_cast<llvm::FunctionType>(return_ptr_type);

        auto arguments = node->arguments;
        size_t arguments_size = arguments.size();
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

    // If callee is literal expression that mean it a function call or function pointer call
    if (callee_ast_node_type == AstNodeType::AST_LITERAL) {
        auto callee = std::dynamic_pointer_cast<LiteralExpression>(node->callee);
        auto callee_literal = callee->name.literal;
        auto function = lookup_function(callee_literal);
        if (not function && functions_declaraions.contains(callee_literal)) {
            auto declaraion = functions_declaraions[callee_literal];
            function = resolve_generic_function(declaraion, node->generic_arguments);
            callee_literal += mangle_types(node->generic_arguments);
        }

        if (not function) {
            auto value = llvm_node_value(alloca_inst_table.lookup(callee_literal));
            if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
                auto loaded = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
                auto function_type = llvm_type_from_amun_type(node->get_type_node());
                if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                    auto arguments = node->arguments;
                    size_t arguments_size = arguments.size();
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

        auto arguments = node->arguments;
        auto arguments_size = arguments.size();
        auto parameter_size = function->arg_size();
        std::vector<llvm::Value*> arguments_values;
        size_t implicit_arguments_count = 0;

        if (is_lambda_function_name(function->getName().str())) {
            auto extra_literal_parameters = lambda_extra_parameters[function->getName().str()];
            implicit_arguments_count = extra_literal_parameters.size();

            std::vector<llvm::Value*> implicit_values;
            for (auto& outer_variable_name : extra_literal_parameters) {
                auto value = llvm_resolve_variable(outer_variable_name);
                auto resolved_value = llvm_resolve_value(value);
                implicit_values.push_back(resolved_value);
            }

            arguments_values.insert(arguments_values.begin(), implicit_values.begin(),
                                    implicit_values.end());
        }
        else {
            arguments_values.reserve(arguments_size);
        }

        // Resolve explicit arguments
        for (size_t i = 0; i < arguments_size; i++) {
            auto argument = arguments[i];
            auto value = llvm_node_value(argument->accept(this));

            // This condition works only if this function has varargs flag
            if (i >= parameter_size) {
                if (argument->get_ast_node_type() == AstNodeType::AST_LITERAL) {
                    auto argument_type = llvm_type_from_amun_type(argument->get_type_node());
                    auto loaded_value = Builder.CreateLoad(argument_type, value);
                    arguments_values.push_back(loaded_value);
                    continue;
                }

                arguments_values.push_back(value);
                continue;
            }

            auto function_argument_type = function->getArg(i + implicit_arguments_count)->getType();

            // If argument type is the same parameter type just pass it directly
            if (function_argument_type == value->getType()) {
                arguments_values.push_back(value);
                continue;
            }

            // Load the constants first and then pass it to the arguments values
            auto resolved_value = Builder.CreateLoad(function_argument_type, value);
            arguments_values.push_back(resolved_value);
        }
        return Builder.CreateCall(function, arguments_values);
    }

    // If callee is lambda expression that mean we can call it as function pointer
    if (callee_ast_node_type == AstNodeType::AST_LAMBDA) {
        auto lambda = std::dynamic_pointer_cast<LambdaExpression>(node->callee);
        auto lambda_value = llvm_node_value(lambda->accept(this));
        auto function = llvm::dyn_cast<llvm::Function>(lambda_value);

        auto arguments = node->arguments;
        auto arguments_size = arguments.size();
        auto parameter_size = function->arg_size();
        std::vector<llvm::Value*> arguments_values;
        arguments_values.reserve(arguments_size);
        for (size_t i = 0; i < arguments_size; i++) {
            auto argument = arguments[i];
            auto value = llvm_node_value(argument->accept(this));

            // This condition works only if this function has varargs flag
            if (i >= parameter_size) {
                if (argument->get_ast_node_type() == AstNodeType::AST_LITERAL) {
                    auto argument_type = llvm_type_from_amun_type(argument->get_type_node());
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

    // If callee is dot expression that mean we call function pointer from struct element
    if (callee_ast_node_type == AstNodeType::AST_DOT) {
        auto dot = std::dynamic_pointer_cast<DotExpression>(node->callee);
        auto struct_fun_ptr = llvm_node_value(dot->accept(this));

        auto function_value = derefernecs_llvm_pointer(struct_fun_ptr);

        auto function_ptr_type = std::static_pointer_cast<amun::PointerType>(dot->get_type_node());
        auto llvm_type = llvm_type_from_amun_type(function_ptr_type->base_type);
        auto llvm_fun_type = llvm::dyn_cast<llvm::FunctionType>(llvm_type);

        auto arguments = node->arguments;
        auto arguments_size = arguments.size();
        auto parameter_size = llvm_fun_type->getNumParams();
        std::vector<llvm::Value*> arguments_values;
        arguments_values.reserve(arguments_size);
        for (size_t i = 0; i < arguments_size; i++) {
            auto argument = arguments[i];
            auto value = llvm_node_value(argument->accept(this));

            // This condition works only if this function has varargs flag
            if (i >= parameter_size) {
                if (argument->get_ast_node_type() == AstNodeType::AST_LITERAL) {
                    auto argument_type = llvm_type_from_amun_type(argument->get_type_node());
                    auto loaded_value = Builder.CreateLoad(argument_type, value);
                    arguments_values.push_back(loaded_value);
                    continue;
                }

                arguments_values.push_back(value);
                continue;
            }

            // If argument type is the same parameter type just pass it directly
            if (llvm_fun_type->getParamType(i) == value->getType()) {
                arguments_values.push_back(value);
                continue;
            }

            // Load the constants first and then pass it to the arguments values
            auto expected_type = llvm_fun_type->getParamType(i);
            auto loaded_value = Builder.CreateLoad(expected_type, value);
            arguments_values.push_back(loaded_value);
        }

        return Builder.CreateCall(llvm_fun_type, function_value, arguments_values);
    }

    internal_compiler_error("Invalid call expression callee type");
}

auto amun::LLVMBackend::visit(InitializeExpression* node) -> std::any
{

    // Convert struct and resolve it first if it generic to LLVM struct type
    llvm::Type* struct_type;
    if (node->type->type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto generic = std::static_pointer_cast<amun::GenericStructType>(node->type);
        struct_type = resolve_generic_struct(generic);
    }
    else {
        struct_type = llvm_type_from_amun_type(node->type);
    }

    // If it on global scope, must initialize it as Constants Struct with constants values
    if (is_global_block()) {
        auto llvm_struct_type = llvm::dyn_cast<llvm::StructType>(struct_type);
        // Resolve constants arguments
        std::vector<llvm::Constant*> constants_arguments;
        constants_arguments.reserve(node->arguments.size());
        for (auto& argument : node->arguments) {
            constants_arguments.push_back(resolve_constant_expression(argument));
        }

        // Return Constants struct instance
        return llvm::ConstantStruct::get(llvm_struct_type, constants_arguments);
    }

    auto alloc_inst = Builder.CreateAlloca(struct_type);

    // Loop over arguments and set them by index
    size_t argument_index = 0;
    for (auto& argument : node->arguments) {
        auto argument_value = llvm_resolve_value(argument->accept(this));
        auto index = llvm::ConstantInt::get(llvm_context, llvm::APInt(32, argument_index, true));
        auto member_ptr = Builder.CreateGEP(struct_type, alloc_inst, {zero_int32_value, index});
        Builder.CreateStore(argument_value, member_ptr);
        argument_index++;
    }

    // Return refernce to the struct initializer
    return alloc_inst;
}

auto amun::LLVMBackend::visit(LambdaExpression* node) -> std::any
{
    auto lambda_name = "_lambda" + std::to_string(lambda_unique_id++);
    auto function_ptr_type = std::static_pointer_cast<amun::PointerType>(node->get_type_node());
    auto node_llvm_type = llvm_type_from_amun_type(function_ptr_type->base_type);
    auto function_type = llvm::dyn_cast<llvm::FunctionType>(node_llvm_type);

    auto linkage = llvm::Function::InternalLinkage;
    auto function = llvm::Function::Create(function_type, linkage, lambda_name, llvm_module.get());

    auto previous_insert_block = Builder.GetInsertBlock();

    auto entry_block = llvm::BasicBlock::Create(llvm_context, "entry", function);
    Builder.SetInsertPoint(entry_block);

    push_alloca_inst_scope();

    auto outer_parameter_names = node->implict_parameters_names;
    auto outer_parameters_size = outer_parameter_names.size();

    std::vector<std::string> implicit_parameters;
    implicit_parameters.reserve(outer_parameter_names.size());
    for (auto& outer_parameter_name : outer_parameter_names) {
        implicit_parameters.push_back(outer_parameter_name);
    }

    lambda_extra_parameters[lambda_name] = implicit_parameters;

    size_t i = 0;
    size_t explicit_parameter_index = 0;
    for (auto& arg : function->args()) {
        if (i < outer_parameters_size) {
            arg.setName(implicit_parameters[i++]);
        }
        else {
            arg.setName(node->explicit_parameters[explicit_parameter_index++]->name.literal);
        }
        const std::string arg_name_str = std::string(arg.getName());
        auto* alloca_inst = create_entry_block_alloca(function, arg_name_str, arg.getType());
        alloca_inst_table.define(arg_name_str, alloca_inst);
        Builder.CreateStore(&arg, alloca_inst);
    }

    defer_calls_stack.push({});

    node->body->accept(this);

    defer_calls_stack.pop();

    pop_alloca_inst_scope();

    alloca_inst_table.define(lambda_name, function);

    verifyFunction(*function);

    Builder.SetInsertPoint(previous_insert_block);

    return function;
}

auto amun::LLVMBackend::visit(DotExpression* node) -> std::any
{
    auto callee = node->callee;
    auto callee_type = callee->get_type_node();
    auto callee_llvm_type = llvm_type_from_amun_type(callee_type);
    auto expected_llvm_type = llvm_type_from_amun_type(node->get_type_node());

    // Compile array attributes
    if (callee_llvm_type->isArrayTy()) {
        if (node->field_name.literal == "count") {
            auto llvm_array_type = llvm::dyn_cast<llvm::ArrayType>(callee_llvm_type);
            auto length = llvm_array_type->getArrayNumElements();
            return create_llvm_int64(length, true);
        }

        internal_compiler_error("Invalid Array Attribute");
    }

    // Compile String literal attributes
    if (amun::is_pointer_of_type(callee_type, amun::i8_type)) {
        if (node->field_name.literal == "count") {

            // If node is string expression, length can calculated without strlen
            if (node->callee->get_ast_node_type() == AstNodeType::AST_STRING) {
                auto string = std::dynamic_pointer_cast<StringExpression>(node->callee);
                auto length = string->value.literal.size();
                return create_llvm_int64(length, true);
            }

            auto string_ptr = llvm_node_value(callee->accept(this));

            // Check if it **int8 that mean the string is store in variable and should
            // dereferneceed
            if (string_ptr->getType() != llvm_int8_ptr_type) {
                string_ptr = derefernecs_llvm_pointer(string_ptr);
            }
            return create_llvm_string_length(string_ptr);
        }

        internal_compiler_error("Invalid String Attribute");
    }

    auto member_ptr = access_struct_member_pointer(node);

    // If expected type is pointer no need for loading it for example node.next.data
    if (expected_llvm_type->isPointerTy()) {
        return member_ptr;
    }

    return Builder.CreateLoad(expected_llvm_type, member_ptr);
}

auto amun::LLVMBackend::visit(CastExpression* node) -> std::any
{
    auto value = llvm_resolve_value(node->value->accept(this));
    auto value_type = llvm_type_from_amun_type(node->value->get_type_node());
    auto target_type = llvm_type_from_amun_type(node->get_type_node());

    // No need for castring if both sides has the same type
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
        auto value_type = value->getType();

        // Load array if it stored on variable
        // Example -> var ptr = cast(*int64) array;
        if (const auto& load_inst = dyn_cast<llvm::LoadInst>(value)) {
            return Builder.CreateGEP(value_type, load_inst->getPointerOperand(),
                                     {zero_int32_value, zero_int32_value});
        }

        // No need for load instruction if array is array literal not variable
        // Example -> var ptr = cast(*int64) [1, 2, 3];
        auto alloca = Builder.CreateAlloca(value_type);
        Builder.CreateStore(value, alloca);
        auto load = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
        const auto& load_inst = dyn_cast<llvm::LoadInst>(load);
        auto ptr_operand = load_inst->getPointerOperand();
        return Builder.CreateGEP(value_type, ptr_operand, {zero_int32_value, zero_int32_value});
    }

    // Bit casting
    return Builder.CreateBitCast(value, target_type);
}

auto amun::LLVMBackend::visit(TypeSizeExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_amun_type(node->type);
    auto type_alloc_size = llvm_module->getDataLayout().getTypeAllocSize(llvm_type);
    return create_llvm_int64(type_alloc_size, true);
}

auto amun::LLVMBackend::visit(TypeAlignExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_amun_type(node->type);
    auto allign = llvm_module->getDataLayout().getABITypeAlign(llvm_type);
    return create_llvm_int64(allign.value(), true);
}

auto amun::LLVMBackend::visit(ValueSizeExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_amun_type(node->value->get_type_node());
    auto type_alloc_size = llvm_module->getDataLayout().getTypeAllocSize(llvm_type);
    auto type_size = llvm::ConstantInt::get(llvm_int64_type, type_alloc_size);
    return type_size;
}

auto amun::LLVMBackend::visit(IndexExpression* node) -> std::any
{
    auto index = llvm_resolve_value(node->index->accept(this));
    return access_array_element(node->value, index);
}

auto amun::LLVMBackend::visit(EnumAccessExpression* node) -> std::any
{
    auto element_type = llvm_type_from_amun_type(node->get_type_node());
    auto element_index = llvm::ConstantInt::get(element_type, node->enum_element_index);
    return llvm::dyn_cast<llvm::Value>(element_index);
}

auto amun::LLVMBackend::visit(LiteralExpression* node) -> std::any
{
    const auto name = node->name.literal;
    // If found in alloca inst table that mean it local variable
    auto alloca_inst = alloca_inst_table.lookup(name);
    if (alloca_inst.type() != typeid(nullptr)) {
        return alloca_inst;
    }
    // If it not in alloca inst table,that mean it global variable
    return llvm_module->getNamedGlobal(name);
}

auto amun::LLVMBackend::visit(NumberExpression* node) -> std::any
{
    auto number_type = std::static_pointer_cast<amun::NumberType>(node->get_type_node());
    return llvm_number_value(node->value.literal, number_type->number_kind);
}

auto amun::LLVMBackend::visit(ArrayExpression* node) -> std::any
{
    auto node_values = node->values;
    auto size = node_values.size();
    if (node->is_constant()) {
        auto llvm_type = llvm_type_from_amun_type(node->get_type_node());
        auto arrayType = llvm::dyn_cast<llvm::ArrayType>(llvm_type);

        std::vector<llvm::Constant*> values;
        values.reserve(size);
        for (auto& value : node_values) {
            auto llvm_value = llvm_resolve_value(value->accept(this));
            values.push_back(llvm::dyn_cast<llvm::Constant>(llvm_value));
        }
        return llvm::ConstantArray::get(arrayType, values);
    }

    auto arrayType = llvm_type_from_amun_type(node->get_type_node());

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

auto amun::LLVMBackend::visit(StringExpression* node) -> std::any
{
    std::string literal = node->value.literal;
    return resolve_constant_string_expression(literal);
}

auto amun::LLVMBackend::visit(CharacterExpression* node) -> std::any
{
    char char_asci_value = node->value.literal[0];
    return create_llvm_int8(char_asci_value, false);
}

auto amun::LLVMBackend::visit(BooleanExpression* node) -> std::any
{
    return create_llvm_int1(node->value.kind == TokenKind::TOKEN_TRUE);
}

auto amun::LLVMBackend::visit(NullExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_amun_type(node->null_base_type);
    return create_llvm_null(llvm_type);
}

auto amun::LLVMBackend::visit(UndefinedExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_amun_type(node->base_type);
    return llvm::UndefValue::get(llvm_type);
}

auto amun::LLVMBackend::llvm_node_value(std::any any_value) -> llvm::Value*
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
    else if (any_value.type() == typeid(llvm::UndefValue*)) {
        return std::any_cast<llvm::UndefValue*>(any_value);
    }
    internal_compiler_error("Unknown type llvm node type ");
}

auto amun::LLVMBackend::llvm_resolve_value(std::any any_value) -> llvm::Value*
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

auto amun::LLVMBackend::llvm_resolve_variable(const std::string& name) -> llvm::Value*
{
    // If found in alloca inst table that mean it local variable
    auto alloca_inst = alloca_inst_table.lookup(name);
    if (alloca_inst.type() != typeid(nullptr)) {
        return llvm_node_value(alloca_inst);
    }
    // If it not in alloca inst table,that mean it global variable
    return llvm_module->getNamedGlobal(name);
}

inline auto amun::LLVMBackend::llvm_number_value(const std::string& value_litearl,
                                                 amun::NumberKind size) -> llvm::Value*
{
    switch (size) {
    case amun::NumberKind::INTEGER_1: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int1_type, value);
    }
    case amun::NumberKind::INTEGER_8: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int8_type, value);
    }
    case amun::NumberKind::U_INTEGER_8: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int8_type, value, true);
    }
    case amun::NumberKind::INTEGER_16: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int16_type, value);
    }
    case amun::NumberKind::U_INTEGER_16: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int16_type, value, true);
    }
    case amun::NumberKind::INTEGER_32: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int32_type, value);
    }
    case amun::NumberKind::U_INTEGER_32: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int32_type, value, true);
    }
    case amun::NumberKind::INTEGER_64: {
        auto value = std::strtoll(value_litearl.c_str(), nullptr, 0);
        return llvm::ConstantInt::get(llvm_int64_type, value);
    }
    case amun::NumberKind::U_INTEGER_64: {
        auto value = std::strtoll(value_litearl.c_str(), nullptr, 0);
        return llvm::ConstantInt::get(llvm_int64_type, value, true);
    }
    case amun::NumberKind::FLOAT_32: {
        auto value = std::stod(value_litearl);
        return llvm::ConstantFP::get(llvm_float32_type, value);
    }
    case amun::NumberKind::FLOAT_64: {
        auto value = std::stod(value_litearl);
        return llvm::ConstantFP::get(llvm_float64_type, value);
    }
    }
}

auto amun::LLVMBackend::llvm_type_from_amun_type(std::shared_ptr<amun::Type> type) -> llvm::Type*
{
    amun::TypeKind type_kind = type->type_kind;
    if (type_kind == amun::TypeKind::NUMBER) {
        auto amun_number_type = std::static_pointer_cast<amun::NumberType>(type);
        amun::NumberKind number_kind = amun_number_type->number_kind;
        switch (number_kind) {
        case amun::NumberKind::INTEGER_1: return llvm_int1_type;
        case amun::NumberKind::INTEGER_8: return llvm_int8_type;
        case amun::NumberKind::INTEGER_16: return llvm_int16_type;
        case amun::NumberKind::INTEGER_32: return llvm_int32_type;
        case amun::NumberKind::INTEGER_64: return llvm_int64_type;
        case amun::NumberKind::U_INTEGER_8: return llvm_int8_type;
        case amun::NumberKind::U_INTEGER_16: return llvm_int16_type;
        case amun::NumberKind::U_INTEGER_32: return llvm_int32_type;
        case amun::NumberKind::U_INTEGER_64: return llvm_int64_type;
        case amun::NumberKind::FLOAT_32: return llvm_float32_type;
        case amun::NumberKind::FLOAT_64: return llvm_float64_type;
        }
    }

    if (type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto amun_array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
        auto element_type = llvm_type_from_amun_type(amun_array_type->element_type);
        auto size = amun_array_type->size;
        return llvm::ArrayType::get(element_type, size);
    }

    if (type_kind == amun::TypeKind::POINTER) {
        auto amun_pointer_type = std::static_pointer_cast<amun::PointerType>(type);
        // In llvm *void should be generated as *i8
        if (amun_pointer_type->base_type->type_kind == amun::TypeKind::VOID) {
            return llvm_void_ptr_type;
        }
        auto point_to_type = llvm_type_from_amun_type(amun_pointer_type->base_type);
        return llvm::PointerType::get(point_to_type, 0);
    }

    if (type_kind == amun::TypeKind::FUNCTION) {
        auto amun_function_type = std::static_pointer_cast<amun::FunctionType>(type);
        auto parameters = amun_function_type->parameters;
        int parameters_size = parameters.size();
        std::vector<llvm::Type*> arguments(parameters_size);
        for (int i = 0; i < parameters_size; i++) {
            arguments[i] = llvm_type_from_amun_type(parameters[i]);
        }
        auto return_type = llvm_type_from_amun_type(amun_function_type->return_type);
        auto function_type = llvm::FunctionType::get(return_type, arguments, false);
        return function_type;
    }

    if (type_kind == amun::TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<amun::StructType>(type);
        auto struct_name = struct_type->name;
        if (structures_types_map.contains(struct_name)) {
            return structures_types_map[struct_name];
        }

        auto is_packed = struct_type->is_packed;
        auto is_extern = struct_type->is_extern;
        return create_llvm_struct_type(struct_name, struct_type->fields_types, is_packed,
                                       is_extern);
    }

    if (type_kind == amun::TypeKind::TUPLE) {
        auto tuple_type = std::static_pointer_cast<amun::TupleType>(type);
        return create_llvm_struct_type(tuple_type->name, tuple_type->fields_types, false, false);
    }

    if (type_kind == amun::TypeKind::ENUM_ELEMENT) {
        auto enum_element_type = std::static_pointer_cast<amun::EnumElementType>(type);
        return llvm_type_from_amun_type(enum_element_type->element_type);
    }

    if (type_kind == amun::TypeKind::VOID) {
        return llvm_void_type;
    }

    if (type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto generic_struct_type = std::static_pointer_cast<amun::GenericStructType>(type);
        return resolve_generic_struct(generic_struct_type);
    }

    internal_compiler_error("Can't find LLVM Type for this amun Type");
}

auto amun::LLVMBackend::create_llvm_numbers_bianry(TokenKind op, llvm::Value* left,
                                                   llvm::Value* right) -> llvm::Value*
{
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        return create_llvm_integers_bianry(op, left, right);
    }

    if (left->getType()->isFloatingPointTy() && right->getType()->isFloatingPointTy()) {
        return create_llvm_floats_bianry(op, left, right);
    }

    internal_compiler_error("llvm binary operator with number expect integers or floats");
}

auto amun::LLVMBackend::create_llvm_integers_bianry(TokenKind op, llvm::Value* left,
                                                    llvm::Value* right) -> llvm::Value*
{
    switch (op) {
    case TokenKind::TOKEN_PLUS: {
        return Builder.CreateAdd(left, right, "addtemp");
    }
    case TokenKind::TOKEN_MINUS: {
        return Builder.CreateSub(left, right, "subtmp");
    }
    case TokenKind::TOKEN_STAR: {
        return Builder.CreateMul(left, right, "multmp");
    }
    case TokenKind::TOKEN_SLASH: {
        return Builder.CreateUDiv(left, right, "divtmp");
    }
    case TokenKind::TOKEN_PERCENT: {
        return Builder.CreateURem(left, right, "remtemp");
    }
    default: {
        internal_compiler_error("Invalid binary operator for integers types");
    }
    }
}

auto amun::LLVMBackend::create_llvm_floats_bianry(TokenKind op, llvm::Value* left,
                                                  llvm::Value* right) -> llvm::Value*
{
    switch (op) {
    case TokenKind::TOKEN_PLUS: {
        return Builder.CreateFAdd(left, right, "addtemp");
    }
    case TokenKind::TOKEN_MINUS: {
        return Builder.CreateFSub(left, right, "subtmp");
    }
    case TokenKind::TOKEN_STAR: {
        return Builder.CreateFMul(left, right, "multmp");
    }
    case TokenKind::TOKEN_SLASH: {
        return Builder.CreateFDiv(left, right, "divtmp");
    }
    case TokenKind::TOKEN_PERCENT: {
        return Builder.CreateFRem(left, right, "remtemp");
    }
    default: {
        internal_compiler_error("Invalid binary operator for floating point types");
    }
    }
}

auto amun::LLVMBackend::create_llvm_numbers_comparison(TokenKind op, llvm::Value* left,
                                                       llvm::Value* right) -> llvm::Value*
{
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        return create_llvm_integers_comparison(op, left, right);
    }

    if (left->getType()->isFloatingPointTy() && right->getType()->isFloatingPointTy()) {
        return create_llvm_floats_comparison(op, left, right);
    }

    internal_compiler_error("llvm binary comparison with number expect integers or floats");
}

auto amun::LLVMBackend::create_llvm_integers_comparison(TokenKind op, llvm::Value* left,
                                                        llvm::Value* right) -> llvm::Value*
{
    switch (op) {
    case TokenKind::TOKEN_EQUAL_EQUAL: {
        return Builder.CreateICmpEQ(left, right);
    }
    case TokenKind::TOKEN_BANG_EQUAL: {
        return Builder.CreateICmpNE(left, right);
    }
    case TokenKind::TOKEN_GREATER: {
        return Builder.CreateICmpSGT(left, right);
    }
    case TokenKind::TOKEN_GREATER_EQUAL: {
        return Builder.CreateICmpSGE(left, right);
    }
    case TokenKind::TOKEN_SMALLER: {
        return Builder.CreateICmpSLT(left, right);
    }
    case TokenKind::TOKEN_SMALLER_EQUAL: {
        return Builder.CreateICmpSLE(left, right);
    }
    default: {
        internal_compiler_error("Invalid Integers Comparison operator");
    }
    }
}

auto amun::LLVMBackend::create_llvm_unsigned_integers_comparison(TokenKind op, llvm::Value* left,
                                                                 llvm::Value* right) -> llvm::Value*
{
    switch (op) {
    case TokenKind::TOKEN_EQUAL_EQUAL: {
        return Builder.CreateICmpEQ(left, right);
    }
    case TokenKind::TOKEN_BANG_EQUAL: {
        return Builder.CreateICmpNE(left, right);
    }
    case TokenKind::TOKEN_GREATER: {
        return Builder.CreateICmpUGT(left, right);
    }
    case TokenKind::TOKEN_GREATER_EQUAL: {
        return Builder.CreateICmpUGE(left, right);
    }
    case TokenKind::TOKEN_SMALLER: {
        return Builder.CreateICmpULT(left, right);
    }
    case TokenKind::TOKEN_SMALLER_EQUAL: {
        return Builder.CreateICmpULE(left, right);
    }
    default: {
        internal_compiler_error("Invalid Integers Comparison operator");
    }
    }
}

auto amun::LLVMBackend::create_llvm_floats_comparison(TokenKind op, llvm::Value* left,
                                                      llvm::Value* right) -> llvm::Value*
{
    switch (op) {
    case TokenKind::TOKEN_EQUAL_EQUAL: {
        return Builder.CreateFCmpOEQ(left, right);
    }
    case TokenKind::TOKEN_BANG_EQUAL: {
        return Builder.CreateFCmpONE(left, right);
    }
    case TokenKind::TOKEN_GREATER: {
        return Builder.CreateFCmpOGT(left, right);
    }
    case TokenKind::TOKEN_GREATER_EQUAL: {
        return Builder.CreateFCmpOGE(left, right);
    }
    case TokenKind::TOKEN_SMALLER: {
        return Builder.CreateFCmpOLT(left, right);
    }
    case TokenKind::TOKEN_SMALLER_EQUAL: {
        return Builder.CreateFCmpOLE(left, right);
    }
    default: {
        internal_compiler_error("Invalid floats Comparison operator");
    }
    }
}

auto amun::LLVMBackend::create_llvm_strings_comparison(TokenKind op, llvm::Value* left,
                                                       llvm::Value* right) -> llvm::Value*
{

    std::string function_name = "strcmp";
    auto function = lookup_function(function_name);
    if (!function) {
        auto fun_type = llvm::FunctionType::get(llvm_int32_type,
                                                {llvm_int8_ptr_type, llvm_int8_ptr_type}, false);
        auto linkage = llvm::Function::ExternalLinkage;
        function = llvm::Function::Create(fun_type, linkage, function_name, *llvm_module);
    }

    auto function_call = Builder.CreateCall(function, {left, right});

    switch (op) {
    case TokenKind::TOKEN_EQUAL_EQUAL: {
        return Builder.CreateICmpEQ(function_call, zero_int32_value);
    }
    case TokenKind::TOKEN_BANG_EQUAL: {
        return Builder.CreateICmpNE(function_call, zero_int32_value);
    }
    case TokenKind::TOKEN_GREATER: {
        return Builder.CreateICmpSGT(function_call, zero_int32_value);
    }
    case TokenKind::TOKEN_GREATER_EQUAL: {
        return Builder.CreateICmpSGE(function_call, zero_int32_value);
    }
    case TokenKind::TOKEN_SMALLER: {
        return Builder.CreateICmpSLT(function_call, zero_int32_value);
    }
    case TokenKind::TOKEN_SMALLER_EQUAL: {
        return Builder.CreateICmpSLE(function_call, zero_int32_value);
    }
    default: {
        internal_compiler_error("Invalid strings Comparison operator");
    }
    }
}

auto amun::LLVMBackend::create_llvm_value_increment(std::shared_ptr<Expression> operand,
                                                    bool is_prefix) -> llvm::Value*
{
    auto number_type = std::static_pointer_cast<amun::NumberType>(operand->get_type_node());
    auto constants_one = llvm_number_value("1", number_type->number_kind);

    std::any right = nullptr;
    if (operand->get_ast_node_type() == AstNodeType::AST_DOT) {
        auto dot_expression = std::dynamic_pointer_cast<DotExpression>(operand);
        right = access_struct_member_pointer(dot_expression.get());
    }
    else {
        right = operand->accept(this);
    }

    if (right.type() == typeid(llvm::LoadInst*)) {
        auto current_value = std::any_cast<llvm::LoadInst*>(right);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_PLUS, current_value, constants_one);
        Builder.CreateStore(new_value, current_value->getPointerOperand());
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::AllocaInst*)) {
        auto alloca = std::any_cast<llvm::AllocaInst*>(right);
        auto current_value = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_PLUS, current_value, constants_one);
        Builder.CreateStore(new_value, alloca);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::GlobalVariable*)) {
        auto global_variable = std::any_cast<llvm::GlobalVariable*>(right);
        auto current_value = Builder.CreateLoad(global_variable->getValueType(), global_variable);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_PLUS, current_value, constants_one);
        Builder.CreateStore(new_value, global_variable);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::Value*)) {
        auto current_value_ptr = std::any_cast<llvm::Value*>(right);
        auto number_llvm_type = llvm_type_from_amun_type(number_type);
        auto current_value = Builder.CreateLoad(number_llvm_type, current_value_ptr);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_PLUS, current_value, constants_one);
        Builder.CreateStore(new_value, current_value_ptr);
        return is_prefix ? new_value : current_value;
    }

    internal_compiler_error("Unary expression with non global or alloca type");
}

auto amun::LLVMBackend::create_llvm_value_decrement(std::shared_ptr<Expression> operand,
                                                    bool is_prefix) -> llvm::Value*
{
    auto number_type = std::static_pointer_cast<amun::NumberType>(operand->get_type_node());
    auto constants_one = llvm_number_value("1", number_type->number_kind);

    std::any right = nullptr;
    if (operand->get_ast_node_type() == AstNodeType::AST_DOT) {
        auto dot_expression = std::dynamic_pointer_cast<DotExpression>(operand);
        right = access_struct_member_pointer(dot_expression.get());
    }
    else {
        right = operand->accept(this);
    }

    if (right.type() == typeid(llvm::LoadInst*)) {
        auto current_value = std::any_cast<llvm::LoadInst*>(right);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_MINUS, current_value, constants_one);
        Builder.CreateStore(new_value, current_value->getPointerOperand());
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::AllocaInst*)) {
        auto alloca = std::any_cast<llvm::AllocaInst*>(right);
        auto current_value = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_MINUS, current_value, constants_one);
        Builder.CreateStore(new_value, alloca);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::GlobalVariable*)) {
        auto global_variable = std::any_cast<llvm::GlobalVariable*>(right);
        auto current_value = Builder.CreateLoad(global_variable->getValueType(), global_variable);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_MINUS, current_value, constants_one);
        Builder.CreateStore(new_value, global_variable);
        return is_prefix ? new_value : current_value;
    }

    if (right.type() == typeid(llvm::Value*)) {
        auto current_value_ptr = std::any_cast<llvm::Value*>(right);
        auto number_llvm_type = llvm_type_from_amun_type(number_type);
        auto current_value = Builder.CreateLoad(number_llvm_type, current_value_ptr);
        auto new_value =
            create_llvm_integers_bianry(TokenKind::TOKEN_MINUS, current_value, constants_one);
        Builder.CreateStore(new_value, current_value_ptr);
        return is_prefix ? new_value : current_value;
    }

    internal_compiler_error("Unary expression with non global or alloca type");
}

auto amun::LLVMBackend::create_llvm_string_length(llvm::Value* string) -> llvm::Value*
{
    std::string function_name = "strlen";
    auto function = lookup_function(function_name);
    if (!function) {
        auto fun_type = llvm::FunctionType::get(llvm_int64_type, {llvm_int8_ptr_type}, false);
        auto linkage = llvm::Function::ExternalLinkage;
        function = llvm::Function::Create(fun_type, linkage, function_name, *llvm_module);
    }
    return Builder.CreateCall(function, {string});
}

auto amun::LLVMBackend::create_llvm_struct_type(std::string name,
                                                std::vector<Shared<amun::Type>> members,
                                                bool is_packed, bool is_extern) -> llvm::StructType*
{
    if (structures_types_map.contains(name)) {
        return llvm::dyn_cast<llvm::StructType>(structures_types_map[name]);
    }

    auto* struct_llvm_type = llvm::StructType::create(llvm_context, name);

    // External mean that this struct is opaque to be used for type safe c libraries
    if (is_extern) {
        structures_types_map[name] = struct_llvm_type;
        return struct_llvm_type;
    }

    std::vector<llvm::Type*> struct_fields;
    struct_fields.reserve(members.size());

    for (const auto& field : members) {

        // Handle case where field type is pointer to the current struct
        if (field->type_kind == amun::TypeKind::POINTER) {
            auto pointer = std::static_pointer_cast<amun::PointerType>(field);
            if (pointer->base_type->type_kind == amun::TypeKind::STRUCT) {
                auto struct_ty = std::static_pointer_cast<amun::StructType>(pointer->base_type);
                if (struct_ty->name == name) {
                    struct_fields.push_back(struct_llvm_type->getPointerTo());
                    continue;
                }
            }
        }

        // Handle case where field type is array of pointers to the current struct
        if (field->type_kind == amun::TypeKind::STATIC_ARRAY) {
            auto array = std::static_pointer_cast<amun::StaticArrayType>(field);
            if (array->element_type->type_kind == amun::TypeKind::POINTER) {
                auto pointer = std::static_pointer_cast<amun::PointerType>(array->element_type);
                if (pointer->base_type->type_kind == amun::TypeKind::STRUCT) {
                    auto struct_ty = std::static_pointer_cast<amun::StructType>(pointer->base_type);
                    if (struct_ty->name == name) {
                        auto struct_ptr_ty = struct_llvm_type->getPointerTo();
                        auto array_type = create_llvm_array_type(struct_ptr_ty, array->size);
                        struct_fields.push_back(array_type);
                        continue;
                    }
                }
            }
        }

        struct_fields.push_back(llvm_type_from_amun_type(field));
    }

    struct_llvm_type->setBody(struct_fields, is_packed);
    structures_types_map[name] = struct_llvm_type;

    return struct_llvm_type;
}

auto amun::LLVMBackend::create_overloading_function_call(std::string& name,
                                                         std::vector<llvm::Value*> args)
    -> llvm::Value*
{
    auto function = lookup_function(name);
    return Builder.CreateCall(function, args);
}

auto amun::LLVMBackend::access_struct_member_pointer(DotExpression* expression) -> llvm::Value*
{

    auto callee = expression->callee;
    auto callee_value = llvm_node_value(callee->accept(this));
    auto callee_llvm_type = llvm_type_from_amun_type(callee->get_type_node());

    auto index = create_llvm_int32(expression->field_index, true);

    // Access struct member allocaed on the stack or derefernecs from pointer
    // struct.member or (*struct).member
    if (callee_llvm_type->isStructTy()) {
        // Access struct member coming from pattern matching if or switch expression
        if (auto* phi_node = llvm::dyn_cast<llvm::PHINode>(callee_value)) {
            // Struct type used it to access member from it
            auto struct_type = callee_value->getType();
            assert(struct_type->isStructTy());
            // Store result from pattern matching Phi node in alloca
            auto alloca = Builder.CreateAlloca(struct_type);
            Builder.CreateStore(callee_value, alloca);
            // Access stuct field from alloca inst
            return Builder.CreateGEP(struct_type, alloca, {zero_int32_value, index});
        }

        // Access struct member in general
        if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(callee_value)) {
            return Builder.CreateGEP(callee_llvm_type, alloca, {zero_int32_value, index});
        }

        // Access struct member from other struct member for example elemnet.parent.value
        if (auto* load = llvm::dyn_cast<llvm::LoadInst>(callee_value)) {
            auto* operand = load->getPointerOperand();
            if (auto* alloca = llvm::dyn_cast<llvm::GetElementPtrInst>(operand)) {
                return Builder.CreateGEP(callee_llvm_type, operand, {zero_int32_value, index});
            }
        }

        // Return a pointer to struct member
        return Builder.CreateGEP(callee_llvm_type, callee_value, {zero_int32_value, index});
    }

    // Syntax sugger for accessing struct member from pointer to struct, like -> operator in c
    if (callee_llvm_type->isPointerTy()) {
        // Struct type used it to access member from it
        auto struct_type = callee_llvm_type->getPointerElementType();
        assert(struct_type->isStructTy());
        // Auto Dereferencing the struct pointer
        auto struct_value = derefernecs_llvm_pointer(callee_value);
        // Return a pointer to struct member
        return Builder.CreateGEP(struct_type, struct_value, {zero_int32_value, index});
    }

    // Access struct field from function call
    if (callee_llvm_type->isFunctionTy()) {
        // Struct type used it to access member from it
        auto struct_type = callee_value->getType();
        assert(struct_type->isStructTy());
        // Create alloca inst to save return value from function call
        auto alloca = Builder.CreateAlloca(struct_type);
        Builder.CreateStore(callee_value, alloca);
        return Builder.CreateGEP(struct_type, alloca, {zero_int32_value, index});
    }

    internal_compiler_error("Invalid callee type in access_struct_member_pointer");
}
auto amun::LLVMBackend::access_array_element(std::shared_ptr<Expression> node_value,
                                             llvm::Value* index) -> llvm::Value*
{
    auto values = node_value->get_type_node();

    if (values->type_kind == amun::TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<amun::PointerType>(values);
        auto element_type = llvm_type_from_amun_type(pointer_type->base_type);
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
            auto element_type = alloca_type->getArrayElementType();

            if (element_type->isPointerTy() or element_type->isStructTy()) {
                return ptr;
            }

            return derefernecs_llvm_pointer(ptr);
        }

        if (array.type() == typeid(llvm::GlobalVariable*)) {
            auto global_variable_array =
                llvm::dyn_cast<llvm::GlobalVariable>(llvm_node_value(array));

            auto local_insert_block = Builder.GetInsertBlock();

            // Its local
            if (local_insert_block) {
                auto ptr = Builder.CreateGEP(global_variable_array->getValueType(),
                                             global_variable_array, {zero_int32_value, index});
                return derefernecs_llvm_pointer(ptr);
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
            return derefernecs_llvm_pointer(ptr);
        }

        if (auto constants_array = llvm::dyn_cast<llvm::Constant>(array)) {
            auto constants_index = llvm::dyn_cast<llvm::ConstantInt>(index);
            return constants_array->getAggregateElement(constants_index);
        }
    }

    // Index expression from array expression for example [1, 2, 3][0]
    if (auto array_expession = std::dynamic_pointer_cast<ArrayExpression>(node_value)) {
        auto array = llvm_node_value(array_expession->accept(this));
        if (auto load_inst = dyn_cast<llvm::LoadInst>(array)) {
            auto ptr = Builder.CreateGEP(array->getType(), load_inst->getPointerOperand(),
                                         {zero_int32_value, index});
            return derefernecs_llvm_pointer(ptr);
        }

        if (auto constants_array = llvm::dyn_cast<llvm::Constant>(array)) {
            auto constants_index = llvm::dyn_cast<llvm::ConstantInt>(index);
            return constants_array->getAggregateElement(constants_index);
        }
    }

    // Index expression from struct field array for example node.children[i]
    if (auto dot_expression = std::dynamic_pointer_cast<DotExpression>(node_value)) {
        auto struct_field = dot_expression->accept(this);
        if (auto load_inst = std::any_cast<llvm::LoadInst*>(struct_field)) {
            auto ptr = Builder.CreateGEP(llvm_node_value(struct_field)->getType(),
                                         load_inst->getPointerOperand(), {zero_int32_value, index});
            return derefernecs_llvm_pointer(ptr);
        }
    }

    internal_compiler_error("Invalid Index expression");
}

auto amun::LLVMBackend::resolve_constant_expression(std::shared_ptr<Expression> value)
    -> llvm::Constant*
{
    auto field_type = value->get_type_node();

    // If there are no value, return default value
    if (value == nullptr) {
        return create_llvm_null(llvm_type_from_amun_type(field_type));
    }

    // If right value is index expression resolve it and return constant value
    if (value->get_ast_node_type() == AstNodeType::AST_INDEX) {
        auto index_expression = std::dynamic_pointer_cast<IndexExpression>(value);
        return resolve_constant_index_expression(index_expression);
    }

    // If right value is if expression, resolve it to constant value
    if (value->get_ast_node_type() == AstNodeType::AST_IF_EXPRESSION) {
        auto if_expression = std::dynamic_pointer_cast<IfExpression>(value);
        return resolve_constant_if_expression(if_expression);
    }

    // Resolve non index constants value
    auto llvm_value = llvm_resolve_value(value->accept(this));
    return llvm::dyn_cast<llvm::Constant>(llvm_value);
}

auto amun::LLVMBackend::resolve_constant_index_expression(
    std::shared_ptr<IndexExpression> expression) -> llvm::Constant*
{
    auto llvm_array = llvm_node_value(expression->value->accept(this));
    auto index_value = expression->index->accept(this);
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

auto amun::LLVMBackend::resolve_constant_if_expression(std::shared_ptr<IfExpression> expression)
    -> llvm::Constant*
{
    auto condition = llvm_resolve_value(expression->condition->accept(this));
    auto constant_condition = llvm::dyn_cast<llvm::Constant>(condition);
    if (constant_condition->isZeroValue()) {
        return llvm::dyn_cast<llvm::Constant>(
            llvm_resolve_value(expression->else_expression->accept(this)));
    }
    return llvm::dyn_cast<llvm::Constant>(
        llvm_resolve_value(expression->if_expression->accept(this)));
}

auto amun::LLVMBackend::resolve_constant_switch_expression(
    std::shared_ptr<SwitchExpression> expression) -> llvm::Constant*
{
    auto argument = llvm_resolve_value(expression->argument->accept(this));
    auto constant_argument = llvm::dyn_cast<llvm::Constant>(argument);
    auto switch_cases = expression->switch_cases;
    auto cases_size = switch_cases.size();
    for (size_t i = 0; i < cases_size; i++) {
        auto switch_case = switch_cases[i];
        auto case_value = llvm_resolve_value(switch_case->accept(this));
        auto constant_case = llvm::dyn_cast<llvm::Constant>(case_value);
        if (constant_argument == constant_case) {
            auto value = llvm_resolve_value(expression->switch_cases_values[i]->accept(this));
            return llvm::dyn_cast<llvm::Constant>(value);
        }
    }
    auto default_value = llvm_resolve_value(expression->default_value->accept(this));
    return llvm::dyn_cast<llvm::Constant>(default_value);
}

auto amun::LLVMBackend::resolve_constant_string_expression(const std::string& literal)
    -> llvm::Constant*
{
    // Resolve constants string from string pool if it generated before
    if (constants_string_pool.contains(literal)) {
        return constants_string_pool[literal];
    }

    // Handle empty string literal
    if (literal.empty()) {
        auto str = Builder.CreateGlobalStringPtr("");
        constants_string_pool[""] = str;
        return str;
    }

    auto size = literal.size();
    auto length = size + 1;
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

auto amun::LLVMBackend::resolve_generic_struct(Shared<amun::GenericStructType> generic)
    -> llvm::StructType*
{
    const auto struct_type = generic->struct_type;
    const auto struct_name = struct_type->name;
    const auto mangled_name = struct_name + mangle_types(generic->parameters);
    if (structures_types_map.contains(mangled_name)) {
        return llvm::dyn_cast<llvm::StructType>(structures_types_map[mangled_name]);
    }

    auto* struct_llvm_type = llvm::StructType::create(llvm_context);
    struct_llvm_type->setName(mangled_name);

    const auto fields = struct_type->fields_types;
    std::vector<llvm::Type*> struct_fields;
    struct_fields.reserve(fields.size());

    for (const auto& field : fields) {

        // Handle case where field type is pointer to the current struct
        if (field->type_kind == amun::TypeKind::POINTER) {
            auto pointer = std::static_pointer_cast<amun::PointerType>(field);
            if (pointer->base_type->type_kind == amun::TypeKind::STRUCT) {
                auto struct_ty = std::static_pointer_cast<amun::StructType>(pointer->base_type);
                if (struct_ty->name == struct_name) {
                    struct_fields.push_back(struct_llvm_type->getPointerTo());
                    continue;
                }
            }
        }

        // Handle case where field type is array of pointers to the current struct
        if (field->type_kind == amun::TypeKind::STATIC_ARRAY) {
            auto array = std::static_pointer_cast<amun::StaticArrayType>(field);
            if (array->element_type->type_kind == amun::TypeKind::POINTER) {
                auto pointer = std::static_pointer_cast<amun::PointerType>(array->element_type);
                if (pointer->base_type->type_kind == amun::TypeKind::STRUCT) {
                    auto struct_ty = std::static_pointer_cast<amun::StructType>(pointer->base_type);
                    if (struct_ty->name == struct_name) {
                        auto struct_ptr_ty = struct_llvm_type->getPointerTo();
                        auto array_type = create_llvm_array_type(struct_ptr_ty, array->size);
                        struct_fields.push_back(array_type);
                        continue;
                    }
                }
            }
        }

        if (field->type_kind == amun::TypeKind::GENERIC_PARAMETER) {
            auto generic_type = std::static_pointer_cast<amun::GenericParameterType>(field);
            auto position = index_of(struct_type->generic_parameters, generic_type->name);
            struct_fields.push_back(llvm_type_from_amun_type(generic->parameters[position]));
            continue;
        }

        struct_fields.push_back(llvm_type_from_amun_type(field));
    }

    struct_llvm_type->setBody(struct_fields, struct_type->is_packed);
    structures_types_map[mangled_name] = struct_llvm_type;
    return struct_llvm_type;
}

inline auto amun::LLVMBackend::create_entry_block_alloca(llvm::Function* function,
                                                         const std::string var_name,
                                                         llvm::Type* type) -> llvm::AllocaInst*
{
    llvm::IRBuilder<> builder_object(&function->getEntryBlock(), function->getEntryBlock().begin());
    return builder_object.CreateAlloca(type, nullptr, var_name);
}

auto amun::LLVMBackend::create_switch_case_branch(llvm::SwitchInst* switch_inst,
                                                  llvm::Function* current_function,
                                                  llvm::BasicBlock* basic_block,
                                                  std::shared_ptr<SwitchCase> switch_case) -> void
{
    auto branch_block = llvm::BasicBlock::Create(llvm_context, "", current_function);
    Builder.SetInsertPoint(branch_block);

    bool body_has_return_statement = false;

    auto branch_body = switch_case->body;

    // If switch body is block, check if the last node is return statement or not,
    // if it not block, check if it return statement or not
    if (branch_body->get_ast_node_type() == AstNodeType::AST_BLOCK) {
        auto block = std::dynamic_pointer_cast<BlockStatement>(branch_body);
        auto nodes = block->statements;
        if (not nodes.empty()) {
            body_has_return_statement =
                nodes.back()->get_ast_node_type() == AstNodeType::AST_RETURN;
        }
    }
    else {
        body_has_return_statement = branch_body->get_ast_node_type() == AstNodeType::AST_RETURN;
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
    auto switch_case_values = switch_case->values;
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

auto amun::LLVMBackend::lookup_function(std::string& name) -> llvm::Function*
{
    if (auto* function = llvm_module->getFunction(name)) {
        return function;
    }

    auto function_prototype = functions_table.find(name);
    if (function_prototype != functions_table.end()) {
        return std::any_cast<llvm::Function*>(function_prototype->second->accept(this));
    }

    return llvm_functions[name];
}

inline auto amun::LLVMBackend::is_lambda_function_name(const std::string& name) -> bool
{
    return name.starts_with("_lambda");
}

inline auto amun::LLVMBackend::is_global_block() -> bool { return is_on_global_scope; }

inline auto amun::LLVMBackend::execute_defer_call(std::shared_ptr<amun::DeferCall>& defer_call)
    -> void
{
    if (defer_call->defer_kind == amun::DeferCallKind::DEFER_FUNCTION_CALL) {
        auto fun_call = std::static_pointer_cast<amun::DeferFunctionCall>(defer_call);
        Builder.CreateCall(fun_call->function, fun_call->arguments);
    }
    else if (defer_call->defer_kind == amun::DeferCallKind::DEFER_FUNCTION_CALL) {
        auto fun_ptr = std::static_pointer_cast<amun::DeferFunctionPtrCall>(defer_call);
        Builder.CreateCall(fun_ptr->function_type, fun_ptr->callee, fun_ptr->arguments);
    }
}

inline auto amun::LLVMBackend::execute_scope_defer_calls() -> void
{
    auto defer_calls = defer_calls_stack.top().get_scope_elements();
    for (auto& defer_call : defer_calls) {
        execute_defer_call(defer_call);
    }
}

inline auto amun::LLVMBackend::execute_all_defer_calls() -> void
{
    auto current_defer_stack = defer_calls_stack.top();

    const auto size = current_defer_stack.size();
    for (int i = size - 1; i >= 0; i--) {
        auto defer_calls = current_defer_stack.get_scope_elements(i);
        for (auto& defer_call : defer_calls) {
            execute_defer_call(defer_call);
        }
    }
}

inline auto amun::LLVMBackend::push_alloca_inst_scope() -> void
{
    alloca_inst_table.push_new_scope();
}

inline auto amun::LLVMBackend::pop_alloca_inst_scope() -> void
{
    alloca_inst_table.pop_current_scope();
}

inline auto amun::LLVMBackend::internal_compiler_error(const char* message) -> void
{
    amun::loge << "Internal Compiler Error: " << message << '\n';
    exit(EXIT_FAILURE);
}