#include "TestAllocationCounter.h"

#include <cstdlib>
#include <new>

#if defined (_MSC_VER)
#include <malloc.h>
#endif

namespace
{
thread_local bool monitoringAllocations = false;
thread_local std::size_t observedAllocations = 0;

void recordAllocation() noexcept
{
    if (monitoringAllocations)
        ++observedAllocations;
}

void handleAllocationFailure()
{
    if (const auto handler = std::get_new_handler())
    {
        handler();
        return;
    }

    throw std::bad_alloc {};
}

[[nodiscard]] void* allocate (std::size_t size)
{
    for (;;)
    {
        if (auto* memory = std::malloc (size == 0 ? 1 : size))
        {
            recordAllocation();
            return memory;
        }

        handleAllocationFailure();
    }
}

[[nodiscard]] void* allocateAligned (std::size_t size, std::size_t alignment)
{
    for (;;)
    {
#if defined (_MSC_VER)
        auto* memory = _aligned_malloc (size == 0 ? 1 : size, alignment);
#else
        const auto adjustedSize = ((size == 0 ? 1 : size) + alignment - 1)
                                / alignment * alignment;
        auto* memory = std::aligned_alloc (alignment, adjustedSize);
#endif

        if (memory != nullptr)
        {
            recordAllocation();
            return memory;
        }

        handleAllocationFailure();
    }
}

void freeAligned (void* memory) noexcept
{
#if defined (_MSC_VER)
    _aligned_free (memory);
#else
    std::free (memory);
#endif
}
}

void* operator new (std::size_t size)
{
    return allocate (size);
}

void* operator new[] (std::size_t size)
{
    return allocate (size);
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return allocate (size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    try
    {
        return allocate (size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new (std::size_t size, std::align_val_t alignment)
{
    return allocateAligned (size, static_cast<std::size_t> (alignment));
}

void* operator new[] (std::size_t size, std::align_val_t alignment)
{
    return allocateAligned (size, static_cast<std::size_t> (alignment));
}

void* operator new (std::size_t size,
                    std::align_val_t alignment,
                    const std::nothrow_t&) noexcept
{
    try
    {
        return allocateAligned (size, static_cast<std::size_t> (alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[] (std::size_t size,
                      std::align_val_t alignment,
                      const std::nothrow_t&) noexcept
{
    try
    {
        return allocateAligned (size, static_cast<std::size_t> (alignment));
    }
    catch (...)
    {
        return nullptr;
    }
}

void operator delete (void* memory) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, std::size_t) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, const std::nothrow_t&) noexcept
{
    std::free (memory);
}

void operator delete[] (void* memory, const std::nothrow_t&) noexcept
{
    std::free (memory);
}

void operator delete (void* memory, std::align_val_t) noexcept
{
    freeAligned (memory);
}

void operator delete[] (void* memory, std::align_val_t) noexcept
{
    freeAligned (memory);
}

void operator delete (void* memory, std::size_t, std::align_val_t) noexcept
{
    freeAligned (memory);
}

void operator delete[] (void* memory, std::size_t, std::align_val_t) noexcept
{
    freeAligned (memory);
}

void operator delete (void* memory,
                      std::align_val_t,
                      const std::nothrow_t&) noexcept
{
    freeAligned (memory);
}

void operator delete[] (void* memory,
                        std::align_val_t,
                        const std::nothrow_t&) noexcept
{
    freeAligned (memory);
}

namespace spektrummer::test
{
ScopedAllocationCounter::ScopedAllocationCounter() noexcept
{
    observedAllocations = 0;
    monitoringAllocations = true;
}

ScopedAllocationCounter::~ScopedAllocationCounter() noexcept
{
    if (! stopped)
        monitoringAllocations = false;
}

std::size_t ScopedAllocationCounter::stop() noexcept
{
    monitoringAllocations = false;
    stopped = true;
    return observedAllocations;
}
}
