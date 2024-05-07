#ifndef CORO_HOST_HPP
#define CORO_HOST_HPP

#include <tuple>
#include <variant>
#include <any>
#include <unordered_map>
#include <unordered_set>
#include <coroutine>

namespace utils {

template <typename MapT>
struct multimap_equal_range {
    MapT::iterator left;
    MapT::iterator right;
    auto begin() { return left; }
    auto end() { return right; }
    template <typename KeyT>
    multimap_equal_range(MapT& mulmap, KeyT key) {
        std::tie(left, right) = mulmap.equal_range(key);
    }
};

/**
 * 这个类的作用是放在一个activity中以运行协程。
 * 非常好协程，使我不用手写状态机。
 */
class coro_host_base {
public:
    using enum_set = std::unordered_set<size_t>;
    struct subscriber;
    template <size_t code> struct code_info {};

protected:
    std::unordered_multimap<size_t, subscriber*> pending;
    std::unordered_multimap<subscriber*, decltype(pending)::iterator> relation;

    template <typename RetT, typename... ArgTs>
    inline void process_all_of(size_t code, ArgTs&&... args);

    template <typename TraitsT, typename... ArgTs>
    void process_all_of(ArgTs&&... args) {
        using return_type = TraitsT::return_type;
        process_all_of<return_type>(TraitsT::code, std::forward<ArgTs>(args)...);
    }

    void add_subscriber(subscriber* sub, enum_set& codes) {
        for (auto&& code : codes) {
            auto it = pending.emplace(code, sub);
            relation.emplace(sub, it);
        }
    }

public:
    inline coro_host_base();
    inline ~coro_host_base();
    inline subscriber wait_event(enum_set&& codes);

    template <typename TraitsT>
    static decltype(auto) extract_data(const std::any& data) {
        using return_type = const TraitsT::return_type&;
        return std::any_cast<return_type>(data);
    }
    template <typename TraitsT>
    static decltype(auto) extract_data(std::any& data) {
        using return_type = TraitsT::return_type&;
        return std::any_cast<return_type>(data);
    }
    template <typename TraitsT>
    static decltype(auto) extract_data(std::any&& data) {
        using return_type = TraitsT::return_type&&;
        return std::any_cast<return_type>(data);
    }
};

struct coro_host_base::subscriber {
    using argument_type = std::pair<coro_host_base*, enum_set>;
    using result_type = std::pair<size_t, std::any>;

    std::coroutine_handle<> caller;
    std::variant<argument_type, result_type> data;

    subscriber(coro_host_base* host, enum_set&& codes):
        data(std::in_place_type<argument_type>, host, std::move(codes)) {}

    constexpr bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> ch) {
        caller = ch;
        auto&& [host, codes] = std::get<argument_type>(data);
        host->add_subscriber(this, codes);
    }
    result_type await_resume() noexcept {
        return std::move(std::get<result_type>(data));
    }

    template <typename RetT, typename... ArgTs>
    void fulfill(size_t code, ArgTs&&... params) {
        if constexpr (std::is_void_v<RetT>) {
            data.template emplace<result_type>(std::piecewise_construct,
                std::forward_as_tuple(code), std::forward_as_tuple()
            );
        } else {
            data.template emplace<result_type>(std::piecewise_construct,
                std::forward_as_tuple(code),
                std::forward_as_tuple(std::in_place_type<RetT>, std::forward<ArgTs>(params)...)
            );
        }
        caller.resume();
    }
};

coro_host_base::coro_host_base() {}
coro_host_base::~coro_host_base() {
    for (auto&& [code, sub] : pending) { sub->caller.destroy(); }
}

template <typename RetT, typename... ArgTs>
void coro_host_base::process_all_of(size_t code, ArgTs&&... args) {
    // 写成这个样子的原因是这一段在做一种在飞机上拆飞机的操作。
    auto targets = multimap_equal_range(pending, code);
    for (auto it = targets.begin(); it != targets.end() && it != pending.end(); ) {
        auto sub = (it++)->second;
        auto other_subs = multimap_equal_range(relation, sub);
        for (auto it = other_subs.begin(); it != other_subs.end() && it != relation.end(); ) {
            pending.erase(it->second);
            relation.erase(it++);
        }
        sub->template fulfill<RetT>(code, std::forward<ArgTs>(args)...);
    }
}

coro_host_base::subscriber coro_host_base::wait_event(enum_set&& codes) {
    return {this, std::move(codes)};
}

class coro_host : public coro_host_base {
public:
    enum basic_events {
        EVT_KEY_CHANGE,
        EVT_KEY_SIGNAL,
        EVT_TICK,
        EVT_BASIC_MAX
    };

    struct evt_key_change {
        static constexpr size_t code = EVT_KEY_CHANGE;
        using return_type = void;
    };
    struct evt_key_signal {
        static constexpr size_t code = EVT_KEY_SIGNAL;
        using return_type = void;
    };
    struct evt_tick {
        static constexpr size_t code = EVT_TICK;
        using return_type = std::tuple<double, double>;
    };

    void on_key_change() { process_all_of<evt_key_change>(); }
    void on_key_signal() { process_all_of<evt_key_signal>(); }
    void on_tick(double this_time, double last_time) {
        process_all_of<evt_tick>(this_time, last_time);
    }
};


} // namespace utils

#endif // CORO_HOST_HPP
