#include "bookmgmt/Lending.h"

#include <algorithm>
#include <cctype>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
void requirePatron(const std::string& patron) {
    const bool blank = std::all_of(patron.begin(), patron.end(),
                                   [](unsigned char ch) { return std::isspace(ch) != 0; });
    if (blank) throw std::invalid_argument("patron name must not be blank");
}

// Checks that the resource is of the kind that this operation is for.
void requireKind(const Resource& r, bool digital) {
    if (r.isDigital() == digital) return;
    throw LendingError(r.id() + (r.isDigital()
                                     ? " is an electronic resource: open a session instead"
                                     : " is a print resource: borrow a copy instead"));
}
}  // namespace

LendingDesk::LendingDesk(Catalog& catalog) : catalog_(catalog) {}

void LendingDesk::start(const std::string& patron, const std::string& resourceId,
                        bool digital) {
    requirePatron(patron);
    const Resource& r = catalog_.get(resourceId);  // may throw NotFoundError
    requireKind(r, digital);

    const char* what = digital ? "an open session on " : "a copy of ";
    auto it = active_.find(resourceId);
    if (it != active_.end() && it->second.count(patron))
        throw LendingError(patron + " already has " + what + resourceId);

    if (catalog_.available(resourceId) <= 0)
        throw UnavailableError(std::string("no ") + (digital ? "seats" : "copies") + " of " +
                               resourceId + " are free (" +
                               std::to_string(catalog_.inUse(resourceId)) + " of " +
                               std::to_string(catalog_.holdings(resourceId)) + " in use)");

    catalog_.addInUse(resourceId, 1);
    active_[resourceId].insert(patron);
}

void LendingDesk::finish(const std::string& patron, const std::string& resourceId,
                         bool digital) {
    requirePatron(patron);
    const Resource& r = catalog_.get(resourceId);  // may throw NotFoundError
    requireKind(r, digital);

    auto it = active_.find(resourceId);
    if (it == active_.end() || it->second.erase(patron) == 0)
        throw LendingError(patron + " has no " + (digital ? "open session on " : "copy of ") +
                           resourceId);
    if (it->second.empty()) active_.erase(it);
    catalog_.addInUse(resourceId, -1);
}

void LendingDesk::borrowCopy(const std::string& patron, const std::string& resourceId) {
    start(patron, resourceId, false);
}

void LendingDesk::returnCopy(const std::string& patron, const std::string& resourceId) {
    finish(patron, resourceId, false);
}

void LendingDesk::openSession(const std::string& patron, const std::string& resourceId) {
    start(patron, resourceId, true);
}

void LendingDesk::closeSession(const std::string& patron, const std::string& resourceId) {
    finish(patron, resourceId, true);
}

int LendingDesk::available(const std::string& resourceId) const {
    return catalog_.available(resourceId);
}

int LendingDesk::inUse(const std::string& resourceId) const {
    return catalog_.inUse(resourceId);
}

bool LendingDesk::isActive(const std::string& patron, const std::string& resourceId) const {
    auto it = active_.find(resourceId);
    return it != active_.end() && it->second.count(patron) > 0;
}

std::vector<std::string> LendingDesk::patronsOf(const std::string& resourceId) const {
    auto it = active_.find(resourceId);
    if (it == active_.end()) return {};
    return std::vector<std::string>(it->second.begin(), it->second.end());  // std::set: sorted
}

std::vector<std::string> LendingDesk::resourcesOf(const std::string& patron) const {
    std::vector<std::string> ids;
    for (const auto& entry : active_)
        if (entry.second.count(patron)) ids.push_back(entry.first);  // std::map: sorted
    return ids;
}

void LendingDesk::printReport(std::ostream& os) const {
    if (active_.empty()) {
        os << "Nothing is on loan and no session is open\n";
        return;
    }
    for (const auto& [id, patrons] : active_) {
        const Resource& r = catalog_.get(id);
        os << "  " << id << "  " << r.title() << ": " << patrons.size() << " of "
           << catalog_.holdings(id) << (r.isDigital() ? " seats in use: " : " copies on loan: ");
        bool first = true;
        for (const auto& patron : patrons) {
            os << (first ? "" : ", ") << patron;
            first = false;
        }
        os << "\n";
    }
}

}  // namespace bookmgmt