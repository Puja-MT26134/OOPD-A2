#pragma once
// AcquisitionManager: turns purchase requests into orders, enforcing the
// Budget's quotas, updating Catalog holdings and keeping an order history.

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "bookmgmt/Budget.h"
#include "bookmgmt/Catalog.h"
#include "bookmgmt/TaxPolicy.h"

namespace bookmgmt {

struct PurchaseRequest {
    std::string resourceId;
    int quantity;  // copies for print, seats for electronic
    // Q9: the department charged for the purchase; empty = the default budget.
    std::string department = std::string();
};

struct PurchaseRecord {
    int orderNo;
    std::string resourceId;
    std::string title;
    ResourceCategory category;
    int quantity;
    Money cost;  // total charged, i.e. post-tax (equals preTaxCost when there is no tax)
    bool approved;
    std::string reason;  // why it was rejected; empty if approved
    Money preTaxCost;    // Q6: price before tax
    Money taxAmount;     // Q6: tax charged; cost == preTaxCost + taxAmount
    // Q8: a cancellation is a separate record. Its quantity is the units returned,
    // its cost, preTaxCost and taxAmount are the refunded amounts (negative), and
    // cancelsOrderNo is the order it cancels. The original record is never changed.
    bool isCancellation = false;
    int cancelsOrderNo = 0;
    // Q9: the department that was charged (empty = the default budget). A
    // cancellation record carries the department of the order it cancels.
    std::string department;
    // Q12: Vendor selected for this order.
std::string vendor;
};

class AcquisitionManager {
public:
    AcquisitionManager(Catalog& catalog, Budget& budget);

    // Q9: registers a department with its own budget and quotas. The budget is
    // owned by the caller and must outlive this manager. Throws
    // std::invalid_argument if the name is empty or already registered.
    // Purchases that do not name a department are charged to the default budget
    // given to the constructor, as before.
    void addDepartment(const std::string& name, Budget& budget);
    bool hasDepartment(const std::string& name) const;
    std::vector<std::string> departmentNames() const;  // sorted
    // Throws UnknownDepartmentError.
    Budget& departmentBudget(const std::string& name) const;

    // Q6: tax rates used for every quote and purchase (default: no tax).
    void setTaxPolicy(TaxPolicy policy) { tax_ = policy; }
    const TaxPolicy& taxPolicy() const { return tax_; }

    // Price of a request without buying anything, including tax: this is the
    // amount that would be charged and checked against the quotas.
    // Throws NotFoundError.
    Money quote(const std::string& id, int quantity) const;
    // The same request before tax.
    Money quotePreTax(const std::string& id, int quantity) const;

    // True if the purchase would be approved; if not, `reason` explains why.
    bool canPurchase(const std::string& id, int quantity,
                     std::string* reason = nullptr) const;
    // Q9: the same, charged to a department.
    bool canPurchase(const std::string& id, int quantity, const std::string& department,
                     std::string* reason = nullptr) const;

    // Buys immediately. Throws NotFoundError, QuotaExceededError,
    // BudgetExceededError or std::invalid_argument. On success the budget
    // and holdings are updated and the record is added to history.
    // Note: the returned reference points into the history, so it is invalidated
    // by later purchases; copy the record if you need it afterwards.
    const PurchaseRecord& purchase(const std::string& id, int quantity);
    // Q9: the same, charged to a department (also throws UnknownDepartmentError).
    const PurchaseRecord& purchase(const std::string& id, int quantity,
                                   const std::string& department);

    /* Processes requests in order; each is approved or rejected on its own
     (never throws for a rejected request). Every outcome is recorded.
     EXTENSION POINT: priority ordering, all-or-nothing batches, ...
    std::vector<PurchaseRecord> processBatch(const std::vector<PurchaseRequest>& reqs);*/

    /*Q11: Process purchase requests.
    atomic = false: process each request independtly.
    atomic = true: all requests must succed, otherwise none is purchased.*/
   std::vector<PurchaseRecord> processBatch(
    const std::vector<PurchaseRequest>& reqs,
    bool atomic = false);

    // Q8: cancels an approved order: refunds the budget and the quota usage,
    // reduces the holdings and appends a cancellation record (the original record
    // stays in the history). The refund uses the amounts recorded at purchase time,
    // so later changes to prices or tax rates do not matter. Throws
    // CancellationError (and changes nothing) if the order does not exist, was
    // rejected, is itself a cancellation, or is already cancelled.
    const PurchaseRecord& cancel(int orderNo);
    const PurchaseRecord* findOrder(int orderNo) const;  // nullptr if there is none
    bool isCancelled(int orderNo) const;

    const std::vector<PurchaseRecord>& history() const { return history_; }
    // The three totals are net of cancellations (refunds are negative records).
    // Q9: the same, for one department only ("" = the default budget).
    Money totalSpent(const std::string& department) const;
    Money totalSpent() const;     // post-tax: what was charged to the budget
    Money totalPreTax() const;    // Q6: sum of approved orders before tax
    Money totalTax() const;       // Q6: sum of tax on approved orders

    void printReport(std::ostream& os) const;
    // Q9: each department's budget, quotas and net spending.
    void printDepartmentReport(std::ostream& os) const;

    // Q12: Register or update a vendor's unit price for a resource.
void setVendorPrice(const std::string& resourceId,
                    const std::string& vendor,
                    Money unitPrice);

// Cheapest vendor for a resource.
// Returns an empty string if no vendors are registered.
std::string cheapestVendor(const std::string& resourceId) const;

private:
    PurchaseRecord& record(const Resource* r, const std::string& id, int qty,
                           TaxedAmount amount, bool approved, std::string reason,
                           const std::string& department);
    // The budget charged for a department ("" = the default budget); nullptr if unknown.
    Budget* budgetFor(const std::string& department) const;

    // Q12: Resource ID -> Vendor name -> Unit price
std::map<std::string, std::map<std::string, Money>> vendorPrices_;

// Calculate the vendor-adjusted pre-tax price.
Money vendorCost(const Resource& resource, int quantity) const;

    Catalog& catalog_;
    Budget& budget_;
    TaxPolicy tax_;
    std::map<std::string, Budget*> departments_;
    std::vector<PurchaseRecord> history_;
    int nextOrderNo_ = 1;
};

}  // namespace bookmgmt