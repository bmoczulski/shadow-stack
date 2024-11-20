#include "callee_traits.hpp"
#include <cstdio>

void do_backtrace(void *callee, const char *file, int line, const char *function, const char *pretty_function)
{
    size_t injected = callee ? 1 : 0;
    void *buffer[50];
    buffer[0] = callee;
    int depth = backtrace(buffer + injected, sizeof(buffer) - injected);
    fprintf(stderr,
        "LOCATION        : [%s:%d]\n"
        "FUNCTION        : %s\n"
        "PRETTY FUNCTION : %s\n"
        "CALLEE NAME     : %s\n"
        "BACKTRACE SIZE  : %zd%s\n",
        file, line, function, pretty_function,
        callee_traits::name(callee).c_str(),
        depth + injected,
        injected ? " (including +1 callee on top)" : " (pristine)"
    );
    backtrace_symbols_fd(buffer, depth + injected, 2);
    fprintf(stderr, "\n");
}
#define DO_BACKTRACE_C(callee) do_backtrace(reinterpret_cast<void*>(callee), __FILE__, __LINE__, __FUNCTION__, __PRETTY_FUNCTION__)
#define DO_BACKTRACE() DO_BACKTRACE_C(0)

long foo()
{
    DO_BACKTRACE_C(foo);
    return 42;
}

struct S
{
    S() = default;
    S(int f) : f(f){} // to please const

    int f;
    int foo() { DO_BACKTRACE(); return ++f; }
    int foo() const { DO_BACKTRACE(); return -f; }
    virtual int bar() { return f; }
};

struct D : public S
{
    virtual int bar() { return f + 1; }
};

void print_name(const char *the_name, void *a)
{
    auto n = callee_traits::name(a);
    printf("callee_traits: %20s = %16p %s\n",
        the_name, a, n.c_str()
    );
}
#define PRINT_NAME(...) print_name(#__VA_ARGS__, callee_traits::address(__VA_ARGS__))

int main()
{
  	long a = 5;
    auto lambda = [&](long b)
    {
        DO_BACKTRACE();
        return a + b + 1;
    };
    auto sf = std::function(lambda);

    S s, *p = &s;
    const S cs{21};
    D d, *dp = &d;
    S *pp = &d, &pr = d;
    {
        printf("foo()     = %ld\n", foo());
        printf("&lambda   = %p\n", &lambda);
        printf("lambda(1) = %ld\n", lambda(1));
        printf("sf(1)     = %ld\n", sf(1));
        printf("s.foo()   = %d\n", s.foo());
        printf("cs.foo()  = %d\n", cs.foo());
        printf("\n");

        PRINT_NAME(&main);       // canonical function address
        PRINT_NAME(&foo);
        PRINT_NAME(foo);         // raw function name as address (without &)
        PRINT_NAME(lambda);      // lambda
        PRINT_NAME(sf);          // std::function

        PRINT_NAME(&S::foo);     // non-virtual method
        PRINT_NAME(cs, &S::foo); // non-virtual method (const)    // WRONG, this picks non-const one!

        PRINT_NAME(&S::bar);     // unbound virtual method
        PRINT_NAME(s, &S::bar);  // bound virtual method via instance reference
        PRINT_NAME(p, &S::bar);  // bound virtual method via instance pointer
        PRINT_NAME(cs, &S::bar); // bound virtual method (const)  // WRONG, this picks non-const one!


        PRINT_NAME(&D::bar);     // unbound virtual method in derived class
        PRINT_NAME(d, &D::bar);  // bound (child) virtual method in derived class via instance reference
        PRINT_NAME(d, &S::bar);  // bound (parent) virtual method in derived class via instance reference
        PRINT_NAME(dp, &D::bar);  // bound (child) virtual method in derived class via instance pointer
        PRINT_NAME(dp, &S::bar);  // bound (parent) virtual method in derived class via instance pointer
        PRINT_NAME(pr, &D::bar);  // bound (child) virtual method in derived class via reference to parent
        PRINT_NAME(pr, &S::bar);  // bound (parent) virtual method in derived class via reference to parent
        PRINT_NAME(pp, &D::bar);  // bound (child) virtual method in derived class via pointer to parent
        PRINT_NAME(pp, &S::bar);  // bound (parent) virtual method in derived class via pointer to parent
    }
}
