#include "bookmgmt/Resource.h"

#include <ostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace bookmgmt {

const char* categoryName(ResourceCategory c) {
    switch (c) {
        case ResourceCategory::Book: return "Book";
        case ResourceCategory::ElectronicResource: return "ElectronicResource";
        case ResourceCategory::Journal: return "Journal";
        case ResourceCategory::EBook: return "EBook";
        case ResourceCategory::AudioBook: return "AudioBook";
        case ResourceCategory::Thesis: return "Thesis";
    }
    return "Unknown";
}

Resource::Resource(std::string id, std::string title, std::string publisher,
                   int year, Money unitPrice)
    : id_(std::move(id)),
      title_(std::move(title)),
      publisher_(std::move(publisher)),
      year_(year),
      unitPrice_(unitPrice) {
    if (id_.empty()) throw std::invalid_argument("resource id must not be empty");
    if (title_.empty()) throw std::invalid_argument("resource title must not be empty");
    if (unitPrice_.isNegative()) throw std::invalid_argument("price must not be negative");
}

void Resource::setUnitPrice(Money price) {
    if (price.isNegative()) throw std::invalid_argument("price must not be negative");
    unitPrice_ = price;
}

void Resource::requirePositive(int quantity) {
    if (quantity <= 0) throw std::invalid_argument("quantity must be positive");
}

/*Money Resource::costFor(int quantity) const {
    requirePositive(quantity);
    return unitPrice_ * quantity;
}*/

/*For Q13*/
namespace {
std::string lowerCase(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return s;
}

// Lower case, without hyphens and spaces: "978-0-13-235088-4" -> "9780132350884".
std::string normalizedCode(const std::string& code) {
    std::string out;
    for (unsigned char ch : code)
        if (ch != '-' && !std::isspace(ch)) out += static_cast<char>(std::tolower(ch));
    return out;
}
}  // namespace

bool Resource::hasAuthor(const std::string&) const { return false; }

bool Resource::hasIdentifier(const std::string&) const { return false; }

bool Resource::anyAuthorMatches(const std::vector<std::string>& authors,
                                const std::string& name) {
    const std::string needle = lowerCase(name);
    return std::any_of(authors.begin(), authors.end(), [&needle](const std::string& author) {
        return lowerCase(author).find(needle) != std::string::npos;
    });
}

bool Resource::sameIdentifier(const std::string& stored, const std::string& code) {
    const std::string wanted = normalizedCode(code);
    return !wanted.empty() && normalizedCode(stored) == wanted;
}

Money Resource::applyPrintBulkDiscount(Money total, int quantity) {
    if (quantity < bulkDiscountMinCopies) return total;
    // Integer arithmetic on minor units, rounded half up (totals are >= 0).
   return total.withMinorUnits((total.minorUnits() * (100 - bulkDiscountPercent) + 50) / 100); //return Money::fromMinor((total.minorUnits() * (100 - bulkDiscountPercent) + 50) / 100);
}

/*Money Resource::costFor(int quantity) const {
    requirePositive(quantity);
    return applyPrintBulkDiscount(unitPrice_ * quantity, quantity);
}*/
Money Resource::costFor(int quantity) const {
    return costAt(unitPrice_, quantity);
}

Money Resource::costAt(Money price, int quantity) const {
    requirePositive(quantity);
    return applyPrintBulkDiscount(price * quantity, quantity);
}

void Resource::print(std::ostream& os) const {
    os << categoryName(category()) << " " << id_ << "\n"
       << "  title: " << title_ << "\n"
       << "  publisher: " << publisher_ << "\n"
       << "  year: " << year_ << "\n"
       << "  unit price: " << unitPrice_ << "\n";
    printDetails(os);
}

void Resource::printDetails(std::ostream&) const {}

std::string Resource::summary() const {
    std::ostringstream os;
    os << "[" << categoryName(category()) << "] " << id_ << "  " << title_
       << " (" << year_ << ")  @ " << unitPrice_;
    return os.str();
}

std::ostream& operator<<(std::ostream& os, const Resource& r) {
    r.print(os);
    return os;
}

}  // namespace bookmgmt
