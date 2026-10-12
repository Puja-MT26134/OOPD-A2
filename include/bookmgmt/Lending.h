#pragma once
// LendingDesk (Q14): patrons borrow and return print copies, and open and close
// sessions on electronic resources.
//
// The number of copies / seats a resource has is its holdings in the Catalog; the
// Catalog also counts how many are in use, so the limit can never be exceeded and a
// purchase cancellation cannot take away copies that are out on loan. The desk
// remembers which patron has what.
//
// Rules: a patron can have at most one copy or session of the same resource at a time;
// print resources are borrowed and returned, electronic resources (isDigital()) are
// opened and closed.

#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bookmgmt/Catalog.h"

namespace bookmgmt {

class LendingDesk {
public:
    explicit LendingDesk(Catalog& catalog);

    // Print resources. Throw NotFoundError (unknown resource), LendingError (not a print
    // resource; already borrowed / not borrowed by this patron), UnavailableError (no
    // copy free) or std::invalid_argument (blank patron name). Nothing changes on failure.
    void borrowCopy(const std::string& patron, const std::string& resourceId);
    void returnCopy(const std::string& patron, const std::string& resourceId);

    // Electronic resources: the same rules, and the limit is the number of licensed seats.
    void openSession(const std::string& patron, const std::string& resourceId);
    void closeSession(const std::string& patron, const std::string& resourceId);
    

    int available(const std::string& resourceId) const;  // copies / seats still free
    int inUse(const std::string& resourceId) const;      // copies on loan / open sessions
    bool isActive(const std::string& patron, const std::string& resourceId) const;
    std::vector<std::string> patronsOf(const std::string& resourceId) const;  // sorted
    std::vector<std::string> resourcesOf(const std::string& patron) const;    // sorted ids

    void printReport(std::ostream& os) const;

private:
    void start(const std::string& patron, const std::string& resourceId, bool digital);
    void finish(const std::string& patron, const std::string& resourceId, bool digital);

    Catalog& catalog_;
    std::map<std::string, std::set<std::string>> active_;  // resource id -> patrons
};

}  // namespace bookmgmt