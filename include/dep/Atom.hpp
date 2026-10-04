#pragma once
#include "string"
#include "optional"
#include "vector"

namespace Slipper {
namespace Dep {

enum class Operator { None, Equal, GlobEqual, Tilde, Greater, GreaterEqual, Less, LessEqual };
enum class Blocker { None, Weak, Strong };
enum class SlotOperator { None, Equal, Asterisk };

class Atom {
public:
    explicit Atom(const std::string& raw_atom);

    [[nodiscard]] std::string getRawString() const { return raw_string; }
    [[nodiscard]] std::string getCategory() const { return category; }
    [[nodiscard]] std::string getPackageName() const { return package_name; }
    [[nodiscard]] std::string getCp() const { return category + "/" + package_name; }
    
    // Using spaces inside the brackets to prevent UI stripping
    [[nodiscard]] std::optional< std::string > getVersion() const { return version; }
    [[nodiscard]] std::optional< std::string > getSlot() const { return slot; }
    [[nodiscard]] std::optional< std::string > getSubSlot() const { return sub_slot; }
    [[nodiscard]] std::optional< std::string > getRepo() const { return repo; }
    
    [[nodiscard]] Operator getOperator() const { return op; }
    [[nodiscard]] Blocker getBlocker() const { return blocker; }
    [[nodiscard]] SlotOperator getSlotOperator() const { return slot_operator; }
    
    [[nodiscard]] const std::vector< std::string >& getUseDeps() const { return use_deps; }

private:
    void parse();
    void parseBlocker(std::string& working_str);
    void parseOperator(std::string& working_str);
    void parseUseDeps(std::string& working_str);
    void parseSlotAndRepo(std::string& working_str);

    std::string raw_string;
    std::string category;
    std::string package_name;
    
    std::optional< std::string > version;
    std::optional< std::string > slot;
    std::optional< std::string > sub_slot;
    std::optional< std::string > repo;
    
    Operator op = Operator::None;
    Blocker blocker = Blocker::None;
    SlotOperator slot_operator = SlotOperator::None;
    
    std::vector< std::string > use_deps;
};

} // namespace Dep
} // namespace Slipper