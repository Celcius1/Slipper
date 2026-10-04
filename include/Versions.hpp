#pragma once
#include "string"
#include "optional"

namespace Slipper {
namespace Versions {

// Compares two Gentoo version strings.
// Returns:
//   1 if ver1 is greater than ver2
//  -1 if ver1 is less than ver2
//   0 if ver1 equals ver2
// std::nullopt if either version string is invalid according to Gentoo specifications.
std::optional< int > vercmp(const std::string& ver1, const std::string& ver2);

// Validates if a version string conforms to Gentoo's syntax rules.
bool ververify(const std::string& myver);

} // namespace Versions
} // namespace Slipper