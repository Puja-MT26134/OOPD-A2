#include "bookmgmt/ElectronicResource.h"

#include <ostream>
#include <stdexcept>
#include <algorithm>

namespace bookmgmt {

const char* licenseName(LicenseModel m) {
    switch (m) {
        case LicenseModel::Perpetual: return "Perpetual";
        case LicenseModel::AnnualSubscription: return "Annual subscription";
    }
    return "Unknown";
}

ElectronicResource::ElectronicResource(std::string id, std::string title,
                                       std::string publisher, int year,
                                       Money pricePerSeat, std::string accessUrl,
                                       LicenseModel license, Money platformFee)
    : Resource(std::move(id), std::move(title), std::move(publisher), year, pricePerSeat),
      accessUrl_(std::move(accessUrl)),
      license_(license),
      platformFee_(platformFee) {
    if (platformFee_.isNegative())
        throw std::invalid_argument("platform fee must not be negative");
}

/*Money ElectronicResource::costFor(int seats) const {
    requirePositive(seats);
    return platformFee_ + unitPrice() * seats;
}*/

/*Money ElectronicResource::costFor(int seats) const {*/
 Money ElectronicResource::costAt(Money price, int seats) const{
    requirePositive(seats);
    const int fullPriceSeats = std::min(seats, kFullPriceSeats);
    const int extraSeats = seats - fullPriceSeats;
    Money total = platformFee_ + price * fullPriceSeats;
    if (extraSeats > 0) {
        // Seats beyond the 50th are charged together at 50% of the unit price,
        // rounded half up to the nearest paisa/cent. The platform fee is not discounted.
        total += price.withMinorUnits(
            (price.minorUnits() * extraSeats * kExtraSeatPercent + 50) / 100);
    }
    return total;
}

void ElectronicResource::printDetails(std::ostream& os) const {
    os << "  access url: " << accessUrl_ << "\n"
       << "  license: " << licenseName(license_) << "\n"
       << "  platform fee: " << platformFee_ << "\n";
}

}  // namespace bookmgmt
