#include "../include/jot_type.hpp"

bool JotNumberType::equals(const std::shared_ptr<JotType> &other) {
    if (auto other_number = std::dynamic_pointer_cast<JotNumberType>(other)) {
        return other_number->get_kind() == kind;
    }
    return false;
}

bool JotNumberType::castable(const std::shared_ptr<JotType> &other) {
    return other->get_type_kind() == TypeKind::Number;
}

bool JotPointerType::equals(const std::shared_ptr<JotType> &other) {
    if (auto other_pointer = std::dynamic_pointer_cast<JotPointerType>(other)) {
        return other_pointer->get_point_to()->equals(this->get_point_to());
    }
    if (auto other_array = std::dynamic_pointer_cast<JotArrayType>(other)) {
        return get_point_to()->equals(other_array->get_element_type());
    }
    return false;
}

bool JotPointerType::castable(const std::shared_ptr<JotType> &other) {
    // *void can be casted to any thing
    if (this->get_point_to()->get_type_kind() == TypeKind::Void) {
        return true;
    }
    return false;
}

bool JotArrayType::equals(const std::shared_ptr<JotType> &other) {
    if (other->get_type_kind() == TypeKind::Array) {
        auto other_array = std::dynamic_pointer_cast<JotArrayType>(other);
        return other_array->get_size() == size &&
               other_array->get_element_type()->equals(element_type);
    }
    if (other->get_type_kind() == TypeKind::Pointer) {
        auto other_pointer = std::dynamic_pointer_cast<JotPointerType>(other);
        return other_pointer->get_point_to()->equals(get_element_type());
    }
    return false;
}

bool JotArrayType::castable(const std::shared_ptr<JotType> &other) {
    // Array of type T can be casted to a pointer of type T
    if (auto other_pointer = std::dynamic_pointer_cast<JotPointerType>(other)) {
        return element_type->equals(other_pointer->get_point_to());
    }
    return false;
}

bool JotFunctionType::equals(const std::shared_ptr<JotType> &other) {
    if (auto other_function = std::dynamic_pointer_cast<JotFunctionType>(other)) {
        size_t parameter_size = other_function->get_parameters().size();
        if (parameter_size != parameters.size())
            return false;
        auto other_parameters = other_function->get_parameters();
        for (size_t i = 0; i < parameter_size; i++) {
            if (not parameters[i]->equals(other_parameters[i]))
                return false;
        }
        return return_type->equals(other_function->get_return_type());
    }
    return false;
}

bool JotFunctionType::castable(const std::shared_ptr<JotType> &other) { return false; }

bool JotEnumType::equals(const std::shared_ptr<JotType> &other) { return false; }

bool JotEnumType::castable(const std::shared_ptr<JotType> &other) { return false; }

bool JotEnumElementType::equals(const std::shared_ptr<JotType> &other) {
    if (other->get_type_kind() == TypeKind::EnumerationElement) {
        auto other_enum_type = std::dynamic_pointer_cast<JotEnumElementType>(other);
        if (other_enum_type->get_type_token().get_literal() == get_type_token().get_literal()) {
            return true;
        }
        return false;
    }
    return false;
}

bool JotEnumElementType::castable(const std::shared_ptr<JotType> &other) { return false; }

bool JotNoneType::equals(const std::shared_ptr<JotType> &other) { return false; }

bool JotNoneType::castable(const std::shared_ptr<JotType> &other) { return false; }

bool JotVoidType::equals(const std::shared_ptr<JotType> &other) {
    return other->get_type_kind() == TypeKind::Void;
}

bool JotVoidType::castable(const std::shared_ptr<JotType> &other) { return false; }

bool JotNullType::equals(const std::shared_ptr<JotType> &other) {
    return other->get_type_kind() == TypeKind::Pointer || other->get_type_kind() == TypeKind::Void;
}

bool JotNullType::castable(const std::shared_ptr<JotType> &other) { return false; }
