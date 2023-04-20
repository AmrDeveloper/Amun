#pragma once

#include <list>
#include <vector>

namespace amun {

template <typename T>
class ScopedList {
  public:
    ScopedList() = default;

    void push_front(T element) { linked_scopes.back().push_front(element); }

    void push_back(T element) { linked_scopes.back().push_back(element); }

    auto get_scope_elements() -> std::list<T> { return linked_scopes.back(); }

    auto get_scope_elements(std::size_t i) -> std::list<T> { return linked_scopes[i]; }

    auto push_new_scope() -> void { linked_scopes.push_back({}); }

    auto pop_current_scope() -> void { linked_scopes.pop_back(); }

    auto size() -> std::size_t { return linked_scopes.size(); }

  private:
    std::vector<std::list<T>> linked_scopes;
};

} // namespace amun