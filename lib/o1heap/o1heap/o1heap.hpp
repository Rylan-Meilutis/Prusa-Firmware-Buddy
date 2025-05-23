#pragma once

#include <array>
#include <cstddef>

#include "o1heap.h"

/// C++ wrapper for o1heap
template <size_t size_>
class O1Heap
{
public:
    static constexpr size_t size = size_;

    O1Heap() { heap_ = o1heapInit(buffer_.data(), size); }

    void* alloc(size_t bytes) { return o1heapAllocate(heap_, bytes); }
    void  free(void* ptr) { o1heapFree(heap_, ptr); }

private:
    alignas(O1HEAP_ALIGNMENT) std::array<std::byte, size> buffer_;
    O1HeapInstance* heap_;
};
