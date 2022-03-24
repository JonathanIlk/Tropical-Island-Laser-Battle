#pragma once

// @Kersten sorry fuers reinzeihen von clean-core, aber dieser container
// ist ziemlich nuetzlich um eine simple render world zu basteln und waere
// etwas umstandlich zu porten
#include <clean-core/alloc_array.hh>
#include <clean-core/atomic_linked_pool.hh>
#include <clean-core/storage.hh>
#include <clean-core/utility.hh>

namespace gamedev
{
// O(1) acquire, release, access
// memory contiguous
// access via handles is double indirected
template <class T>
struct compact_pool
{
public:
    using handle_t = cc::atomic_linked_pool<size_t>::handle_t;

    compact_pool() = default;
    explicit compact_pool(size_t size, cc::allocator* alloc = cc::system_allocator) { initialize(size, alloc); }

    compact_pool(compact_pool&&) = delete;
    compact_pool(compact_pool const&) = delete;
    compact_pool& operator=(compact_pool&&) = delete;
    compact_pool& operator=(compact_pool const&) = delete;

    ~compact_pool() { _destroy(); }

    void initialize(size_t size, cc::allocator* alloc = cc::system_allocator)
    {
        if (size == 0)
            return;

        CC_ASSERT(_compact_values.size() == 0 && "double init");
        _sparse_indices.initialize(size, alloc);
        _compact_values = _compact_values.uninitialized(size, alloc);
        _compact_pool_handles = _compact_pool_handles.uninitialized(size, alloc);
    }

    void destroy() { _destroy(); }

    /// acquire a slot
    [[nodiscard]] handle_t acquire()
    {
        CC_ASSERT(!is_full() && "overcommited");
        // acquire
        auto const pool_handle = _sparse_indices.acquire();
        auto const compact_idx = _compact_head;
        ++_compact_head;

        // write cross-reference indices
        _sparse_indices.get(pool_handle) = compact_idx;
        _compact_pool_handles[compact_idx] = pool_handle;

        // ctor
        if constexpr (!std::is_trivially_constructible_v<T>)
            new (cc::placement_new, &_compact_values[compact_idx].value) T();
        return pool_handle;
    }

    /// free a slot
    /// returns true if a move happened from element [index head (after call)] to element [index of handle], false otherwise
    bool release(handle_t handle)
    {
        CC_ASSERT(!is_empty() && "already empty");

        // release pool
        size_t const compact_idx = _sparse_indices.get(handle);
        _sparse_indices.release(handle);

        // dtor
        if constexpr (!std::is_trivially_destructible_v<T>)
            _compact_values[compact_idx].value.~T();

        --_compact_head;

        if (compact_idx == _compact_head)
        {
            // this was the last element, done
            return false;
        }

        // move head to this position
        _compact_values[compact_idx].value = cc::move(_compact_values[_compact_head].value);
        _compact_pool_handles[compact_idx] = _compact_pool_handles[_compact_head];

        // update pool entry
        uint32_t const modified_pool_handle = _compact_pool_handles[compact_idx];
        _sparse_indices.get(modified_pool_handle) = compact_idx;

        return true;
    }

    unsigned release_all()
    {
        if constexpr (!std::is_trivially_destructible_v<T>)
            for (auto& val : get_span())
            {
                val.~T();
            }

        _compact_head = 0;
        return _sparse_indices.release_all();
    }

    /// access a slot
    /// do not maintain direct references, they are invalidated after the next release
    CC_FORCE_INLINE T& get(handle_t handle) { return _compact_values[_sparse_indices.get(handle)].value; }
    CC_FORCE_INLINE T const& get(handle_t handle) const { return _compact_values[_sparse_indices.get(handle)].value; }

    CC_FORCE_INLINE T* data() { return reinterpret_cast<T*>(_compact_values.data()); }
    CC_FORCE_INLINE T const* data() const { return reinterpret_cast<T const*>(_compact_values.data()); }

    size_t size() const { return _compact_head; }
    size_t size_bytes() const { return size() * sizeof(T); }

    size_t max_size() const { return _compact_values.size(); }

    bool is_full() const { return size() == max_size(); }
    bool is_empty() const { return _compact_head == 0; }

    bool is_alive(handle_t handle) const { return handle != unsigned(-1) && _sparse_indices.is_alive(handle); }

    /// iterate allocated slots
    /// for (auto const& val : pool.get_span()) { ... }
    CC_FORCE_INLINE cc::span<T> get_span() { return {data(), size()}; }
    CC_FORCE_INLINE cc::span<T const> get_span() const { return {data(), size()}; }

    CC_FORCE_INLINE size_t get_handle_index(handle_t handle) const { return _sparse_indices.get(handle); }
    CC_FORCE_INLINE size_t get_index_head() const { return _compact_head; }
    CC_FORCE_INLINE handle_t get_nth_handle(unsigned index) const { return _compact_pool_handles[index]; }

private:
    void _destroy()
    {
        if (max_size() == 0)
            return;

        if constexpr (!std::is_trivially_destructible_v<T>)
            for (auto& val : get_span())
            {
                val.~T();
            }

        _sparse_indices.destroy();
        _compact_values = {};
        _compact_pool_handles = {};
        _compact_head = 0;
    }


private:
    cc::atomic_linked_pool<size_t, true> _sparse_indices;
    cc::alloc_array<cc::storage_for<T>> _compact_values;
    cc::alloc_array<handle_t> _compact_pool_handles;
    size_t _compact_head = 0;
};

}
