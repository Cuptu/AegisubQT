// Copyright (c) 2010, Thomas Goyne <plorkyeran@aegisub.org>
// Modernized C++20 implementation for AegisubQtQuick
#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace agi::signal {
class Connection;

namespace detail {
    class SignalBase;
    class ConnectionToken {
        friend class agi::signal::Connection;
        friend class SignalBase;

        SignalBase *signal = nullptr;
        bool blocked = false;
        bool claimed = false;

        ConnectionToken(SignalBase *signal) : signal(signal) { }
        inline void Disconnect();
    public:
        ~ConnectionToken() { Disconnect(); }
    };
}

class [[nodiscard("Store the connection in a Connection object.")]] UnscopedConnection {
    friend class Connection;
    detail::ConnectionToken *token;
public:
    UnscopedConnection(detail::ConnectionToken *token) : token(token) { }
};

class Connection {
    std::unique_ptr<detail::ConnectionToken> token;
public:
    Connection() = default;
    Connection(UnscopedConnection src) noexcept : token(src.token) { if (token) token->claimed = true; }
    Connection(Connection&& that) noexcept = default;
    Connection(detail::ConnectionToken *token) noexcept : token(token) { if (token) token->claimed = true; }
    Connection& operator=(Connection&& that) noexcept = default;

    void Disconnect() { if (token) token->Disconnect(); }
    void Block() { if (token) token->blocked = true; }
    void Unblock() { if (token) token->blocked = false; }
};

namespace detail {
    class SignalBase {
        friend class ConnectionToken;
        virtual void Disconnect(ConnectionToken *tok) = 0;

        SignalBase(SignalBase const&) = delete;
        SignalBase& operator=(SignalBase const&) = delete;
    protected:
        SignalBase() = default;
        virtual ~SignalBase() = default;

        void DisconnectToken(ConnectionToken *tok) { tok->signal = nullptr; }
        bool TokenClaimed(ConnectionToken *tok) { return tok->claimed; }
        ConnectionToken *MakeToken() { return new ConnectionToken(this); }
        bool Blocked(ConnectionToken *tok) { return tok->blocked; }
    };

    inline void ConnectionToken::Disconnect() {
        if (signal) signal->Disconnect(this);
        signal = nullptr;
    }
}

template<typename... Args>
class Signal final : private detail::SignalBase {
    using Slot = std::function<void(Args...)>;
    std::vector<std::pair<detail::ConnectionToken*, Slot>> m_slots;

    void Disconnect(detail::ConnectionToken *tok) override {
        std::erase_if(m_slots, [=](auto& slot) { return slot.first == tok; });
    }

    UnscopedConnection DoConnect(Slot&& sig) {
        std::unique_ptr<detail::ConnectionToken> token(MakeToken());
        m_slots.emplace_back(token.get(), std::move(sig));
        return UnscopedConnection(token.release());
    }

public:
    ~Signal() {
        for (auto& slot : m_slots) {
            DisconnectToken(slot.first);
            if (!TokenClaimed(slot.first)) delete slot.first;
        }
    }

    void operator()(Args... args) {
        for (size_t i = m_slots.size(); i > 0; --i) {
            if (!Blocked(m_slots[i - 1].first))
                m_slots[i - 1].second(args...);
        }
    }

    UnscopedConnection Connect(Slot&& sig) {
        return DoConnect(std::move(sig));
    }

    template<typename T>
    UnscopedConnection Connect(void (T::*func)(Args...), T* a1) {
        return DoConnect([=](Args... args) { (a1->*func)(args...); });
    }

    template<typename Thunk, typename = decltype((*(Thunk *)0)())>
    UnscopedConnection Connect(Thunk&& func) {
        return DoConnect([=](Args...) mutable { func(); });
    }

    template<typename T, typename MemberThunk>
    UnscopedConnection Connect(MemberThunk func, T* obj) {
        return DoConnect([=](Args...) { (obj->*func)(); });
    }
};

} // namespace agi::signal

#define DEFINE_SIGNAL_ADDERS(sig, method) \
    template<typename... Args> \
    agi::signal::UnscopedConnection method(Args&&... args) { \
        return sig.Connect(std::forward<Args>(args)...); \
    }
