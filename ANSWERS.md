# Assignment 2: Answers

## Questions attempted
Q1 (update this list as you complete more questions)

## Q1: Journal

**Files added/changed:** include/bookmgmt/Journal.h, src/Journal.cpp,
include/bookmgmt/Resource.h, src/Resource.cpp, src/Budget.cpp,
include/bookmgmt/bookmgmt.h, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Journal derives directly from Resource, not Book. A journal has an ISSN,
  issues per year and subscription length, but no ISBN, authors, edition or
  binding, and its price is a recurring subscription. Inheriting from Book
  would carry fields that make no sense for a journal.
- Added ResourceCategory::Journal in three places: the enum, categoryName(),
  and kAllCategories. kAllCategories matters because Budget::print() only
  lists categories in that array, so without it journals would be missing
  from reports.
- costFor(n) = unitPrice * n * subscriptionYears. It calls requirePositive(n)
  first, so zero or negative copies throw std::invalid_argument, the same as
  for other resources.
- The constructor throws std::invalid_argument if subscriptionYears < 1.
  The default is 1, declared only in the header.
- issuesPerYear is not validated, because the assignment does not require it.
  [Or: "I also reject values below 1, because ..." if you added a check.]
- printDetails() prints the ISSN, issues per year and subscription years,
  in the same style as Book.

**Testing:** testJournal() adds 21 checks: category and isDigital, getters,
default and multi-year cost, invalid years, invalid copy counts, polymorphic
cost and printing through a Resource reference, and a purchase through a
Journal quota (accepted within quota, rejected over it, shown in the budget
report).

**Demo:** the demo adds a Journal to the catalog, sets a Journal quota, and
includes a journal order in the batch.

## Q2: EBook

**Files added/changed:** include/bookmgmt/EBook.h, src/EBook.cpp,
include/bookmgmt/Resource.h, src/Resource.cpp, src/Budget.cpp,
include/bookmgmt/bookmgmt.h, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- EBook derives from ElectronicResource, so pricing (platform fee + unit
  price x seats) and isDigital() are inherited unchanged. EBook does not
  override costFor() or isDigital(), as the spec requires.
- Added ResourceCategory::EBook in the enum, categoryName() and
  kAllCategories, so e-books can have a quota separate from other electronic
  resources. A test shows that buying an e-book uses only the EBook quota.
- Added an EBookFormat enum (PDF, EPUB, HTML) with a formatName() helper.
  Format defaults to PDF and DRM protection defaults to false.
- printDetails() first calls ElectronicResource::printDetails() and then
  prints authors, ISBN, format and DRM status. A test checks that the
  parent's lines appear before the new ones.

**Written answer: duplication between Book and EBook**
An e-book is also a kind of book, so Book and EBook both store authors and an
ISBN, have the same getters and constructor code, and print the same two
lines (EBook includes Book.h only to reuse joinAuthors). To avoid this, the
shared data and its printing could move into a small helper class (e.g.
BookInfo) that both classes hold as a member (composition). I prefer this to
inheritance because EBook already derives from ElectronicResource and Book
derives from Resource, so making EBook also inherit from Book would need
multiple inheritance and the diamond problem on Resource.

**Testing:** testEBook() adds 19 checks (category, getters and defaults,
inherited cost, parent and new fields in the printed output, separate EBook
quota, rejection over quota). Total checks: 104.

## Q3: AudioBook and Thesis

**Files added/changed:** include/bookmgmt/AudioBook.h, src/AudioBook.cpp,
include/bookmgmt/Thesis.h, src/Thesis.cpp, Resource.h, Resource.cpp,
Budget.cpp, bookmgmt.h, tests/test_main.cpp, examples/demo.cpp.

**AudioBook: base class ElectronicResource**
An audiobook is a digital product that listeners stream or download under a
licence, so per-seat pricing (platform fee + price x seats), the access URL
and the licence model fit it, and isDigital() is already true. I did not use
Book, because edition and binding mean nothing for audio, and print-only
rules added later (hardcover surcharge, print bulk discount) would wrongly
apply to it. It does not override costFor(). Extra fields: authors,
narrator, duration in minutes. Design decision: the constructor rejects a
duration below 1 with std::invalid_argument.

**Thesis: base class Resource**
A thesis has no seat licence, access URL or platform fee, so
ElectronicResource does not fit, and it has no ISBN, edition or binding, so
Book does not fit. It is usually free: unitPrice defaults to zero, so the
inherited costFor() (unitPrice x quantity) returns 0.00 without an override.
A non-zero price is still allowed (for example a bound copy). The university
is stored as the Resource's publisher and university() returns it, so it is
not stored twice.

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

**Testing:** testBulkDiscounts() adds 26 checks (thresholds at 9/10/11 and
49/50/51 seats, hardcover + discount order, journals, theses, rounding,
inheritance by EBook and AudioBook, and quota checks on discounted cost).
Total checks: 183.

## Q6: Taxes

**Files added/changed:** include/bookmgmt/TaxPolicy.h, src/TaxPolicy.cpp,
Acquisition.h, Acquisition.cpp, bookmgmt.h, CMakeLists.txt,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- A new TaxPolicy class holds two configurable rates, one for print items
  and one for electronic items, stored in basis points (500 = 5%) so that
  money maths stays in exact integers and fractional rates (5.5%) work.
  The default policy is 0% everywhere, so nothing changes until a policy
  is set on the AcquisitionManager.
- A resource's rate is chosen with isDigital(): electronic resources, e-books
  and audiobooks use the electronic rate; books, journals and theses the
  print rate. No per-class tax code is needed.
- Budget is unchanged. AcquisitionManager now passes the post-tax cost to
  Budget::check/commit, so unit/spend quotas and the overall budget are all
  checked against the post-tax amount, and the budget's "spent" is post-tax.
- Order of operations: hardcover surcharge, then bulk discount, then tax on
  the result. Tax is computed per order and rounded half up to the nearest
  paisa/cent.
- quote() now returns the post-tax price (what would actually be charged);
  quotePreTax() returns the old pre-tax value. With no tax set they agree.
- PurchaseRecord::cost is the post-tax amount charged; new fields
  preTaxCost and taxAmount hold the parts. Rejected orders show the quoted
  amounts but are not counted in the totals.
- printReport() prints per-order "(pre-tax X + tax Y)" for taxed orders and
  the totals Total pre-tax, Total tax and Total spent (post-tax).
- Added the new source files to CMakeLists.txt, which lists sources explicitly.

**Testing:** testTaxes() adds 41 checks: rate selection (including derived
electronic classes), rounding, 5.5% rates, negative rates rejected, records
and reports, tax after discounts, and quota/overall budget checks that
only fail once tax is included. Total checks: 224.

## Q7: Limit on different titles per category

**Files changed:** include/bookmgmt/Budget.h, src/Budget.cpp,
src/Acquisition.cpp, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- A "title" is one catalogue entry (one resource id). Buying more copies of
  a title already bought in that category is not a new title.
- Quota gets an optional field maxTitles (std::optional<int>, default no
  limit), so every existing quota {units, spend} behaves exactly as before.
- Usage records the units bought per title (a map from resource id to
  units); the number of titles is its size. Keeping units per title, not
  just a set of ids, lets a later cancellation remove a title again.
- Budget::check and Budget::commit got overloads that take the title id,
  because the original signatures cannot know which title is bought.
  AcquisitionManager uses the new overloads. The old overloads still work
  but do not count titles.
- Order of checks: unit quota, spend quota, title limit, then the overall
  budget. A title-limit failure throws QuotaExceededError and changes
  nothing; in processBatch it is recorded as a rejection with its reason.
- Budget::print() gained a "Titles used/max" column. A limit of 0 titles
  blocks all purchases in the category; negative limits are rejected with
  std::invalid_argument.

**Testing:** testTitleQuota() adds 25 checks (new vs repeat titles, third
title rejected with no side effects, per-category limits, batch reasons,
check order, zero and negative limits, report column, old overload).
Total checks: 249.

## Q8: Cancellation

**Files changed:** include/bookmgmt/Exceptions.h, include/bookmgmt/Budget.h,
src/Budget.cpp, include/bookmgmt/Acquisition.h, src/Acquisition.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- AcquisitionManager::cancel(orderNo) cancels a whole approved order,
  identified by its order number. It throws the new CancellationError
  (and changes nothing) if the order does not exist, was rejected, is itself
  a cancellation record, or is already cancelled.
- The original record is never modified. A separate cancellation record is
  appended with the next order number; it has isCancellation = true and
  cancelsOrderNo pointing at the original. Its quantity is the units
  returned and its cost, preTaxCost and taxAmount are the refunded amounts
  as negative numbers, so totalSpent/totalPreTax/totalTax are automatically
  net of cancellations and totalSpent still equals Budget::spent().
  isCancelled(orderNo) is derived by searching for such a record.
- The refund uses the amounts stored on the original record, not a new
  calculation, so later changes to prices or tax rates do not matter.
- New Budget::refund() returns units and money to the category and to the
  overall budget. Usage keeps units per title, so when a title's units
  reach zero it stops counting towards the Q7 title limit.
- Order of operations: all checks first (including that the holdings still
  cover the units), then Budget::refund and the holdings reduction. A failed
  cancellation therefore leaves no partial changes.
- The report shows cancellation records as CANCELLED with the negative
  amount and "(cancels order #N)", and marks the original with "[cancelled]".
- Partial cancellation of an order is not supported (not asked for).

**Testing:** testCancellation() adds 55 checks (refund of budget, quotas,
titles and holdings; original kept and cancellation added; all failure
cases; frees a title slot; two orders for one title; refund independent
of later tax changes; report; Budget::refund directly). Total checks: 304.

## Q9: Department budgets

**Files changed:** include/bookmgmt/Exceptions.h, include/bookmgmt/Acquisition.h,
src/Acquisition.cpp, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- A department is a named Budget (with its own quotas and overall total)
  registered on the AcquisitionManager with addDepartment(name, budget); the
  caller owns the budgets. Because Budget already holds quotas, usage and
  title counts, every limit is automatically per department.
- Backwards compatible: a purchase that names no department is charged to the
  default budget passed to the constructor, so all existing calls and tests
  are unchanged. PurchaseRequest got an optional department field, and
  purchase()/canPurchase() got overloads that take a department.
- An unknown department makes purchase() throw UnknownDepartmentError;
  canPurchase() and processBatch() report it as a rejection with the reason
  "unknown department: X".
- Holdings stay library-wide in the Catalog: departments only decide who pays.
- PurchaseRecord stores its department; a cancellation record copies the
  department of the order it cancels, and cancel() refunds that department's
  budget (and only it).
- Tax is applied as before and each department's quotas and total are checked
  against the post-tax cost.
- Reporting: totalSpent(department) gives one department's net total ("" = the
  default budget), printDepartmentReport() prints each department's budget
  table, and order lines show "(dept: X)".
- The reference returned by purchase()/cancel() points into the history and is
  invalidated by later purchases; callers who keep a record should copy it.

**Testing:** testDepartments() adds 48 checks (registration and errors,
charging only the named department, default budget unchanged, per-department
unit/title/overall limits, unknown departments, batches, per-department
totals, cancellation refunds, tax, reports). Total checks: 352.

## Q10: Year-end rollover

**Files changed:** include/bookmgmt/Budget.h, src/Budget.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Budget::carryForward(percent) returns the given whole-number percentage
  (0..100) of the unspent amount (remaining()), rounded down to the paisa so a
  rollover never creates money. Percentages outside 0..100 throw
  std::invalid_argument.
- Budget::rollover(percent) builds next year's budget with a total of
  "this year's total + carried-forward amount". Budget::rollover(percent,
  baseAllocation) does the same with a different base allocation for the new
  year (a negative allocation throws std::invalid_argument).
- Rollover returns a new Budget and leaves this year's budget untouched, so
  last year's records are preserved.
- The category quotas (units, spend and title limits) are copied; usage
  (units, spend, titles) starts again from zero.
- A cancelled order has been refunded, so it counts as unspent and is
  carried forward.
- All money arithmetic is in integer minor units (no floating point).

**Testing:** testRollover() checks 0%, 50% and 100%, the explicit base
allocation, copied quotas, reset usage, the unchanged original budget,
rounding down and invalid arguments. Demo: 25000.00 total, 8975.00 unspent,
50% carried forward = 4487.50, next year's total 29487.50.

## Q11: All-or-nothing batches

**Files changed:** include/bookmgmt/Acquisition.h, src/Acquisition.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- processBatch(requests, atomic = false) has a new optional flag. With the
  default (false) every request is approved or rejected on its own, exactly
  as before, so existing calls are unchanged.
- With atomic = true the batch is first validated on temporary copies of the
  budgets, so the real budgets and holdings are not touched while checking.
  Each request is checked against the copy and, if it fits, committed to the
  copy, so later requests see the quota used by earlier ones (two requests
  that fit alone but not together are caught). There is one copy per Budget
  object, so two department names sharing one budget also share one copy.
- If any request fails, nothing is bought. Every request is still recorded
  as rejected: the failing ones with their own reason, the others with
  "atomic batch rejected: another request failed".
- If every request passes, the real purchases are made with purchase().
  Because the validation used the same rules, none of them can fail.
- An empty batch returns no records.

**Testing:** cumulative unit, spend and title limits across a batch, mixed
departments, unknown resources and departments, invalid quantities, success
and failure paths, no change to budgets/holdings/history on failure, empty
batches, default (non-atomic) behaviour, and two names sharing one budget.

## Q12: Vendors

**Files changed:** include/bookmgmt/Resource.h, src/Resource.cpp, Book.h/.cpp,
Journal.h/.cpp, ElectronicResource.h/.cpp, Acquisition.h, Acquisition.cpp,
tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- AcquisitionManager::setVendorPrice(resourceId, vendor, price) records or
  updates a vendor's unit price (an empty vendor name, a negative price or an
  unknown resource throws). cheapestVendor(id) returns the vendor with the
  lowest unit price; a tie goes to the first name alphabetically; "" means
  no vendors, in which case the listed price is used as before. A vendor's
  price is used even if it is above the listed price.
- The vendor's price must go through the same pricing rules as the listed
  price. My first version added (vendor price - listed price) x quantity to
  costFor(), which is wrong for hardcovers (the 20% surcharge), bulk
  discounts, seat tiers beyond the 50th seat and journal subscription years.
  Now Resource has a virtual costAt(price, quantity) that states each
  class's rule as a function of the unit price, and costFor(q) is simply
  costAt(listedPrice, q). Book, Journal and ElectronicResource override
  costAt (instead of costFor); EBook, AudioBook and Thesis inherit it. The
  vendor cost is resource.costAt(vendorPrice, quantity).
- Quotes, quota/budget checks, tax and the atomic batch all use the vendor
  cost. PurchaseRecord has a vendor field set on approved purchases; a
  cancellation record copies it from the original order, and the report
  shows "(vendor: X)".
- Vendor prices are stored in the AcquisitionManager, not in the Catalog,
  so two managers do not share them.

**Testing:** cheapest-vendor selection and price updates, invalid input,
the vendor price through hardcover surcharge, bulk discount, seat tiers and
subscription years, EBook inheritance, tax and quota checks on the vendor
cost, vendor recorded in the history and on cancellation, atomic batches.
Total checks: 449.

## Q13: Searches

**Files changed:** include/bookmgmt/Resource.h, src/Resource.cpp, Book.h,
EBook.h, AudioBook.h, Journal.h, include/bookmgmt/Catalog.h,
src/Catalog.cpp, tests/test_main.cpp, examples/demo.cpp.

**Design decisions**
- Catalog gets searchAuthor, searchIdentifier (ISBN or ISSN) and
  searchYears, named like the existing searchTitle; results are in id order
  and point to the catalogue's own objects.
- Each search is a query on the resource, not a type check in Catalog:
  Resource has virtual hasAuthor() and hasIdentifier() that return false by
  default. Book, EBook and AudioBook override hasAuthor; Book, EBook (ISBN)
  and Journal (ISSN) override hasIdentifier. A thesis or an electronic
  database has neither, so it is never found. No dynamic_cast is needed and
  a new resource type only overrides what applies to it. The matching code
  is written once, in protected helpers of Resource, so the classes do not
  repeat it.
- Author search is case-insensitive and matches part of a name, on any
  author; only the authors list counts (not a narrator or a supervisor).
- ISBN/ISSN search compares the whole code ignoring case, hyphens and
  spaces, so 978-0132350884 equals 9780132350884 and an ISSN check digit
  x equals X; a prefix does not match.
- searchYears is inclusive at both ends.
- Blank author/code and a range with the first year after the last throw
  std::invalid_argument.

**Testing:** testSearches() adds 34 checks (author, ISBN, ISSN and year
cases, ordering, errors, base-class use, removed resources). Total: 483.

## Q14: Lending

**Files added/changed:** include/bookmgmt/Lending.h, src/Lending.cpp,
include/bookmgmt/Exceptions.h, include/bookmgmt/Catalog.h, src/Catalog.cpp,
src/Acquisition.cpp, bookmgmt.h, CMakeLists.txt, tests/test_main.cpp,
examples/demo.cpp.

**Design decisions**
- A new LendingDesk class records which patron has which resource. The
  limits come from the Catalog's holdings (copies owned / seats licensed): the
  Catalog also counts the units in use (inUse, available = holdings - inUse),
  so there is one source of truth for the numbers.
- Print resources are borrowed and returned (borrowCopy / returnCopy);
  electronic resources (isDigital(), including EBook and AudioBook) are
  opened and closed (openSession / closeSession). The wrong verb for the kind
  of resource throws LendingError.
- Policy: a patron can have at most one copy or session of the same resource
  at a time, which also identifies who is returning what.
- Exceptions: LendingError for rule violations and UnavailableError (a
  subclass) when every copy or seat is in use; NotFoundError for unknown
  resources and std::invalid_argument for a blank patron name. All checks come
  before any change, so a failed request changes nothing.
- Catalog::addHoldings refuses to drop the holdings below the units in use and
  Catalog::remove refuses a resource that is in use. AcquisitionManager::cancel
  now checks that the cancelled units are free (available, not just held), so
  a purchase cannot be cancelled while its copies are out on loan; this check
  happens before the budget is refunded, so a refused cancellation changes
  nothing.
- Not included (not asked for): loan history, due dates, fines.

**Testing:** testLending() adds 51 checks (borrowing and returning limits,
one copy per patron, sessions and seats, wrong verb for the kind, unknown
resources and blank names, catalogue consistency, the report, and lending
together with purchasing and cancellation). Total checks: 534.

## Q15: Currency code on Money

**Files changed:** include/bookmgmt/Money.h, src/Money.cpp,
include/bookmgmt/Exceptions.h, Book.cpp, Resource.cpp, ElectronicResource.cpp,
TaxPolicy.cpp, Budget.h, Budget.cpp, Acquisition.cpp, tests/test_main.cpp,
examples/demo.cpp.

**Design decisions**
- Money has a three-letter currency code (stored in upper case; anything else
  throws std::invalid_argument). Inside Money the letters are packed into one
  integer so copies stay cheap. The default currency is INR (kDefaultCurrency),
  used when none is named, so all existing code and tests are unchanged;
  amounts in the default currency print without the code ("450.00"), other
  currencies with it ("12.50 USD").
- Adding, subtracting and comparing (including == and !=) amounts in different
  currencies throws the new CurrencyMismatchError, a LibraryError. There is no
  currency conversion. Even a zero in one currency cannot be compared with a
  zero in another.
- A default-constructed Money is a zero with no currency yet and takes the
  currency of whatever it is combined with. The library uses Money{} for the
  start of a sum, for "is it zero?" tests, for the price of a free thesis and
  for default platform fees; these have to work in every currency. Once a
  Money has a currency it keeps it.
- Results built from minor units (hardcover surcharge, bulk discount, half-price
  seats, tax, carried-forward amount) used Money::fromMinor(), which would turn
  dollars into rupees. They now use withMinorUnits(), which keeps the currency
  of the amount it is called on.
- A budget has the currency of its total: costs must match it (check() gives
  the reason "currency mismatch ...", so canPurchase and processBatch reject
  the request normally, while commit/purchase throw CurrencyMismatchError before
  changing anything); a quota in another currency is refused by setQuota; a
  vendor price must be in the currency of the listed price.
- Departments can have budgets in different currencies, but the total over
  all of them (totalSpent()) cannot be added up and throws; per-department
  totals work.

**Testing:** testMoneyCurrency() and testCurrencyInTheLibrary() add 90
checks (default and named currencies, invalid codes, printing, all arithmetic
and comparison operators in one currency and across currencies, the no-currency
zero, the pricing chain, tax, budgets, quotas, vendors, cancellation, rollover,
batches and departments in a second currency). Total checks: 624.

