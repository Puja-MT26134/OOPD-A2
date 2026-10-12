#pragma once
// TaxPolicy (Q6): configurable tax rates for print and electronic resources.
//
// Rates are stored in basis points (1 basis point = 0.01%), so 1800 means 18%
// and 550 means 5.5%. Integers keep all money arithmetic exact.

#include "bookmgmt/Money.h"
#include "bookmgmt/Resource.h"

namespace bookmgmt {

constexpr int kBasisPointsPerHundredPercent = 10000;

// A pre-tax amount together with the tax charged on it.
struct TaxedAmount {
    Money preTax;
    Money tax;
    Money total() const { return preTax + tax; }  // post-tax
};

class TaxPolicy {
public:
    // Default: no tax on anything, so behaviour is unchanged until a policy is set.
    TaxPolicy() = default;
    TaxPolicy(int printRateBasisPoints, int electronicRateBasisPoints);

    int printRate() const { return printRate_; }
    int electronicRate() const { return electronicRate_; }
    void setPrintRate(int basisPoints);
    void setElectronicRate(int basisPoints);

    // Rate that applies to a resource: electronic rate if r.isDigital(),
    // otherwise the print rate.
    int rateFor(const Resource& r) const;

    // Tax on a pre-tax amount for this resource (rounded half up to the
    // nearest paisa/cent).
    TaxedAmount apply(const Resource& r, Money preTax) const;

private:
    int printRate_ = 0;
    int electronicRate_ = 0;
};

}  // namespace bookmgmt