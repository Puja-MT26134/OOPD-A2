#include "bookmgmt/AudioBook.h"

#include <ostream>
#include <stdexcept>
#include <utility>

#include "bookmgmt/Book.h"

namespace bookmgmt
{

    AudioBook::AudioBook(std::string id, std::string title, std::vector<std::string> authors,
                         std::string narrator, std::string publisher, int year,
                         Money pricePerSeat, std::string accessUrl, int durationMinutes,
                         LicenseModel license, Money platformFee)
        : ElectronicResource(std::move(id), std::move(title), std::move(publisher), year,
                             pricePerSeat, std::move(accessUrl), license, platformFee),
          authors_(std::move(authors)),
          narrator_(std::move(narrator)),
          durationMinutes_(durationMinutes)
    {

        if (durationMinutes_ < 1)
            throw std::invalid_argument("durationMinutes must be >=1");
    }

    void AudioBook::printDetails(std::ostream &os) const
    {

        ElectronicResource::printDetails(os);
        os << "  authors: " << joinAuthors(authors_) << "\n"
           << "  narrator: " << narrator_ << "\n"
           << "  duration (minutes): " << durationMinutes_ << "\n";
    }

}