#pragma once
#include <cstddef>
#include <optional>
#include <type_traits>

template <typename T>
size_t byte_size(T&& t)
{
    using TT = std::remove_cvref_t<T>;
    if constexpr (std::is_standard_layout_v<TT> && std::is_trivial_v<TT>) {
        return sizeof(t);
    } else {
        return t.byte_size();
    }
}

template <typename T>
size_t byte_size(const std::optional<T>& t)
{
    if (t)
        return 1 + byte_size(*t);
    return 1;
}
