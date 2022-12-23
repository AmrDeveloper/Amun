#pragma once

#include <unordered_map>
#include <vector>

template <typename K, typename V>
class JotScopedMap {
  public:
    bool define(K key, V value)
    {
        if (linked_scoped.back().contains(key)) {
            return false;
        }
        linked_scoped.back()[key] = value;
        return true;
    }

    bool is_defined(K key)
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

    V lookup(K key)
    {
        for (int i = linked_scoped.size() - 1; i >= 0; i--) {
            if (linked_scoped[i].contains(key)) {
                return linked_scoped[i][key];
            }
        }
        return nullptr;
    }

    inline void push_new_scope() { linked_scoped.push_back({}); }

    inline void pop_current_scope() { linked_scoped.pop_back(); }

  private:
    std::vector<std::unordered_map<K, V>> linked_scoped;
};