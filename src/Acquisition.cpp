#include "bookmgmt/Acquisition.h"

#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <map>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget)
    : catalog_(catalog), budget_(budget) {

    }
void AcquisitionManager::setVendorPrice(
    const std::string& resourceId,
    const std::string& vendor,
    Money unitPrice) {

    if (vendor.empty()) {
        throw std::invalid_argument("Vendor name cannot be empty");
    }

    if (unitPrice.isNegative()) {
        throw std::invalid_argument("Vendor price cannot be negative");
    }

    const Resource& resource = catalog_.get(resourceId);  // Check resource exists

    // Q15: a vendor's price must be in the same currency as the listed price
    if (!unitPrice.sameCurrencyAs(resource.unitPrice())) {
        throw CurrencyMismatchError("vendor price for " + resourceId + " is in " +
                                    unitPrice.currency() + " but the listed price is in " +
                                    resource.unitPrice().currency());
    }

    vendorPrices_[resourceId][vendor] = unitPrice;
}

std::string AcquisitionManager::cheapestVendor(
    const std::string& resourceId) const {

    catalog_.get(resourceId);

    auto it = vendorPrices_.find(resourceId);

    if (it == vendorPrices_.end() || it->second.empty())
        return "";

    auto best = it->second.begin();

    for (auto current = it->second.begin();
         current != it->second.end(); ++current) {

        if (current->second < best->second)
            best = current;
    }

    return best->first;
}

Money AcquisitionManager::vendorCost(const Resource& resource, int quantity) const {
    const std::string vendor = cheapestVendor(resource.id());
    if (vendor.empty()) return resource.costFor(quantity);   // no vendors: the listed price

    // Price the purchase with the vendor's unit price, using the resource's own rules
    // (hardcover surcharge, bulk discount, seat tiers, subscription years, ...).
    return resource.costAt(vendorPrices_.at(resource.id()).at(vendor), quantity);
}

void AcquisitionManager::addDepartment(const std::string& name, Budget& budget) {
    if (name.empty()) throw std::invalid_argument("department name must not be empty");
    if (!departments_.emplace(name, &budget).second)
        throw std::invalid_argument("department already registered: " + name);
}

bool AcquisitionManager::hasDepartment(const std::string& name) const {
    return departments_.count(name) != 0;
}

std::vector<std::string> AcquisitionManager::departmentNames() const {
    std::vector<std::string> names;
    for (const auto& entry : departments_) names.push_back(entry.first);
    return names;  // std::map keeps them sorted
}

Budget& AcquisitionManager::departmentBudget(const std::string& name) const {
    auto it = departments_.find(name);
    if (it == departments_.end()) throw UnknownDepartmentError(name);
    return *it->second;
}

Budget* AcquisitionManager::budgetFor(const std::string& department) const {
    if (department.empty()) return &budget_;
    auto it = departments_.find(department);
    return it == departments_.end() ? nullptr : it->second;
}

Money AcquisitionManager::quotePreTax(const std::string& id, int quantity) const {
   // return catalog_.get(id).costFor(quantity);
    const Resource& r = catalog_.get(id);
    return vendorCost(r, quantity);
}

Money AcquisitionManager::quote(const std::string& id, int quantity) const {
    const Resource& r = catalog_.get(id);
    //return tax_.apply(r, r.costFor(quantity)).total();
    return tax_.apply(r, vendorCost(r, quantity)).total();
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason) const {
    return canPurchase(id, quantity, std::string(), reason);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     const std::string& department,
                                     std::string* reason) const {
    std::string why;
    if (const Resource* r = catalog_.find(id)) {
        if (const Budget* budget = budgetFor(department)) {
            if (quantity <= 0)
                why = "quantity must be positive";
            else
                /*why = budget->check(r->category(), id, quantity,
                                    tax_.apply(*r, r->costFor(quantity)).total());*/
                                    
    why = budget->check(r->category(), id, quantity,
                        tax_.apply(*r, vendorCost(*r, quantity)).total());

        } else {
            why = "unknown department: " + department;
        }
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

PurchaseRecord& AcquisitionManager::record(const Resource* r, const std::string& id,
                                           int qty, TaxedAmount amount, bool approved,
                                           std::string reason,
                                           const std::string& department) {
    history_.push_back(PurchaseRecord{
        nextOrderNo_++, id, r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book, qty, amount.total(), approved,
        std::move(reason), amount.preTax, amount.tax, false, 0, department, ""});
    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity) {
    return purchase(id, quantity, std::string());
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity,
                                                   const std::string& department) {
    const Resource& r = catalog_.get(id);        // may throw NotFoundError
    Budget* budget = budgetFor(department);
    if (!budget) throw UnknownDepartmentError(department);
    // may throw invalid_argument; quotas and the budget are checked on the post-tax cost
    //const TaxedAmount amount = tax_.apply(r, r.costFor(quantity));
     // Q12: Calculate cost using cheapest vendor.
    const TaxedAmount amount =
        tax_.apply(r, vendorCost(r, quantity));

    budget->commit(r.category(), id, quantity, amount.total());  // may throw quota/budget errors
    catalog_.addHoldings(id, quantity);
    //return record(&r, id, quantity, amount, true, {}, department);
     // Create the purchase record.
    PurchaseRecord& rec =
        record(&r, id, quantity, amount, true, {}, department);
         //Q12: Save the vendor selected for this purchase.
    rec.vendor = cheapestVendor(id);

    return rec;
}

/*std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs) {
    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());
    for (const auto& req : reqs) {
        const Resource* r = catalog_.find(req.resourceId);
        TaxedAmount amount;
        std::string why;
        if (!r) {
            why = "resource not found: " + req.resourceId;
        } else if (const Budget* budget = budgetFor(req.department); !budget) {
            why = "unknown department: " + req.department;
        } else if (req.quantity <= 0) {
            why = "quantity must be positive";
        } else {
            amount = tax_.apply(*r, r->costFor(req.quantity));
            why = budget->check(r->category(), req.resourceId, req.quantity, amount.total());
        }

        if (why.empty()) {
            results.push_back(purchase(req.resourceId, req.quantity, req.department));
        } else {
            results.push_back(
                record(r, req.resourceId, req.quantity, amount, false, why, req.department));
        }
    }
    return results;
}*/

// Q11: Batch processing with optional all-or-nothing behavior.
std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs, bool atomic) {

    // Existing behavior: process each request independently.
    if (!atomic) {
        std::vector<PurchaseRecord> results;
        results.reserve(reqs.size());

        for (const auto& req : reqs) {
            const Resource* r = catalog_.find(req.resourceId);
            TaxedAmount amount;
            std::string why;

            if (!r) {
                why = "resource not found: " + req.resourceId;
            } else if (const Budget* budget = budgetFor(req.department); !budget) {
                why = "unknown department: " + req.department;
            } else if (req.quantity <= 0) {
                why = "quantity must be positive";
            } else {
                //amount = tax_.apply(*r, r->costFor(req.quantity)); changing it with the vendor cost
                amount = tax_.apply(*r, vendorCost(*r, req.quantity));

                why = budget->check(
                    r->category(),
                    req.resourceId,
                    req.quantity,
                    amount.total()
                );
            }

            if (why.empty()) {
                results.push_back(
                    purchase(req.resourceId, req.quantity, req.department)
                );
            } else {
                results.push_back(
                    record(r, req.resourceId, req.quantity,
                           amount, false, why, req.department)
                );
            }
        }

        return results;
    }

    // Q11: Atomic processing begins here.

    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());

    if (reqs.empty()) {
        return results;
    }

    // Temporary copies of the budgets for validation: the real budgets are not modified
    // while checking. There is one copy per Budget object, so two department names that
    // share a budget also share its temporary copy.
    std::map<const Budget*, Budget> temporaryBudgets;
    auto temporaryFor = [&](const std::string& department) -> Budget* {
        const Budget* real = budgetFor(department);
        if (!real) return nullptr;
        return &temporaryBudgets.emplace(real, *real).first->second;  // keeps an existing copy
    };

    std::vector<std::string> reasons(reqs.size());
    std::vector<TaxedAmount> amounts(reqs.size());

    bool batchFailed = false;

    // Phase 1: Validate every request using temporary budgets.
    for (std::size_t i = 0; i < reqs.size(); ++i) {

        const auto& req = reqs[i];

        const Resource* r = catalog_.find(req.resourceId);

        if (!r) {
            reasons[i] = "resource not found: " + req.resourceId;
        }
              else if (temporaryFor(req.department) == nullptr) {

            reasons[i] = "unknown department: " + req.department;
        }
        else if (req.quantity <= 0) {
            reasons[i] = "quantity must be positive";
        }
        else {
            //amounts[i] = tax_.apply(*r, r->costFor(req.quantity)); change it with vendor

amounts[i] = tax_.apply(*r, vendorCost(*r, req.quantity));
            Budget& tempBudget = *temporaryFor(req.department);

            reasons[i] = tempBudget.check(
                r->category(),
                req.resourceId,
                req.quantity,
                amounts[i].total()
            );

            if (reasons[i].empty()) {
                // Reserve budget in the temporary copy.
                // Later requests see earlier simulated purchases.
                tempBudget.commit(
                    r->category(),
                    req.resourceId,
                    req.quantity,
                    amounts[i].total()
                );
            }
        }

        if (!reasons[i].empty()) {
            batchFailed = true;
        }
    }

    // Phase 2: If any request failed, reject the entire batch.
    if (batchFailed) {

        for (std::size_t i = 0; i < reqs.size(); ++i) {

            const auto& req = reqs[i];
            const Resource* r = catalog_.find(req.resourceId);

            std::string reason = reasons[i];

            if (reason.empty()) {
                reason = "atomic batch rejected: another request failed";
            }

            results.push_back(
                record(
                    r,
                    req.resourceId,
                    req.quantity,
                    amounts[i],
                    false,
                    reason,
                    req.department
                )
            );
        }

        return results;
    }

    // Phase 3: Every request passed validation.
    // Commit all purchases to the real budgets and catalogue.
    for (const auto& req : reqs) {

        results.push_back(
            purchase(req.resourceId, req.quantity, req.department)
        );
    }

    return results;
}

const PurchaseRecord* AcquisitionManager::findOrder(int orderNo) const {
    for (const auto& rec : history_)
        if (rec.orderNo == orderNo) return &rec;
    return nullptr;
}

bool AcquisitionManager::isCancelled(int orderNo) const {
    for (const auto& rec : history_)
        if (rec.isCancellation && rec.cancelsOrderNo == orderNo) return true;
    return false;
}

const PurchaseRecord& AcquisitionManager::cancel(int orderNo) {
    const PurchaseRecord* found = findOrder(orderNo);
    const std::string label = "order #" + std::to_string(orderNo);
    if (!found) throw CancellationError("no such order: " + label);
    if (found->isCancellation)
        throw CancellationError(label + " is a cancellation record and cannot be cancelled");
    if (!found->approved)
        throw CancellationError(label + " was rejected, so there is nothing to cancel");
    if (isCancelled(orderNo)) throw CancellationError(label + " is already cancelled");

    // Copy what we need: push_back below may invalidate `found`.
    const PurchaseRecord original = *found;

        // Units that are on loan cannot be taken out of the holdings.
    if (catalog_.available(original.resourceId) < original.quantity)
        throw CancellationError(label + " cannot be cancelled: its units are no longer held, or are on loan");

    Budget* budget = budgetFor(original.department);
    if (!budget)
        throw CancellationError(label + " cannot be cancelled: unknown department " +
                                original.department);

    // Every check has passed, so the two state changes below cannot fail.
    budget->refund(original.category, original.resourceId, original.quantity, original.cost);
    catalog_.addHoldings(original.resourceId, -original.quantity);

    history_.push_back(PurchaseRecord{
        nextOrderNo_++, original.resourceId, original.title, original.category,
        original.quantity, Money{} - original.cost, true, std::string(),
        Money{} - original.preTaxCost, Money{} - original.taxAmount, true, orderNo,
        original.department, original.vendor});
    return history_.back();
}

Money AcquisitionManager::totalSpent(const std::string& department) const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved && rec.department == department) sum += rec.cost;
    return sum;
}

Money AcquisitionManager::totalSpent() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) sum += rec.cost;
    return sum;
}

Money AcquisitionManager::totalPreTax() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) sum += rec.preTaxCost;
    return sum;
}

Money AcquisitionManager::totalTax() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) sum += rec.taxAmount;
    return sum;
}


void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";
    for (const auto& rec : history_) {
        const char* status = rec.isCancellation ? "CANCELLED"
                             : rec.approved     ? "APPROVED"
                                                : "REJECTED";
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " " << std::setw(10)
           << status << std::setw(6) << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << std::setw(12) << std::right << rec.cost.toString() << std::left << "  "
           << rec.title;
        if (!rec.department.empty()) os << "  (dept: " << rec.department << ")";
        // Q12: Display vendor used for the purchase.
if (!rec.vendor.empty())
    os << "  (vendor: " << rec.vendor << ")";
        if (rec.approved && rec.taxAmount != Money{})
            os << "  (pre-tax " << rec.preTaxCost << " + tax " << rec.taxAmount << ")";
        if (rec.isCancellation) os << "  (cancels order #" << rec.cancelsOrderNo << ")";
        if (!rec.isCancellation && rec.approved && isCancelled(rec.orderNo))
            os << "  [cancelled]";
        if (!rec.approved) os << "\n        reason: " << rec.reason;
        os << "\n";
    }
    os << "Total pre-tax: " << totalPreTax() << "\n";
    os << "Total tax: " << totalTax() << "\n";
    os << "Total spent: " << totalSpent() << "\n";
}

void AcquisitionManager::printDepartmentReport(std::ostream& os) const {
    for (const auto& entry : departments_) {
        os << "Department " << entry.first << " (net charged: " << totalSpent(entry.first)
           << ")\n";
        entry.second->print(os);
    }
}

}  // namespace bookmgmt