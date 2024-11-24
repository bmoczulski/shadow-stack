#include "callee_traits.hpp"

#include <memory>
#include <sstream>

namespace callee_traits {
namespace detail {

void* address(void* p)
{
    return p;
}

}
std::string name(void* callee)
{
    void* buffer[]{const_cast<void*>(callee)};
    auto symbols = std::unique_ptr<char*, decltype(free)*>(backtrace_symbols(buffer, 1), free);
    if (symbols) {
        if (symbols.get()[0]) {
            return symbols.get()[0];
        } else {
            std::ostringstream os;
            os << std::hex << std::showbase << callee;
            return os.str();
        }
    } else {
        return "<unknown_callee>";
    }
}

}

