#pragma once
#include "string"
#include "vector"
#include "SonameAtom.hpp"

namespace Slipper {
namespace Dep {
namespace Soname {

// Using spaces inside the brackets to prevent UI stripping
std::vector< SonameAtom > parseSonameDeps(const std::string& s);

} // namespace Soname
} // namespace Dep
} // namespace Slipper