#ifndef JNP1_7_TRI_LIST_H
#define JNP1_7_TRI_LIST_H

#include <vector>
#include <functional>

#include "tri_list_concepts.h"

template<typename T>
auto identity = [](T x) {
    return x;
};

template<typename T>
std::function<T(T)> compose(modifier<T> auto f1, modifier<T> auto f2) {
    return [f1 = std::move(f1), f2 = std::move(f2)] (T t) mutable {
        return std::invoke(f1, std::invoke(f2, t));
    };
}

// checks if type is present in variant and if it doesn't repeat
// needed while validating types in tri_list methods
template<typename T1, typename T2, typename T3, typename R>
concept is_valid =
    (std::same_as<R, T1> || std::same_as<R, T2> || std::same_as<R, T3>)
    && !(std::same_as<R, T1> && std::same_as<R, T2>)
    && !(std::same_as<R, T1> && std::same_as<R, T3>)
    && !(std::same_as<R, T2> && std::same_as<R, T3>);

template <typename T1, typename T2, typename T3>
class tri_list {
private:
    using variant_t = std::variant<T1, T2, T3>;
    using variant_vec = std::vector<variant_t>;
    using func_t = std::function<variant_t(variant_t)>;

    // vector of functions (for each of three types different functions
    // compositions)
    std::vector<func_t> fs;
    variant_vec v;

    // used to return function of needed type
    template<typename T>
    auto get_func() {
        if (std::same_as<T, T1>)
            return &fs[0];
        else if (std::same_as<T, T2>)
            return &fs[1];
        else
            return &fs[2];
    }

    // lambda used in iterators (when it is needed to convert variants via
    // provided functions)
    func_t var_lambda = [&](variant_t var) {
        return std::visit([this, &var] <class T> (T x [[maybe_unused]]) {
            if constexpr(std::same_as<T, T1>)
                return fs[0](var);
            else if constexpr(std::same_as<T, T2>)
                return fs[1](var);
            else
                return fs[2](var);
        }, var);
        };

    // view used in iterators
    decltype(v | std::views::transform(var_lambda)) view_v =
            v | std::views::transform(var_lambda);

public:
    tri_list(std::initializer_list<variant_t> l) : v(l) {
        fs.resize(3, identity<variant_t>);
    }

    tri_list() : tri_list({}) {}

    template<typename T>
    requires is_valid<T1, T2, T3, T>
    void push_back(const T &t) {
        v.push_back(t);
    }

    // composing existing function with given one
    template<typename T, modifier<T> F>
    requires is_valid<T1, T2, T3, T>
    void modify_only(F m = F{}) {
        auto func = get_func<T>();
        *func = compose<variant_t>([&](variant_t var)
            { return variant_t(m(std::get<T>(var))); }, std::move(*func));
    }

    // resetting function means setting it to identity
    template<typename T>
    requires is_valid<T1, T2, T3, T>
    void reset() {
        auto func = get_func<T>();
        *func = identity<variant_t>;
    }

    // filtering via given type and transforming with proper function
    template<typename T>
    requires is_valid<T1, T2, T3, T>
    auto range_over() const {
        auto check_type = [](variant_t var) {
            return std::visit([] (auto x) {
                return std::same_as<T, decltype(x)>;
            }, var);
        };

        return view_v | std::views::filter(check_type)
                      | std::views::transform([] (auto x)
                        { return std::get<T>(x); });
    }

    auto begin() const {
        return view_v.begin();
    }

    auto end() const {
        return view_v.end();
    }
};

#endif
