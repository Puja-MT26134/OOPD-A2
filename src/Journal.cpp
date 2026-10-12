#include "bookmgmt/Journal.h"

#include <ostream>
#include <stdexcept>
#include<utility>

namespace bookmgmt
{

    

    Journal::Journal(std::string id, std::string title, std::string issn,
                     std::string publisher, int year, Money unitPrice,
                     int issuesPerYear, int subscriptionYears)
        : Resource(std::move(id), std::move(title), std::move(publisher), year, unitPrice),
          issn_(std::move(issn)),
          issuesPerYear_(issuesPerYear),
          subscriptionYears_(subscriptionYears)
          
    {
        if (subscriptionYears_ < 1)
            throw std::invalid_argument("subscriptionYears must be >= 1");
    }

    /*Money Journal::costFor(int quantity) const{*/Money Journal::costAt(Money price, int quantity) const{
        requirePositive(quantity);
        //return unitPrice() * quantity * subscriptionYears_;
            return applyPrintBulkDiscount(price* quantity * subscriptionYears_, quantity);
    }

    void Journal::printDetails(std::ostream &os) const
    {
           os << "  issn: " << issn_ << "\n"
           << "  issues per year: " << issuesPerYear_ << "\n"
           << "  subscription years: " << subscriptionYears_ << "\n";
    }

} // namespace bookmgmt
