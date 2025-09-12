#include "../src/shadow-stack.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

int BOOM_OFFSET = 0;
void set_boom_offset(int offset)
{
    BOOM_OFFSET = offset;
}

int get_boom_offset()
{
    return BOOM_OFFSET;
}

struct Ancestor
{
    virtual void care(char* legacy) = 0;
};

struct DescendantMD : Ancestor
{
    void care(char* legacy) override
    {
        shst::invoke(&::printf, "I'm a doctor and I care about my %s!", legacy );
    }
};

struct DescendantLawyer : Ancestor
{
    void care(char* legacy) override
    {
        shst::invoke(&::printf, "I'm a lawyer and I also care about my %s", legacy);
    }
};

struct DescendantIT : Ancestor
{
    void care(char* legacy) override
    {
        legacy += get_boom_offset();
        shst::invoke(&::strcpy, legacy, "I'm a black sheep and I don't care");
    }
};

void generation3(char* legacy)
{
    DescendantIT it;
    shst::invoke(&Ancestor::care, it, legacy);
}

void generation2(char* legacy)
{
    DescendantLawyer lawyer;
    shst::invoke(&Ancestor::care, lawyer, legacy);
    shst::invoke(generation3, legacy);
}

void generation1(char* legacy)
{
    DescendantMD md;
    shst::invoke(&Ancestor::care, md, legacy); // <-- md.care(legacy);
    shst::invoke(generation2, legacy);
}

void generations(char* legacy)
{
    shst::invoke(generation1, legacy);
}

void life()
{
    char legacy[32] = "Legacy";
    shst::invoke(generations, legacy);
}

int main(int argc, char* argv[])
{
    if (argc > 1) {
        set_boom_offset(atoi(argv[1]));
    }
    shst::invoke(life);
}
