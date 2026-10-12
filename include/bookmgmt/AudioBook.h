#pragma once

#include <string>
#include <vector>

#include "bookmgmt/ElectronicResource.h"

namespace bookmgmt
{
    /*Justification: Base Class- ElectronicResource
    AudioBook is a digital product that people stream or download under a licence.
    Per-seat pricing (platform fee + price × seats) fits, and it needs an access URL.*/

    class AudioBook : public ElectronicResource
    {
    public:
        AudioBook(std::string id, std::string title, std::vector<std::string> authors,
                  std::string narrator, std::string publisher, int year,
                  Money pricePerSeat, std::string accessUrl, int durationMinutes,
                  LicenseModel license = LicenseModel::AnnualSubscription,
                  Money platformFee = Money{});

        const std::vector<std::string> &authors() const { return authors_; }
        const std::string &narrator() const { return narrator_; }
        int durationMinutes() const { return durationMinutes_; }

        ResourceCategory category() const override { return ResourceCategory::AudioBook; }
            bool hasAuthor(const std::string& name) const override { return anyAuthorMatches(authors_, name); }

        // costFor() and isDigital() are inherited from ElectronicResource.
        
    protected:
        // printDetails() override
        void printDetails(std::ostream &os) const override;

    private:
        
        std::vector<std::string> authors_;
        std::string narrator_;
        int durationMinutes_;
    };
} // namespace bookmgmt