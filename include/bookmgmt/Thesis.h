#pragma once

#include <string>

#include "bookmgmt/Resource.h"

namespace bookmgmt
{
    // Q3 justification: Thesis derives directly from Resource.
    // A thesis is not licensed per seat and has no access URL or platform fee,
    // so ElectronicResource does not fit; it is not a commercially printed book
    // with ISBN, edition or binding either, so Book does not fit. It is usually
    // free: unitPrice defaults to zero, and the inherited costFor()
    // (unitPrice x quantity) then gives 0.00, so no override is needed.
    // The university is stored as the Resource's publisher (a thesis is
    // "published" by its university), so university() just returns publisher().

    class Thesis : public Resource
    {
    public:
        Thesis(std::string id, std::string title, std::string university,
               std::string degree, std::string supervisor, int year,
               Money unitPrice = Money{});

        const std::string &university() const { return publisher(); }
        const std::string &degree() const { return degree_; }
        const std::string &supervisor() const { return supervisor_; }

        ResourceCategory category() const override { return ResourceCategory::Thesis; }

    protected:
        void printDetails(std::ostream &os) const override;

    private:
        std::string degree_;
        std::string supervisor_;
    };

} // namespace bookmgmt