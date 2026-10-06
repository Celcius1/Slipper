#pragma once
#include <string>
#include <optional>

namespace Slipper {
namespace Versions {

std::optional< int > vercmp(const std::string& ver1, const std::string& ver2);
bool ververify(const std::string& myver);

// EAPI 8 Strict Name Validation Allow-Lists
bool isValidCategory(const std::string& category);
bool isValidPackageName(const std::string& package_name);
bool isValidUseFlag(const std::string& flag);
} // namespace Versions
} // namespace Slipper