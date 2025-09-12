#include <algorithm>
#include <array>
#include <cstddef>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <execinfo.h>
#include <iterator>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>
#include "shadow-stack.hpp"
#include "shadow-stack-common.h"
#include "callee_traits.hpp"

#ifdef HAVE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

namespace shst {

// ANSI color codes for hex dump differences
static constexpr const char* ANSI_RED_BLINK = "\033[5;41m";
static constexpr const char* ANSI_GREEN_BLINK = "\033[5;42m";
static constexpr const char* ANSI_RESET = "\033[0m";

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
        actual,
        shadow
    };

    Reaction desired_reaction();
    int dump_width();
    DumpArea dump_area();
    bool dump_hide_equal_lines();
    bool should_use_color();

    void push(void* callee, void* stack_pointer);
    void check(Direction);
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
    } else if (strcmp(area, "actual") == 0) {
        return DumpArea::actual;
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

bool StackShadow::should_use_color()
{
    auto color = getenv("SHST_DUMP_COLOR");
    if (color == nullptr || strcmp(color, "auto") == 0) {
        return isatty(STDERR_FILENO);
    } else if (strcmp(color, "always") == 0) {
        return true;
    } else if (strcmp(color, "never") == 0) {
        return false;
    } else {
        return isatty(STDERR_FILENO);
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
                  StackShadow::DumpArea area = StackShadow::DumpArea::both,
                  bool use_color = false)
        : line_lenght(line_lenght)
        , hide_equal_lines(hide_equal_lines)
        , area(area)
        , use_color(use_color)
    {
    }

    size_t line_lenght = 0;
    bool hide_equal_lines = false;
    StackShadow::DumpArea area = StackShadow::DumpArea::both;
    bool use_color = false;

    void print_header()
    {
        if (area == StackShadow::DumpArea::both) {
            fprintf(stderr,
                    "                      %*s      %s\n",
                    -static_cast<int>(line_lenght) * 5,
                    "ACTUAL STACK (CORRUPTED):",
                    "SHADOW STACK (CORRECT):");
        } else if (area == StackShadow::DumpArea::actual) {
            fprintf(stderr, "                      %s\n", "ACTUAL STACK (CORRUPTED):");
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
                fprintf(out, "%c %16p |", line_differs ? '*' : ' ', line_start);
            }

            auto print_hex_section = [&](const uint8_t* data_source, const char* color_code, auto get_preview_char) {
                bool prev_differs = false;
                for (auto this_byte = line_start; this_byte < line_start + line_lenght; ++this_byte) {
                    auto in_area = this_byte >= address && this_byte < address + length;
                    if (in_area) {
                        bool differs = shadow ? *this_byte != shadow[this_byte - address] : false;

                        const char* prefix = " ";
                        const char* suffix = "";
                        const char* color_start = "";
                        const char* color_end = "";
                        const char* suffix_reset = "";

                        if (differs && !prev_differs) {
                            prefix = "[";
                            color_start = use_color ? color_code : "";
                        } else if (!differs && prev_differs) {
                            prefix = "]";
                            color_end = use_color ? ANSI_RESET : "";
                        }

                        bool is_last_char = (this_byte + 1 >= line_start + line_lenght) ||
                                            (this_byte + 1 >= address + length);
                        if (differs && is_last_char) {
                            suffix = "]";
                            suffix_reset = use_color ? ANSI_RESET : "";
                        } else if (is_last_char) {
                            suffix = " ";
                        }

                        fprintf(out, "%s%s%s%02x%s%s", color_start, prefix, color_end, data_source[this_byte - address], suffix, suffix_reset);
                        prev_differs = differs;
                    } else {
                        if (prev_differs) {
                            fprintf(out, "]%s", use_color ? ANSI_RESET : "");
                            prev_differs = false;
                        }
                        fprintf(out, "   ");
                    }
                }
                if (with_preview) {
                    fprintf(out, "| ");
                    for (auto this_byte = line_start; this_byte < line_start + line_lenght; ++this_byte) {
                        auto in_area = this_byte >= address && this_byte < address + length;
                        bool color = in_area && use_color && (shadow ? *this_byte != shadow[this_byte - address] : false);
                        fprintf(out, "%s%c%s",
                            color ? color_code : "",
                            in_area ? get_preview_char(this_byte - address) : ' ',
                            color ? ANSI_RESET : ""
                        );
                    }
                }
            };

            // actual
            if (area == StackShadow::DumpArea::both || area == StackShadow::DumpArea::actual) {
                print_hex_section(address, ANSI_RED_BLINK, [&](size_t offset) {
                    return isprint(address[offset]) ? address[offset] : '.';
                });
            }
            if (area == StackShadow::DumpArea::both) {
                fprintf(out, " |");
            }
            // shadow
            if (area == StackShadow::DumpArea::both || area == StackShadow::DumpArea::shadow) {
                print_hex_section(shadow, ANSI_GREEN_BLINK, [&](size_t offset) {
                    return isprint(shadow[offset]) ? shadow[offset] : '.';
                });
            }
            fprintf(out, "%s\n", with_preview ? " |" : "");
        }
        if (hidden_bytes || hidden_lines) {
            fprintf(out, "    (%d equal bytes in %d lines hidden)\n", hidden_bytes, hidden_lines);
            hidden_bytes = hidden_lines = 0;
        }
    }
};

void StackShadow::check(Direction direction)
{
    size_t last_position = orig.position(orig.cend());
    if (!stack_frames.empty()) {
        last_position = stack_frames.back().position;
    }

    auto ot = orig.caddress(last_position);
    auto ob = orig.cend();
    auto st = caddress(last_position);
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
    MemoryPrinter orig_dump(dump_width(), dump_hide_equal_lines(), dump_area(), should_use_color());
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

    auto print_enhanced_backtrace = []() {
        fprintf(stderr, "\nbacktrace:\n");
        std::array<void*, 1024> buff;
        auto n = backtrace(buff.data(), buff.size());

        // Try libunwind if backtrace() failed (common on musl)
        if (n == 0) {
#ifdef HAVE_LIBUNWIND
            fprintf(stderr, "backtrace() failed, trying libunwind...\n");

            unw_cursor_t cursor;
            unw_context_t context;

            int ret = unw_getcontext(&context);
            if (ret < 0) {
                fprintf(stderr, "ERROR: unw_getcontext() failed: %d\n", ret);
                return;
            }

            ret = unw_init_local(&cursor, &context);
            if (ret < 0) {
                fprintf(stderr, "ERROR: unw_init_local() failed: %d\n", ret);
                return;
            }

            int frame = 0;
            do {
                unw_word_t ip;
                ret = unw_get_reg(&cursor, UNW_REG_IP, &ip);
                if (ret < 0) {
                    fprintf(stderr, "ERROR: unw_get_reg() failed: %d\n", ret);
                    break;
                }

                // Try to get symbol name
                char symbol[256];
                unw_word_t offset;
                ret = unw_get_proc_name(&cursor, symbol, sizeof(symbol), &offset);

                if (ret == 0) {
                    fprintf(stderr, "Frame %2d: 0x%016lx in %s+0x%lx\n",
                           frame, (unsigned long)ip, symbol, (unsigned long)offset);
                } else {
                    fprintf(stderr, "Frame %2d: 0x%016lx in <unknown>\n",
                           frame, (unsigned long)ip);
                }

                frame++;
                if (frame > 20) {  // Prevent infinite loops
                    fprintf(stderr, "... (truncated, too many frames)\n");
                    break;
                }

            } while ((ret = unw_step(&cursor)) > 0);

            if (ret < 0) {
                fprintf(stderr, "ERROR: unw_step() failed: %d\n", ret);
            }

            fprintf(stderr, "Total frames: %d\n", frame);
#else
            fprintf(stderr, "backtrace() failed and libunwind not available\n");
#endif
        } else {
            // backtrace() worked, use it
            backtrace_symbols_fd(buff.data(), n, 2);
        }
    };

    print_enhanced_backtrace();

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

StackThreadContext& getStackThreadContext()
{
    thread_local StackThreadContext ctx;
    return ctx;
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

} // namespace detail
} // namespace shst

extern "C" void*
shst_invoke_impl(void* callee, void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6, void* x7)
{
    long stack_position;
    shst::detail::guard g{callee, &stack_position};
    return reinterpret_cast<shst_f>(callee)(x0, x1, x2, x3, x4, x5, x6, x7);
}
