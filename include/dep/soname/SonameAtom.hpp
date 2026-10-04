#pragma once
#include "string"

namespace Slipper {
namespace Dep {
namespace Soname {

// ---------------------------------------------------------
// SONAME ATOM
// Represents a specific shared library dependency bound to an architecture.
// ---------------------------------------------------------
class SonameAtom {
public:
    SonameAtom(const std::string& category, const std::string& name)
        : multilib_category(category), soname(name) {}

    [[nodiscard]] std::string getCategory() const { return multilib_category; }
    [[nodiscard]] std::string getSoname() const { return soname; }
    
    // Returns the string formatted exactly like Portage (e.g. "x86_64: libc.so.6")
    [[nodiscard]] std::string toString() const { return multilib_category + ": " + soname; }

    bool operator==(const SonameAtom& other) const {
        return multilib_category == other.multilib_category && soname == other.soname;
    }

private:
    std::string multilib_category;
    std::string soname;
};

} // namespace Soname
} // namespace Dep
} // namespace Slipper