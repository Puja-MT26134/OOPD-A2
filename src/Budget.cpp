#include "bookmgmt/Budget.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt
{

    namespace
    {
        // Every category, in the order Budget::print() lists them.
        const ResourceCategory kAllCategories[] = {ResourceCategory::Book,
                                                   ResourceCategory::ElectronicResource,
                                                   ResourceCategory::Journal,
                                                   ResourceCategory::EBook,
                                                   ResourceCategory::AudioBook,
                                                   ResourceCategory::Thesis};
    }

    Budget::Budget(Money total) : total_(total)
    {
        if (total_.isNegative())
            throw std::invalid_argument("budget must not be negative");
    }

    void Budget::setQuota(ResourceCategory c, Quota q)
    {
        if (q.maxUnits < 0 || q.maxSpend.isNegative() || (q.maxTitles && *q.maxTitles < 0))
            throw std::invalid_argument("quota limits must not be negative");
                    // Q15: a quota is a limit on spending, so it must be in the budget's currency
        if (!q.maxSpend.sameCurrencyAs(total_))
            throw CurrencyMismatchError("the quota's spend limit is in " + q.maxSpend.currency() +
                                        " but the budget is in " + total_.currency());
        quotas_[c] = q;
    }

    void Budget::removeQuota(ResourceCategory c) { quotas_.erase(c); }

    std::optional<Quota> Budget::quotaFor(ResourceCategory c) const
    {
        auto it = quotas_.find(c);
        if (it == quotas_.end())
            return std::nullopt;
        return it->second;
    }

    Usage Budget::usageFor(ResourceCategory c) const
    {
        auto it = usage_.find(c);
        return it == usage_.end() ? Usage{} : it->second;
    }

    std::optional<int> Budget::unitsRemaining(ResourceCategory c) const
    {
        auto q = quotaFor(c);
        if (!q)
            return std::nullopt;
        return q->maxUnits - usageFor(c).units;
    }

    std::optional<Money> Budget::spendRemaining(ResourceCategory c) const
    {
        auto q = quotaFor(c);
        if (!q)
            return std::nullopt;
        return q->maxSpend - usageFor(c).spent;
    }

    std::optional<int> Budget::titlesRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q || !q->maxTitles) return std::nullopt;
    return *q->maxTitles - usageFor(c).titles();
}

    Budget::Failure Budget::evaluate(ResourceCategory c, const std::string& titleId, int units, Money cost,
                                     std::string& why) const
    {
        if (units <= 0)
        {
            why = "quantity must be positive";
            return Failure::BadInput;
        }
        if (cost.isNegative())
        {
            why = "cost must not be negative";
            return Failure::BadInput;
        }
                // Q15: the cost must be in the budget's currency (there is no conversion)
        if (!cost.sameCurrencyAs(total_))
        {
            why = "currency mismatch: the cost is in " + cost.currency() +
                  " but the budget is in " + total_.currency();
            return Failure::Currency;
        }
        if (auto left = unitsRemaining(c); left && units > *left)
        {
            why = std::string(categoryName(c)) + " unit quota exceeded: requested " +
                  std::to_string(units) + ", " + std::to_string(*left) + " remaining";
            return Failure::Quota;
        }
        if (auto left = spendRemaining(c); left && cost > *left)
        {
            why = std::string(categoryName(c)) + " spend quota exceeded: cost " +
                  cost.toString() + ", " + left->toString() + " remaining";
            return Failure::Quota;
        }
           // Q7: only a title not bought before in this category counts as a new title.
    if (!titleId.empty() && usageFor(c).titleUnits.count(titleId) == 0) {
        if (auto left = titlesRemaining(c); left && *left < 1) {
            why = std::string(categoryName(c)) + " title quota exceeded: " + titleId +
                  " would be a new title, limit is " + std::to_string(*quotaFor(c)->maxTitles);
            return Failure::Quota;
        }
    }
        if (cost > remaining())
        {
            why = "overall budget exceeded: cost " + cost.toString() + ", " +
                  remaining().toString() + " remaining";
            return Failure::Overall;
        }
        why.clear();
        return Failure::None;
    }

    std::string Budget::check(ResourceCategory c, int units, Money cost) const {
    return check(c, std::string(), units, cost);
}

    std::string Budget::check(ResourceCategory c, const std::string& titleId, int units, Money cost) const
    {
        std::string why;
        evaluate(c, titleId, units, cost, why);
        return why;
    }

    void Budget::commit(ResourceCategory c, int units, Money cost) {
    commit(c, std::string(), units, cost);
}
    void Budget::commit(ResourceCategory c, const std::string& titleId, int units, Money cost)
    {
        std::string why;
        switch (evaluate(c, titleId, units, cost, why))
        {
        case Failure::None:
            break;
        case Failure::BadInput:
            throw std::invalid_argument(why);
        case Failure::Quota:
            throw QuotaExceededError(why);
        case Failure::Overall:
            throw BudgetExceededError(why);
                    case Failure::Currency:
            throw CurrencyMismatchError(why);
        }
        Usage &u = usage_[c];
        u.units += units;
        u.spent += cost;
        if (!titleId.empty()) u.titleUnits[titleId] += units;
        spent_ += cost;
    }

    void Budget::refund(ResourceCategory c, const std::string& titleId, int units, Money cost) {
    if (units <= 0) throw std::invalid_argument("quantity must be positive");
    if (cost.isNegative()) throw std::invalid_argument("cost must not be negative");
    auto it = usage_.find(c);
    if (it == usage_.end() || units > it->second.units || cost > it->second.spent ||
        cost > spent_)
        throw std::invalid_argument(std::string("cannot refund more than was spent in ") +
                                    categoryName(c));
    Usage& u = it->second;
    auto title = u.titleUnits.end();
    if (!titleId.empty()) {
        title = u.titleUnits.find(titleId);
        if (title != u.titleUnits.end() && units > title->second)
            throw std::invalid_argument("cannot refund more units than were bought of " +
                                        titleId);
    }
    // All checks passed: now change state.
    if (title != u.titleUnits.end()) {
        title->second -= units;
        if (title->second == 0) u.titleUnits.erase(title);  // the title slot is free again
    }
    u.units -= units;
    u.spent -= cost;
    spent_ -= cost;
}

// Q10: Calculate the amount carried forward.
Money Budget::carryForward(int percent) const {

    if (percent < 0 || percent > 100) {
        throw std::invalid_argument(
            "Carry forward percentage must be between 0 and 100"
        );
    }

        const Money unspent = remaining();
    return unspent.withMinorUnits(
        unspent.minorUnits() * percent / 100);//return Money::fromMinor(remaining().minorUnits() * percent / 100);
}


// Q10: Rollover using this year's total budget.
Budget Budget::rollover(int percent) const {

    return rollover(percent, total_);
}


// Q10: Rollover with a configurable base allocation.
Budget Budget::rollover(int percent, Money baseAllocation) const {

    if (baseAllocation.isNegative()) {
        throw std::invalid_argument(
            "Base allocation cannot be negative"
        );
    }

    Money carried = carryForward(percent);

    Budget nextYear(baseAllocation + carried);

    // Copy category quotas.
    nextYear.quotas_ = quotas_;

    // Do not copy usage. Next year's spending and title counts start at zero.

    return nextYear;
}

    void Budget::print(std::ostream& os) const {
    os << "Budget: total " << total_ << ", spent " << spent_ << ", remaining "
       << remaining() << "\n";
    os << std::left << std::setw(22) << "  Category" << std::setw(18) << "Units used/max"
       << std::setw(22) << "Spend used/max" << "Titles used/max\n";
    for (ResourceCategory c : kAllCategories) {
        const Usage u = usageFor(c);
        const auto q = quotaFor(c);
        const std::string units =
            std::to_string(u.units) + "/" + (q ? std::to_string(q->maxUnits) : "-");
        const std::string spend =
            u.spent.toString() + "/" + (q ? q->maxSpend.toString() : "-");
        const std::string titles =
            std::to_string(u.titles()) + "/" +
            (q && q->maxTitles ? std::to_string(*q->maxTitles) : "-");
        os << "  " << std::setw(20) << categoryName(c) << std::setw(18) << units
           << std::setw(22) << spend << titles << "\n";
    }
}

}  // namespace bookmgmt