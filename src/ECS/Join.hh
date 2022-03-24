// SPDX-License-Identifier: MIT
#pragma once
#include <cassert>
#include <map>
#include <tuple>
#include <vector>

#include <ECS.hh>

namespace ECS {

template<typename ...T> struct JoinIter;
template<>
struct JoinIter<> {
    using result = std::tuple<entity>;
    entity id;

    JoinIter(entity start = 0) : id{start} {}
    // returns the current entity ID. should be the same accross all items,
    // so all other implementations should forward the call to this one
    entity entityID() const {return id;}

    // fast-forward to the next component with ID no less than 'target',
    // INVALID if none exist. (note: INVALID is higher than all valid IDs)
    // contract: target should be no lower than entityID()
    // must uphold the 'all IDs are the same' rule
    // (e. g. by calling nested implementations)
    entity gallop(entity target) {return (id = target);}

    // iterator interface
    std::tuple<entity> operator*() const {return std::make_tuple(id);}
    void operator++() {++id;}
    bool operator==(const JoinIter<> &other) const {return id == other.id;}
    bool operator!=(const JoinIter<> &other) const {return id == other.id;}
};

template<typename T, typename Tup>struct ConsTuple;
template<typename T, typename ...Tail>
struct ConsTuple<T, std::tuple<Tail...>>{using type = std::tuple<T, Tail...>;};

template<typename T, typename Compare, typename Alloc, typename ...Tail>
struct JoinIter<std::map<entity, T, Compare, Alloc> &, Tail...> {
    using result = typename ConsTuple<T &, typename JoinIter<Tail...>::result>::type;
    using map_type = std::map<entity, T, Compare, Alloc>;

    JoinIter<Tail...> tail;
    typename map_type::iterator iter;
    map_type *map;

    JoinIter(entity start, map_type &map, Tail... tail) : tail(start, tail...), iter{map.begin()}, map{&map} {}
    entity entityID() const {
        return tail.entityID();
    }
    entity gallop(entity target) {
        assert(target >= entityID());
        while (true) {
            target = tail.gallop(target);
            if (target == INVALID) {
                iter = map->end();
                return INVALID;
            }
            iter = map->upper_bound(target);
            if (iter != map->begin()) {
                --iter;
                if (iter->first < target) {
                    ++iter;
                } else {return target;}
            }
            if (iter == map->end()) {
                target = INVALID;
            } else {
                entity id = iter->first;
                if (id == target) {
                    return id;
                } else {
                    assert(id > target);
                    target = id;
                }
            }
        }
    }

    result operator*() {
        std::tuple<T &> res = {iter->second};
        return std::tuple_cat(res, *tail);
    }
    void operator++() {
        if (entityID() != INVALID) {
            gallop(entityID() + 1);
        }
    }
    bool operator==(const JoinIter<map_type &, Tail...> &other) const {
        return entityID() == other.entityID();
    }
    bool operator!=(const JoinIter<map_type &, Tail...> &other) const {
        return entityID() != other.entityID();
    }
};

template<typename ...T>
struct Join {
    using iterator = JoinIter<T&...>;
    iterator start;
    Join(T&... relations) : start(0, relations...) {
        start.gallop(0);
    }
    iterator begin() {return start;}
    iterator find(entity id) {
        auto iter = start;
        iter.gallop(id);
        if (iter.entityID() != id) {iter.gallop(INVALID);}
        return iter;
    }
    iterator end() {
        auto iter = start;
        iter.gallop(INVALID);
        return iter;
    }
};

}
