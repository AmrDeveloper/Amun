#include "../include/jot_type.hpp"

bool JotNumber::equals(const std::shared_ptr<JotType> &other) {
    if (auto other_number = std::dynamic_pointer_cast<JotNumber>(other)) {
        return other_number->get_kind() == kind;
    }
    return kind == NumberKind::Integer64 && other->get_type_kind() == TypeKind::Pointer;
}

bool JotPointerType::equals(const std::shared_ptr<JotType> &other) {
    if (auto other_number = std::dynamic_pointer_cast<JotNumber>(other)) {
        return other_number->get_kind() == NumberKind::Integer64;
    }
    if (auto other_pointer = std::dynamic_pointer_cast<JotPointerType>(other)) {
        return other_pointer->get_point_to()->equals(this->get_point_to());
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
            if (!parameters[i]->equals(other_parameters[1]))
                return false;
        }
        return return_type->equals(other_function->get_return_type());
    }
    return false;
}

bool JotEnumType::equals(const std::shared_ptr<JotType> &other) { return false; }

bool JotNamedType::equals(const std::shared_ptr<JotType> &other) { return false; }

bool JotVoid::equals(const std::shared_ptr<JotType> &other) {
    return other->get_type_kind() == TypeKind::Void;
}

bool JotNull::equals(const std::shared_ptr<JotType> &other) {
    return other->get_type_kind() == TypeKind::Pointer || other->get_type_kind() == TypeKind::Void;
}