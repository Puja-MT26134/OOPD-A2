#pragma once
// Catalog: owns every Resource, keyed by its id, and tracks copies/seats held.

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

class Catalog {
public:
    // Takes ownership. Throws DuplicateIdError if the id is already present.
    Resource& add(std::unique_ptr<Resource> r);

    // Convenience: catalog.emplace<Book>(ctor args...)
    template <typename T, typename... Args>
    T& emplace(Args&&... args) {
        auto p = std::make_unique<T>(std::forward<Args>(args)...);
        T& ref = *p;
        add(std::move(p));
        return ref;
    }

    bool contains(const std::string& id) const;
    Resource* find(const std::string& id);              // nullptr if absent
    const Resource* find(const std::string& id) const;  // nullptr if absent
    Resource& get(const std::string& id);               // throws NotFoundError
    const Resource& get(const std::string& id) const;   // throws NotFoundError
    void remove(const std::string& id);                 // throws NotFoundError

    std::size_t size() const { return items_.size(); }
    bool empty() const { return items_.empty(); }

    // Holdings: number of copies (print) or seats (electronic) owned.
    int holdings(const std::string& id) const;
    void addHoldings(const std::string& id, int units);

        // Q14: units currently lent out (print copies on loan, electronic sessions open).
    // available = holdings - inUse. addInUse(id, +n) lends units out, addInUse(id, -n)
    // takes them back; it throws std::invalid_argument if that would make the number in
    // use negative or larger than the holdings. addHoldings() refuses to drop the holdings
    // below the units in use, and remove() refuses to remove a resource that is in use.
    int inUse(const std::string& id) const;
    int available(const std::string& id) const;
    void addInUse(const std::string& id, int units);

    // Queries. Results are in id order and point into the catalog.
    std::vector<const Resource*> all() const;
    std::vector<const Resource*> byCategory(ResourceCategory c) const;
    std::vector<const Resource*> searchTitle(const std::string& text) const;  // case-insensitive

                // Q13: searches (results in id order). searchAuthor: any author's name contains the
    // text, ignoring case. searchIdentifier: the ISBN or ISSN equals the code, ignoring
    // case, hyphens and spaces. searchYears: publication year from..to, both inclusive.
    // searchAuthor/searchIdentifier throw std::invalid_argument for blank text and
    // searchYears for from > to.
    std::vector<const Resource*> searchAuthor(const std::string& name) const;
    std::vector<const Resource*> searchIdentifier(const std::string& code) const;
    std::vector<const Resource*> searchYears(int fromYear, int toYear) const;
    std::vector<const Resource*> where(
        const std::function<bool(const Resource&)>& pred) const;

private:
    struct Entry {
        std::unique_ptr<Resource> resource;
        int holdings = 0;
        int inUse=0; //Q14
    };
    std::map<std::string, Entry> items_;
};

}  // namespace bookmgmt
