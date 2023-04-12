#include "../include/jot_llvm_backend.hpp"
#include "../include/jot_ast_visitor.hpp"
#include "../include/jot_llvm_intrinsic.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_name_mangle.hpp"
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
#include <vector>

auto JotLLVMBackend::compile(std::string module_name,
                             std::shared_ptr<CompilationUnit> compilation_unit)
    -> std::unique_ptr<llvm::Module>
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

auto JotLLVMBackend::visit(BlockStatement* node) -> std::any
{
    push_alloca_inst_scope();
    defer_calls_stack.top().push_new_scope();
    bool should_execute_defers = true;
    for (auto& statement : node->get_nodes()) {
        const auto ast_node_type = statement->get_ast_node_type();
        if (ast_node_type == AstNodeType::Return) {
            should_execute_defers = false;
            execute_all_defer_calls();
        }

        statement->accept(this);

        // In the same block there are no needs to generate code for unreachable code
        if (ast_node_type == AstNodeType::Return or ast_node_type == AstNodeType::Break or
            ast_node_type == AstNodeType::Continue) {
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

auto JotLLVMBackend::visit(FieldDeclaration* node) -> std::any
{
    auto var_name = node->get_name().literal;
    auto field_type = node->get_type();
    if (field_type->type_kind == TypeKind::GENERIC_PARAMETER) {
        auto generic = std::static_pointer_cast<JotGenericParameterType>(field_type);
        field_type = generic_types[generic->name];
    }

    llvm::Type* llvm_type;
    if (field_type->type_kind == TypeKind::GENERIC_STRUCT) {
        auto generic_type = std::static_pointer_cast<JotGenericStructType>(field_type);
        llvm_type = resolve_generic_struct(generic_type);
    }
    else {
        llvm_type = llvm_type_from_jot_type(field_type);
    }

    // Globals code generation block can be moved into other function to be clear and handle more
    // cases and to handle also soem compile time evaluations
    if (node->is_global()) {
        // if field has initalizer evaluate it, else initalize it with default value
        llvm::Constant* constants_value;
        if (node->get_value() == nullptr) {
            constants_value = create_llvm_null(llvm_type_from_jot_type(field_type));
        }
        else {
            constants_value = resolve_constant_expression(node->get_value());
        }

        auto global_variable =
            new llvm::GlobalVariable(*llvm_module, llvm_type, false,
                                     llvm::GlobalValue::ExternalLinkage, constants_value, var_name);
        global_variable->setAlignment(llvm::MaybeAlign(0));
        return 0;
    }

    // if field has initalizer evaluate it, else initalize it with default value
    std::any value;
    if (node->get_value() == nullptr) {
        value = create_llvm_null(llvm_type_from_jot_type(field_type));
    }
    else {
        value = node->get_value()->accept(this);
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
    else {
        internal_compiler_error("Un supported rvalue for field declaration");
    }
    return 0;
}

auto JotLLVMBackend::visit(FunctionPrototype* node) -> std::any
{
    auto parameters = node->get_parameters();
    size_t parameters_size = parameters.size();
    std::vector<llvm::Type*> arguments(parameters_size);
    for (size_t i = 0; i < parameters_size; i++) {
        arguments[i] = llvm_type_from_jot_type(parameters[i]->type);
    }

    auto return_type = llvm_type_from_jot_type(node->get_return_type());
    auto function_type = llvm::FunctionType::get(return_type, arguments, node->has_varargs());
    auto function_name = node->get_name().literal;
    auto linkage = node->is_external() || function_name == "main" ? llvm::Function::ExternalLinkage
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

auto JotLLVMBackend::visit(IntrinsicPrototype* node) -> std::any
{
    auto name = node->name.literal;
    auto prototype_parameters = node->parameters;

    std::vector<llvm::Type*> parameters_types;
    parameters_types.reserve(prototype_parameters.size());
    for (const auto& parameters : prototype_parameters) {
        parameters_types.push_back(llvm_type_from_jot_type(parameters->type));
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

auto JotLLVMBackend::resolve_generic_function(FunctionDeclaration* node,
                                              std::vector<Shared<JotType>> generic_parameters)
    -> llvm::Function*
{

    is_on_global_scope = false;
    auto prototype = node->get_prototype();
    auto name = prototype->get_name().literal;
    auto mangled_name = name + mangle_types(generic_parameters);

    if (alloca_inst_table.is_defined(mangled_name)) {
        auto value = alloca_inst_table.lookup(mangled_name);
        return std::any_cast<llvm::Function*>(value);
    }

    auto return_type = prototype->return_type;
    if (prototype->return_type->type_kind == TypeKind::GENERIC_PARAMETER) {
        auto generic_type = std::static_pointer_cast<JotGenericParameterType>(return_type);
        auto position = index_of(prototype->generic_parameters, generic_type->name);
        generic_types[generic_type->name] = generic_parameters[position];
        return_type = generic_parameters[position];
    }

    std::vector<llvm::Type*> arguments;
    for (const auto& parameter : prototype->parameters) {
        if (parameter->type->type_kind == TypeKind::GENERIC_PARAMETER) {
            auto generic_type = std::static_pointer_cast<JotGenericParameterType>(parameter->type);
            auto position = index_of(prototype->generic_parameters, generic_type->name);
            generic_types[generic_type->name] = generic_parameters[position];
            arguments.push_back(llvm_type_from_jot_type(generic_parameters[position]));
        }
        else {
            arguments.push_back(llvm_type_from_jot_type(parameter->type));
        }
    }

    functions_table[mangled_name] = prototype;

    auto linkage =
        name == "main" ? llvm::Function::ExternalLinkage : llvm::Function::InternalLinkage;

    auto function_type =
        llvm::FunctionType::get(llvm_type_from_jot_type(return_type), arguments, false);

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

    const auto& body = node->get_body();
    body->accept(this);

    pop_alloca_inst_scope();
    defer_calls_stack.pop();

    alloca_inst_table.define(mangled_name, function);

    // Assert that this block end with return statement or unreachable
    if (body->get_ast_node_type() == AstNodeType::Block) {
        const auto& body_statement = std::dynamic_pointer_cast<BlockStatement>(body);
        const auto& statements = body_statement->statements;
        if (statements.empty() || statements.back()->get_ast_node_type() != AstNodeType::Return) {
            Builder.CreateUnreachable();
        }
    }
    verifyFunction(*function);

    has_return_statement = false;
    is_on_global_scope = true;

    Builder.SetInsertPoint(previous_insert_block);

    return function;
}

auto JotLLVMBackend::visit(FunctionDeclaration* node) -> std::any
{
    is_on_global_scope = false;
    auto prototype = node->get_prototype();
    auto name = prototype->get_name().literal;
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

    const auto& body = node->get_body();
    body->accept(this);

    pop_alloca_inst_scope();
    defer_calls_stack.pop();

    alloca_inst_table.define(name, function);

    // Assert that this block end with return statement or unreachable
    if (body->get_ast_node_type() == AstNodeType::Block) {
        const auto& body_statement = std::dynamic_pointer_cast<BlockStatement>(body);
        const auto& statements = body_statement->statements;
        if (statements.empty() || statements.back()->get_ast_node_type() != AstNodeType::Return) {
            Builder.CreateUnreachable();
        }
    }

    verifyFunction(*function);

    has_return_statement = false;
    is_on_global_scope = true;
    return function;
}

auto JotLLVMBackend::visit(StructDeclaration* node) -> std::any
{
    const auto struct_type = node->get_struct_type();

    // Generic Struct is a template and should be defined only when used
    if (struct_type->is_generic) {
        return 0;
    }

    const auto struct_name = struct_type->name;
    const auto struct_llvm_type = llvm::StructType::create(llvm_context);
    struct_llvm_type->setName(struct_name);

    const auto fields = struct_type->fields_types;
    std::vector<llvm::Type*> struct_fields;
    struct_fields.reserve(fields.size());

    for (const auto& field : fields) {

        // Handle case where field type is pointer to the current struct
        if (field->type_kind == TypeKind::POINTER) {
            auto pointer = std::static_pointer_cast<JotPointerType>(field);
            if (pointer->base_type->type_kind == TypeKind::STRUCT) {
                auto struct_ty = std::static_pointer_cast<JotStructType>(pointer->base_type);
                if (struct_ty->name == struct_name) {
                    struct_fields.push_back(struct_llvm_type->getPointerTo());
                    continue;
                }
            }
        }

        // Handle case where field type is array of pointers to the current struct
        if (field->type_kind == TypeKind::ARRAY) {
            auto array = std::static_pointer_cast<JotArrayType>(field);
            if (array->element_type->type_kind == TypeKind::POINTER) {
                auto pointer = std::static_pointer_cast<JotPointerType>(array->element_type);
                if (pointer->base_type->type_kind == TypeKind::STRUCT) {
                    auto struct_ty = std::static_pointer_cast<JotStructType>(pointer->base_type);
                    if (struct_ty->name == struct_name) {
                        auto struct_ptr_ty = struct_llvm_type->getPointerTo();
                        auto array_type = create_llvm_array_type(struct_ptr_ty, array->size);
                        struct_fields.push_back(array_type);
                        continue;
                    }
                }
            }
        }

        struct_fields.push_back(llvm_type_from_jot_type(field));
    }

    struct_llvm_type->setBody(struct_fields, struct_type->is_packed);
    structures_types_map[struct_name] = struct_llvm_type;
    return 0;
}

auto JotLLVMBackend::visit([[maybe_unused]] EnumDeclaration* node) -> std::any
{
    // Enumeration type is only compile time type, no need to generate any IR for it
    return 0;
}

auto JotLLVMBackend::visit(IfStatement* node) -> std::any
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

auto JotLLVMBackend::visit(ForRangeStatement* node) -> std::any
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
        auto number_type = std::static_pointer_cast<JotNumberType>(node_type);
        step = llvm_number_value("1", number_type->number_kind);
    }

    start = create_llvm_numbers_bianry(TokenKind::Minus, start, step);
    end = create_llvm_numbers_bianry(TokenKind::Minus, end, step);

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
    auto condition = create_llvm_numbers_comparison(TokenKind::SmallerEqual, variable, end);
    Builder.CreateCondBr(condition, body_block, end_block);

    // Generate For body IR Code
    current_function->getBasicBlockList().push_back(body_block);
    Builder.SetInsertPoint(body_block);

    // Increment loop variable
    auto value_ptr = Builder.CreateLoad(alloc_inst->getAllocatedType(), alloc_inst);
    auto new_value = create_llvm_numbers_bianry(TokenKind::Plus, value_ptr, step);
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

auto JotLLVMBackend::visit(ForEachStatement* node) -> std::any
{
    auto collection_expression = node->collection;
    auto collection_exp_type = collection_expression->get_type_node();
    auto collection_value = llvm_node_value(collection_expression->accept(this));
    auto collection = llvm_resolve_value(collection_value);
    auto collection_type = collection->getType();

    auto is_foreach_string = is_jot_types_equals(collection_exp_type, jot_int8ptr_ty);

    auto zero_value = create_llvm_int64(-1, true);
    auto step = create_llvm_int64(1, true);

    auto length = is_foreach_string
                      ? create_llvm_string_length(collection)
                      : create_llvm_int64(collection_type->getArrayNumElements(), true);

    auto end = create_llvm_integers_bianry(TokenKind::Minus, length, step);
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

    // Resolve it to be collection[it_index]
    const auto element_name = node->element_name;
    auto element_type = is_foreach_string ? llvm_int8_type : collection_type->getArrayElementType();
    const auto element_alloca =
        create_entry_block_alloca(current_function, element_name, element_type);

    Builder.CreateBr(condition_block);

    // Generate condition block
    current_function->getBasicBlockList().push_back(condition_block);
    Builder.SetInsertPoint(condition_block);
    auto variable = derefernecs_llvm_pointer(index_alloca);
    auto condition = create_llvm_numbers_comparison(TokenKind::Smaller, variable, end);
    Builder.CreateCondBr(condition, body_block, end_block);

    // Generate For body IR Code
    current_function->getBasicBlockList().push_back(body_block);
    Builder.SetInsertPoint(body_block);

    // Increment loop variable
    auto value_ptr = Builder.CreateLoad(index_alloca->getAllocatedType(), index_alloca);
    auto new_value = create_llvm_numbers_bianry(TokenKind::Plus, value_ptr, step);
    Builder.CreateStore(new_value, index_alloca);

    // If array expression is passed directly we should first save it on temp variable
    if (node->collection->get_ast_node_type() == AstNodeType::ArrayExpr) {
        auto temp_name = "_temp";
        auto temp_alloca = create_entry_block_alloca(current_function, temp_name, collection_type);
        Builder.CreateStore(collection, temp_alloca);
        alloca_inst_table.define(temp_name, temp_alloca);

        auto collection_jot_type = node->collection->get_type_node();

        auto location = TokenSpan();
        auto token = Token{TokenKind::Symbol, location, temp_name};
        collection_expression = std::make_shared<LiteralExpression>(token, collection_jot_type);
    }

    // Update it variable with the element in the current index
    auto current_index = derefernecs_llvm_pointer(index_alloca);
    auto value = access_array_element(collection_expression, current_index);
    Builder.CreateStore(value, element_alloca);
    alloca_inst_table.define(element_name, element_alloca);

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

auto JotLLVMBackend::visit(ForeverStatement* node) -> std::any
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

auto JotLLVMBackend::visit(WhileStatement* node) -> std::any
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

auto JotLLVMBackend::visit(SwitchStatement* node) -> std::any
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

auto JotLLVMBackend::visit(ReturnStatement* node) -> std::any
{
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
    else if (value.type() == typeid(llvm::ConstantInt*)) {
        auto init_value = std::any_cast<llvm::ConstantInt*>(value);
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

auto JotLLVMBackend::visit(DeferStatement* node) -> std::any
{
    auto call_expression = node->get_call_expression();
    auto callee = std::dynamic_pointer_cast<LiteralExpression>(call_expression->get_callee());
    auto callee_literal = callee->get_name().literal;
    auto function = lookup_function(callee_literal);
    if (not function) {
        auto value = llvm_node_value(alloca_inst_table.lookup(callee_literal));
        if (auto alloca = llvm::dyn_cast<llvm::AllocaInst>(value)) {
            auto loaded = Builder.CreateLoad(alloca->getAllocatedType(), alloca);
            auto function_type = llvm_type_from_jot_type(call_expression->get_type_node());
            if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                auto arguments = call_expression->get_arguments();
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
                auto defer_function_call = std::make_shared<DeferFunctionPtrCall>(
                    function_pointer, loaded, arguments_values);
                defer_calls_stack.top().push_front(defer_function_call);
            }
        }
        return 0;
    }

    auto arguments = call_expression->get_arguments();
    auto arguments_size = arguments.size();
    auto parameter_size = function->arg_size();
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
    defer_calls_stack.top().push_front(defer_function_call);
    return 0;
}

auto JotLLVMBackend::visit(BreakStatement* node) -> std::any
{
    has_break_or_continue_statement = true;

    for (int i = 1; i < node->get_times(); i++) {
        break_blocks_stack.pop();
    }

    Builder.CreateBr(break_blocks_stack.top());
    return 0;
}

auto JotLLVMBackend::visit(ContinueStatement* node) -> std::any
{
    has_break_or_continue_statement = true;

    for (int i = 1; i < node->get_times(); i++) {
        continue_blocks_stack.pop();
    }

    Builder.CreateBr(continue_blocks_stack.top());
    return 0;
}

auto JotLLVMBackend::visit(ExpressionStatement* node) -> std::any
{
    node->get_expression()->accept(this);
    return 0;
}

auto JotLLVMBackend::visit(IfExpression* node) -> std::any
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

auto JotLLVMBackend::visit(SwitchExpression* node) -> std::any
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

auto JotLLVMBackend::visit(GroupExpression* node) -> std::any
{
    return node->get_expression()->accept(this);
}

auto JotLLVMBackend::visit(AssignExpression* node) -> std::any
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

            alloca_inst_table.update(name, alloca);
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
                    llvm_node_value(alloca_inst_table.lookup(array_literal->get_name().literal)));
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
                return Builder.CreateStore(right_value, ptr);
            }
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

auto JotLLVMBackend::visit(BinaryExpression* node) -> std::any
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

auto JotLLVMBackend::visit(ShiftExpression* node) -> std::any
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

auto JotLLVMBackend::visit(ComparisonExpression* node) -> std::any
{
    const auto left_node = node->get_left();
    const auto right_node = node->get_right();
    const auto left = llvm_resolve_value(left_node->accept(this));
    const auto right = llvm_resolve_value(right_node->accept(this));
    const auto op = node->get_operator_token().kind;

    const auto left_jot_type = left_node->get_type_node();
    const auto right_jot_type = right_node->get_type_node();

    const auto left_type = left->getType();
    const auto right_type = right->getType();

    // Comparison Operations for integers types
    if (left_type->isIntegerTy() and right_type->isIntegerTy()) {
        if (is_unsigned_integer_type(left_jot_type)) {
            return create_llvm_unsigned_integers_comparison(op, left, right);
        }
        return create_llvm_integers_comparison(op, left, right);
    }

    // Comparison Operations for floating point types
    if (left_type->isFloatingPointTy() and right_type->isFloatingPointTy()) {
        return create_llvm_floats_comparison(op, left, right);
    }

    // Comparison Operations for pointers types thay points to the same type, no need for casting
    if (left_type->isPointerTy() and right_type->isPointerTy()) {

        // Can be optimized by checking if both sides are String literal expression
        if (left_node->get_ast_node_type() == AstNodeType::StringExpr &&
            right_node->get_ast_node_type() == AstNodeType::StringExpr) {
            auto left_str = std::dynamic_pointer_cast<StringExpression>(left_node)->value.literal;
            auto right_str = std::dynamic_pointer_cast<StringExpression>(right_node)->value.literal;
            auto compare = std::strcmp(left_str.c_str(), right_str.c_str());
            auto result_llvm = create_llvm_int32(compare, true);
            return create_llvm_integers_comparison(op, result_llvm, zero_int32_value);
        }

        // If both sides are strings use strcmp function
        if (is_jot_types_equals(left_jot_type, jot_int8ptr_ty) &&
            is_jot_types_equals(right_jot_type, jot_int8ptr_ty)) {
            return create_llvm_strings_comparison(op, left, right);
        }

        // Compare address of both pointers
        return create_llvm_integers_comparison(op, left, right);
    }

    // This line must be unreacable and any type error must handled in type checker pass
    internal_compiler_error("Binary Operators work only for Numeric types");
}

auto JotLLVMBackend::visit(LogicalExpression* node) -> std::any
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

auto JotLLVMBackend::visit(PrefixUnaryExpression* node) -> std::any
{
    auto operand = node->get_right();
    auto operator_kind = node->get_operator_token().kind;

    // Unary - minus operator
    if (operator_kind == TokenKind::Minus) {
        auto right = llvm_resolve_value(operand->accept(this));
        if (right->getType()->isFloatingPointTy()) {
            return Builder.CreateFNeg(right);
        }
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
        auto is_expect_struct_type = node->get_type_node()->type_kind == TypeKind::STRUCT;
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

auto JotLLVMBackend::visit(PostfixUnaryExpression* node) -> std::any
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

auto JotLLVMBackend::visit(CallExpression* node) -> std::any
{
    auto callee_ast_node_type = node->get_callee()->get_ast_node_type();

    // If callee is also a CallExpression this case when you have a function that return a
    // function pointer and you call it for example function()();
    if (callee_ast_node_type == AstNodeType::CallExpr) {
        auto callee_function = llvm_node_value(node->get_callee()->accept(this));
        auto call_instruction = llvm::dyn_cast<llvm::CallInst>(callee_function);
        auto function = call_instruction->getCalledFunction();
        auto callee_function_type = function->getFunctionType();
        auto return_ptr_type = callee_function_type->getReturnType()->getPointerElementType();
        auto function_pointer_type = llvm::dyn_cast<llvm::FunctionType>(return_ptr_type);

        auto arguments = node->get_arguments();
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
    if (callee_ast_node_type == AstNodeType::LiteralExpr) {
        auto callee = std::dynamic_pointer_cast<LiteralExpression>(node->get_callee());
        auto callee_literal = callee->get_name().literal;
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
                auto function_type = llvm_type_from_jot_type(node->get_type_node());
                if (auto function_pointer = llvm::dyn_cast<llvm::FunctionType>(function_type)) {
                    auto arguments = node->get_arguments();
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

        auto arguments = node->get_arguments();
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
                if (argument->get_ast_node_type() == AstNodeType::LiteralExpr) {
                    auto argument_type = llvm_type_from_jot_type(argument->get_type_node());
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
    if (callee_ast_node_type == AstNodeType::LambdaExpr) {
        auto lambda = std::dynamic_pointer_cast<LambdaExpression>(node->get_callee());
        auto lambda_value = llvm_node_value(lambda->accept(this));
        auto function = llvm::dyn_cast<llvm::Function>(lambda_value);

        auto arguments = node->get_arguments();
        auto arguments_size = arguments.size();
        auto parameter_size = function->arg_size();
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

    // If callee is dot expression that mean we call function pointer from struct element
    if (callee_ast_node_type == AstNodeType::DotExpr) {
        auto dot = std::dynamic_pointer_cast<DotExpression>(node->get_callee());
        auto struct_fun_ptr = llvm_node_value(dot->accept(this));

        auto function_value = derefernecs_llvm_pointer(struct_fun_ptr);

        auto function_ptr_type = std::static_pointer_cast<JotPointerType>(dot->get_type_node());
        auto llvm_type = llvm_type_from_jot_type(function_ptr_type->base_type);
        auto llvm_fun_type = llvm::dyn_cast<llvm::FunctionType>(llvm_type);

        auto arguments = node->get_arguments();
        auto arguments_size = arguments.size();
        auto parameter_size = llvm_fun_type->getNumParams();
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

auto JotLLVMBackend::visit(InitializeExpression* node) -> std::any
{

    // Convert struct and resolve it first if it generic to LLVM struct type
    llvm::Type* struct_type;
    if (node->type->type_kind == TypeKind::GENERIC_STRUCT) {
        auto generic = std::static_pointer_cast<JotGenericStructType>(node->type);
        struct_type = resolve_generic_struct(generic);
    }
    else {
        struct_type = llvm_type_from_jot_type(node->type);
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

auto JotLLVMBackend::visit(LambdaExpression* node) -> std::any
{
    auto lambda_name = "_lambda" + std::to_string(lambda_unique_id++);
    auto function_ptr_type = std::static_pointer_cast<JotPointerType>(node->get_type_node());
    auto node_llvm_type = llvm_type_from_jot_type(function_ptr_type->base_type);
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

auto JotLLVMBackend::visit(DotExpression* node) -> std::any
{
    auto callee = node->get_callee();
    auto callee_type = callee->get_type_node();
    auto callee_llvm_type = llvm_type_from_jot_type(callee_type);
    auto expected_llvm_type = llvm_type_from_jot_type(node->get_type_node());

    // Compile array attributes
    if (callee_llvm_type->isArrayTy()) {
        if (node->get_field_name().literal == "count") {
            auto llvm_array_type = llvm::dyn_cast<llvm::ArrayType>(callee_llvm_type);
            auto length = llvm_array_type->getArrayNumElements();
            return create_llvm_int64(length, true);
        }

        internal_compiler_error("Invalid Array Attribute");
    }

    // Compile String literal attributes
    if (is_pointer_of_type(callee_type, jot_int8_ty)) {
        if (node->get_field_name().literal == "count") {

            // If node is string expression, length can calculated without strlen
            if (node->callee->get_ast_node_type() == AstNodeType::StringExpr) {
                auto string = std::dynamic_pointer_cast<StringExpression>(node->callee);
                auto length = string->value.literal.size();
                return create_llvm_int64(length, true);
            }

            auto string_ptr = llvm_node_value(callee->accept(this));

            // Check if it **int8 that mean the string is store in variable and should dereferneceed
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

auto JotLLVMBackend::visit(CastExpression* node) -> std::any
{
    auto value = llvm_resolve_value(node->get_value()->accept(this));
    auto value_type = llvm_type_from_jot_type(node->get_value()->get_type_node());
    auto target_type = llvm_type_from_jot_type(node->get_type_node());

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
        auto load_inst = dyn_cast<llvm::LoadInst>(value);
        return Builder.CreateGEP(value->getType(), load_inst->getPointerOperand(),
                                 {zero_int32_value, zero_int32_value});
    }

    // Bit casting
    return Builder.CreateBitCast(value, target_type);
}

auto JotLLVMBackend::visit(TypeSizeExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_jot_type(node->get_type());
    auto type_alloc_size = llvm_module->getDataLayout().getTypeAllocSize(llvm_type);
    auto type_size = llvm::ConstantInt::get(llvm_int64_type, type_alloc_size);
    return type_size;
}

auto JotLLVMBackend::visit(ValueSizeExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_jot_type(node->get_value()->get_type_node());
    auto type_alloc_size = llvm_module->getDataLayout().getTypeAllocSize(llvm_type);
    auto type_size = llvm::ConstantInt::get(llvm_int64_type, type_alloc_size);
    return type_size;
}

auto JotLLVMBackend::visit(IndexExpression* node) -> std::any
{
    auto index = llvm_resolve_value(node->get_index()->accept(this));
    return access_array_element(node->get_value(), index);
}

auto JotLLVMBackend::visit(EnumAccessExpression* node) -> std::any
{
    auto element_type = llvm_type_from_jot_type(node->get_type_node());
    auto element_index = llvm::ConstantInt::get(element_type, node->get_enum_element_index());
    return llvm::dyn_cast<llvm::Value>(element_index);
}

auto JotLLVMBackend::visit(LiteralExpression* node) -> std::any
{
    const auto name = node->get_name().literal;
    // If found in alloca inst table that mean it local variable
    auto alloca_inst = alloca_inst_table.lookup(name);
    if (alloca_inst.type() != typeid(nullptr)) {
        return alloca_inst;
    }
    // If it not in alloca inst table,that mean it global variable
    return llvm_module->getNamedGlobal(name);
}

auto JotLLVMBackend::visit(NumberExpression* node) -> std::any
{
    auto number_type = std::static_pointer_cast<JotNumberType>(node->get_type_node());
    return llvm_number_value(node->get_value().literal, number_type->number_kind);
}

auto JotLLVMBackend::visit(ArrayExpression* node) -> std::any
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

    auto arrayType = llvm_type_from_jot_type(node->get_type_node());

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

auto JotLLVMBackend::visit(StringExpression* node) -> std::any
{
    std::string literal = node->get_value().literal;
    return resolve_constant_string_expression(literal);
}

auto JotLLVMBackend::visit(CharacterExpression* node) -> std::any
{
    char char_asci_value = node->get_value().literal[0];
    return create_llvm_int8(char_asci_value, false);
}

auto JotLLVMBackend::visit(BooleanExpression* node) -> std::any
{
    return create_llvm_int1(node->get_value().kind == TokenKind::TrueKeyword);
}

auto JotLLVMBackend::visit(NullExpression* node) -> std::any
{
    auto llvm_type = llvm_type_from_jot_type(node->null_base_type);
    return create_llvm_null(llvm_type);
}

auto JotLLVMBackend::llvm_node_value(std::any any_value) -> llvm::Value*
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

auto JotLLVMBackend::llvm_resolve_value(std::any any_value) -> llvm::Value*
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

auto JotLLVMBackend::llvm_resolve_variable(const std::string& name) -> llvm::Value*
{
    // If found in alloca inst table that mean it local variable
    auto alloca_inst = alloca_inst_table.lookup(name);
    if (alloca_inst.type() != typeid(nullptr)) {
        return llvm_node_value(alloca_inst);
    }
    // If it not in alloca inst table,that mean it global variable
    return llvm_module->getNamedGlobal(name);
}

inline auto JotLLVMBackend::llvm_number_value(const std::string& value_litearl, NumberKind size)
    -> llvm::Value*
{
    switch (size) {
    case NumberKind::INTEGER_1: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int1_type, value);
    }
    case NumberKind::INTEGER_8: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int8_type, value);
    }
    case NumberKind::U_INTEGER_8: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int8_type, value, true);
    }
    case NumberKind::INTEGER_16: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int16_type, value);
    }
    case NumberKind::U_INTEGER_16: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int16_type, value, true);
    }
    case NumberKind::INTEGER_32: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int32_type, value);
    }
    case NumberKind::U_INTEGER_32: {
        auto value = std::stoi(value_litearl);
        return llvm::ConstantInt::get(llvm_int32_type, value, true);
    }
    case NumberKind::INTEGER_64: {
        auto value = std::strtoll(value_litearl.c_str(), nullptr, 0);
        return llvm::ConstantInt::get(llvm_int64_type, value);
    }
    case NumberKind::U_INTEGER_64: {
        auto value = std::strtoll(value_litearl.c_str(), nullptr, 0);
        return llvm::ConstantInt::get(llvm_int64_type, value, true);
    }
    case NumberKind::FLOAT_32: {
        auto value = std::stod(value_litearl);
        return llvm::ConstantFP::get(llvm_float32_type, value);
    }
    case NumberKind::FLOAT_64: {
        auto value = std::stod(value_litearl);
        return llvm::ConstantFP::get(llvm_float64_type, value);
    }
    }
}

auto JotLLVMBackend::llvm_type_from_jot_type(std::shared_ptr<JotType> type) -> llvm::Type*
{
    TypeKind type_kind = type->type_kind;
    if (type_kind == TypeKind::NUMBER) {
        auto jot_number_type = std::static_pointer_cast<JotNumberType>(type);
        NumberKind number_kind = jot_number_type->number_kind;
        switch (number_kind) {
        case NumberKind::INTEGER_1: return llvm_int1_type;
        case NumberKind::INTEGER_8: return llvm_int8_type;
        case NumberKind::INTEGER_16: return llvm_int16_type;
        case NumberKind::INTEGER_32: return llvm_int32_type;
        case NumberKind::INTEGER_64: return llvm_int64_type;
        case NumberKind::U_INTEGER_8: return llvm_int8_type;
        case NumberKind::U_INTEGER_16: return llvm_int16_type;
        case NumberKind::U_INTEGER_32: return llvm_int32_type;
        case NumberKind::U_INTEGER_64: return llvm_int64_type;
        case NumberKind::FLOAT_32: return llvm_float32_type;
        case NumberKind::FLOAT_64: return llvm_float64_type;
        }
    }

    if (type_kind == TypeKind::ARRAY) {
        auto jot_array_type = std::static_pointer_cast<JotArrayType>(type);
        auto element_type = llvm_type_from_jot_type(jot_array_type->element_type);
        auto size = jot_array_type->size;
        return llvm::ArrayType::get(element_type, size);
    }

    if (type_kind == TypeKind::POINTER) {
        auto jot_pointer_type = std::static_pointer_cast<JotPointerType>(type);
        // In llvm *void should be generated as *i8
        if (jot_pointer_type->base_type->type_kind == TypeKind::VOID) {
            return llvm_void_ptr_type;
        }
        auto point_to_type = llvm_type_from_jot_type(jot_pointer_type->base_type);
        return llvm::PointerType::get(point_to_type, 0);
    }

    if (type_kind == TypeKind::FUNCTION) {
        auto jot_function_type = std::static_pointer_cast<JotFunctionType>(type);
        auto parameters = jot_function_type->parameters;
        int parameters_size = parameters.size();
        std::vector<llvm::Type*> arguments(parameters_size);
        for (int i = 0; i < parameters_size; i++) {
            arguments[i] = llvm_type_from_jot_type(parameters[i]);
        }
        auto return_type = llvm_type_from_jot_type(jot_function_type->return_type);
        auto function_type = llvm::FunctionType::get(return_type, arguments, false);
        return function_type;
    }

    if (type_kind == TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<JotStructType>(type);
        auto struct_name = struct_type->name;
        return structures_types_map[struct_name];
    }

    if (type_kind == TypeKind::ENUM_ELEMENT) {
        auto enum_element_type = std::static_pointer_cast<JotEnumElementType>(type);
        return llvm_type_from_jot_type(enum_element_type->element_type);
    }

    if (type_kind == TypeKind::VOID) {
        return llvm_void_type;
    }

    if (type_kind == TypeKind::GENERIC_STRUCT) {
        auto generic_struct_type = std::static_pointer_cast<JotGenericStructType>(type);
        return resolve_generic_struct(generic_struct_type);
    }

    internal_compiler_error("Can't find LLVM Type for this Jot Type");
}

auto JotLLVMBackend::create_llvm_numbers_bianry(TokenKind op, llvm::Value* left, llvm::Value* right)
    -> llvm::Value*
{
    if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
        return create_llvm_integers_bianry(op, left, right);
    }

    if (left->getType()->isFloatingPointTy() && right->getType()->isFloatingPointTy()) {
        return create_llvm_floats_bianry(op, left, right);
    }

    internal_compiler_error("llvm binary operator with number expect integers or floats");
}

auto JotLLVMBackend::create_llvm_integers_bianry(TokenKind op, llvm::Value* left,
                                                 llvm::Value* right) -> llvm::Value*
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

auto JotLLVMBackend::create_llvm_floats_bianry(TokenKind op, llvm::Value* left, llvm::Value* right)
    -> llvm::Value*
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

auto JotLLVMBackend::create_llvm_numbers_comparison(TokenKind op, llvm::Value* left,
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

auto JotLLVMBackend::create_llvm_integers_comparison(TokenKind op, llvm::Value* left,
                                                     llvm::Value* right) -> llvm::Value*
{
    switch (op) {
    case TokenKind::EqualEqual: {
        return Builder.CreateICmpEQ(left, right);
    }
    case TokenKind::BangEqual: {
        return Builder.CreateICmpNE(left, right);
    }
    case TokenKind::Greater: {
        return Builder.CreateICmpSGT(left, right);
    }
    case TokenKind::GreaterEqual: {
        return Builder.CreateICmpSGE(left, right);
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

auto JotLLVMBackend::create_llvm_unsigned_integers_comparison(TokenKind op, llvm::Value* left,
                                                              llvm::Value* right) -> llvm::Value*
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
        return Builder.CreateICmpULT(left, right);
    }
    case TokenKind::SmallerEqual: {
        return Builder.CreateICmpULE(left, right);
    }
    default: {
        internal_compiler_error("Invalid Integers Comparison operator");
    }
    }
}

auto JotLLVMBackend::create_llvm_floats_comparison(TokenKind op, llvm::Value* left,
                                                   llvm::Value* right) -> llvm::Value*
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

auto JotLLVMBackend::create_llvm_strings_comparison(TokenKind op, llvm::Value* left,
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
    case TokenKind::EqualEqual: {
        return Builder.CreateICmpEQ(function_call, zero_int32_value);
    }
    case TokenKind::BangEqual: {
        return Builder.CreateICmpNE(function_call, zero_int32_value);
    }
    case TokenKind::Greater: {
        return Builder.CreateICmpSGT(function_call, zero_int32_value);
    }
    case TokenKind::GreaterEqual: {
        return Builder.CreateICmpSGE(function_call, zero_int32_value);
    }
    case TokenKind::Smaller: {
        return Builder.CreateICmpSLT(function_call, zero_int32_value);
    }
    case TokenKind::SmallerEqual: {
        return Builder.CreateICmpSLE(function_call, zero_int32_value);
    }
    default: {
        internal_compiler_error("Invalid strings Comparison operator");
    }
    }
}

auto JotLLVMBackend::create_llvm_value_increment(std::shared_ptr<Expression> operand,
                                                 bool is_prefix) -> llvm::Value*
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

auto JotLLVMBackend::create_llvm_value_decrement(std::shared_ptr<Expression> operand,
                                                 bool is_prefix) -> llvm::Value*
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

auto JotLLVMBackend::create_llvm_string_length(llvm::Value* string) -> llvm::Value*
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

auto JotLLVMBackend::access_struct_member_pointer(DotExpression* expression) -> llvm::Value*
{

    auto callee = expression->get_callee();
    auto callee_value = llvm_node_value(callee->accept(this));
    auto callee_llvm_type = llvm_type_from_jot_type(callee->get_type_node());

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
auto JotLLVMBackend::access_array_element(std::shared_ptr<Expression> node_value,
                                          llvm::Value* index) -> llvm::Value*
{
    auto values = node_value->get_type_node();

    if (values->type_kind == TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<JotPointerType>(values);
        auto element_type = llvm_type_from_jot_type(pointer_type->base_type);
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

auto JotLLVMBackend::resolve_constant_expression(std::shared_ptr<Expression> value)
    -> llvm::Constant*
{
    auto field_type = value->get_type_node();

    // If there are no value, return default value
    if (value == nullptr) {
        return create_llvm_null(llvm_type_from_jot_type(field_type));
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
    auto llvm_value = llvm_resolve_value(value->accept(this));
    return llvm::dyn_cast<llvm::Constant>(llvm_value);
}

auto JotLLVMBackend::resolve_constant_index_expression(std::shared_ptr<IndexExpression> expression)
    -> llvm::Constant*
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

auto JotLLVMBackend::resolve_constant_if_expression(std::shared_ptr<IfExpression> expression)
    -> llvm::Constant*
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

auto JotLLVMBackend::resolve_constant_switch_expression(
    std::shared_ptr<SwitchExpression> expression) -> llvm::Constant*
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

auto JotLLVMBackend::resolve_constant_string_expression(const std::string& literal)
    -> llvm::Constant*
{
    // Resolve constants string from string pool if it generated before
    if (constants_string_pool.contains(literal)) {
        return constants_string_pool[literal];
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

auto JotLLVMBackend::resolve_generic_struct(Shared<JotGenericStructType> generic)
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
        if (field->type_kind == TypeKind::POINTER) {
            auto pointer = std::static_pointer_cast<JotPointerType>(field);
            if (pointer->base_type->type_kind == TypeKind::STRUCT) {
                auto struct_ty = std::static_pointer_cast<JotStructType>(pointer->base_type);
                if (struct_ty->name == struct_name) {
                    struct_fields.push_back(struct_llvm_type->getPointerTo());
                    continue;
                }
            }
        }

        // Handle case where field type is array of pointers to the current struct
        if (field->type_kind == TypeKind::ARRAY) {
            auto array = std::static_pointer_cast<JotArrayType>(field);
            if (array->element_type->type_kind == TypeKind::POINTER) {
                auto pointer = std::static_pointer_cast<JotPointerType>(array->element_type);
                if (pointer->base_type->type_kind == TypeKind::STRUCT) {
                    auto struct_ty = std::static_pointer_cast<JotStructType>(pointer->base_type);
                    if (struct_ty->name == struct_name) {
                        auto struct_ptr_ty = struct_llvm_type->getPointerTo();
                        auto array_type = create_llvm_array_type(struct_ptr_ty, array->size);
                        struct_fields.push_back(array_type);
                        continue;
                    }
                }
            }
        }

        if (field->type_kind == TypeKind::GENERIC_PARAMETER) {
            auto generic_type = std::static_pointer_cast<JotGenericParameterType>(field);
            auto position = index_of(struct_type->generic_parameters, generic_type->name);
            struct_fields.push_back(llvm_type_from_jot_type(generic->parameters[position]));
            continue;
        }

        struct_fields.push_back(llvm_type_from_jot_type(field));
    }

    struct_llvm_type->setBody(struct_fields, struct_type->is_packed);
    structures_types_map[mangled_name] = struct_llvm_type;
    return struct_llvm_type;
}

inline auto JotLLVMBackend::create_entry_block_alloca(llvm::Function* function,
                                                      const std::string var_name, llvm::Type* type)
    -> llvm::AllocaInst*
{
    llvm::IRBuilder<> builder_object(&function->getEntryBlock(), function->getEntryBlock().begin());
    return builder_object.CreateAlloca(type, nullptr, var_name);
}

auto JotLLVMBackend::create_switch_case_branch(llvm::SwitchInst* switch_inst,
                                               llvm::Function* current_function,
                                               llvm::BasicBlock* basic_block,
                                               std::shared_ptr<SwitchCase> switch_case) -> void
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

auto JotLLVMBackend::lookup_function(std::string& name) -> llvm::Function*
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

inline auto JotLLVMBackend::is_lambda_function_name(const std::string& name) -> bool
{
    return name.starts_with("_lambda");
}

inline auto JotLLVMBackend::is_global_block() -> bool { return is_on_global_scope; }

inline auto JotLLVMBackend::execute_defer_call(std::shared_ptr<DeferCall>& defer_call) -> void
{
    if (defer_call->defer_kind == DeferCallKind::DEFER_FUNCTION_CALL) {
        auto fun_call = std::static_pointer_cast<DeferFunctionCall>(defer_call);
        Builder.CreateCall(fun_call->function, fun_call->arguments);
    }
    else if (defer_call->defer_kind == DeferCallKind::DEFER_FUNCTION_CALL) {
        auto fun_ptr = std::static_pointer_cast<DeferFunctionPtrCall>(defer_call);
        Builder.CreateCall(fun_ptr->function_type, fun_ptr->callee, fun_ptr->arguments);
    }
}

inline auto JotLLVMBackend::execute_scope_defer_calls() -> void
{
    auto defer_calls = defer_calls_stack.top().get_scope_elements();
    for (auto& defer_call : defer_calls) {
        execute_defer_call(defer_call);
    }
}

inline auto JotLLVMBackend::execute_all_defer_calls() -> void
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

inline auto JotLLVMBackend::push_alloca_inst_scope() -> void { alloca_inst_table.push_new_scope(); }

inline auto JotLLVMBackend::pop_alloca_inst_scope() -> void
{
    alloca_inst_table.pop_current_scope();
}

inline auto JotLLVMBackend::internal_compiler_error(const char* message) -> void
{
    jot::loge << "Internal Compiler Error: " << message << '\n';
    exit(EXIT_FAILURE);
}