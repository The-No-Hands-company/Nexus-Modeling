#include <nexus/cad/CadDesignRule.h>

namespace nexus::cad {

void CadDesignChecker::addRule(const DesignRule& r) { m_rules.push_back(r); }
std::vector<DesignRuleViolation> CadDesignChecker::check(const CadDocument&) const noexcept { return {}; }
std::vector<DesignRule> CadDesignChecker::builtinRules() noexcept {
    std::vector<DesignRule> rules;
    for(const auto& def : allDesignRules()) {
        DesignRule r; r.id=def.id; r.description=def.description;
        r.severity=(def.severity==0)?DesignRuleSeverity::Info:(def.severity==1)?DesignRuleSeverity::Warning:(def.severity==2)?DesignRuleSeverity::Error:DesignRuleSeverity::Critical;
        rules.push_back(r);
    }
    return rules;
}
const std::vector<DesignRule>& CadDesignChecker::rules() const noexcept { return m_rules; }
size_t CadDesignChecker::errorCount(const std::vector<DesignRuleViolation>& v) const noexcept {
    size_t c=0; for(auto& x:v) if(x.severity==DesignRuleSeverity::Error||x.severity==DesignRuleSeverity::Critical)c++; return c;
}
size_t CadDesignChecker::warningCount(const std::vector<DesignRuleViolation>& v) const noexcept {
    size_t c=0; for(auto& x:v) if(x.severity==DesignRuleSeverity::Warning)c++; return c;
}

}
