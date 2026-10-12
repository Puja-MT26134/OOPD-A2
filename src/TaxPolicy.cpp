#include "bookmgmt/TaxPolicy.h"

#include <stdexcept>

namespace bookmgmt {

namespace {
void requireValidRate(int basisPoints) {
    if (basisPoints < 0) throw std::invalid_argument("tax rate must not be negative");
}
}  // namespace

TaxPolicy::TaxPolicy(int printRateBasisPoints, int electronicRateBasisPoints) {
    setPrintRate(printRateBasisPoints);
    setElectronicRate(electronicRateBasisPoints);
}

void TaxPolicy::setPrintRate(int basisPoints) {
    requireValidRate(basisPoints);
    printRate_ = basisPoints;
}

void TaxPolicy::setElectronicRate(int basisPoints) {
    requireValidRate(basisPoints);
    electronicRate_ = basisPoints;
}

int TaxPolicy::rateFor(const Resource& r) const {
    return r.isDigital() ? electronicRate_ : printRate_;
}

TaxedAmount TaxPolicy::apply(const Resource& r, Money preTax) const {
    // Integer arithmetic on minor units, rounded half up (amounts are >= 0).
    const std::int64_t tax =
        (preTax.minorUnits() * rateFor(r) + kBasisPointsPerHundredPercent / 2) /
        kBasisPointsPerHundredPercent;
    return TaxedAmount{preTax, preTax.withMinorUnits(tax)};
}

}  // namespace bookmgmt