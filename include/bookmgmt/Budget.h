#pragma once
// Budget: an overall spending limit plus optional per-category purchase quotas.
//
// A quota caps how many units (copies/seats) and how much money may be spent on
// one category. Categories with no quota are limited only by the total budget.

#include <iosfwd>
#include <map>
#include <optional>
#include <string>

#include "bookmgmt/Money.h"
#include "bookmgmt/Resource.h"

namespace bookmgmt {

struct Quota {
    int maxUnits;    // maximum copies/seats that may be bought
    Money maxSpend;  // maximum money that may be spent

    //Q7: maximum number of different titles that may be bought; nullopt = no limit
    std::optional<int> maxTitles = std::nullopt;
};

struct Usage {
    int units = 0;
    Money spent;

     // Q7: copies/seats bought so far for each title (resource id) in the category.
    std::map<std::string, int> titleUnits;

    int titles() const { return static_cast<int>(titleUnits.size()); }  // different titles bought
};

class Budget {
public:
    explicit Budget(Money total);

    Money total() const { return total_; }
    Money spent() const { return spent_; }
    Money remaining() const { return total_ - spent_; }

    void setQuota(ResourceCategory c, Quota q);
    void removeQuota(ResourceCategory c);
    std::optional<Quota> quotaFor(ResourceCategory c) const;
    Usage usageFor(ResourceCategory c) const;

    // Remaining allowance in a category; nullopt means "no quota set".
    std::optional<int> unitsRemaining(ResourceCategory c) const;
    std::optional<Money> spendRemaining(ResourceCategory c) const;

        // Q7: how many more different titles may be bought; nullopt = no title limit.
    std::optional<int> titlesRemaining(ResourceCategory c) const;

    // Returns an empty string if the purchase fits, otherwise the reason it
    // does not. Does not change state.
    std::string check(ResourceCategory c, int units, Money cost) const;

     // Q7: the same, but also enforces the title limit for the title `titleId`.
    // (The overload above does not know the title, so it cannot count titles.)
    std::string check(ResourceCategory c, const std::string& titleId, int units,
                      Money cost) const;

    // Records a purchase. Throws QuotaExceededError / BudgetExceededError
    // (and changes nothing) if it would not fit.
    void commit(ResourceCategory c, int units, Money cost);
        // Q7: the same, but also enforces and records the title.
    void commit(ResourceCategory c, const std::string& titleId, int units, Money cost);

        /* Q8: undoes a commit: returns the units and money to the category and to
    the overall budget. If the title has no units left in the category it no
    longer counts as a bought title. Throws std::invalid_argument (and changes
    nothing) if more would be refunded than was ever spent.*/
    void refund(ResourceCategory c, const std::string& titleId, int units, Money cost);

    // Q10: Calculate the percentage of unspent money to carry forward into the next financial year.
Money carryForward(int percent) const;

// Q10: Create next year's budget using the current year's total as the base allocation.
Budget rollover(int percent) const;

// Q10: Create next year's budget using a specified base allocation.
Budget rollover(int percent, Money baseAllocation) const;

    void print(std::ostream& os) const;

private:

    enum class Failure { None, BadInput, Currency, Quota, Overall };
    Failure evaluate(ResourceCategory c, const std::string& titleId, int units, Money cost, std::string& why) const;

    Money total_;
    Money spent_;
    std::map<ResourceCategory, Quota> quotas_;
    std::map<ResourceCategory, Usage> usage_;
};

}  // namespace bookmgmt
