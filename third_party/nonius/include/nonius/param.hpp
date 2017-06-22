// Nonius - C++ benchmarking tool
//
// Written in 2014 by Martinho Fernandes <martinho.fernandes@gmail.com>
//
// To the extent possible under law, the author(s) have dedicated all copyright and related
// and neighboring rights to this software to the public domain worldwide. This software is
// distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along with this software.
// If not, see <http://creativecommons.org/publicdomain/zero/1.0/>

// User-facing param

#ifndef NONIUS_PARAM_HPP
#define NONIUS_PARAM_HPP

#include <nonius/detail/meta.hpp>
#include <nonius/detail/noexcept.hpp>
#include <unordered_map>
#include <typeinfo>
#include <memory>
#include <sstream>

namespace nonius {

struct param_bad_operation : std::exception {
    char const* what() const NONIUS_NOEXCEPT override {
        return "operation not supported for this parameter type";
    }
};

namespace detail {

struct eq_fn {
    template <typename T>
    auto operator() (T x, T y) -> decltype(x == y) { return x == y; }
};

struct plus_fn {
    template <typename T>
    auto operator() (T x, T y) -> decltype(x + y) { return x + y; }
};

struct mult_fn {
    template <typename T>
    auto operator() (T x, T y) -> decltype(x * y) { return x * y; }
};

} // namespace detail

class param {
    struct interface {
        virtual ~interface() = default;
        virtual std::istream& pull(std::istream&) = 0;
        virtual std::ostream& push(std::ostream&) const = 0;
        virtual bool eq(param const&) const = 0;
        virtual param plus(param const&) const = 0;
        virtual param mult(param const&) const = 0;
        virtual param clone() const = 0;
    };

    template <typename T>
    struct model : interface {
        T value;

        model(T v) : value(std::move(v)) {}
        std::istream& pull(std::istream& is) override { is >> value; return is; }
        std::ostream& push(std::ostream& os) const override { os << value; return os; }
        bool eq(param const& y) const override { return operate<bool>(detail::eq_fn{}, value, y.as<T>()); }
        param plus(param const& y) const override { return operate<T>(detail::plus_fn{}, value, y.as<T>()); }
        param mult(param const& y) const override { return operate<T>(detail::mult_fn{}, value, y.as<T>()); }
        param clone() const override { return value; }

        template <typename R, typename Op, typename ...Args>
        auto operate(Op&& op, Args&& ...xs) const
            -> typename std::enable_if<detail::is_callable<Op&&(Args&&...)>::value, R>::type {
            return std::forward<Op>(op)(std::forward<Args>(xs)...);
        }

        template <typename R, typename Op, typename ...Args>
        auto operate(Op&&, Args&&...) const
            -> typename std::enable_if<!detail::is_callable<Op&&(Args&&...)>::value, R>::type {
            throw param_bad_operation{};
        }
    };

public:
    template <typename T,
              typename std::enable_if<!std::is_base_of<param, typename std::decay<T>::type>::value, int>::type = 0>
    param(T&& v)
        : impl_(new model<typename std::decay<T>::type>{std::forward<T>(v)}) {}

    template <typename T>
    const T& as() const& { return dynamic_cast<model<T> const&>(*impl_).value; }

    friend std::istream& operator>>(std::istream& is, param& x) { x.writable_(); return x.impl_->pull(is); }
    friend std::ostream& operator<<(std::ostream& os, const param& x) { return x.impl_->push(os); }
    friend param operator+(const param& x, const param& y) { return x.impl_->plus(y); }
    friend param operator*(const param& x, const param& y) { return x.impl_->mult(y); }
    friend bool operator==(const param& x, const param& y) { return x.impl_->eq(y); }

    param parse(std::string const& s) const {
		std::stringstream ss;
		ss << s;
        auto cpy = *this;
        ss.exceptions(std::ios::failbit);
        ss >> cpy;
        return cpy;
    }

private:
    void writable_() {
        if (impl_.use_count() > 1) {
            *this = impl_->clone();
        }
    }

    std::shared_ptr<interface> impl_;
};

struct parameters : std::unordered_map<std::string, param> {
    using base_t = std::unordered_map<std::string, param>;
    using base_t::base_t;

    template <typename Tag>
    auto get() const -> typename Tag::type {
        return at(Tag::name()).template as<typename Tag::type>();
    }

    parameters merged(parameters m) const& {
        m.insert(begin(), end());
        return m;
    }
    parameters merged(parameters m) && {
        m.insert(std::make_move_iterator(begin()), std::make_move_iterator(end()));
        return m;
    }

    friend std::ostream& operator<< (std::ostream& os, const parameters& m) {
        for(auto&& p : m) os << "  " << p.first << " = " << p.second << "\n";
        return os;
    }
};

struct param_registry {
    parameters const& defaults() const { return defaults_; }
    void add(std::string name, param v) { defaults_.emplace(name, v); }
    void remove(std::string name) { defaults_.erase(name); }

private:
    parameters defaults_;
};

inline param_registry& global_param_registry() {
    static param_registry instance;
    return instance;
}

template <typename Tag>
struct param_declaration {
    using type = typename Tag::type;
    param_registry& registry;

    param_declaration(type v, param_registry& r = global_param_registry())
        : registry{r}
    {
        r.add(Tag::name(), param{type{v}});
    }
};

template <typename Tag>
struct scoped_param_declaration : param_declaration<Tag> {
    using param_declaration<Tag>::param_declaration;

    ~scoped_param_declaration() {
        this->registry.remove(Tag::name());
    }
};

} /* namespace nonius */

#define NONIUS_PARAM(name_, ...)                                        \
    namespace {                                                         \
    struct name_ {                                                      \
        using type = decltype(__VA_ARGS__);                             \
        static const char* name() { return #name_; }                    \
    };                                                                  \
    static auto NONIUS_DETAIL_UNIQUE_NAME(param_declaration) =          \
                   ::nonius::param_declaration<name_>{ (__VA_ARGS__) }; \
    }                                                                   \
    //

#endif // NONIUS_PARAM_HPP
