#include <algorithm>
#include <array>
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <pthread.h>
#include <sys/types.h>
#include <vector>
#include <ranges>
#include "shadow-stack-detail.h"
#include "callee_traits.hpp"

namespace shst {

class Stack
{
  public:
    [[nodiscard]] uint8_t const* cbegin() const
    {
        return caddress();
    }

    [[nodiscard]] uint8_t const* cend() const
    {
        return caddress(size());
    }

    size_t position(void const* sp) const
    {
        auto caddr_begin = caddress();
        auto caddr_sp = caddress(sp);
        return std::distance(caddr_begin, caddr_sp);
    }

    [[nodiscard]] uint8_t const* caddress(size_t position) const
    {
        return (position <= size()) ? (caddress() + position) : nullptr;
    }

    [[nodiscard]] uint8_t const* caddress() const noexcept
    {
        return static_cast<uint8_t const*>(cstack());
    }

    [[nodiscard]] uint8_t const* caddress(void const* sp) const
    {
        auto const* u8sp = static_cast<uint8_t const*>(sp);
        return (cbegin() <= u8sp && u8sp <= cend()) ? u8sp : nullptr;
    }

    [[nodiscard]] virtual size_t size() const noexcept = 0;

  protected:
    [[nodiscard]] virtual void const* cstack() const noexcept = 0;
    [[nodiscard]] virtual void* stack() noexcept = 0;

    [[nodiscard]] uint8_t* address() noexcept
    {
        return static_cast<uint8_t*>(stack());
    }

    [[nodiscard]] uint8_t* address(size_t position) noexcept
    {
        return (position < size()) ? (address() + position) : nullptr;
    }
};

class StackBase final : public Stack
{
  public:
    StackBase(void* stack_base, size_t stack_size)
        : stack_address{stack_base}
        , stack_size{stack_size}
    {
    }

    [[nodiscard]] size_t size() const noexcept override
    {
        return stack_size;
    }

  protected:
    [[nodiscard]] void const* cstack() const noexcept override
    {
        return stack_address;
    }
    [[nodiscard]] void* stack() noexcept override
    {
        return nullptr;
    }

  private:
    void* const stack_address;
    size_t const stack_size;
};

StackBase makeStackBase()
{
    pthread_attr_t attr;
    void* stackaddr{};
    size_t stacksize{};

    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);

    return {stackaddr, stacksize};
}

class StackShadow final : public Stack
{
  public:
    StackShadow()
        : orig{makeStackBase()}
        , shadow(orig.size())
        , ignore_threshold{orig.size()}
    {
    }

    [[nodiscard]] size_t size() const noexcept override
    {
        return shadow.size();
    }

    void push(void* callee, void* stack_pointer);
    void check();
    void pop();
    void ignore_above(void* stack_pointer);

  protected:
    [[nodiscard]] void const* cstack() const noexcept override
    {
        return shadow.data();
    }

    [[nodiscard]] void* stack() noexcept override
    {
        return shadow.data();
    }

  private:
    struct StackFrame
    {
        StackFrame(void const* callee, size_t position, size_t size)
            : callee{callee}
            , position{position}
            , size{size}
        {
        }
        void const* const callee;
        size_t const position;
        size_t const size;
    };

    StackBase const orig;
    std::vector<uint8_t> shadow;
    std::vector<StackFrame> stack_frames;
    size_t ignore_threshold;
};

void StackShadow::ignore_above(void* stack_pointer)
{
    auto const pos = orig.position(stack_pointer);
    if (pos < orig.size()) {
        ignore_threshold = pos;
    }
}

void StackShadow::push(void* callee, void* sp)
{
    auto const last_stack_position = stack_frames.empty() ? orig.size() : stack_frames.back().position;
    auto const orig_stack_pointer = orig.caddress(sp);
    auto const stack_position = orig.position(sp);

    assert(last_stack_position >= stack_position);
    auto const size = last_stack_position - stack_position;

    assert(size);
    std::copy_n(orig_stack_pointer, size, address(stack_position));

    stack_frames.emplace_back(callee, stack_position, size);
}

void StackShadow::check()
{
#if 1
    size_t last_position = orig.position(orig.cend());
    if (!stack_frames.empty()) {
        last_position = stack_frames.back().position;
    }
    if (last_position >= ignore_threshold) {
        return;
    }

    auto ot = orig.caddress(last_position);
    auto ob = orig.cbegin() + ignore_threshold;
    auto st = caddress(last_position);
    // auto sb = cbegin() + ignore_threshold;
    auto depth = ob - ot;

    if (memcmp(ot, st, depth) == 0)
    {
        // all is OK
        return;
    }

    size_t const max_line = 32;

    fprintf(stderr, "ORIG (CORRUPTED):");
    for (int i = 0; i < depth; ++i)
    {
        if (i % max_line == 0) {
            fprintf(stderr, "\n%16p: ", ot + i);
        }
        fprintf(stderr, "%c%02x%c", ot[i] != st[i] ? '[' : ' ', ot[i], ot[i] != st[i] ? ']' : ' ');
    }
    fprintf(stderr, "\n\nSHADOW (CORRECT):");
    for (int i = 0; i < depth; ++i)
    {
        if (i % max_line == 0) {
            fprintf(stderr, "\n%16p: ", st + i);
        }
        fprintf(stderr, "%c%02x%c", ot[i] != st[i] ? '[' : ' ', st[i], ot[i] != st[i] ? ']' : ' ');
    }

    fprintf(stderr, "\n\nFRAMES (recent first):\n");
    for (auto frame = stack_frames.rbegin(); frame != stack_frames.rend(); ++frame)
    {
        fprintf(stderr, "position %10zd, size %10zd, callee %16p = %s\n",
            frame->position, frame->size, frame->callee,
            callee_traits::name(const_cast<void*>(frame->callee)).c_str()
        );
    }
    fprintf(stderr, "\n");

    fprintf(stderr, "backtrace:\n");
    std::array<void*, 1024> buff;
    auto n = backtrace(buff.data(), buff.size());
    backtrace_symbols_fd(buff.data(), n, 2);
    abort();

#else
    using std::ranges::for_each;
    using std::ranges::subrange;
    using std::views::chunk;
    using std::views::counted;
    using std::views::filter;
    using std::views::zip;
#if 1
    size_t last_position = orig.position(orig.cend());
    if (!stack_frames.empty()) {
        last_position = stack_frames.back().position;
    }
    if (last_position >= ignore_threshold) {
        return;
    }
    auto orig_range = subrange(orig.caddress(last_position), orig.cbegin() + ignore_threshold);
    auto shadow_range = subrange(caddress(last_position), cbegin() + ignore_threshold);
    assert(orig_range.size() == shadow_range.size());
#else
    size_t last_position = orig.position(orig.cend());
    if (!stack_frames.empty()) {
        last_position = stack_frames.back().position;
    }
    auto orig_range = subrange(orig.caddress(last_position), orig.cend());
    auto shadow_range = subrange(caddress(last_position), cend());
    assert(orig_range.size() == shadow_range.size());

#endif
    size_t const max_line = 32;

    for_each(std::views::zip(orig_range | chunk(max_line), shadow_range | chunk(max_line)) |
                     filter([](auto const& pair) {
                         auto res = std::ranges::mismatch(get<0>(pair), get<1>(pair));
                         return res.in1 != get<0>(pair).end();
                     }),
             []([[maybe_unused]] auto const& pair) {
                 std::cout << std::hex;
                 std::cout << std::setw(16) << static_cast<void const*>(&(*get<0>(pair).begin())) << ": ";

                 auto old_flags = std::cout.setf(std::ios::hex);
                 auto old_fill = std::cout.fill('0');

                 for_each(get<0>(pair) | chunk(4), [first = bool{true}](auto const& ch8) mutable {
                     std::cout << ((first) ? (first = false, "") : " ");
                     for (auto const b : ch8) {
                         std::cout << std::setw(2) << static_cast<unsigned>(b & 0xFF);
                     }
                 });
                 std::cout << "    ";
                 for_each(get<1>(pair) | chunk(4), [first = bool{true}](auto const& ch8) mutable {
                     std::cout << ((first) ? (first = false, "") : " ");
                     for (auto const b : ch8) {
                         std::cout << std::setw(2) << static_cast<unsigned>(b & 0xFF);
                     }
                 });
                 std::cout << '\n';

                 std::cout.setf(old_flags);
                 std::cout.fill(old_fill);
                 std::array<void*, 1024> buff;

                 auto n = backtrace(buff.data(), buff.size());
                 backtrace_symbols_fd(buff.data(), n, 2);
                 abort();
             });
#endif
}

void StackShadow::pop()
{
    assert(!stack_frames.empty());
    stack_frames.pop_back();
}

class StackThreadContext
{
  public:
    StackThreadContext() = default;

    void push(void* callee, void* stack_pointer);
    void check();
    void pop();
    void ignore_above(void* stack_pointer);

  private:
    StackShadow shadow;
};

void StackThreadContext::push(void* callee, void* stack_pointer)
{
    shadow.push(callee, stack_pointer);
}

void StackThreadContext::check()
{
    shadow.check();
}

void StackThreadContext::pop()
{
    shadow.pop();
}

void StackThreadContext::ignore_above(void* stack_pointer)
{
    shadow.ignore_above(stack_pointer);
}

StackThreadContext& getStackThreadContext()
{
    thread_local StackThreadContext ctx;
    return ctx;
}

void ignore_above(void* stack_pointer)
{
    getStackThreadContext().ignore_above(stack_pointer);
}
namespace detail {

guard::guard(void* callee, void* stack_pointer)
{
    StackThreadContext& ctx = getStackThreadContext();
    ctx.push(callee, stack_pointer);
    ctx.check();
}
guard::~guard()
{
    StackThreadContext& ctx = getStackThreadContext();
    ctx.check();
    ctx.pop();
}
}// namespace detail

extern "C" void* invoke_impl(
        [[maybe_unused]] void* callee, void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
    long stack_position;
    detail::guard g{callee, &stack_position};
    using f_t = void* (*)(void*, void*, void*, void*, void*, void*, void*, void*);
    return reinterpret_cast<f_t>(callee)(x0, x1, x2, x3, x4, x5, x6, x7);
}

}// namespace shst
