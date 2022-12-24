#pragma once

#include <list>
#include <vector>

template <typename T>
class JotScopedList {
  public:
    JotScopedList() = default;

    void push_front(T element) { linked_scopes.back().push_front(element); }

    void push_back(T element) { linked_scopes.back().push_back(element); }

    std::list<T> get_scope_elements() { return linked_scopes.back(); }

    std::list<T> get_scope_elements(size_t i) { return linked_scopes[i]; }

    void push_new_scope() { linked_scopes.push_back({}); }

    void pop_current_scope() { linked_scopes.pop_back(); }

    size_t size() { return linked_scopes.size(); }

  private:
    std::vector<std::list<T>> linked_scopes;
};