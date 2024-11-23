#include "shadow-stack.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <ios>
#include <ranges>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace std;

bool is_addr_in_range(void* addr, void* from, size_t size)
{
    return (static_cast<uint8_t*>(addr) >= static_cast<uint8_t*>(from)) &&
           (static_cast<uint8_t*>(addr) < (static_cast<uint8_t*>(from) + size));
}

void test_stack_range()
{
    int dummy{};
    pthread_attr_t attr;
    pthread_getattr_np(pthread_self(), &attr);
    void* stackaddr{};
    size_t stacksize{};
    pthread_attr_getstack(&attr, &stackaddr, &stacksize);
    cout << "Stack: " << stackaddr << ": " << stacksize << ": local var: " << &dummy
         << " in range: " << is_addr_in_range(&dummy, stackaddr, stacksize) << '\n';
}

//////////////////////////////////////////////////////////////////////////
/// experiment
//////////////////////////////////////////////////////////////////////////

long foo(...)
{
    return 0;
}

void* foo_wrapper(void* x0, void* x1, void* x2, void* x3, void* x4, void* x5, void* x6)
{
    return shst_invoke_impl(reinterpret_cast<void*>(foo), x0, x1, x2, x3, x4, x5, x6);
}

//////////////////////////////////////////////////////////////////////////
/// end of experiment
//////////////////////////////////////////////////////////////////////////
void ranges_test()
{
    std::string s1{"ala ma kota"};
    std::string s2{"ala ma też psa"};
    auto res = std::ranges::mismatch(s1, s2);
    std::cout << std::string_view(res.in1, s1.end()) << '\n';
    std::cout << std::string_view(res.in2, s2.end()) << '\n';
}

void print_range(std::string_view msg, auto const& range)
{
    std::cout << msg << ": " << std::string_view(range.cbegin(), range.cend()) << '\n';
}

void test_check()
{
    std::vector<char> orig(128, '+');
    std::vector<char> shadow(128, '+');
    shadow[54] = '?';
    shadow[55] = '?';

    auto ot = orig.data();
    auto ob = ot + orig.size();
    auto st = shadow.data();
    // auto sb = st + shadow.size();
    auto depth = ob - ot;

    size_t const max_line = 16;

        fprintf(stderr, "ORIG (CORRUPTED):\n");
        for (int i = 0; i < depth; ++i)
        {
            if (i % max_line == 0) {
                fprintf(stderr, "\n%16p: ", ot + i);
            }
            fprintf(stderr, "%c%02x%c", ot[i] != st[i] ? '[' : ' ', ot[i], ot[i] != st[i] ? ']' : ' ');
        }
        fprintf(stderr, "\n");
        fprintf(stderr, "SHADOW (CORRECT):\n");
        for (int i = 0; i < depth; ++i)
        {
            if (i % max_line == 0) {
                fprintf(stderr, "\n%16p: ", st + i);
            }
            fprintf(stderr, "%c%02x%c", ot[i] != st[i] ? '[' : ' ', st[i], ot[i] != st[i] ? ']' : ' ');
        }
        fprintf(stderr, "\n");
#if 0
    using std::ranges::for_each;
    using std::ranges::subrange;
    using std::views::chunk;
    using std::views::counted;
    using std::views::filter;
    using std::views::zip;

    std::vector<char> orig(128, '+');
    std::vector<char> shadow(128, '+');
    shadow[54] = '?';
    auto orig_range = subrange(orig);
    auto shadow_range = subrange(shadow);

    size_t const max_line = 16;

    for_each(std::views::zip(orig_range | chunk(max_line), shadow_range | chunk(max_line)) |
                     filter([](auto const& pair) {
                         /*print_range("check orig", get<0>(pair));*/
                         /*print_range("check shad", get<1>(pair));*/
                         auto res = std::ranges::mismatch(get<0>(pair), get<1>(pair));
                         return res.in1 != get<0>(pair).end();
                     }),
             []([[maybe_unused]] auto const& pair) {
                 /*print_range(" diff orig", get<0>(pair));*/
                 /*print_range(" diff shad", get<1>(pair));*/

                 cout << std::hex;
                 cout << std::setw(16) << static_cast<void*>(&(*get<0>(pair).begin())) << ": ";

                 auto old_flags = cout.setf(std::ios::hex);
                 auto old_fill = cout.fill('0');

                 for_each(get<0>(pair) | chunk(4), [first = bool{true}](auto const& ch8) mutable {
                     std::cout << ((first) ? (first = false, "") : " ");
                     for_each(ch8, [](auto b) { cout << std::setw(2) << static_cast<unsigned>(b & 0xFF); });
                 });
                 cout << "    ";
                 for_each(get<1>(pair) | chunk(4), [first = bool{true}](auto const& ch8) mutable {
                     std::cout << ((first) ? (first = false, "") : " ");
                     for_each(ch8, [](auto b) { cout << std::setw(2) << static_cast<unsigned>(b & 0xFF); });
                 });
                 cout << '\n';

                 cout.setf(old_flags);
                 cout.fill(old_fill);
             });
#endif
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    long sp{};
    std::cout << "main: " << &sp << "\n";
    /*ranges_test();*/
    test_check();
    std::cout << "main done\n";
}
