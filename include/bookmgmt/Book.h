#pragma once

#include <string>
#include <vector>

#include "bookmgmt/Resource.h"

namespace bookmgmt {

enum class Binding { Paperback, Hardcover };

// Q4: hardcover books cost this many percent more than their listed unit price.
constexpr int kHardcoverSurchargePercent = 20;

class Book : public Resource {
public:
    Book(std::string id, std::string title, std::vector<std::string> authors,
         std::string isbn, std::string publisher, int year, Money unitPrice,
         int edition = 1, Binding binding = Binding::Paperback);

    const std::vector<std::string>& authors() const { return authors_; }
    const std::string& isbn() const { return isbn_; }
    int edition() const { return edition_; }
    Binding binding() const { return binding_; }

        /*Paperback: unitPrice * copies. Hardcover: the unit price is first raised
     by kHardcoverSurchargePercent (rounded half up to the nearest paisa/cent),
     then multiplied by the number of copies.*/
    //Money costFor(int quantity) const override; 
       Money costAt(Money price, int quantity) const override;
    ResourceCategory category() const override { return ResourceCategory::Book; }
        bool hasAuthor(const std::string& name) const override { return anyAuthorMatches(authors_, name); }
    bool hasIdentifier(const std::string& code) const override { return sameIdentifier(isbn_, code); }

protected:
    void printDetails(std::ostream& os) const override;

private:
    std::vector<std::string> authors_;
    std::string isbn_;
    int edition_;
    Binding binding_;
};

// Joins a list of author names: {"A", "B", "C"} -> "A, B and C"
std::string joinAuthors(const std::vector<std::string>& authors);

}  // namespace bookmgmt
