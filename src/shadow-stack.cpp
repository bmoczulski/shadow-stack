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

    enum class Direction
    {
        PreCall,
        PostReturn
    };

    enum class Reaction
    {
        ignore,
        report_and_continue,
        report_and_abort,
        report_heal_and_continue,
        heal_and_continue
    };

    enum class DumpArea
    {
        both,
        original,
        shadow
    };

    Reaction desired_reaction();
    int dump_width();
    DumpArea dump_area();
    bool dump_hide_equal_lines();

    void push(void* callee, void* stack_pointer);
    void check(Direction);
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

StackShadow::Reaction StackShadow::desired_reaction()
{
    auto reaction = getenv("SHST_REACTION");
    if (reaction == nullptr || strcmp(reaction, "abort") == 0) {
        return Reaction::report_and_abort;
    } else if (strcmp(reaction, "ignore") == 0) {
        return Reaction::ignore;
    } else if (strcmp(reaction, "report") == 0) {
        return Reaction::report_and_continue;
    } else if (strcmp(reaction, "heal") == 0) {
        return Reaction::report_heal_and_continue;
    } else if (strcmp(reaction, "quiet-heal") == 0) {
        return Reaction::heal_and_continue;
    } else {
        // default
        return Reaction::report_and_abort;
    }
}

int StackShadow::dump_width()
{
    int width = 16;
    if (auto env = getenv("SHST_DUMP_WIDTH")) {
        width = std::atoi(env);
        if (!width) {
            width = 16;
        }
    }
    return width;
}

StackShadow::DumpArea StackShadow::dump_area()
{
    auto area = getenv("SHST_DUMP_AREA");
    if (area == nullptr || strcmp(area, "both") == 0) {
        return DumpArea::both;
    } else if (strcmp(area, "original") == 0) {
        return DumpArea::original;
    } else if (strcmp(area, "shadow") == 0) {
        return DumpArea::shadow;
    } else {
        return DumpArea::both;
    }
}

bool StackShadow::dump_hide_equal_lines()
{
    auto hide = getenv("SHST_DUMP_HIDE_EQUAL");
    if (hide) {
        if (strcasecmp(hide, "yes") == 0 || strcasecmp(hide, "true") == 0 || strcasecmp(hide, "1") == 0) {
            return true;
        }
    }
    return false;
}

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

struct MemoryPrinter
{
    MemoryPrinter(size_t line_lenght = 0,
                  bool hide_equal_lines = false,
                  StackShadow::DumpArea area = StackShadow::DumpArea::both)
        : line_lenght(line_lenght)
        , hide_equal_lines(hide_equal_lines)
        , area(area)
    {
    }

    size_t line_lenght = 0;
    bool hide_equal_lines = false;
    StackShadow::DumpArea area = StackShadow::DumpArea::both;

    void print_header()
    {
        if (area == StackShadow::DumpArea::both) {
            fprintf(stderr,
                    "                      %*s      %s\n",
                    -static_cast<int>(line_lenght) * 5,
                    "ORIGINAL STACK (CORRUPTED):",
                    "SHADOW STACK (CORRECT):");
        } else if (area == StackShadow::DumpArea::original) {
            fprintf(stderr, "                      %s\n", "ORIGINAL STACK (CORRUPTED):");
        } else {
            fprintf(stderr, "                      %s\n", "SHADOW STACK (CORRECT):");
        }
    }

    void dump(FILE* out,
              const uint8_t* address,
              const uint8_t* shadow,
              size_t length,
              bool with_address = true,
              bool with_preview = true)
    {
        if (!address || !length) {
            return;
        }
        if (!out) {
            out = stderr;
        }
        if (line_lenght == 0) {
            line_lenght = 16;
        }

        auto align_start = reinterpret_cast<uintptr_t>(address) % line_lenght;
        const uint8_t* print_start = address - align_start;
        auto align_end = reinterpret_cast<uintptr_t>(address + length) % line_lenght;
        align_end = -align_end + (align_end ? line_lenght : 0);
        const uint8_t* print_end = address + length + align_end;

        int hidden_lines = 0;
        int hidden_bytes = 0;
        for (auto line_start = print_start; line_start < print_end; line_start += line_lenght) {
            auto content_start = std::max(line_start, address);
            auto content_end = std::min(line_start + line_lenght, address + length);
            auto content_lenght = content_end - content_start;
            auto content_offset = content_start - address;
            auto line_differs = memcmp(content_start, shadow + content_offset, content_lenght);

            if (hide_equal_lines && !line_differs) {
                hidden_bytes += content_lenght;
                hidden_lines += 1;
                continue;
            }
            if (hidden_bytes || hidden_lines) {
                fprintf(out, "    (%d equal bytes in %d lines hidden)\n", hidden_bytes, hidden_lines);
                hidden_bytes = hidden_lines = 0;
            }

            if (with_address) {
                fprintf(out, "%c %16p | ", line_differs ? '*' : ' ', line_start);
            }

            // actual
            if (area == StackShadow::DumpArea::both || area == StackShadow::DumpArea::original) {
                for (auto this_byte = line_start; this_byte < line_start + line_lenght; ++this_byte) {
                    auto in_area = this_byte >= address && this_byte < address + length;
                    if (in_area) {
                        bool differs = shadow ? *this_byte != shadow[this_byte - address] : false;
                        fprintf(out, "%c%02x%c", differs ? '[' : ' ', *this_byte, differs ? ']' : ' ');
                    } else {
                        fprintf(out, "    ");
                    }
                }
                if (with_preview) {
                    fprintf(out, " | ");
                    for (auto this_byte = line_start; this_byte < line_start + line_lenght; ++this_byte) {
                        auto in_area = this_byte >= address && this_byte < address + length;
                        fprintf(out, "%c", in_area ? isprint(*this_byte) ? *this_byte : '.' : ' ');
                    }
                }
            }
            if (area == StackShadow::DumpArea::both) {
                fprintf(out, " | ");
            }
            // shadow
            if (area == StackShadow::DumpArea::both || area == StackShadow::DumpArea::shadow) {
                for (auto this_byte = line_start; this_byte < line_start + line_lenght; ++this_byte) {
                    auto in_area = this_byte >= address && this_byte < address + length;
                    if (in_area) {
                        bool differs = shadow ? *this_byte != shadow[this_byte - address] : false;
                        fprintf(out, "%c%02x%c", differs ? '[' : ' ', shadow[this_byte - address], differs ? ']' : ' ');
                    } else {
                        fprintf(out, "    ");
                    }
                }
                if (with_preview) {
                    fprintf(out, " | ");
                    for (auto this_byte = line_start; this_byte < line_start + line_lenght; ++this_byte) {
                        auto in_area = this_byte >= address && this_byte < address + length;
                        fprintf(out,
                                "%c",
                                in_area ? isprint(shadow[this_byte - address]) ? shadow[this_byte - address] : '.'
                                        : ' ');
                    }
                }
            }
            fprintf(out, "\n");
        }
        if (hidden_bytes || hidden_lines) {
            fprintf(out, "    (%d equal bytes in %d lines hidden)\n", hidden_bytes, hidden_lines);
            hidden_bytes = hidden_lines = 0;
        }
    }
};

void StackShadow::check(Direction direction)
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

    if (memcmp(ot, st, depth) == 0) {
        // all is OK
        return;
    }

    auto reaction = desired_reaction();
    if (reaction == Reaction::ignore) {
        return;
    }
    if (reaction == Reaction::heal_and_continue) {
        memcpy(const_cast<uint8_t*>(ot), st, depth);
        return;
    }

    fprintf(stderr, "SHADOW STACK REPORT\n");

    fprintf(stderr, "\nDuring %s:\n", direction == Direction::PreCall ? "PRE-CALL to" : "POST-RETURN from");
    bool first = true;
    for (auto frame = stack_frames.rbegin(); frame != stack_frames.rend(); ++frame) {
        fprintf(stderr,
                "  position %10zd, size %10zd, callee %16p = %s\n",
                frame->position,
                frame->size,
                frame->callee,
                callee_traits::name(const_cast<void*>(frame->callee)).c_str());
        if (first) {
            fprintf(stderr, "NEXT SHADOW FRAMES (recent first):\n");
            first = false;
        }
    }

    fprintf(stderr, "\n");
    int const max_line = dump_width();
    const bool hide_equal_lines = false;
    MemoryPrinter orig_dump(max_line, dump_hide_equal_lines(), dump_area());
    orig_dump.print_header();

    for (auto frame = stack_frames.rbegin(); frame != stack_frames.rend(); ++frame) {
        fprintf(stderr,
                "above is frame of: %16p = %s\n",
                frame->callee,
                callee_traits::name(const_cast<void*>(frame->callee)).c_str());
        orig_dump.dump(stderr, orig.caddress(frame->position), caddress(frame->position), frame->size);
    }

    // fprintf(stderr, "ALL OF IT\n");
    // orig_dump.dump(stderr, ot, st, depth);

    fprintf(stderr, "\nbacktrace:\n");
    std::array<void*, 1024> buff;
    auto n = backtrace(buff.data(), buff.size());
    backtrace_symbols_fd(buff.data(), n, 2);

    switch (reaction) {
        case Reaction::report_and_continue:
            // no-op, report already printed
            break;
        case Reaction::report_heal_and_continue:
            memcpy(const_cast<uint8_t*>(ot), st, depth);
            break;
        case Reaction::ignore:
        case Reaction::heal_and_continue:
            // no-op, handled above
            break;
        case Reaction::report_and_abort:
        default:
            abort();
            break;
    }

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
                 std::cout.flush();
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
    void check(StackShadow::Direction direction);
    void pop();
    void ignore_above(void* stack_pointer);

  private:
    StackShadow shadow;
};

void StackThreadContext::push(void* callee, void* stack_pointer)
{
    shadow.push(callee, stack_pointer);
}

void StackThreadContext::check(StackShadow::Direction direction)
{
    shadow.check(direction);
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
    ctx.check(StackShadow::Direction::PreCall);
}

guard::~guard()
{
    StackThreadContext& ctx = getStackThreadContext();
    ctx.check(StackShadow::Direction::PostReturn);
    ctx.pop();
}

}// namespace detail
}// namespace shst

extern "C" void*
shst_invoke_impl(void* callee, void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
    long stack_position;
    shst::detail::guard g{callee, &stack_position};
    return reinterpret_cast<shst_f>(callee)(x0, x1, x2, x3, x4, x5, x6, x7);
}
