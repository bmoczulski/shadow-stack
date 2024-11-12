#include <algorithm>
#include <cstddef>
#include <cassert>
#include <iostream>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <pthread.h>
#include <sys/types.h>
#include <vector>
#include <ranges>

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

class StackShadow final : public Stack
{
  public:
    StackShadow(StackBase const& orig)
        : orig{orig}
        , shadow(orig.size())
    {
    }

    [[nodiscard]] size_t size() const noexcept override
    {
        return shadow.size();
    }

    void push(void* callee, void* stack_pointer);
    void check();
    void pop();

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
};

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
    using std::ranges::for_each;
    using std::ranges::subrange;
    using std::views::chunk;
    using std::views::counted;
    using std::views::filter;
    using std::views::zip;

    auto last_position = stack_frames.back().position;
    auto orig_range = subrange(orig.caddress(last_position), orig.cend());
    auto shadow_range = subrange(caddress(last_position), cend());
    assert(orig_range.size() == shadow_range.size());

    size_t const max_line = 32;

    for_each(std::views::zip(orig_range | chunk(max_line), shadow_range | chunk(max_line)) |
                     filter([](auto const& pair) {
                         auto res = std::ranges::mismatch(get<0>(pair), get<1>(pair));
                         return res.in1 != get<0>(pair).end();
                     }),
             []([[maybe_unused]] auto const& pair) {
                 /*for_each(*/
                 /*	 get<0>(pair) | chunk(4) | std::ranges::transform([](auto const& ch4) {*/
                 /**/
                 /*		 })*/
                 /*	 [](auto const& pair) {}*/
                 /*	 );*/
             });
}

void StackShadow::pop()
{
    assert(!stack_frames.empty());
    stack_frames.pop_back();
}

StackBase makeStackBase()
{
    pthread_attr_t attr;
    void* stackaddr{};
    size_t stacksize{};

    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);

    return {stackaddr, stacksize};
}

class StackThreadContext
{
  public:
    StackThreadContext()
        : orig{makeStackBase()}
        , shadow{orig}
    {
    }

    [[nodiscard]] StackBase const& getStackBase() const
    {
        return orig;
    }

    void push(void* callee, void* stack_pointer);
    void check();
    void pop();

  private:
    StackBase orig;
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

StackThreadContext& getStackThreadContext()
{
    thread_local StackThreadContext ctx;
    return ctx;
}

struct Caller
{
    Caller(void* callee)
        : callee_fun(reinterpret_cast<f_t>(callee))
    {
        long stack_pointer;
        StackThreadContext& ctx = getStackThreadContext();
        ctx.check();
        ctx.push(callee, &stack_pointer);
    }

    ~Caller()
    {
        StackThreadContext& ctx = getStackThreadContext();
        ctx.check();
        ctx.pop();
    }

    void* call(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
    {
        return callee_fun(x0, x1, x2, x3, x4, x5, x6, x7);
    }

    using f_t = void* (*)(void*, void*, void*, void*, void*, void*, void*, void*);
    f_t callee_fun;
};

extern "C" void* wrapper_impl(
        [[maybe_unused]] void* callee, void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
    Caller caller{callee};
    return caller.call(x0, x1, x2, x3, x4, x5, x6, x7);
}

}// namespace shst
