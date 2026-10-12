# Assignment 2: Answers

## Questions attempted
Q1 (update this list as you complete more questions)

## Q1: Journal

**Files added/changed:** include/bookmgmt/Journal.h, src/Journal.cpp,
include/bookmgmt/Resource.h, src/Resource.cpp, src/Budget.cpp,
include/bookmgmt/bookmgmt.h, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Journal derives directly from Resource, not Book. A journal has an ISSN,issues per year and subscription length, but no ISBN, authors, edition ormbinding, and its price is a recurring subscription. Inheriting from Book would carry fields that make no sense for a journal.
- ResourceCategory::Journal in the enum, categoryName(), and kAllCategories to include it in budget reports.
- Formula is unitPrice * n * subscriptionYears. Copies must be positive, and subscription years must be at least 1

**Testing:** Added 21 tests to check getters, negative inputs, cost logic, quotas, and polymorphism

**Demo:** the demo adds a Journal to the catalog, sets a Journal quota, and includes a journal order in the batch.

## Q2: EBook

**Files added/changed:** include/bookmgmt/EBook.h, src/EBook.cpp,
include/bookmgmt/Resource.h, src/Resource.cpp, src/Budget.cpp,
include/bookmgmt/bookmgmt.h, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- EBook derives from ElectronicResource, so pricing (platform fee + unit
  price x seats) and isDigital() are inherited unchanged. EBook does not
  override costFor() or isDigital(), as the spec requires.
- Added ResourceCategory::EBook in the enum, categoryName() and
  kAllCategories, so e-books can have a quota separate from other electronic resources. Also added an EBookFormat enum (PDF, EPUB, HTML).
  - Calls the base class ElectronicResource::printDetails() first, followed by author, ISBN, format, and DRM details.

**Written answer: duplication between Book and EBook**
An e-book is also a kind of book, so Book and EBook both store authors and an ISBN, have the same getters and constructor code, and print the same two lines (EBook includes Book.h only to reuse joinAuthors). To avoid this, the shared data and its printing could move into a small helper class (e.g.BookInfo) that both classes hold as a member (composition). I prefer this to inheritance because EBook already derives from ElectronicResource and Book derives from Resource, so making EBook also inherit from Book would need
multiple inheritance and the diamond problem on Resource.

**Testing:** Added 19 unit tests to verify formats, output strings, separate quotas, and defaults.. Total checks: 104.

## Q3: AudioBook and Thesis

**Files added/changed:** include/bookmgmt/AudioBook.h, src/AudioBook.cpp,
include/bookmgmt/Thesis.h, src/Thesis.cpp, Resource.h, Resource.cpp,
Budget.cpp, bookmgmt.h, tests/test_main.cpp, examples/demo.cpp.

**AudioBook: base class ElectronicResource**
- An audiobook is a digital product that listeners stream or download under a licence, so per-seat pricing (platform fee + price x seats), the access URL and the licence model fit it, and isDigital() is already true. 
- I did not useBook, because edition and binding mean nothing for audio, and print-only rules added later (hardcover surcharge, print bulk discount) would wrongly apply to it. It does not override costFor().

**Thesis: base class Resource**
- A thesis has no seat licence, access URL or platform fee, so ElectronicResource does not fit, and it has no ISBN, edition or binding, so Book does not fit. It is usually free: unitPrice defaults to zero, so the inherited costFor() (unitPrice x quantity) returns 0.00 without an override.
- A non-zero price is still allowed (for example a bound copy). The university is stored as the Resource's publisher and university() returns it, so it is not stored twice.

**Categories:** both have their own ResourceCategory value, so each can
have its own quota (enum, categoryName, kAllCategories).

**Testing:** testAudioBook() and testThesis() add 35 checks. Total: 139.

## Q4: Hardcover surcharge

**Files changed:** include/bookmgmt/Book.h, src/Book.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Book now overrides costFor(). Paperbacks are unchanged; for hardcovers the
  unit price is raised by 20% and then multiplied by the number of copies.
  No other class needed to change, because quotes, quota checks, purchase()
  and processBatch() all price through costFor().
- The 20% is a named constant, kHardcoverSurchargePercent, in Book.h rather
  than a magic number inside the function.
- Money stays in integer minor units (paise/cents): the new unit price is
  (minor * 120 + 50) / 100, which rounds half up. No floating point is used.
- The surcharge is applied per copy, then multiplied, matching the spec's
  "20% more than their listed unit price". Ten copies therefore cost exactly
  ten times one copy. (Surcharging the total instead could differ by a paisa.)
- unitPrice() still returns the listed price; only the cost changes.
- Existing tests did not use hardcovers, so none changed. The demo's Book
  spend quota went from 8000 to 9200 because B002 (a hardcover) now costs
  7200 for 5 copies instead of 6000.

**Testing:** testBookHardcover() adds 18 checks: paperback unchanged,
hardcover 1200 -> 1440, rounding cases (3 -> 4, 1 -> 1, 25 -> 30), invalid
quantities, virtual dispatch through Resource, and budget/quota checks using
the surcharged cost. Total checks: 157.

## Q5: Bulk discounts

**Files changed:** Resource.h, Resource.cpp, Book.cpp, Journal.cpp,
ElectronicResource.h, ElectronicResource.cpp, tests/test_main.cpp,
examples/demo.cpp.

**Design decisions**
- Print items (Book, Journal, and Thesis via the Resource default) get 10% off
  the total when 10 or more copies are bought in a single purchase ("at once"
  means one quantity, not cumulative across orders). The rule lives in one
  protected helper, Resource::applyPrintBulkDiscount, called by each print
  class, so the threshold and percentage are named constants in one place.
- Order of operations: the Q4 hardcover surcharge is applied per copy first,
  then the 10% discount on the total. For journals the threshold counts
  copies (not years) and the discount applies to the whole subscription total.
- Electronic resources: the first 50 seats cost full price, every seat beyond
  the 50th costs half. The platform fee is not discounted. EBook and
  AudioBook inherit this unchanged because they do not override costFor().
- Rounding: integer minor units, rounded half up; the extra seats are
  rounded together as one amount.
- Side effect of a flat discount: 10 copies cost the same as 9 copies for the
  same book. A marginal or sliding discount would avoid this.
- Existing test updated: in testBookHardcover() the check that bought 10 tiny
  hardcovers now buys 9, because 10 copies now trigger the discount.

**Testing:** testBulkDiscounts() adds 26 checks (thresholds at 9/10/11 and 49/50/51 seats, hardcover + discount order, journals, theses, rounding,inheritance by EBook and AudioBook, and quota checks on discounted cost).
Total checks: 183.

## Q6: Taxes

**Files added/changed:** include/bookmgmt/TaxPolicy.h, src/TaxPolicy.cpp,
Acquisition.h, Acquisition.cpp, bookmgmt.h, CMakeLists.txt,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
-TaxPolicy Class: Introduced to hold two configurable tax rates—one for print items and one for electronic items.

- Rate Selection: Determined by isDigital(). Electronic resources (e-books, audiobooks) use the electronic rate, while print resources (books, journals, theses) use the print rate. No per-class tax codes are required.

- Budget Handling: Budget logic remains unchanged, but AcquisitionManager now passes post-tax costs to Budget::check/commit. Quotas and totals are validated against post-tax amounts.

- Order of Operations: Taxes are applied after hardcover surcharge and bulk discount. Tax is computed per order and rounded half up to the nearest paisa/cent.

- Quote Functions: quote() returns post-tax price; quotePreTax() returns pre-tax. Both match when no tax is set.

- Purchase Records: Store post-tax cost in cost, with new fields preTaxCost and taxAmount.

- Reports: printReport() shows per-order breakdown and totals (pre-tax, tax, spent).

**Testing:** testTaxes() adds 41 checks covering rate selection, rounding, invalid rates, reporting, discounts, and budget validation. Total checks: 224.

## Q7: Limit on different titles per category

**Files changed:** include/bookmgmt/Budget.h, src/Budget.cpp,
src/Acquisition.cpp, tests/test_main.cpp, examples/demo.cpp.

- It is defined as a unique resource ID in the catalog; Buying repeat units of an existing title does not count as a new title.   
- Quota Tracking: Added optional maxTitles field. Tracked via a map of resourceId -> units.   
- Validation Order: Checked in order: unit quota, spend quota, title limit, and overall budget.   Reporting: Added "Titles used/max" column to Budget::print().   
- Testing: Added 25 tests for repeat titles, quota overflow exceptions, and budget reporting.

## Q8: Cancellation

**Files changed:** include/bookmgmt/Exceptions.h, include/bookmgmt/Budget.h,
src/Budget.cpp, include/bookmgmt/Acquisition.h, src/Acquisition.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Order Cancellation: cancel(orderNo) checks that the order is valid, non-rejected, and not previously cancelled.   
- Non-Destructive History: Keeps original record intact and appends a cancellation record with negative costs and returned units.  
- Budget & Title Refund: Uses original billed amounts (ignoring current price/tax changes). Reclaims the title slot if units drop to zero.  
 - Validation Check: Verifies inventory is still available before refunding. Partial cancellations are not allowed.   
 **Testing:** Added 55 tests verifying refunds, failure handling, inventory updates, and report output. Total checks: 304.

## Q9: Department budgets

**Files changed:** include/bookmgmt/Exceptions.h, include/bookmgmt/Acquisition.h,
src/Acquisition.cpp, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
Design: Added addDepartment(name, budget) to assign independent named budgets to AcquisitionManager.  
- Compatibility: Requests without a department use the default budget without breaking existing flows.  
- Error Handling: Throws UnknownDepartmentError if a requested department does not exist. 
-  Refunds & History: Cancellation records trace the original order's department and refund only that department's funds. 
-  Testing: Added 48 tests for registration, isolation between departments, batch updates, and departmental reports. Total checks: 352.

## Q10: Year-end rollover

**Files changed:** include/bookmgmt/Budget.h, src/Budget.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Calculation: carryForward(percent) computes an unspent percentage rounded down to minor integer units. 
-  Rollover Method: rollover() returns a fresh Budget instance containing base allocation plus carried funds, preserving previous year's budget.  
- Quota State: Copies quota ceilings but resets current usage counters to zero.
-   Cancelled Orders: Cancelled orders count as unspent funds available for rollover.  
- Testing: Verified 0%, 50%, and 100% rollover cases, invalid percentage bounds, and clean counter resets.

## Q11: All-or-nothing batches

**Files changed:** include/bookmgmt/Acquisition.h, src/Acquisition.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Added an optional atomic parameter to processBatch(). By default, it is false, so each purchase request is processed independently.
- When atomic = true, the system checks all purchase requests before making any actual purchases.
- Temporary copies of the budgets are created to check whether sufficient funds and quotas are available.
- Each successful request updates the temporary budget, ensuring that later requests are checked against the remaining limits.
- If any request fails, the entire batch is rejected, and no changes are made to the actual budgets or holdings.
- If all requests pass validation, the system completes every purchase using purchase().
- An empty batch returns an empty result without making changes.
 - Testing:Tested successful and rejected batches, budget limits, department budgets, invalid requests, empty batches, and unchanged holdings after rejection.

## Q12: Vendors

**Files changed:** include/bookmgmt/Resource.h, src/Resource.cpp, Book.h/.cpp,
Journal.h/.cpp, ElectronicResource.h/.cpp, Acquisition.h, Acquisition.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Added setVendorPrice() to store or update vendor prices for each resource. Invalid vendor names, negative prices, and unknown resources are rejected.
- The cheapestVendor() function automatically selects the vendor offering the lowest price. If prices are equal, the alphabetically first vendor is selected.
- If no vendor is available, the system uses the original listed price.
- Added costAt() to calculate costs using vendor prices while applying the existing pricing rules.
- Pricing rules include hardcover surcharges, bulk discounts, electronic resource seat pricing, and journal subscription periods.
- Vendor prices are used for purchase quotations, budget checks, quotas, taxes, and atomic batch processing.
- The selected vendor is recorded in purchase history and cancellation records.
- Vendor prices are stored separately in each AcquisitionManager, preventing unwanted sharing between managers.
- Testing: Tested vendor selection, price updates, invalid inputs, pricing rules, taxes, quotas, purchase history, cancellations, and atomic batches.
Total checks: 449.

## Q13: Searches

**Files changed:** include/bookmgmt/Resource.h, src/Resource.cpp, Book.h,
EBook.h, AudioBook.h, Journal.h, include/bookmgmt/Catalog.h,
src/Catalog.cpp, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Added searchAuthor(), searchIdentifier(), and searchYears() to the Catalog class.
- Search results are returned in resource ID order.
- Used virtual functions hasAuthor() and hasIdentifier() to support different resource types without dynamic_cast.
- Author searches are case-insensitive and support partial name matching.
- ISBN/ISSN searches ignore spaces, hyphens, and letter case but require an exact match.
- Publication year searches include both starting and ending years.
- Invalid inputs, such as empty searches or incorrect year ranges, throw std::invalid_argument.
- Running demo --search displays a menu for author, ISBN/ISSN, and year searches.
- Users enter their search values and receive matching resources.
- Invalid inputs display errors without closing the menu.
- Testing: testSearches() adds 34 checks (author, ISBN, ISSN and year
cases, ordering, errors, base-class use, removed resources). Total: 483.

## Q14: Lending

**Files added/changed:** include/bookmgmt/Lending.h, src/Lending.cpp,
include/bookmgmt/Exceptions.h, include/bookmgmt/Catalog.h, src/Catalog.cpp,
src/Acquisition.cpp, bookmgmt.h, CMakeLists.txt, tests/test_main.cpp,
examples/demo.cpp.

**Design decisions**
- Added LendingDesk class to manage resource borrowing, returning, and digital sessions.
- The Catalog tracks total holdings, resources in use, and available copies or seats.
- Print resources use borrowCopy() and returnCopy(), while digital resources use openSession() and closeSession().
- Each patron can borrow or access the same resource only once at a time.
- Invalid operations throw appropriate exceptions without changing existing records.
- Resources currently in use cannot be removed from the catalog.
- Purchase cancellation is prevented when the required copies are unavailable.
- Loan history, due dates, and fines are not included.
- Testing:Added 51 checks covering borrowing, returns, digital sessions, availability, invalid operations, catalog consistency, and cancellations.Total checks: 534.

## Q15: Currency code on Money

**Files changed:** include/bookmgmt/Money.h, src/Money.cpp,
include/bookmgmt/Exceptions.h, Book.cpp, Resource.cpp, ElectronicResource.cpp,
TaxPolicy.cpp, Budget.h, Budget.cpp, Acquisition.cpp, tests/test_main.cpp,
examples/demo.cpp.

**Design decisions**
- Added three-letter currency codes to Money, with INR as the default currency.
- Invalid currency codes are rejected, and non-INR amounts display their currency codes.
- Added CurrencyMismatchError to prevent arithmetic and comparisons between different currencies.
- Default zero values can adopt a currency when used in calculations.
- Added withMinorUnits() to preserve currency during discounts, taxes, and other calculations.
- Budgets, quotas, and vendor prices must use compatible currencies.
- Purchases with mismatched currencies are rejected without changing budgets or holdings.
- Departments can use different currencies, but their combined spending cannot be calculated without conversion.
- Testing:Added 90 checks covering currencies, arithmetic, pricing, taxes, budgets, vendors, cancellations, and department operations. Total checks: 624.

## Q16: Replace the test macros with assert

**Files changed:** tests/test_main.cpp.

**Design decisions**
- Every CHECK(cond) is now assert(cond) from <cassert>.
- CHECK_THROWS(expr, Type) cannot be written with assert alone, so it
  became assert(throwsException<Type>([&] { (void)(expr); })), where
  throwsException is a small function template (no macro). It keeps the old
  meaning: true only if the expression throws Type or a class derived from
  it; no exception or a different exception type is a failure.
- assert does nothing when NDEBUG is defined (for example in a Release
  build), which would silently turn the tests into no-ops. The test file
  therefore does #undef NDEBUG before including <cassert>.
- The failure counters were removed. A failing assert now stops the run and
  prints the file, line and expression; a successful run prints
  "All tests passed". The checks themselves are unchanged.