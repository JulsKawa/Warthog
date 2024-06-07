#pragma once
#include "helper_types.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {
struct MessageWriter;
}
class Reader;
class Sndbuffer;

template <size_t i, typename T>
class B {
public:
    const T& get() const { return t; }
    T& get() { return t; }
    B(T t)
        : t(std::move(t))
    {
    }
    B(Reader& r)
        : t(r)
    {
    }

private:
    T t;
};

template <uint8_t M>
struct MsgCode {
    static constexpr uint8_t msgcode { M };
    static MessageWriter gen_msg(size_t len);
};

template <size_t code, typename... Ts>
class MsgPacker {
    using seq = std::make_index_sequence<sizeof...(Ts)>;

    template <typename T>
    class Internal;

    template <size_t i>
    using parent_t =
        typename std::tuple_element<i, std::tuple<Ts...>>::type;

    template <size_t... Is>
    class Internal<std::index_sequence<Is...>> : public MsgCode<code>, B<Is, Ts>... {

        template <size_t i>
        size_t sum_byte_size(size_t cumsum) const;

        template <size_t i>
        MessageWriter& write_message(MessageWriter& w) const
        {
            if constexpr (i < sizeof...(Is)) {
                return write_message<i + 1>(w << get<i>());
            }
            return w;
        }

    public:
        using Base = Internal;
        template <typename... Args>
        Internal(Args&&... args)
            : B<Is, Ts>(std::forward<Args>(args))...
        {
        }
        Internal(Reader& r)
            : B<Is, Ts>(r)...
        {
        }
        operator Sndbuffer() const;

        template <size_t i>
        auto& get() const
        {
            return B<i, parent_t<i>>::get();
        }

        template <size_t i>
        auto& get()
        {
            return B<i, parent_t<i>>::get();
        }

        [[nodiscard]] size_t byte_size() const
        {
            return sum_byte_size<0>(0);
        }
    };

public:
    using type = Internal<seq>;
};

template <size_t code, typename... Ts>
using MsgCombine = MsgPacker<code, Ts...>::type;

template <size_t code, typename... Ts>
class MsgCombineRequest : public MsgCombine<code, RandNonce, Ts...> {
public:
    using MsgCombine<code, RandNonce, Ts...>::MsgCombine;
    using Base = MsgCombineRequest;

    template <typename... Args>
    MsgCombineRequest(Args&&... args)
        : MsgCombine<code, RandNonce, Ts...>(RandNonce {}, args...)
    {
    }

    static constexpr bool is_request = true;

    auto nonce() const
    {
        return MsgCombine<code, RandNonce, Ts...>::template get<0>().nonce();
    }

    template <size_t i>
    auto& get() const
    {
        return MsgCombine<code, RandNonce, Ts...>::template get<i + 1>();
    }

    template <size_t i>
    auto& get()
    {
        return MsgCombine<code, RandNonce, Ts...>::template get<i + 1>();
    }
};

template <size_t code, typename... Ts>
class MsgCombineReply : public MsgCombine<code, WithNonce, Ts...> {
public:
    using MsgCombine<code, WithNonce, Ts...>::MsgCombine;
    using Base = MsgCombineReply;

    static constexpr bool is_reply = true;
    auto nonce() const
    {
        return MsgCombine<code, WithNonce, Ts...>::template get<0>().nonce();
    }

    template <size_t i>
    auto& get() const
    {
        return MsgCombine<code, WithNonce, Ts...>::template get<i + 1>();
    }

    template <size_t i>
    auto& get()
    {
        return MsgCombine<code, WithNonce, Ts...>::template get<i + 1>();
    }
};
