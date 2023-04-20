#pragma once

#include <unordered_map>
#include <utility>
#include <vector>

namespace amun {

template <typename K, typename V>
class ScopedMap {
  public:
    auto define(K key, V value) -> bool
    {
        if (linked_scoped.back().contains(key)) {
            return false;
        }
        linked_scoped.back()[key] = value;
        return true;
    }

    auto is_defined(K key) -> bool
    {
        for (int i = linked_scoped.size() - 1; i >= 0; i--) {
            if (linked_scoped[i].contains(key)) {
                return true;
            }
        }
        return false;
    }

    void update(K key, V value)
    {
        for (int i = linked_scoped.size() - 1; i >= 0; i--) {
            if (linked_scoped[i].contains(key)) {
                linked_scoped[i][key] = value;
            }
        }
    }

    auto lookup(K key) -> V
    {
        for (int i = linked_scoped.size() - 1; i >= 0; i--) {
            if (linked_scoped[i].contains(key)) {
                return linked_scoped[i][key];
            }
        }
        return nullptr;
    }

    auto lookup_on_current(K key) -> V
    {
        size_t i = linked_scoped.size() - 1;
        if (linked_scoped[i].contains(key)) {
            return linked_scoped[i][key];
        }

        return nullptr;
    }

    auto lookup_with_level(K key) -> std::pair<V, int>
    {
        for (int i = linked_scoped.size() - 1; i >= 0; i--) {
            if (linked_scoped[i].contains(key)) {
                return {linked_scoped[i][key], i};
            }
        }
        return {nullptr, -1};
    }

    auto push_new_scope() -> void { linked_scoped.push_back({}); }

    auto pop_current_scope() -> void { linked_scoped.pop_back(); }

    auto size() -> size_t { return linked_scoped.size(); }

  private:
    std::vector<std::unordered_map<K, V>> linked_scoped;
};

} // namespace amun