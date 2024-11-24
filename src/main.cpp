#include "shadow-stack.h"
#include <cstdint>
#include <cstring>
#include <iostream>
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
    for (int i = 0; i < depth; ++i) {
        if (i % max_line == 0) {
            fprintf(stderr, "\n%16p: ", ot + i);
        }
        fprintf(stderr, "%c%02x%c", ot[i] != st[i] ? '[' : ' ', ot[i], ot[i] != st[i] ? ']' : ' ');
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "SHADOW (CORRECT):\n");
    for (int i = 0; i < depth; ++i) {
        if (i % max_line == 0) {
            fprintf(stderr, "\n%16p: ", st + i);
        }
        fprintf(stderr, "%c%02x%c", ot[i] != st[i] ? '[' : ' ', st[i], ot[i] != st[i] ? ']' : ' ');
    }
    fprintf(stderr, "\n");
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    long sp{};
    std::cout << "main: " << &sp << "\n";
    /*ranges_test();*/
    test_check();
    std::cout << "main done\n";
}
