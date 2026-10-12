#pragma once

#include <string>
#include <vector>

#include "bookmgmt/ElectronicResource.h"


namespace bookmgmt
{

    enum class EBookFormat
    {
        PDF,
        EPUB,
        HTML
    };

    const char *formatName(EBookFormat f);

    // Q2 answer (move this into your own words, 4-6 sentences):
    // An e-book is also a kind of book. Duplicated between Book and EBook:
    //   - the authors_ and isbn_ members and their getters
    //   - the code that prints them (joinAuthors, "authors:" / "isbn:" lines)
    // How to avoid it: [your answer here]

    // Q2 answer: an e-book is also a kind of book, so some code is now duplicated
// between Book and EBook:
//   - the data members authors_ and isbn_ and their getters authors() / isbn()
//   - the constructor code that stores them
//   - the printing code that writes the "authors:" and "isbn:" lines
//     (EBook even includes Book.h just to reuse joinAuthors)
//
// How to avoid it: move the shared bibliographic data into a small helper
// class, for example BookInfo { authors, isbn, print() }, and give both Book
// and EBook a BookInfo member (composition). Each class then forwards its
// getters and printing to the helper, so the logic lives in one place.
// Inheritance does not work well here: EBook already derives from
// ElectronicResource and Book derives from Resource, so making EBook inherit
// from Book as well would mean multiple inheritance, with Resource appearing
// twice (the diamond problem) unless virtual inheritance is used. If the
// helper is a plain class that does not derive from Resource, a class could
// also inherit from it without a diamond, but composition is simpler and
// keeps the class hierarchy easy to follow.

    class EBook : public ElectronicResource
    {
    public:
        EBook(std::string id, std::string title, std::vector<std::string> authors,
              std::string isbn, std::string publisher, int year, Money pricePerSeat,
              std::string accessUrl, EBookFormat format = EBookFormat::PDF,
              bool drmProtected = false,
              LicenseModel license = LicenseModel::AnnualSubscription,
              Money platformFee = Money{});

        
        const std::vector<std::string> &authors() const { return authors_; }
        const std::string isbn() const { return isbn_; }
        EBookFormat format() const { return format_; }
        bool drmProtected() const { return drmProtected_; }

        ResourceCategory category() const override { return ResourceCategory::EBook; }
            bool hasAuthor(const std::string& name) const override { return anyAuthorMatches(authors_, name); }
    bool hasIdentifier(const std::string& code) const override { return sameIdentifier(isbn_, code); }

        // costFor() and isDigital() are inherited unchanged from ElectronicResource.
        

    protected:
        // printDetails() override
        void printDetails(std::ostream &os) const override;

    private:
        // authors_, isbn_, format_, drmProtected_
        std::vector<std::string> authors_;
        std::string isbn_;
        EBookFormat format_;
        bool drmProtected_;
    };

} // namespace bookmgmt