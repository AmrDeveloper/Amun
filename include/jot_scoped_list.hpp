#pragma once

#include <list>

template <typename T>
class JotScopedList {
  public:
    JotScopedList() = default;

    void push_front(T element) { linked_scopes.back().push_front(element); }

    void push_back(T element) { linked_scopes.back().push_back(element); }

    std::list<T> get_scope_elements() { return linked_scopes.back(); }

    void push_new_scope() { linked_scopes.push_back({}); }

    void pop_current_scope() { linked_scopes.pop_back(); }

  private:
    std::list<std::list<T>> linked_scopes;
};