// SPDX-License-Identifier: MIT
#pragma once
#include <type_traits>
#include <utility>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

namespace Util {

template<typename T, bool is_enum=std::is_enum<T>::value>
struct IntRange;

template<typename T>
struct IntRange<T, false> {
    static_assert(!std::is_enum<T>::value);
    T start, end_;
    struct end_marker;
    struct iterator {
        T val;
        iterator &operator++() {++val; return *this;}
        iterator operator++(int) {iterator res {val}; ++val; return res;}
        T operator*() const {return val;}
        constexpr bool operator!=(const iterator &other) const {return val != other.val;}
        constexpr bool operator!=(const end_marker &other);
    };
    struct end_marker {T val;};

    IntRange(T end) : start{0}, end_{end} {}
    IntRange(T start, T end) : start{start}, end_{end} {}
    iterator begin() const {return {start};}
    end_marker end() const {return {end_};}
};

template<typename T>
constexpr bool IntRange<T, false>::iterator::operator!=(const typename IntRange<T, false>::end_marker &end) {
    return val < end.val;
}

template<typename T>
struct IntRange<T, true> {
    static_assert(std::is_enum<T>::value);
    std::underlying_type_t<T> start, end_;
    struct iterator {
        std::underlying_type_t<T> val;
        iterator &operator++() {++val; return *this;}
        iterator operator++(int) {iterator res {val}; ++val; return res;}
        T operator*() const {return static_cast<T>(val);}
        bool operator!=(iterator &other) {return val != other.val;}
    };

    IntRange(T end) : start{0}, end_{end} {}
    IntRange(T start, T end) : start{start}, end_{end} {}
    iterator begin() const {return {start};}
    iterator end() const {return {end_};}
};

template<typename T> IntRange(T) -> IntRange<T, std::is_enum<T>::value>;
template<typename T> IntRange(T, T) -> IntRange<T, std::is_enum<T>::value>;

template<typename T>
std::pair<T, T> ordered(T a, T b) {
    return a <= b ? std::make_pair(a, b) : std::make_pair(b, a);
}

template<typename T>
T pow2(T a) {return a * a;}

}
