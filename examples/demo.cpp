// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

int main()
{
    Catalog catalog;

    catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                          "978-0132350884", "Prentice Hall", 2008, Money::of(450));
    catalog.emplace<Book>("B002", "The C++ Programming Language",
                          std::vector<std::string>{"Bjarne Stroustrup"}, "978-0321563842",
                          "Addison-Wesley", 2013, Money::of(1200), 4, Binding::Hardcover);
    catalog.emplace<ElectronicResource>("R001", "IEEE Xplore Digital Library", "IEEE", 2026,
                                        Money::of(150), "https://ieeexplore.example",
                                        LicenseModel::AnnualSubscription, Money::of(2000));
    catalog.emplace<ElectronicResource>("R002", "MATLAB Campus Licence", "MathWorks", 2026,
                                        Money::of(400), "https://licensing.example/matlab",
                                        LicenseModel::Perpetual);
    catalog.emplace<Journal>("J001", "Communications of the ACM", "0001-0782", "ACM", 2026,
                             Money::of(300), 12, 2);
    catalog.emplace<EBook>("E001", "Clean Code (e-book)",
                           std::vector<std::string>{"Robert C. Martin"}, "978-0132350884",
                           "Prentice Hall", 2008, Money::of(20), "https://ebooks.example/e001",
                           EBookFormat::EPUB, true, LicenseModel::AnnualSubscription,
                           Money::of(500));
    catalog.emplace<AudioBook>("A001", "Clean Code (audio)",
                               std::vector<std::string>{"Robert C. Martin"}, "Narrator One",
                               "Prentice Hall", 2020, Money::of(15), "https://audio.example/a001",
                               480, LicenseModel::AnnualSubscription, Money::of(200));
    catalog.emplace<Thesis>("T001", "Graph Algorithms", "IIT Delhi", "PhD", "Prof. Rao", 2023);
    catalog.emplace<Thesis>("T002", "Compilers", "IISc", "MTech", "Prof. Iyer", 2022);

    std::cout << "=== Catalog ===\n";
    for (const Resource *r : catalog.all())
        std::cout << r->summary() << "\n";

    std::cout << "\n=== Details of R001 ===\n"
              << catalog.get("R001");

    Budget budget(Money::of(25000)); // earlier 20000
    budget.setQuota(ResourceCategory::Book, {10, Money::of(9200)});
    budget.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(12000)});
    budget.setQuota(ResourceCategory::Journal, {10, Money::of(3000)});
    budget.setQuota(ResourceCategory::EBook, {30, Money::of(2000)});
    budget.setQuota(ResourceCategory::AudioBook, {20, Money::of(1000)});
    budget.setQuota(ResourceCategory::Thesis, {5, Money::of(500), 1}); // at most 1 title

    AcquisitionManager acq(catalog, budget);

    std::cout << "\n=== Quotes ===\n";
    std::cout << "5 copies of B002  = " << acq.quote("B002", 5) << "\n";
    std::cout << "20 seats of R001  = " << acq.quote("R001", 20) << "  (incl. platform fee)\n";
    // Q5
    std::cout << "9 copies of B001  = " << acq.quote("B001", 9) << "\n";
    std::cout << "10 copies of B001 = " << acq.quote("B001", 10) << "  (10% bulk discount)\n";
    std::cout << "60 seats of R001  = " << acq.quote("R001", 60)
              << "  (seats beyond the 50th at half price)\n";

    acq.processBatch({
        {"B001", 4},  // 1800  ok
        {"B002", 5},  // 6000  ok  -> book spend 7800  ;
                      // 7200  ok (hardcover: 1200 + 20% = 1440 each) -> book spend 9000
        {"B001", 1},  // 450   rejected: book spend quota (200 left)
        {"R001", 20}, // 5000  ok
        {"R002", 25}, // 10000 rejected: e-resource unit quota (20 seats left)
        {"R002", 15}, // 6000  ok  -> e-resource spend 11000
        {"R002", 5},  // 2000  rejected: e-resource spend quota (1000 left)
        {"J001", 3},  // 1800  ok (300/yr * 3 copies * 2 years)
        {"E001", 10}, // 500+20*10=700 ok
        {"A001", 5},  // 200 + 15 * 5 = 275  ok (AudioBook quota)
        {"T001", 2},  // free thesis: 0.00 ok
        {"T002", 1},  // rejected: Thesis title limit (1) already used by T001
        {"X999", 1},  // rejected: unknown id

    });

    std::cout << "\n=== Acquisition report ===\n";
    acq.printReport(std::cout);

    std::cout << "\n=== Budget ===\n";
    budget.print(std::cout);

    std::cout << "\n=== Holdings ===\n";
    for (const Resource *r : catalog.all())
        std::cout << "  " << r->id() << ": " << catalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    // Direct purchase: errors are reported with exceptions
    std::cout << "\n=== Direct purchase that breaks a quota ===\n";
    try
    {
        acq.purchase("B002", 1);
    }
    catch (const QuotaExceededError &e)
    {
        std::cout << "QuotaExceededError: " << e.what() << "\n";
    }

    // Cancellation (Q8): cancel order #2 (five hardcover copies of B002). The budget,
    // the quota usage and the holdings are refunded, and a separate cancellation
    // record is added; the original record stays in the history.
    std::cout << "\n=== Cancellation ===\n";
    const PurchaseRecord &cancelled = acq.cancel(2);
    std::cout << "Cancelled order #2, refunded " << Money{} - cancelled.cost << "\n";
    std::cout << "B002 holdings: " << catalog.holdings("B002") << "\n";
    acq.purchase("B001", 1); // the earlier rejection (#3) would fit now: the Book quota was freed
    acq.printReport(std::cout);
    budget.print(std::cout);

    // Departments (Q9): each department has its own budget and quotas. A request
    // names the department to charge; holdings stay library-wide.
    std::cout << "\n=== Departments ===\n";
    Budget csBudget(Money::of(6000));
    csBudget.setQuota(ResourceCategory::Book, {5, Money::of(3000)});
    Budget physicsBudget(Money::of(2000));
    acq.addDepartment("Computer Science", csBudget);
    acq.addDepartment("Physics", physicsBudget);
    const auto deptResults = acq.processBatch({
        {"B001", 4, "Computer Science"}, // 1800 ok
        {"B001", 2, "Computer Science"}, // rejected: CS Book unit quota (5)
        {"B001", 3, "Physics"},          // 1350 ok
        {"R001", 5, "Physics"},          // rejected: 2750 > Physics budget
        {"B001", 1, "Chemistry"},        // rejected: unknown department
    });
    for (const auto &r : deptResults)
        std::cout << "  " << r.department << ": " << r.resourceId << " x" << r.quantity
                  << " -> " << (r.approved ? "approved" : "rejected: " + r.reason) << "\n";
    acq.cancel(deptResults[0].orderNo); // the refund goes back to Computer Science
    std::cout << "Cancelled the first Computer Science order; its refund went back to that budget\n";
    acq.printDepartmentReport(std::cout);

    // Taxes (Q6): 5% on print items, 18% on electronic items. Quotas and the budget are checked against the post-tax cost.
    std::cout << "\n=== Taxes ===\n";
    Budget taxedBudget(Money::of(50000));
    AcquisitionManager taxed(catalog, taxedBudget);
    taxed.setTaxPolicy(TaxPolicy(500, 1800)); // basis points: 500 = 5%, 1800 = 18%
    std::cout << "4 copies of B001: pre-tax " << taxed.quotePreTax("B001", 4)
              << ", with tax " << taxed.quote("B001", 4) << "\n";
    taxed.processBatch({{"B001", 4}, {"R001", 20}, {"E001", 5}});
    taxed.printReport(std::cout);

    // Q10: Year-end budget rollover demonstration
    std::cout << "\n=== Q10: Year-End Rollover ===\n";

    // Use the existing library budget
    std::cout << "Current year's total: "
              << budget.total() << "\n";

    std::cout << "Amount spent: "
              << budget.spent() << "\n";

    std::cout << "Remaining amount: "
              << budget.remaining() << "\n";

    // Carry forward 50% of the unspent amount
    Budget nextYear = budget.rollover(50);

    std::cout << "Carried forward (50%): "
              << budget.carryForward(50) << "\n";

    std::cout << "Next year's total budget: "
              << nextYear.total() << "\n";

    std::cout << "Next year's spending: "
              << nextYear.spent() << "\n";

    // Display the new budget with reset quota usage
    nextYear.print(std::cout);

    // Q11: All-or-Nothing Batch Processing
std::cout << "\n=== Q11: Atomic Batch Processing ===\n";

std::cout << "Budget before batch: "
          << budget.remaining() << "\n";

// B001: 2 copies, B002: 5 copies
// Combined request exceeds the remaining Book unit quota.
auto batchResults = acq.processBatch({
    {"B001", 2},
    {"B002", 5}
}, true);

for (const auto& result : batchResults) {
    std::cout << result.resourceId << ": "
              << (result.approved ? "Approved" : "Rejected")
              << "\n";
}

std::cout << "Budget after batch: "
          << budget.remaining() << "\n";

std::cout << "B001 holdings: "
          << catalog.holdings("B001") << "\n";

std::cout << "B002 holdings: "
          << catalog.holdings("B002") << "\n";

     // Q12: Vendors — integrated with existing BookManagement system.
std::cout << "\n=== Q12: Vendors ===\n";

// B001 already exists in the main library catalog.
acq.setVendorPrice("B001", "Amazon", Money::of(450));
acq.setVendorPrice("B001", "Flipkart", Money::of(420));
acq.setVendorPrice("B001", "College Bookstore", Money::of(400));

std::cout << "Book: " << catalog.get("B001").title() << "\n";

std::cout << "Cheapest vendor: "
          << acq.cheapestVendor("B001") << "\n";

std::cout << "Price for 2 copies: "
          << acq.quote("B001", 2) << "\n";

// Purchase using existing acquisition manager.
const PurchaseRecord vendorOrder = acq.purchase("B001", 2);

std::cout << "Purchase status: "
          << (vendorOrder.approved ? "Approved" : "Rejected") << "\n";

std::cout << "Vendor used: "
          << vendorOrder.vendor << "\n";

std::cout << "Total cost: "
          << vendorOrder.cost << "\n";

std::cout << "Remaining library budget: "
          << budget.remaining() << "\n";

// Existing order history now displays the vendor.
acq.printReport(std::cout);

    // Q13: searches by author, by ISBN/ISSN and by a range of publication years.
    std::cout << "\n=== Q13: Searches ===\n";
    const auto printFound = [](const char* label, const std::vector<const Resource*>& found) {
        std::cout << label << " -> " << found.size() << " found\n";
        for (const Resource* r : found) std::cout << "  " << r->summary() << "\n";
    };
    printFound("Author \"martin\"", catalog.searchAuthor("martin"));
    printFound("ISBN 9780132350884", catalog.searchIdentifier("9780132350884"));
    printFound("ISSN 0001-0782", catalog.searchIdentifier("0001-0782"));
    printFound("Years 2008-2013", catalog.searchYears(2008, 2013));
    printFound("Author \"nobody\"", catalog.searchAuthor("nobody"));
    
        // Q14: lending. Print copies are borrowed and returned (limited by the copies held);
    // electronic resources have sessions (limited by the licensed seats).
    std::cout << "\n=== Q14: Lending ===\n";
    LendingDesk desk(catalog);
    std::cout << "J001: " << catalog.holdings("J001") << " copies held\n";
    desk.borrowCopy("alice", "J001");
    desk.borrowCopy("bob", "J001");
    desk.borrowCopy("carol", "J001");
    std::cout << "J001 available after three loans: " << desk.available("J001") << "\n";
    try {
        desk.borrowCopy("dave", "J001");
    } catch (const UnavailableError& e) {
        std::cout << "dave could not borrow: " << e.what() << "\n";
    }
    desk.returnCopy("bob", "J001");
    desk.borrowCopy("dave", "J001");
    std::cout << "bob returned J001 and dave borrowed it\n";

    desk.openSession("alice", "A001");
    desk.openSession("bob", "A001");
    std::cout << "A001 seats free: " << desk.available("A001") << " of "
              << catalog.holdings("A001") << "\n";
    try {
        desk.borrowCopy("alice", "A001");
    } catch (const LendingError& e) {
        std::cout << "alice: " << e.what() << "\n";
    }
    desk.closeSession("alice", "A001");
    desk.printReport(std::cout);

        // Q15: every amount has a currency. Amounts in different currencies cannot be added,
    // subtracted or compared; there is no conversion.
    std::cout << "\n=== Q15: Currencies ===\n";
    const Money rupees = Money::of(450);                 // the default currency (INR)
    const Money dollars = Money::of(12, 50, "USD");
    std::cout << "rupees: " << rupees << " (" << rupees.currency() << "), dollars: " << dollars
              << " (" << dollars.currency() << ")\n";
    std::cout << "12.50 USD + 7.50 USD = " << dollars + Money::of(7, 50, "USD") << "\n";
    try {
        std::cout << rupees + dollars << "\n";
    } catch (const CurrencyMismatchError& e) {
        std::cout << "adding: " << e.what() << "\n";
    }
    try {
        std::cout << (rupees < dollars) << "\n";
    } catch (const CurrencyMismatchError& e) {
        std::cout << "comparing: " << e.what() << "\n";
    }

    // A budget in dollars buying a book priced in dollars (surcharge, discount and tax keep the currency)
    Catalog usCatalog;
    usCatalog.emplace<Book>("U001", "Design Patterns", std::vector<std::string>{"Erich Gamma"},
                            "978-0201633610", "Addison-Wesley", 1994, Money::of(50, 0, "USD"), 1,
                            Binding::Hardcover);
    usCatalog.emplace<Book>("U002", "Rupee priced book", std::vector<std::string>{"A. Author"},
                            "978-0000000000", "Local Press", 2020, Money::of(300));
    Budget usBudget(Money::of(1000, 0, "USD"));
    AcquisitionManager usAcq(usCatalog, usBudget);
    usAcq.setTaxPolicy(TaxPolicy(500, 1800));
    usAcq.purchase("U001", 4);                           // 4 * (50 + 20%) = 240.00, +5% tax
    std::cout << "4 hardcover copies, spent: " << usBudget.spent() << ", remaining: "
              << usBudget.remaining() << "\n";
    std::string why;
    if (!usAcq.canPurchase("U002", 1, &why)) std::cout << "U002 rejected: " << why << "\n";
    usAcq.printReport(std::cout);
    
    return 0;
}
