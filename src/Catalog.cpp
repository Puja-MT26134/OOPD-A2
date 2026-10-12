#include "bookmgmt/Catalog.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}
bool isBlank(const std::string& s) {
    return std::all_of(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
}
}  // namespace

Resource& Catalog::add(std::unique_ptr<Resource> r) {
    if (!r) throw std::invalid_argument("cannot add a null resource");
    const std::string id = r->id();
    if (items_.count(id)) throw DuplicateIdError(id);
    Entry& e = items_[id];
    e.resource = std::move(r);
    return *e.resource;
}

bool Catalog::contains(const std::string& id) const { return items_.count(id) > 0; }

Resource* Catalog::find(const std::string& id) {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

const Resource* Catalog::find(const std::string& id) const {
    auto it = items_.find(id);
    return it == items_.end() ? nullptr : it->second.resource.get();
}

Resource& Catalog::get(const std::string& id) {
    if (Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

const Resource& Catalog::get(const std::string& id) const {
    if (const Resource* r = find(id)) return *r;
    throw NotFoundError(id);
}

void Catalog::remove(const std::string& id) {
    //if (items_.erase(id) == 0) throw NotFoundError(id);
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    if (it->second.inUse > 0)
        throw std::invalid_argument("cannot remove a resource that is in use: " + id);
    items_.erase(it);
}

int Catalog::holdings(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    return it->second.holdings;
}

void Catalog::addHoldings(const std::string& id, int units) {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    if (it->second.holdings + units < 0)
        throw std::invalid_argument("holdings cannot become negative");
    if (it->second.holdings + units < it->second.inUse)
        throw std::invalid_argument("holdings cannot drop below the units in use");
    it->second.holdings += units;
}

int Catalog::inUse(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    return it->second.inUse;
}

int Catalog::available(const std::string& id) const {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    return it->second.holdings - it->second.inUse;
}

void Catalog::addInUse(const std::string& id, int units) {
    auto it = items_.find(id);
    if (it == items_.end()) throw NotFoundError(id);
    const int updated = it->second.inUse + units;
    if (updated < 0 || updated > it->second.holdings)
        throw std::invalid_argument("units in use must stay between 0 and the holdings");
    it->second.inUse = updated;
}

std::vector<const Resource*> Catalog::where(
    const std::function<bool(const Resource&)>& pred) const {
    std::vector<const Resource*> out;
    for (const auto& [id, entry] : items_)
        if (pred(*entry.resource)) out.push_back(entry.resource.get());
    return out;
}

std::vector<const Resource*> Catalog::all() const {
    return where([](const Resource&) { return true; });
}

std::vector<const Resource*> Catalog::byCategory(ResourceCategory c) const {
    return where([c](const Resource& r) { return r.category() == c; });
}

std::vector<const Resource*> Catalog::searchTitle(const std::string& text) const {
    const std::string needle = lower(text);
    return where([&needle](const Resource& r) {
        return lower(r.title()).find(needle) != std::string::npos;
    });
}

std::vector<const Resource*> Catalog::searchAuthor(const std::string& name) const {
    if (isBlank(name)) throw std::invalid_argument("author to search for must not be blank");
    return where([&name](const Resource& r) { return r.hasAuthor(name); });
}

std::vector<const Resource*> Catalog::searchIdentifier(const std::string& code) const {
    if (isBlank(code)) throw std::invalid_argument("ISBN/ISSN to search for must not be blank");
    return where([&code](const Resource& r) { return r.hasIdentifier(code); });
}

std::vector<const Resource*> Catalog::searchYears(int fromYear, int toYear) const {
    if (fromYear > toYear)
        throw std::invalid_argument("first year of the range must not be after the last");
    return where([fromYear, toYear](const Resource& r) {
        return r.year() >= fromYear && r.year() <= toYear;
    });
}

}  // namespace bookmgmt
