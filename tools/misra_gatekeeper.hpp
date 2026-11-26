#pragma once

#include <string>
#include <vector>

namespace secu::misra {

struct Violation {
  std::string file;
  unsigned line{0U};
  unsigned column{0U};
  std::string rule_id;
  std::string category;
  std::string message;
};

struct AuditReport {
  std::vector<std::string> analyzed_files;
  std::vector<Violation> violations;

  bool passed() const noexcept { return violations.empty(); }
};

}  // namespace secu::misra
