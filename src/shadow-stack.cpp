#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iterator>
#include <pthread.h>
#include <sys/types.h>
#include <vector>

namespace shst {

class Stack
{
  public:
    [[nodiscard]] virtual size_t size() const noexcept = 0;
    [[nodiscard]] virtual const void* stack() const noexcept = 0;

    [[nodiscard]] uint8_t const* cbegin() const
    {
        return address();
    }

    [[nodiscard]] uint8_t const* cend() const
    {
        return address(size());
    }

    ssize_t position(void const* sp) const
    {
        return std::distance(address(), address(sp));
    }

    [[nodiscard]] uint8_t const* address(size_t position) const
    {
        return (position < size()) ? (address() + position) : nullptr;
    }

    [[nodiscard]] uint8_t const* address(void const* sp) const
    {
        auto const* u8sp = static_cast<uint8_t const*>(sp);
        return (cbegin() <= u8sp && u8sp < cend()) ? u8sp : nullptr;
    }

    [[nodiscard]] uint8_t const* address() const noexcept
    {
        return static_cast<uint8_t const*>(stack());
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
    [[nodiscard]] const void* stack() const noexcept override
    {
        return stack_address;
    }

  private:
    void* const stack_address;
    size_t const stack_size;
};

class StackShadow final : public Stack
{
  public:
    StackShadow(StackBase const& base)
        : base{base}
        , shadow(base.size())
    {
    }

    [[nodiscard]] size_t size() const noexcept override
    {
        return shadow.size();
    }

    [[nodiscard]] const void* stack() const noexcept override
    {
        return shadow.data();
    }

    void push(void* callee, void* stack_pointer)
    {
        uint8_t const* last_stack_pointer = nullptr;
        if (stack_frames.empty()) {
            last_stack_pointer = base.cend();
        } else {
            last_stack_pointer = base.address(stack_frames.back().sp);
        }
        auto const size = std::distance(base.address(stack_pointer), last_stack_pointer);
        stack_frames.emplace_back(callee, stack_pointer, size);
    }

    void check() {}
    void pop() {}

  private:
    struct StackFrame
    {
        StackFrame(void const* callee, void const* sp, size_t size)
            : callee{callee}
            , sp{sp}
            , size{size}
        {
        }
        const void* const callee;
        const void* const sp;
        size_t const size;
    };

    StackBase const base;
    std::vector<uint8_t> shadow;
    std::vector<StackFrame> stack_frames;
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

class StackThreadContext
{
  public:
    StackThreadContext()
        : base{makeStackBase()}
        , shadow{base}
    {
    }

    [[nodiscard]] StackBase const& getStackBase() const
    {
        return base;
    }

    void push(void* callee, void* stack_pointer);
    void check();
    void pop();

  private:
    StackBase base;
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
