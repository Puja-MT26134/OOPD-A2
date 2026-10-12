#pragma once

#include <string>

#include "bookmgmt/Resource.h"
#include "bookmgmt/Journal.h"


namespace bookmgmt
{

    class Journal : public Resource
    {
    public:
        Journal(std::string id, std::string title, std::string issn,
                std::string publisher, int year, Money unitPrice,
                int issuesPerYear, int subscriptionYears = 1);
      

        const std::string &issn() const { return issn_; }
        int issuesPerYear() const { return issuesPerYear_; }
        int subscriptionYears() const { return subscriptionYears_; }

        ResourceCategory category() const override { return ResourceCategory::Journal; }
            bool hasIdentifier(const std::string& code) const override { return sameIdentifier(issn_, code); }

          /*Money costFor(int quantity) const override; to*/    
           Money costAt(Money price, int quantity) const override;

    protected:
        void printDetails(std::ostream &os) const override;

    private:
        std::string issn_;
        int issuesPerYear_;
        int subscriptionYears_;
    };

} // namespace bookmgmt