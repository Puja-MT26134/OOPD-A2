#include "bookmgmt/Thesis.h"

#include <ostream>
#include <utility>

namespace bookmgmt
{

    Thesis::Thesis(std::string id, std::string title, std::string university,
                   std::string degree, std::string supervisor, int year,
                   Money unitPrice)
        : Resource(std::move(id), std::move(title), std::move(university), year, unitPrice),
          degree_(std::move(degree)),
          supervisor_(std::move(supervisor))
    {
    }

    void Thesis::printDetails(std::ostream &os) const
    {
        os << "  university: " << university() << "\n"
           << "  degree: " << degree_ << "\n"
           << "  supervisor: " << supervisor_ << "\n";
    }

} // namespace bookmgmt