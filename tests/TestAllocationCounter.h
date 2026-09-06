#pragma once

#include <cstddef>

namespace spektrummer::test
{
class ScopedAllocationCounter final
{
public:
    ScopedAllocationCounter() noexcept;
    ~ScopedAllocationCounter() noexcept;

    ScopedAllocationCounter (const ScopedAllocationCounter&) = delete;
    ScopedAllocationCounter& operator= (const ScopedAllocationCounter&) = delete;

    [[nodiscard]] std::size_t stop() noexcept;

private:
    bool stopped = false;
};
}
