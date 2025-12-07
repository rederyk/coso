#pragma once

#include <esp_heap_caps.h>
#include <new>
#include <string>
#include <vector>

/**
 * Custom allocator that prefers PSRAM for dynamic containers.
 * Falls back to internal DRAM if PSRAM is exhausted.
 */
template <typename T>
struct PsramAllocator {
    using value_type = T;

    PsramAllocator() noexcept = default;

    template <typename U>
    PsramAllocator(const PsramAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        const size_t bytes = n * sizeof(T);
        void* ptr = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ptr) {
            ptr = heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }
        if (!ptr) {
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t) noexcept {
        if (ptr) {
            heap_caps_free(ptr);
        }
    }

    template <typename U>
    struct rebind {
        using other = PsramAllocator<U>;
    };
};

template <typename T, typename U>
inline bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) noexcept {
    return true;
}

template <typename T, typename U>
inline bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) noexcept {
    return false;
}

using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

template <typename T>
using PsramVector = std::vector<T, PsramAllocator<T>>;
