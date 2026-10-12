// Minimal self-contained test runner (no external framework needed).

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cassert>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

/*static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                         \
        ++g_checks;                                                              \
        if (!(cond)) {                                                           \
            ++g_failures;                                                        \
            std::cerr << __FILE__ << ":" << __LINE__ << ": CHECK failed: " #cond \
                      << "\n";                                                   \
        }                                                                        \
    } while (0)         ----> For Q16 */
#define CHECK(cond) assert(cond)
#define CHECK_THROWS(expr, ExType)            \
    do {                                      \
        bool thrown_ = false;                 \
        try {                                 \
            (void)(expr);                     \
        } catch (const ExType&) {             \
            thrown_ = true;                   \
        } catch (...) {                       \
        }                                     \
        CHECK(thrown_ && "expected " #ExType); \
    } while (0)

static void testMoney() {
    CHECK(Money::of(12, 5).toString() == "12.05");
    CHECK(Money::of(-3, 50).toString() == "-3.50");
    CHECK(Money::fromMinor(7).toString() == "0.07");
    CHECK(Money::of(10) + Money::of(0, 50) == Money::fromMinor(1050));
    CHECK(Money::of(3) * 4 == Money::of(12));
    CHECK(Money::of(1) < Money::of(2));
    CHECK_THROWS(Money::of(1, 100), std::invalid_argument);
}

static void testResourcesAndCost() {
    Book b("B1", "T", {"A", "B", "C"}, "isbn", "P", 2020, Money::of(100));
    CHECK(b.category() == ResourceCategory::Book);
    CHECK(!b.isDigital());
    CHECK(b.costFor(3) == Money::of(300));
    CHECK_THROWS(b.costFor(0), std::invalid_argument);
    CHECK(joinAuthors(b.authors()) == "A, B and C");

    ElectronicResource e("R1", "DB", "P", 2026, Money::of(10), "url",
                         LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(e.isDigital());
    CHECK(e.costFor(5) == Money::of(150));

    CHECK(e.category() == ResourceCategory::ElectronicResource);

    // Polymorphism through a base-class reference
    const Resource& r = e;
    CHECK(r.costFor(1) == Money::of(110));
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("platform fee: 100.00") != std::string::npos);

    CHECK_THROWS(Book("", "T", {}, "", "", 2000, Money::of(1)), std::invalid_argument);
    CHECK_THROWS(Book("B", "T", {}, "", "", 2000, Money::fromMinor(-1)),
                 std::invalid_argument);
}

static void testCatalog() {
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", std::vector<std::string>{"M"}, "i", "P", 2008,
                    Money::of(1));
    c.emplace<Book>("B2", "Clean Architecture", std::vector<std::string>{"M"}, "i", "P",
                    2017, Money::of(1));
    c.emplace<ElectronicResource>("R1", "ACM Digital Library", "ACM", 2026, Money::of(1),
                                  "url");

    CHECK(c.size() == 3);
    CHECK(c.contains("B1"));
    CHECK(c.find("nope") == nullptr);
    CHECK_THROWS(c.get("nope"), NotFoundError);
    CHECK_THROWS(c.emplace<Book>("B1", "dup", std::vector<std::string>{}, "", "", 1,
                                 Money::of(1)),
                 DuplicateIdError);

    CHECK(c.searchTitle("clean").size() == 2);
    CHECK(c.byCategory(ResourceCategory::ElectronicResource).size() == 1);
    CHECK(c.where([](const Resource& r) { return r.isDigital(); }).size() == 1);

    CHECK(c.holdings("B1") == 0);
    c.addHoldings("B1", 3);
    CHECK(c.holdings("B1") == 3);
    CHECK_THROWS(c.addHoldings("B1", -5), std::invalid_argument);

    c.remove("R1");
    CHECK(c.size() == 2);
    CHECK_THROWS(c.remove("R1"), NotFoundError);
}

static void testBudget() {
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Book, {5, Money::of(400)});

    CHECK(b.check(ResourceCategory::Book, 2, Money::of(200)).empty());
    CHECK(!b.check(ResourceCategory::Book, 6, Money::of(10)).empty());   // units
    CHECK(!b.check(ResourceCategory::Book, 1, Money::of(401)).empty());  // spend
    CHECK(!b.check(ResourceCategory::ElectronicResource, 1, Money::of(1001)).empty());  // overall
    CHECK(b.check(ResourceCategory::ElectronicResource, 1, Money::of(900)).empty());    // no quota

    b.commit(ResourceCategory::Book, 4, Money::of(300));
    CHECK(b.spent() == Money::of(300));
    CHECK(*b.unitsRemaining(ResourceCategory::Book) == 1);
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(100));
    CHECK(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());

    CHECK_THROWS(b.commit(ResourceCategory::Book, 2, Money::of(10)), QuotaExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 1, Money::of(800)),
                 BudgetExceededError);
    CHECK_THROWS(b.commit(ResourceCategory::ElectronicResource, 0, Money::of(1)),
                 std::invalid_argument);
    CHECK(b.spent() == Money::of(300));  // failed commits changed nothing
}

static void testAcquisition() {
    Catalog c;
    c.emplace<Book>("B1", "Book", std::vector<std::string>{"A"}, "i", "P", 2020,
                    Money::of(100));
    c.emplace<ElectronicResource>("R1", "DB", "P", 2026, Money::of(10), "url",
                                  LicenseModel::AnnualSubscription, Money::of(50));
    Budget b(Money::of(500));
    b.setQuota(ResourceCategory::Book, {3, Money::of(1000)});
    AcquisitionManager acq(c, b);

    CHECK(acq.quote("R1", 5) == Money::of(100));
    std::string why;
    CHECK(acq.canPurchase("B1", 3, &why) && why.empty());
    CHECK(!acq.canPurchase("B1", 4, &why) && !why.empty());
    CHECK(!acq.canPurchase("nope", 1, &why));

    const auto& rec = acq.purchase("B1", 2);
    CHECK(rec.approved && rec.cost == Money::of(200) && rec.orderNo == 1);
    CHECK(c.holdings("B1") == 2);

    CHECK_THROWS(acq.purchase("B1", 2), QuotaExceededError);
    CHECK_THROWS(acq.purchase("nope", 1), NotFoundError);
    CHECK(acq.history().size() == 1);  // exceptions don't record

    auto res = acq.processBatch({{"R1", 10}, {"R1", 100}, {"B1", 1}, {"zzz", 1}, {"B1", 0}});
    CHECK(res.size() == 5);
    CHECK(res[0].approved && res[0].cost == Money::of(150));
    CHECK(!res[1].approved);  // 1050 > remaining 150
    CHECK(res[2].approved);
    CHECK(!res[3].approved && res[3].reason.find("not found") != std::string::npos);
    CHECK(!res[4].approved);
    CHECK(acq.totalSpent() == Money::of(450));
    CHECK(b.spent() == acq.totalSpent());
    CHECK(c.holdings("R1") == 10 && c.holdings("B1") == 3);
    CHECK(acq.history().size() == 6);
}

//checks condition for journal
static void testJournal() {
    Journal j("J1", "Nature", "1234-5678", "Springer", 2024, Money::of(100), 12);
    CHECK(j.category()== ResourceCategory::Journal);
    CHECK(!j.isDigital());
    CHECK(j.issn()=="1234-5678");
    CHECK(j.issuesPerYear()==12);
    CHECK(j.subscriptionYears()==1); 

    CHECK(j.costFor(2) == Money::of(200));
    Journal j3("J2", "Science", "8765-4321", "AAAS", 2024, Money::of(100), 52, 3);
    CHECK(j3.subscriptionYears() == 3);
    CHECK(j3.costFor(2) == Money::of(600));  // 100 * 2 copies * 3 years

    CHECK_THROWS(j.costFor(0), std::invalid_argument);
    CHECK_THROWS(j.costFor(-1), std::invalid_argument);
    CHECK_THROWS(Journal("J3", "T", "i", "P", 2024, Money::of(1), 4, 0), std::invalid_argument);
    CHECK_THROWS(Journal("J3", "T", "i", "P", 2024, Money::of(1), 4, -2), std::invalid_argument);

    // Polymorphism and printing through the base class
    const Resource& r = j3;
    CHECK(r.costFor(1) == Money::of(300));
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("issn: 8765-4321") != std::string::npos);
    CHECK(os.str().find("subscription years: 3") != std::string::npos);

    // Journal quota in a budget, through the acquisition manager
    Catalog c;
    c.emplace<Journal>("J1", "Nature", "1234-5678", "Springer", 2024, Money::of(100), 12, 2);
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Journal, {3, Money::of(500)});
    AcquisitionManager acq(c, b);

    const auto& rec = acq.purchase("J1", 2);  // 100 * 2 * 2 years = 400
    CHECK(rec.approved && rec.cost == Money::of(400));
    CHECK(c.holdings("J1") == 2);
    CHECK(*b.unitsRemaining(ResourceCategory::Journal) == 1);
    CHECK(*b.spendRemaining(ResourceCategory::Journal) == Money::of(100));
    CHECK_THROWS(acq.purchase("J1", 1), QuotaExceededError);  // 200 > 100 left

    std::ostringstream report;
    b.print(report);
    CHECK(report.str().find("Journal") != std::string::npos);
}

//checks condition for EBook
static void testEBook() {
    EBook e("E1", "Clean Code (e-book)", std::vector<std::string>{"Robert C. Martin"},
            "978-0132350884", "Prentice Hall", 2008, Money::of(10),
            "https://ebooks.example/e1", EBookFormat::EPUB, true,
            LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(e.category() == ResourceCategory::EBook);
    CHECK(e.isDigital());
    CHECK(e.isbn() == "978-0132350884");
    CHECK(e.authors().size() == 1 && e.authors()[0] == "Robert C. Martin");
    CHECK(e.format() == EBookFormat::EPUB);
    CHECK(e.drmProtected());

    // Defaults: PDF, no DRM
    EBook d("E2", "Plain", std::vector<std::string>{"A"}, "isbn", "P", 2020,
            Money::of(10), "https://ebooks.example/e2");
    CHECK(d.format() == EBookFormat::PDF);
    CHECK(!d.drmProtected());

    // Pricing is inherited unchanged: platform fee + unit price * seats
    CHECK(e.costFor(5) == Money::of(150));
    CHECK_THROWS(e.costFor(0), std::invalid_argument);

    // Parent's printDetails runs first, then the new fields
    const Resource& r = e;
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("platform fee: 100.00") != std::string::npos);
    CHECK(os.str().find("isbn: 978-0132350884") != std::string::npos);
    CHECK(os.str().find("format: EPUB") != std::string::npos);
    CHECK(os.str().find("drm protected: yes") != std::string::npos);
    CHECK(os.str().find("access url") < os.str().find("format: EPUB"));

    // E-books have their own quota, separate from other electronic resources
    Catalog c;
    c.emplace<EBook>("E1", "Clean Code (e-book)", std::vector<std::string>{"Robert C. Martin"},
                     "978-0132350884", "Prentice Hall", 2008, Money::of(10),
                     "https://ebooks.example/e1", EBookFormat::EPUB, true,
                     LicenseModel::AnnualSubscription, Money::of(100));
    Budget b(Money::of(10000));
    b.setQuota(ResourceCategory::EBook, {10, Money::of(500)});
    AcquisitionManager acq(c, b);

    const auto& rec = acq.purchase("E1", 5);  // 100 + 10 * 5 = 150
    CHECK(rec.approved && rec.cost == Money::of(150));
    CHECK(*b.unitsRemaining(ResourceCategory::EBook) == 5);
    CHECK(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());
    CHECK_THROWS(acq.purchase("E1", 6), QuotaExceededError);  // 6 > 5 units left
}

static void testAudioBook() {
    AudioBook a("A1", "Clean Code (audio)", std::vector<std::string>{"Robert C. Martin"},
                "Narrator One", "Prentice Hall", 2020, Money::of(10),
                "https://audio.example/a1", 480, LicenseModel::AnnualSubscription,
                Money::of(100));
    CHECK(a.category() == ResourceCategory::AudioBook);
    CHECK(a.isDigital());
    CHECK(a.authors().size() == 1 && a.authors()[0] == "Robert C. Martin");
    CHECK(a.narrator() == "Narrator One");
    CHECK(a.durationMinutes() == 480);

    // Pricing is inherited unchanged: platform fee + unit price * seats
    CHECK(a.costFor(5) == Money::of(150));
    CHECK_THROWS(a.costFor(0), std::invalid_argument);

    CHECK_THROWS(AudioBook("A2", "T", std::vector<std::string>{"A"}, "N", "P", 2020,
                           Money::of(1), "u", 0),
                 std::invalid_argument);
    CHECK_THROWS(AudioBook("A2", "T", std::vector<std::string>{"A"}, "N", "P", 2020,
                           Money::of(1), "u", -5),
                 std::invalid_argument);

    // Parent's lines first, then the new ones
    const Resource& r = a;
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("platform fee: 100.00") != std::string::npos);
    CHECK(os.str().find("narrator: Narrator One") != std::string::npos);
    CHECK(os.str().find("duration (minutes): 480") != std::string::npos);
    CHECK(os.str().find("access url") < os.str().find("narrator:"));

    // Own quota, separate from other electronic resources
    Catalog c;
    c.emplace<AudioBook>("A1", "Clean Code (audio)", std::vector<std::string>{"Robert C. Martin"},
                         "Narrator One", "Prentice Hall", 2020, Money::of(10),
                         "https://audio.example/a1", 480, LicenseModel::AnnualSubscription,
                         Money::of(100));
    Budget b(Money::of(10000));
    b.setQuota(ResourceCategory::AudioBook, {10, Money::of(500)});
    AcquisitionManager acq(c, b);
    const auto& rec = acq.purchase("A1", 5);  // 100 + 10 * 5 = 150
    CHECK(rec.approved && rec.cost == Money::of(150));
    CHECK(*b.unitsRemaining(ResourceCategory::AudioBook) == 5);
    CHECK(!b.unitsRemaining(ResourceCategory::ElectronicResource).has_value());
    CHECK_THROWS(acq.purchase("A1", 6), QuotaExceededError);
}

static void testThesis() {
    Thesis t("T1", "Graph Algorithms", "IIT Delhi", "PhD", "Prof. Rao", 2023);
    CHECK(t.category() == ResourceCategory::Thesis);
    CHECK(!t.isDigital());
    CHECK(t.university() == "IIT Delhi");
    CHECK(t.university() == t.publisher());
    CHECK(t.degree() == "PhD");
    CHECK(t.supervisor() == "Prof. Rao");

    // Free by default; inherited costFor is unitPrice * quantity
    CHECK(t.unitPrice() == Money{});
    CHECK(t.costFor(1) == Money{});
    CHECK(t.costFor(5) == Money{});
    CHECK_THROWS(t.costFor(0), std::invalid_argument);

    // A priced thesis (e.g. a bound copy) still works
    Thesis p("T2", "Compilers", "IISc", "MTech", "Prof. Iyer", 2022, Money::of(50));
    CHECK(p.costFor(2) == Money::of(100));

    const Resource& r = t;
    std::ostringstream os;
    os << r;
    CHECK(os.str().find("university: IIT Delhi") != std::string::npos);
    CHECK(os.str().find("degree: PhD") != std::string::npos);
    CHECK(os.str().find("supervisor: Prof. Rao") != std::string::npos);

    // Buying free theses uses units but no money
    Catalog c;
    c.emplace<Thesis>("T1", "Graph Algorithms", "IIT Delhi", "PhD", "Prof. Rao", 2023);
    Budget b(Money::of(1000));
    b.setQuota(ResourceCategory::Thesis, {5, Money::of(500)});
    AcquisitionManager acq(c, b);
    const auto& rec = acq.purchase("T1", 2);
    CHECK(rec.approved && rec.cost == Money{});
    CHECK(*b.unitsRemaining(ResourceCategory::Thesis) == 3);
    CHECK(*b.spendRemaining(ResourceCategory::Thesis) == Money::of(500));
    CHECK_THROWS(acq.purchase("T1", 4), QuotaExceededError);  // 4 > 3 units left
}

static void testBookHardcover() {
    const std::vector<std::string> au{"A"};
    Book paper("P1", "Paper", au, "isbn", "Pub", 2020, Money::of(1200));
    Book hard("H1", "Hard", au, "isbn", "Pub", 2020, Money::of(1200), 1, Binding::Hardcover);

    // Default binding is paperback: no surcharge
    CHECK(paper.binding() == Binding::Paperback);
    CHECK(paper.costFor(1) == Money::of(1200));
    CHECK(paper.costFor(5) == Money::of(6000));

    // Hardcover: 20% more per copy
    CHECK(hard.costFor(1) == Money::of(1440));
    CHECK(hard.costFor(5) == Money::of(7200));
    CHECK(hard.unitPrice() == Money::of(1200));  // listed price itself is unchanged

    // Rounding: surcharge applied to the unit price, rounded half up
    Book tiny("H2", "Tiny", au, "isbn", "Pub", 2020, Money::fromMinor(3), 1, Binding::Hardcover);
    CHECK(tiny.costFor(1) == Money::fromMinor(4));    // 3 * 1.2 = 3.6 -> 4
    CHECK(tiny.costFor(9) == Money::fromMinor(36));  // per copy first: 4 * 10
    Book odd("H3", "Odd", au, "isbn", "Pub", 2020, Money::fromMinor(1), 1, Binding::Hardcover);
    CHECK(odd.costFor(1) == Money::fromMinor(1));     // 1 * 1.2 = 1.2 -> 1
    Book half("H4", "Half", au, "isbn", "Pub", 2020, Money::fromMinor(25), 1, Binding::Hardcover);
    CHECK(half.costFor(1) == Money::fromMinor(30));   // exact: 25 * 1.2 = 30

    CHECK_THROWS(hard.costFor(0), std::invalid_argument);
    CHECK_THROWS(hard.costFor(-1), std::invalid_argument);

    // Through the base class (virtual dispatch)
    const Resource& r = hard;
    CHECK(r.costFor(2) == Money::of(2880));

    // Quotas and budget use the surcharged cost
    Catalog c;
    c.emplace<Book>("H1", "Hard", au, "isbn", "Pub", 2020, Money::of(1200), 1, Binding::Hardcover);
    Budget b(Money::of(10000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(3000)});
    AcquisitionManager acq(c, b);
    CHECK(acq.quote("H1", 2) == Money::of(2880));
    CHECK(acq.canPurchase("H1", 2));
    CHECK(!acq.canPurchase("H1", 3));  // 3 * 1440 = 4320 > 3000 spend quota
    const auto& rec = acq.purchase("H1", 2);
    CHECK(rec.cost == Money::of(2880));
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(120));
}

static void testBulkDiscounts() {
    const std::vector<std::string> au{"A"};

    // Print items: 10% off the total from 10 copies in a single purchase
    Book b("B1", "Book", au, "isbn", "Pub", 2020, Money::of(450));
    CHECK(b.costFor(9) == Money::of(4050));    // no discount below 10
    CHECK(b.costFor(10) == Money::of(4050));   // 4500 - 10%
    CHECK(b.costFor(20) == Money::of(8100));   // 9000 - 10%
    CHECK(b.costFor(11) == Money::of(4455));   // 4950 - 10%
    Book tiny("B2", "Tiny", au, "isbn", "Pub", 2020, Money::fromMinor(1));
    CHECK(tiny.costFor(9) == Money::fromMinor(9));
    CHECK(tiny.costFor(11) == Money::fromMinor(10));  // 11 * 0.9 = 9.9 -> 10 (half up)

    // Surcharge first (Q4), then discount on the total
    Book hard("B3", "Hard", au, "isbn", "Pub", 2020, Money::of(1200), 1, Binding::Hardcover);
    CHECK(hard.costFor(9) == Money::of(12960));    // 9 * 1440
    CHECK(hard.costFor(10) == Money::of(12960));   // 14400 - 10%

    // Journals: discount depends on copies (not years) and applies to the whole subscription
    Journal j("J1", "Nature", "1234-5678", "Springer", 2024, Money::of(100), 12, 3);
    CHECK(j.costFor(9) == Money::of(2700));
    CHECK(j.costFor(10) == Money::of(2700));       // 3000 - 10%
    Journal j1("J2", "Science", "8765-4321", "AAAS", 2024, Money::of(100), 52);
    CHECK(j1.costFor(10) == Money::of(900));       // 1000 - 10%

    // Theses are print items too (free ones stay free)
    Thesis free("T1", "T", "IITD", "PhD", "Sup", 2023);
    CHECK(free.costFor(10) == Money{});
    Thesis priced("T2", "T", "IITD", "PhD", "Sup", 2023, Money::of(50));
    CHECK(priced.costFor(9) == Money::of(450));
    CHECK(priced.costFor(10) == Money::of(450));   // 500 - 10%

    // Electronic resources: every seat beyond the 50th is half price; fee is not discounted
    ElectronicResource e("R1", "DB", "Pub", 2026, Money::of(150), "url",
                         LicenseModel::AnnualSubscription, Money::of(2000));
    CHECK(e.costFor(49) == Money::of(9350));       // 2000 + 49 * 150
    CHECK(e.costFor(50) == Money::of(9500));       // 2000 + 50 * 150
    CHECK(e.costFor(51) == Money::of(9575));       // + 1 seat at 75
    CHECK(e.costFor(60) == Money::of(10250));      // + 10 seats at 75
    CHECK(e.costFor(100) == Money::of(13250));     // 9500 + 50 * 75
    ElectronicResource r1("R2", "DB", "Pub", 2026, Money::fromMinor(3), "url");
    CHECK(r1.costFor(51) == Money::fromMinor(152));  // 150 + (1 * 3 * 0.5 = 1.5 -> 2)
    ElectronicResource r2("R3", "DB", "Pub", 2026, Money::fromMinor(1), "url");
    CHECK(r2.costFor(51) == Money::fromMinor(51));   // 50 + (0.5 -> 1)

    // Derived electronic classes inherit the tiered pricing automatically
    EBook eb("E1", "E", au, "isbn", "Pub", 2020, Money::of(10), "url",
             EBookFormat::PDF, false, LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(eb.costFor(60) == Money::of(650));       // 100 + 50 * 10 + 10 * 5
    AudioBook ab("A1", "A", au, "N", "Pub", 2020, Money::of(10), "url", 60,
                 LicenseModel::AnnualSubscription, Money::of(100));
    CHECK(ab.costFor(60) == Money::of(650));

    // Quotes, quotas and purchases use the discounted cost
    Catalog c;
    c.emplace<Book>("B1", "Book", au, "isbn", "Pub", 2020, Money::of(450));
    Budget bud(Money::of(10000));
    bud.setQuota(ResourceCategory::Book, {10, Money::of(4100)});
    AcquisitionManager acq(c, bud);
    CHECK(acq.quote("B1", 10) == Money::of(4050));
    CHECK(acq.canPurchase("B1", 10));              // 4050 <= 4100 (4500 would not fit)
    const auto& rec = acq.purchase("B1", 10);
    CHECK(rec.cost == Money::of(4050));
}


static void testTaxes() {
    const std::vector<std::string> au{"A"};

    // TaxPolicy on its own: electronic items use the electronic rate, the rest the print rate
    TaxPolicy policy(500, 1800);  // 5% print, 18% electronic
    CHECK(policy.printRate() == 500 && policy.electronicRate() == 1800);
    Book book("B1", "Book", au, "isbn", "Pub", 2020, Money::of(450));
    ElectronicResource db("R1", "DB", "Pub", 2026, Money::of(150), "url",
                          LicenseModel::AnnualSubscription, Money::of(2000));
    EBook ebook("E1", "E", au, "isbn", "Pub", 2020, Money::of(10), "url");
    Thesis thesis("T1", "T", "IITD", "PhD", "Sup", 2023, Money::of(50));
    CHECK(policy.rateFor(book) == 500);
    CHECK(policy.rateFor(thesis) == 500);
    CHECK(policy.rateFor(db) == 1800);
    CHECK(policy.rateFor(ebook) == 1800);  // derived electronic classes count as electronic
    CHECK(policy.apply(book, Money::of(1800)).tax == Money::of(90));
    CHECK(policy.apply(book, Money::of(1800)).total() == Money::of(1890));
    CHECK(TaxPolicy().apply(book, Money::of(1800)).tax == Money{});  // default: no tax
    CHECK(policy.apply(book, Money::fromMinor(10)).tax == Money::fromMinor(1));  // 0.5 -> 1 (half up)
    CHECK(policy.apply(book, Money::fromMinor(9)).tax == Money::fromMinor(0));   // 0.45 -> 0
    CHECK(TaxPolicy(550, 0).apply(book, Money::of(1000)).tax == Money::of(55));  // 5.5%
    CHECK_THROWS(TaxPolicy(-1, 0), std::invalid_argument);
    CHECK_THROWS(TaxPolicy().setElectronicRate(-5), std::invalid_argument);

    // Acquisition: quotes, records and reports
    Catalog c;
    c.emplace<Book>("B1", "Book", au, "isbn", "Pub", 2020, Money::of(450));
    c.emplace<ElectronicResource>("R1", "DB", "Pub", 2026, Money::of(150), "url",
                                  LicenseModel::AnnualSubscription, Money::of(2000));
    Budget b(Money::of(100000));
    AcquisitionManager acq(c, b);

    // No policy set: nothing changes
    CHECK(acq.quote("B1", 4) == Money::of(1800));
    CHECK(acq.quotePreTax("B1", 4) == Money::of(1800));

    acq.setTaxPolicy(policy);
    CHECK(acq.quote("B1", 4) == Money::of(1890));        // 1800 + 5%
    CHECK(acq.quotePreTax("B1", 4) == Money::of(1800));
    CHECK(acq.quote("R1", 20) == Money::of(5900));       // 5000 + 18%
    CHECK(acq.quote("B1", 10) == Money::fromMinor(425250));  // discount first: 4050, then +5%

    const auto& r1 = acq.purchase("B1", 4);
    CHECK(r1.cost == Money::of(1890));
    CHECK(r1.preTaxCost == Money::of(1800) && r1.taxAmount == Money::of(90));
    const auto& r2 = acq.purchase("R1", 20);
    CHECK(r2.cost == Money::of(5900));
    CHECK(r2.preTaxCost == Money::of(5000) && r2.taxAmount == Money::of(900));

    CHECK(acq.totalPreTax() == Money::of(6800));
    CHECK(acq.totalTax() == Money::of(990));
    CHECK(acq.totalSpent() == Money::of(7790));
    CHECK(b.spent() == Money::of(7790));                 // the budget is charged post-tax

    std::ostringstream report;
    acq.printReport(report);
    CHECK(report.str().find("Total pre-tax: 6800.00") != std::string::npos);
    CHECK(report.str().find("Total tax: 990.00") != std::string::npos);
    CHECK(report.str().find("Total spent: 7790.00") != std::string::npos);
    CHECK(report.str().find("pre-tax 1800.00 + tax 90.00") != std::string::npos);

    // Quotas are checked against the post-tax cost
    {
        Catalog c2;
        c2.emplace<Book>("B1", "Book", au, "isbn", "Pub", 2020, Money::of(450));
        Budget b2(Money::of(100000));
        b2.setQuota(ResourceCategory::Book, {10, Money::of(1850)});
        AcquisitionManager acq2(c2, b2);
        acq2.setTaxPolicy(policy);
        std::string why;
        CHECK(!acq2.canPurchase("B1", 4, &why));         // pre-tax 1800 fits, post-tax 1890 does not
        CHECK(why.find("spend quota exceeded") != std::string::npos);
        CHECK_THROWS(acq2.purchase("B1", 4), QuotaExceededError);
        CHECK(acq2.history().empty());                   // a failed purchase leaves no record
        const auto res = acq2.processBatch({{"B1", 4}, {"B1", 3}});
        CHECK(!res[0].approved);
        CHECK(res[0].cost == Money::of(1890) && res[0].preTaxCost == Money::of(1800));
        CHECK(res[1].approved && res[1].cost == Money::fromMinor(141750));  // 1350 + 5%
        CHECK(acq2.totalTax() == Money::fromMinor(6750));  // rejected orders pay no tax
    }

    // The overall budget is checked post-tax too
    {
        Catalog c3;
        c3.emplace<Book>("B1", "Book", au, "isbn", "Pub", 2020, Money::of(450));
        Budget b3(Money::of(1850));
        AcquisitionManager acq3(c3, b3);
        acq3.setTaxPolicy(policy);
        CHECK_THROWS(acq3.purchase("B1", 4), BudgetExceededError);
        acq3.setTaxPolicy(TaxPolicy());                  // without tax the same order fits
        CHECK(acq3.purchase("B1", 4).cost == Money::of(1800));
    }
}

static void testTitleQuota() {
    const std::vector<std::string> au{"A"};
    Catalog c;
    c.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(100));
    c.emplace<Book>("B2", "Two", au, "i2", "Pub", 2020, Money::of(100));
    c.emplace<Book>("B3", "Three", au, "i3", "Pub", 2020, Money::of(100));
    c.emplace<Journal>("J1", "Nature", "1234-5678", "Springer", 2024, Money::of(100), 12);
    Budget b(Money::of(100000));
    b.setQuota(ResourceCategory::Book, {100, Money::of(50000), 2});  // at most 2 different titles
    AcquisitionManager acq(c, b);

    // Quotas without a title limit behave as before
    CHECK(!b.quotaFor(ResourceCategory::Journal).has_value());
    CHECK(!b.titlesRemaining(ResourceCategory::Journal).has_value());
    CHECK(*b.titlesRemaining(ResourceCategory::Book) == 2);

    acq.purchase("B1", 3);
    acq.purchase("B2", 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles() == 2);
    CHECK(*b.titlesRemaining(ResourceCategory::Book) == 0);

    // More copies of a title already bought is not a new title
    CHECK(acq.canPurchase("B1", 5));
    acq.purchase("B1", 5);
    CHECK(b.usageFor(ResourceCategory::Book).titles() == 2);
    CHECK(c.holdings("B1") == 8);

    // A third different title is rejected, and nothing changes
    std::string why;
    CHECK(!acq.canPurchase("B3", 1, &why));
    CHECK(why.find("title quota exceeded") != std::string::npos);
    CHECK_THROWS(acq.purchase("B3", 1), QuotaExceededError);
    CHECK(c.holdings("B3") == 0);
    CHECK(b.usageFor(ResourceCategory::Book).titles() == 2);
    CHECK(acq.history().size() == 3);  // the failed purchase left no record

    // The limit is per category: a journal is a different category
    CHECK(acq.canPurchase("J1", 1));
    acq.purchase("J1", 1);

    // In a batch, the rejected request is recorded with the reason
    const auto res = acq.processBatch({{"B3", 1}, {"B2", 1}});
    CHECK(!res[0].approved && res[0].reason.find("title quota exceeded") != std::string::npos);
    CHECK(res[1].approved);

    // Unit and spend quotas are checked before the title limit
    Catalog c2;
    c2.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(100));
    c2.emplace<Book>("B2", "Two", au, "i2", "Pub", 2020, Money::of(100));
    Budget b2(Money::of(100000));
    b2.setQuota(ResourceCategory::Book, {1, Money::of(50000), 1});
    AcquisitionManager acq2(c2, b2);
    acq2.purchase("B1", 1);
    CHECK(!acq2.canPurchase("B2", 5, &why));  // 5 units > 0 left, and a new title as well
    CHECK(why.find("unit quota exceeded") != std::string::npos);

    // A limit of zero titles means nothing can be bought in that category
    Budget b3(Money::of(1000));
    b3.setQuota(ResourceCategory::Book, {10, Money::of(1000), 0});
    AcquisitionManager acq3(c2, b3);
    CHECK_THROWS(acq3.purchase("B1", 1), QuotaExceededError);

    // Negative title limits are invalid
    CHECK_THROWS(b3.setQuota(ResourceCategory::Book, {10, Money::of(1000), -1}),
                 std::invalid_argument);

    // The budget report shows used/max titles
    std::ostringstream os;
    b.print(os);
    CHECK(os.str().find("Titles used/max") != std::string::npos);
    CHECK(os.str().find("2/2") != std::string::npos);

    // Direct use of the old Budget API cannot know the title, so it does not count titles
    Budget b4(Money::of(1000));
    b4.setQuota(ResourceCategory::Book, {10, Money::of(1000), 1});
    b4.commit(ResourceCategory::Book, 1, Money::of(10));
    b4.commit(ResourceCategory::Book, 1, Money::of(10));
    CHECK(b4.usageFor(ResourceCategory::Book).titles() == 0);
    // ...but the overload that takes a title does
    b4.commit(ResourceCategory::Book, "X", 1, Money::of(10));
    CHECK_THROWS(b4.commit(ResourceCategory::Book, "Y", 1, Money::of(10)), QuotaExceededError);
}

static void testCancellation() {
    const std::vector<std::string> au{"A"};
    Catalog c;
    c.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(450));
    c.emplace<Book>("B2", "Two", au, "i2", "Pub", 2020, Money::of(450));
    c.emplace<Book>("B3", "Three", au, "i3", "Pub", 2020, Money::of(450));
    c.emplace<ElectronicResource>("R1", "DB", "Pub", 2026, Money::of(150), "url",
                                  LicenseModel::AnnualSubscription, Money::of(2000));
    Budget b(Money::of(100000));
    b.setQuota(ResourceCategory::Book, {10, Money::of(5000), 2});
    AcquisitionManager acq(c, b);
    acq.setTaxPolicy(TaxPolicy(500, 1800));

    // Buy, then cancel: everything goes back
    const int order = acq.purchase("B1", 4).orderNo;  // 1800 + 5% = 1890
    CHECK(order == 1);
    CHECK(b.spent() == Money::of(1890));
    CHECK(c.holdings("B1") == 4);
    CHECK(!acq.isCancelled(order));

    const PurchaseRecord cancelRec = acq.cancel(order);
    CHECK(cancelRec.isCancellation && cancelRec.cancelsOrderNo == order);
    CHECK(cancelRec.approved && cancelRec.orderNo == 2 && cancelRec.quantity == 4);
    CHECK(cancelRec.cost == Money::of(-1890));
    CHECK(cancelRec.preTaxCost == Money::of(-1800) && cancelRec.taxAmount == Money::of(-90));
    CHECK(acq.isCancelled(order));
    CHECK(!acq.isCancelled(cancelRec.orderNo));

    CHECK(b.spent() == Money{});                              // overall budget refunded
    CHECK(*b.unitsRemaining(ResourceCategory::Book) == 10);   // unit quota refunded
    CHECK(*b.spendRemaining(ResourceCategory::Book) == Money::of(5000));
    CHECK(*b.titlesRemaining(ResourceCategory::Book) == 2);   // title slot freed
    CHECK(c.holdings("B1") == 0);                             // holdings reduced

    // The original record stays in the history, unchanged, plus the new record
    CHECK(acq.history().size() == 2);
    CHECK(!acq.history()[0].isCancellation && acq.history()[0].approved);
    CHECK(acq.history()[0].cost == Money::of(1890) && acq.history()[0].quantity == 4);
    CHECK(acq.history()[1].isCancellation);
    CHECK(acq.findOrder(1) != nullptr && acq.findOrder(2) != nullptr && acq.findOrder(3) == nullptr);

    // Totals are net of cancellations
    CHECK(acq.totalSpent() == Money{} && acq.totalPreTax() == Money{} && acq.totalTax() == Money{});

    // Orders that cannot be cancelled
    CHECK_THROWS(acq.cancel(order), CancellationError);      // already cancelled
    CHECK_THROWS(acq.cancel(2), CancellationError);          // a cancellation record
    CHECK_THROWS(acq.cancel(99), CancellationError);         // no such order
    CHECK_THROWS(acq.cancel(0), CancellationError);
    CHECK(acq.history().size() == 2);                        // failed cancels add nothing
    CHECK(b.spent() == Money{});

    // A rejected order cannot be cancelled
    acq.purchase("B1", 1);
    acq.purchase("B2", 1);
    const auto res = acq.processBatch({{"B3", 1}});          // third title: rejected
    CHECK(!res[0].approved);
    CHECK_THROWS(acq.cancel(res[0].orderNo), CancellationError);

    // Cancelling frees the title slot, so the rejected title can now be bought
    CHECK(!acq.canPurchase("B3", 1));
    acq.cancel(3);                                           // order #3 was B1 x1
    CHECK(acq.canPurchase("B3", 1));
    acq.purchase("B3", 1);
    CHECK(b.usageFor(ResourceCategory::Book).titles() == 2);

    // Partial: cancelling one of two orders for the same title keeps the other
    Catalog c2;
    c2.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(450));
    Budget b2(Money::of(100000));
    b2.setQuota(ResourceCategory::Book, {10, Money::of(5000), 1});
    AcquisitionManager acq2(c2, b2);
    acq2.setTaxPolicy(TaxPolicy(500, 1800));
    const int first = acq2.purchase("B1", 4).orderNo;
    acq2.purchase("B1", 2);                                  // 900 + 5% = 945
    acq2.cancel(first);
    CHECK(c2.holdings("B1") == 2);
    CHECK(b2.usageFor(ResourceCategory::Book).titles() == 1);  // still a bought title
    CHECK(b2.spent() == Money::of(945));
    CHECK(*b2.unitsRemaining(ResourceCategory::Book) == 8);

    // The refund uses the amounts recorded at purchase time, not today's tax rate
    Catalog c3;
    c3.emplace<ElectronicResource>("R1", "DB", "Pub", 2026, Money::of(150), "url",
                                   LicenseModel::AnnualSubscription, Money::of(2000));
    Budget b3(Money::of(100000));
    b3.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(10000)});
    AcquisitionManager acq3(c3, b3);
    acq3.setTaxPolicy(TaxPolicy(500, 1800));
    const int e = acq3.purchase("R1", 20).orderNo;           // 5000 + 18% = 5900
    CHECK(b3.spent() == Money::of(5900));
    acq3.setTaxPolicy(TaxPolicy());                          // tax rates change afterwards
    const PurchaseRecord& er = acq3.cancel(e);
    CHECK(er.cost == Money::of(-5900));
    CHECK(b3.spent() == Money{});
    CHECK(*b3.unitsRemaining(ResourceCategory::ElectronicResource) == 40);
    CHECK(c3.holdings("R1") == 0);

    // After a cancellation the freed quota can be used again
    acq3.setTaxPolicy(TaxPolicy(500, 1800));
    CHECK(acq3.canPurchase("R1", 40));                       // 2000 + 40*150 = 8000 + 18%
    acq3.purchase("R1", 20);
    CHECK(b3.spent() == Money::of(5900));

    // The report shows both records and the net totals
    std::ostringstream os;
    acq.printReport(os);
    CHECK(os.str().find("CANCELLED") != std::string::npos);
    CHECK(os.str().find("(cancels order #1)") != std::string::npos);
    CHECK(os.str().find("[cancelled]") != std::string::npos);
    CHECK(os.str().find("-1890.00") != std::string::npos);

    // Units that are no longer held cannot be cancelled, and nothing changes
    Catalog c4;
    c4.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(100));
    Budget b4(Money::of(1000));
    AcquisitionManager acq4(c4, b4);
    acq4.purchase("B1", 3);
    c4.addHoldings("B1", -2);                                // 2 copies written off elsewhere
    CHECK_THROWS(acq4.cancel(1), CancellationError);
    CHECK(b4.spent() == Money::of(300) && c4.holdings("B1") == 1 && acq4.history().size() == 1);

    // Budget::refund directly: cannot refund more than was spent; nothing changes on failure
    Budget b5(Money::of(1000));
    b5.commit(ResourceCategory::Book, "X", 2, Money::of(100));
    CHECK_THROWS(b5.refund(ResourceCategory::Book, "X", 3, Money::of(100)), std::invalid_argument);
    CHECK_THROWS(b5.refund(ResourceCategory::Book, "X", 2, Money::of(101)), std::invalid_argument);
    CHECK_THROWS(b5.refund(ResourceCategory::Book, "X", 0, Money::of(0)), std::invalid_argument);
    CHECK_THROWS(b5.refund(ResourceCategory::Journal, "X", 1, Money::of(1)), std::invalid_argument);
    CHECK(b5.spent() == Money::of(100) && b5.usageFor(ResourceCategory::Book).units == 2);
    b5.refund(ResourceCategory::Book, "X", 2, Money::of(100));
    CHECK(b5.spent() == Money{} && b5.usageFor(ResourceCategory::Book).titles() == 0);
}

static void testDepartments() {
    const std::vector<std::string> au{"A"};
    Catalog c;
    c.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(450));
    c.emplace<Book>("B2", "Two", au, "i2", "Pub", 2020, Money::of(450));
    c.emplace<ElectronicResource>("R1", "DB", "Pub", 2026, Money::of(150), "url",
                                  LicenseModel::AnnualSubscription, Money::of(2000));
    Budget library(Money::of(100000));  // the default budget
    Budget cs(Money::of(5000));
    cs.setQuota(ResourceCategory::Book, {10, Money::of(3000), 1});  // 1 title only
    Budget physics(Money::of(2000));
    AcquisitionManager acq(c, library);

    // Registering departments
    CHECK(acq.departmentNames().empty());
    acq.addDepartment("Physics", physics);
    acq.addDepartment("Computer Science", cs);
    CHECK(acq.hasDepartment("Physics") && acq.hasDepartment("Computer Science"));
    CHECK(!acq.hasDepartment("Chemistry"));
    CHECK(acq.departmentNames() == (std::vector<std::string>{"Computer Science", "Physics"}));
    CHECK(&acq.departmentBudget("Physics") == &physics);
    CHECK_THROWS(acq.addDepartment("Physics", physics), std::invalid_argument);
    CHECK_THROWS(acq.addDepartment("", physics), std::invalid_argument);
    CHECK_THROWS(acq.departmentBudget("Chemistry"), UnknownDepartmentError);

    // A purchase is charged to the named department only
    // (Copy the record: a reference into the history is invalidated by later purchases.)
    const PurchaseRecord r1 = acq.purchase("B1", 4, "Computer Science");  // 1800
    CHECK(r1.approved && r1.department == "Computer Science" && r1.cost == Money::of(1800));
    CHECK(cs.spent() == Money::of(1800));
    CHECK(physics.spent() == Money{} && library.spent() == Money{});
    CHECK(c.holdings("B1") == 4);  // holdings belong to the library, shared by departments

    // Without a department the default budget is charged, as before
    const PurchaseRecord r2 = acq.purchase("B1", 1);
    CHECK(r2.department.empty() && library.spent() == Money::of(450));
    CHECK(cs.spent() == Money::of(1800));

    // Each department has its own quotas: CS may buy only one Book title, Physics has no quota
    std::string why;
    CHECK(!acq.canPurchase("B2", 1, "Computer Science", &why));
    CHECK(why.find("title quota exceeded") != std::string::npos);
    CHECK(acq.canPurchase("B2", 1, "Physics"));
    acq.purchase("B1", 1, "Physics");
    acq.purchase("B2", 1, "Physics");
    CHECK(physics.usageFor(ResourceCategory::Book).titles() == 2);
    CHECK(cs.usageFor(ResourceCategory::Book).titles() == 1);

    // Each department has its own overall budget
    CHECK_THROWS(acq.purchase("R1", 20, "Physics"), BudgetExceededError);   // 5000 > 1100 left
    CHECK(acq.canPurchase("R1", 20));                                         // the library could pay
    CHECK(!acq.canPurchase("R1", 20, "Physics", &why));
    CHECK(why.find("overall budget exceeded") != std::string::npos);

    // Unknown departments
    CHECK_THROWS(acq.purchase("B1", 1, "Chemistry"), UnknownDepartmentError);
    CHECK(!acq.canPurchase("B1", 1, "Chemistry", &why));
    CHECK(why == "unknown department: Chemistry");

    // Batches: the request names the department; rejections are recorded with the reason
    const auto res = acq.processBatch({{"B1", 1, "Physics"},
                                       {"B1", 1, "Chemistry"},
                                       {"R1", 20, "Physics"},
                                       {"B1", 1}});
    CHECK(res[0].approved && res[0].department == "Physics");
    CHECK(!res[1].approved && res[1].reason == "unknown department: Chemistry");
    CHECK(!res[2].approved && res[2].reason.find("overall budget exceeded") != std::string::npos);
    CHECK(res[3].approved && res[3].department.empty());

    // Per-department totals
    CHECK(acq.totalSpent("Computer Science") == Money::of(1800));
    CHECK(acq.totalSpent("Physics") == Money::of(1350));      // 3 copies of B1 + 1 of B2
    CHECK(acq.totalSpent("") == Money::of(900));              // two default-budget copies
    CHECK(acq.totalSpent() == Money::of(4050));               // everything together

    // Cancelling refunds the department that was charged, and only that one
    CHECK(cs.spent() == Money::of(1800));
    const int csOrder = r1.orderNo;
    const PurchaseRecord cancelRec = acq.cancel(csOrder);
    CHECK(cancelRec.department == "Computer Science");
    CHECK(cs.spent() == Money{} && cs.usageFor(ResourceCategory::Book).titles() == 0);
    CHECK(physics.spent() == Money::of(1350));
    CHECK(library.spent() == Money::of(900));
    CHECK(acq.totalSpent("Computer Science") == Money{});

    // Tax and quotas work per department on the post-tax cost
    acq.setTaxPolicy(TaxPolicy(500, 1800));
    CHECK(acq.canPurchase("B1", 4, "Computer Science"));      // 1890 <= 3000 spend quota
    CHECK(!acq.canPurchase("B1", 10, "Computer Science"));    // 4050 + 5% = 4252.50 > 3000
    const PurchaseRecord taxed = acq.purchase("B1", 4, "Computer Science");
    CHECK(taxed.cost == Money::of(1890) && cs.spent() == Money::of(1890));

    // Reports
    std::ostringstream os;
    acq.printReport(os);
    CHECK(os.str().find("(dept: Computer Science)") != std::string::npos);
    CHECK(os.str().find("unknown department: Chemistry") != std::string::npos);
    std::ostringstream depts;
    acq.printDepartmentReport(depts);
    CHECK(depts.str().find("Department Computer Science (net charged: 1890.00)") != std::string::npos);
    CHECK(depts.str().find("Department Physics") != std::string::npos);
    CHECK(depts.str().find("Budget: total 5000.00") != std::string::npos);
}

static void testRollover() {

    // Q10: Create current year's budget
    Budget year1(Money::of(10000));

    // Set a quota for books
    year1.setQuota(ResourceCategory::Book,
                   {10, Money::of(8000)});

    // Spend 6000 from the budget
    year1.commit(ResourceCategory::Book,
                 "B001", 6, Money::of(6000));

    // Remaining budget = 4000
    CHECK(year1.remaining() == Money::of(4000));

    // Test different rollover percentages
    CHECK(year1.carryForward(0) == Money::of(0));
    CHECK(year1.carryForward(50) == Money::of(2000));
    CHECK(year1.carryForward(100) == Money::of(4000));

    // Create next year's budget
    Budget year2 = year1.rollover(50);

    // New budget = 10000 + 2000 = 12000
    CHECK(year2.total() == Money::of(12000));

    // New year's spending starts from zero
    CHECK(year2.spent() == Money::of(0));

    CHECK(year2.remaining() == Money::of(12000));

    // Original budget remains unchanged
    CHECK(year1.total() == Money::of(10000));
    CHECK(year1.spent() == Money::of(6000));

    // Check custom base allocation
    Budget year3 = year1.rollover(50, Money::of(15000));

    CHECK(year3.total() == Money::of(17000));

    // Verify quotas are copied
    auto quota = year2.quotaFor(ResourceCategory::Book);

    CHECK(quota.has_value());
    CHECK(quota->maxUnits == 10);
    CHECK(quota->maxSpend == Money::of(8000));

    // Usage should reset for the new year
    CHECK(year2.usageFor(ResourceCategory::Book).units == 0);

    // Invalid percentage should throw exception
    CHECK_THROWS(year1.rollover(-10), std::invalid_argument);
    CHECK_THROWS(year1.rollover(120), std::invalid_argument);
}

static void testAtomicBatch() {

    // Q11: All-or-nothing batch processing.

    Catalog catalog;

    catalog.emplace<Book>(
        "B1", "Book One",
        std::vector<std::string>{"Author"},
        "ISBN1", "Publisher", 2024,
        Money::of(1000)
    );

    catalog.emplace<Book>(
        "B2", "Book Two",
        std::vector<std::string>{"Author"},
        "ISBN2", "Publisher", 2024,
        Money::of(2000)
    );

    catalog.emplace<Book>(
        "B3", "Book Three",
        std::vector<std::string>{"Author"},
        "ISBN3", "Publisher", 2024,
        Money::of(4000)
    );

    Budget budget(Money::of(5000));

    AcquisitionManager acq(catalog, budget);

    // TEST 1: One request exceeds the remaining budget.
    // The entire batch must be rejected.

    auto failed = acq.processBatch({
        {"B1", 1},
        {"B2", 1},
        {"B3", 1}
    }, true);

    CHECK(failed.size() == 3);

    CHECK(!failed[0].approved);
    CHECK(!failed[1].approved);
    CHECK(!failed[2].approved);

    // No money should be spent.
    CHECK(budget.spent() == Money::of(0));

    // No holdings should be added.
    CHECK(catalog.holdings("B1") == 0);
    CHECK(catalog.holdings("B2") == 0);
    CHECK(catalog.holdings("B3") == 0);

    // TEST 2: Every request fits the budget.
    // The entire batch should succeed.

    auto success = acq.processBatch({
        {"B1", 1},
        {"B2", 1}
    }, true);

    CHECK(success.size() == 2);

    CHECK(success[0].approved);
    CHECK(success[1].approved);

    CHECK(budget.spent() == Money::of(3000));

    CHECK(catalog.holdings("B1") == 1);
    CHECK(catalog.holdings("B2") == 1);

    // TEST 3: Invalid resource rejects the whole batch.

    auto invalid = acq.processBatch({
        {"B1", 1},
        {"UNKNOWN", 1}
    }, true);

    CHECK(!invalid[0].approved);
    CHECK(!invalid[1].approved);

    CHECK(budget.spent() == Money::of(3000));

    // TEST 4: Empty batch.
    auto empty = acq.processBatch({}, true);

    CHECK(empty.empty());

    // TEST 5: Default non-atomic behavior remains unchanged.

    Budget budget2(Money::of(2000));
    AcquisitionManager acq2(catalog, budget2);

    auto normal = acq2.processBatch({
        {"B1", 1},
        {"B2", 1}
    });

    CHECK(normal[0].approved);
    CHECK(!normal[1].approved);

    CHECK(budget2.spent() == Money::of(1000));

    // TEST 6: Requests in the same batch share quota limits.

    Catalog catalog3;

    catalog3.emplace<Book>(
        "B4", "Quota Test",
        std::vector<std::string>{"Author"},
        "ISBN4", "Publisher", 2024,
        Money::of(100)
    );

    Budget budget3(Money::of(10000));

    budget3.setQuota(
        ResourceCategory::Book,
        {5, Money::of(10000)}
    );

    AcquisitionManager acq3(catalog3, budget3);

    auto quotaFailed = acq3.processBatch({
        {"B4", 3},
        {"B4", 3}
    }, true);

    CHECK(!quotaFailed[0].approved);
    CHECK(!quotaFailed[1].approved);

    CHECK(budget3.spent() == Money::of(0));
    CHECK(catalog3.holdings("B4") == 0);
}

// Q12: Vendors and cheapest vendor selection
static void testVendors() {
    Catalog catalog;

    catalog.emplace<Book>(
        "V001", "Vendor Test Book",
        std::vector<std::string>{"Test Author"},
        "ISBN-V001", "Test Publisher", 2024,
        Money::of(500)
    );

    Budget budget(Money::of(10000));
    AcquisitionManager acq(catalog, budget);

    // Register multiple vendors for the same book.
    acq.setVendorPrice("V001", "Amazon", Money::of(450));
    acq.setVendorPrice("V001", "Flipkart", Money::of(420));
    acq.setVendorPrice("V001", "College Bookstore", Money::of(400));

    // Cheapest vendor must be selected.
    CHECK(acq.cheapestVendor("V001") == "College Bookstore");

    // Price for 2 copies = 2 * 400 = 800.
    CHECK(acq.quotePreTax("V001", 2) == Money::of(800));
    CHECK(acq.quote("V001", 2) == Money::of(800));

    // Purchase from cheapest vendor.
    const PurchaseRecord order = acq.purchase("V001", 2);

    CHECK(order.approved);
    CHECK(order.vendor == "College Bookstore");
    CHECK(order.cost == Money::of(800));

    // Existing budget and holdings must update.
    CHECK(budget.spent() == Money::of(800));
    CHECK(catalog.holdings("V001") == 2);

    // Update a vendor's price.
    acq.setVendorPrice("V001", "Amazon", Money::of(350));

    CHECK(acq.cheapestVendor("V001") == "Amazon");
    CHECK(acq.quote("V001", 1) == Money::of(350));

    // Earlier order must retain its original vendor.
    CHECK(acq.history().front().vendor == "College Bookstore");

    // New purchase uses the new cheapest vendor.
    const PurchaseRecord secondOrder = acq.purchase("V001", 1);

    CHECK(secondOrder.vendor == "Amazon");
    CHECK(secondOrder.cost == Money::of(350));
    CHECK(budget.spent() == Money::of(1150));
    CHECK(catalog.holdings("V001") == 3);

    // Check vendor in the printed order history.
    std::ostringstream report;
    acq.printReport(report);

    CHECK(report.str().find("vendor: College Bookstore") != std::string::npos);
    CHECK(report.str().find("vendor: Amazon") != std::string::npos);

    // Cancellation preserves the original vendor.
    const PurchaseRecord cancellation = acq.cancel(order.orderNo);

    CHECK(cancellation.vendor == "College Bookstore");
    CHECK(budget.spent() == Money::of(350));
    CHECK(catalog.holdings("V001") == 1);

    // Invalid vendor name.
    CHECK_THROWS(
        acq.setVendorPrice("V001", "", Money::of(100)),
        std::invalid_argument
    );

    // Negative vendor price.
    CHECK_THROWS(
        acq.setVendorPrice("V001", "Invalid", Money::of(-100)),
        std::invalid_argument
    );

    // Unknown resource.
    CHECK_THROWS(
        acq.setVendorPrice("UNKNOWN", "Amazon", Money::of(100)),
        NotFoundError
    );

    // Resource without vendors uses its original catalog price.
    catalog.emplace<Book>(
        "V002", "Book Without Vendor",
        std::vector<std::string>{"Another Author"},
        "ISBN-V002", "Test Publisher", 2024,
        Money::of(600)
    );

    CHECK(acq.cheapestVendor("V002").empty());
    CHECK(acq.quote("V002", 1) == Money::of(600));

    // Q11 + Q12: Atomic batch must use vendor prices.
    auto batch = acq.processBatch({
        {"V001", 1},
        {"V002", 1}
    }, true);

    CHECK(batch.size() == 2);
    CHECK(batch[0].approved);
    CHECK(batch[1].approved);
    CHECK(batch[0].vendor == "Amazon");
    CHECK(batch[0].cost == Money::of(350));
    CHECK(batch[1].cost == Money::of(600));
}

static void testVendorPricingRules() {
    const std::vector<std::string> au{"A"};
    Catalog c;
    c.emplace<Book>("H1", "Hard", au, "i", "Pub", 2020, Money::of(1200), 1, Binding::Hardcover);
    c.emplace<Book>("P1", "Paper", au, "i", "Pub", 2020, Money::of(450));
    c.emplace<ElectronicResource>("R1", "DB", "Pub", 2026, Money::of(150), "url",
                                  LicenseModel::AnnualSubscription, Money::of(2000));
    c.emplace<EBook>("E1", "E", au, "i", "Pub", 2020, Money::of(10), "url",
                     EBookFormat::PDF, false, LicenseModel::AnnualSubscription, Money::of(100));
    c.emplace<Journal>("J1", "Nature", "1234-5678", "Springer", 2024, Money::of(100), 12, 3);
    Budget b(Money::of(10000000));
    AcquisitionManager acq(c, b);

    // Resource::costAt prices units at any unit price using the resource's own rules
    CHECK(c.get("H1").costAt(Money::of(1200), 3) == c.get("H1").costFor(3));
    CHECK(c.get("H1").costAt(Money::of(1000), 1) == Money::of(1200));   // 1000 + 20%
    CHECK(c.get("P1").costAt(Money::of(400), 10) == Money::of(3600));   // 4000 - 10%
    CHECK_THROWS(c.get("P1").costAt(Money::of(400), 0), std::invalid_argument);

    // Without vendors the listed price is used
    CHECK(acq.quotePreTax("H1", 10) == Money::of(12960));

    acq.setVendorPrice("H1", "V", Money::of(1000));
    acq.setVendorPrice("P1", "V", Money::of(400));
    acq.setVendorPrice("R1", "V", Money::of(100));
    acq.setVendorPrice("E1", "V", Money::of(8));
    acq.setVendorPrice("J1", "V", Money::of(80));

    // The vendor's price goes through the hardcover surcharge and the bulk discount
    CHECK(acq.quotePreTax("H1", 1) == Money::of(1200));
    CHECK(acq.quotePreTax("H1", 10) == Money::of(10800));      // 1000 * 1.2 * 10, then -10%
    CHECK(acq.quotePreTax("P1", 2) == Money::of(800));
    CHECK(acq.quotePreTax("P1", 10) == Money::of(3600));       // 4000 - 10%

    // ... through the seat tiers (seats beyond the 50th at half price) ...
    CHECK(acq.quotePreTax("R1", 20) == Money::of(4000));       // 2000 + 20 * 100
    CHECK(acq.quotePreTax("R1", 50) == Money::of(7000));
    CHECK(acq.quotePreTax("R1", 60) == Money::of(7500));       // 2000 + 50 * 100 + 10 * 50
    CHECK(acq.quotePreTax("E1", 60) == Money::of(540));        // 100 + 50 * 8 + 10 * 4

    // ... and through the subscription years
    CHECK(acq.quotePreTax("J1", 2) == Money::of(480));         // 80 * 2 copies * 3 years
    CHECK(acq.quotePreTax("J1", 10) == Money::of(2160));       // 2400 - 10%

    // Tax is applied on top of the vendor-priced cost
    acq.setTaxPolicy(TaxPolicy(500, 1800));
    CHECK(acq.quote("H1", 1) == Money::of(1260));              // 1200 + 5%
    CHECK(acq.quote("R1", 20) == Money::of(4720));             // 4000 + 18%
    acq.setTaxPolicy(TaxPolicy());

    // The listed unit price is not changed by vendors
    CHECK(c.get("H1").unitPrice() == Money::of(1200));

    // Quotas and the budget are checked against the vendor-priced cost
    Catalog c2;
    c2.emplace<Book>("P1", "Paper", au, "i", "Pub", 2020, Money::of(450));
    Budget b2(Money::of(100000));
    b2.setQuota(ResourceCategory::Book, {20, Money::of(3700)});
    AcquisitionManager acq2(c2, b2);
    CHECK(!acq2.canPurchase("P1", 10));                        // listed: 4050 > 3700
    acq2.setVendorPrice("P1", "V", Money::of(400));
    CHECK(acq2.canPurchase("P1", 10));                         // vendor: 3600 <= 3700
    const PurchaseRecord rec = acq2.purchase("P1", 10);
    CHECK(rec.cost == Money::of(3600) && rec.vendor == "V");
}

static void testAtomicBatchSharedBudget() {
    const std::vector<std::string> au{"A"};
    Catalog c;
    c.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(100));
    Budget shared(Money::of(1000));
    shared.setQuota(ResourceCategory::Book, {5, Money::of(1000)});
    AcquisitionManager acq(c, shared);
    acq.addDepartment("Alias", shared);                        // two names, one Budget object

    // 3 + 3 = 6 units against a quota of 5: the whole batch must be rejected up front
    const auto res = acq.processBatch({{"B1", 3}, {"B1", 3, "Alias"}}, true);
    CHECK(!res[0].approved && !res[1].approved);
    CHECK(shared.spent() == Money{} && c.holdings("B1") == 0);

    // 3 + 2 = 5 fits
    const auto ok = acq.processBatch({{"B1", 3}, {"B1", 2, "Alias"}}, true);
    CHECK(ok[0].approved && ok[1].approved);
    CHECK(shared.spent() == Money::of(500) && c.holdings("B1") == 5);
}

static std::vector<std::string> idsOf(const std::vector<const Resource*>& found) {
    std::vector<std::string> ids;
    for (const Resource* r : found) ids.push_back(r->id());
    return ids;
}

static void testSearches() {
    using Ids = std::vector<std::string>;
    Catalog c;
    c.emplace<Book>("B1", "Clean Code", Ids{"Robert C. Martin", "Dean Wampler"}, "978-0132350884",
                    "Prentice Hall", 2008, Money::of(450));
    c.emplace<Book>("B2", "The C++ Programming Language", Ids{"Bjarne Stroustrup"}, "978-0321563842",
                    "Addison-Wesley", 2013, Money::of(1200));
    c.emplace<EBook>("E1", "Clean Architecture (e-book)", Ids{"Robert C. Martin"}, "978-0134494166",
                     "Pearson", 2020, Money::of(20), "url");
    c.emplace<AudioBook>("A1", "The Clean Coder (audio)", Ids{"Robert C. Martin"}, "Narrator",
                         "Pearson", 2020, Money::of(15), "url", 300);
    c.emplace<Journal>("J1", "Communications of the ACM", "0001-0782", "ACM", 2026,
                       Money::of(300), 12);
    c.emplace<Journal>("J2", "Journal of Logic", "2049-363X", "Oxford", 2015, Money::of(100), 4);
    c.emplace<ElectronicResource>("R1", "IEEE Xplore", "IEEE", 2026, Money::of(150), "url");
    c.emplace<Thesis>("T1", "Graph Algorithms", "IIT Delhi", "PhD", "Prof. Rao", 2023);

    // By author: any author, case-insensitive, part of a name is enough, in id order
    CHECK(idsOf(c.searchAuthor("Robert C. Martin")) == (Ids{"A1", "B1", "E1"}));
    CHECK(idsOf(c.searchAuthor("martin")) == (Ids{"A1", "B1", "E1"}));
    CHECK(idsOf(c.searchAuthor("ROBERT C. MARTIN")) == (Ids{"A1", "B1", "E1"}));
    CHECK(idsOf(c.searchAuthor("Wampler")) == (Ids{"B1"}));          // a second author counts
    CHECK(idsOf(c.searchAuthor("stroustrup")) == (Ids{"B2"}));
    CHECK(c.searchAuthor("nobody").empty());
    CHECK(c.searchAuthor("Prof. Rao").empty());                     // a supervisor is not an author
    CHECK(c.searchAuthor("Narrator").empty());                      // nor is a narrator
    CHECK_THROWS(c.searchAuthor(""), std::invalid_argument);
    CHECK_THROWS(c.searchAuthor("   "), std::invalid_argument);

    // By ISBN / ISSN: exact code, ignoring case, hyphens and spaces
    CHECK(idsOf(c.searchIdentifier("978-0132350884")) == (Ids{"B1"}));
    CHECK(idsOf(c.searchIdentifier("9780132350884")) == (Ids{"B1"}));
    CHECK(idsOf(c.searchIdentifier(" 978 0132350884 ")) == (Ids{"B1"}));
    CHECK(idsOf(c.searchIdentifier("978-0134494166")) == (Ids{"E1"}));  // e-books have an ISBN too
    CHECK(idsOf(c.searchIdentifier("0001-0782")) == (Ids{"J1"}));       // an ISSN
    CHECK(idsOf(c.searchIdentifier("00010782")) == (Ids{"J1"}));
    CHECK(idsOf(c.searchIdentifier("2049-363x")) == (Ids{"J2"}));       // check digit X, any case
    CHECK(c.searchIdentifier("978").empty());                          // a prefix is not a match
    CHECK(c.searchIdentifier("978-0000000000").empty());
    CHECK(c.searchIdentifier("---").empty());                           // nothing left once cleaned
    CHECK_THROWS(c.searchIdentifier(""), std::invalid_argument);
    CHECK_THROWS(c.searchIdentifier("  "), std::invalid_argument);

    // By publication year: both ends included, in id order
    CHECK(idsOf(c.searchYears(2008, 2013)) == (Ids{"B1", "B2"}));
    CHECK(idsOf(c.searchYears(2009, 2012)).empty());
    CHECK(idsOf(c.searchYears(2020, 2020)) == (Ids{"A1", "E1"}));
    CHECK(idsOf(c.searchYears(2020, 2026)) == (Ids{"A1", "E1", "J1", "R1", "T1"}));
    CHECK(idsOf(c.searchYears(1900, 1999)).empty());
    CHECK(c.searchYears(1000, 3000).size() == c.size());
    CHECK_THROWS(c.searchYears(2020, 2010), std::invalid_argument);

    // Results are the catalogue's own objects, and removed resources disappear
    CHECK(c.searchIdentifier("978-0132350884")[0] == c.find("B1"));
    c.remove("B1");
    CHECK(idsOf(c.searchAuthor("martin")) == (Ids{"A1", "E1"}));
    CHECK(c.searchIdentifier("978-0132350884").empty());

    // The virtual functions can be used directly, through the base class
    const Resource& thesis = c.get("T1");
    CHECK(!thesis.hasAuthor("Rao") && !thesis.hasIdentifier("T1"));
    const Resource& journal = c.get("J1");
    CHECK(!journal.hasAuthor("ACM") && journal.hasIdentifier("0001 0782"));
}

static void testLending() {
    using Ids = std::vector<std::string>;
    const Ids au{"A"};
    Catalog c;
    c.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(100));
    c.emplace<Book>("B2", "Two", au, "i2", "Pub", 2020, Money::of(100));
    c.emplace<Book>("B3", "Three (not bought yet)", au, "i3", "Pub", 2020, Money::of(100));
    c.emplace<ElectronicResource>("R1", "Database", "Pub", 2026, Money::of(150), "url");
    c.emplace<EBook>("E1", "E-book", au, "i4", "Pub", 2020, Money::of(10), "url");
    c.addHoldings("B1", 2);   // two copies
    c.addHoldings("B2", 1);
    c.addHoldings("R1", 3);   // three licensed seats
    c.addHoldings("E1", 1);
    LendingDesk desk(c);

    // Borrowing print copies is limited by the number of copies held
    CHECK(desk.available("B1") == 2 && desk.inUse("B1") == 0);
    desk.borrowCopy("alice", "B1");
    desk.borrowCopy("bob", "B1");
    CHECK(desk.available("B1") == 0 && desk.inUse("B1") == 2);
    CHECK(c.inUse("B1") == 2 && c.available("B1") == 0);
    CHECK(desk.isActive("alice", "B1") && !desk.isActive("carol", "B1"));
    CHECK((desk.patronsOf("B1") == Ids{"alice", "bob"}));
    CHECK_THROWS(desk.borrowCopy("carol", "B1"), UnavailableError);
    CHECK_THROWS(desk.borrowCopy("carol", "B1"), LendingError);  // UnavailableError is a LendingError
    CHECK(desk.inUse("B1") == 2);                                // a failed borrow changes nothing

    // Returning makes the copy available again
    desk.returnCopy("alice", "B1");
    CHECK(desk.available("B1") == 1 && !desk.isActive("alice", "B1"));
    desk.borrowCopy("carol", "B1");
    CHECK((desk.patronsOf("B1") == Ids{"bob", "carol"}));

    // Rules for one patron
    CHECK_THROWS(desk.borrowCopy("bob", "B1"), LendingError);       // already has it
    CHECK_THROWS(desk.returnCopy("alice", "B1"), LendingError);     // does not have it
    CHECK_THROWS(desk.returnCopy("dave", "B2"), LendingError);
    desk.returnCopy("bob", "B1");
    CHECK_THROWS(desk.returnCopy("bob", "B1"), LendingError);       // cannot return twice
    desk.borrowCopy("bob", "B2");
    desk.borrowCopy("bob", "B1");
    CHECK((desk.resourcesOf("bob") == Ids{"B1", "B2"}));
    CHECK(desk.resourcesOf("nobody").empty());

    // A resource with no copies held cannot be borrowed
    CHECK(c.holdings("B3") == 0);
    CHECK_THROWS(desk.borrowCopy("alice", "B3"), UnavailableError);

    // Sessions on electronic resources are limited by the licensed seats
    desk.openSession("alice", "R1");
    desk.openSession("bob", "R1");
    desk.openSession("carol", "R1");
    CHECK(desk.available("R1") == 0 && desk.inUse("R1") == 3);
    CHECK_THROWS(desk.openSession("dave", "R1"), UnavailableError);
    CHECK_THROWS(desk.openSession("alice", "R1"), LendingError);    // one session each
    desk.closeSession("bob", "R1");
    CHECK(desk.available("R1") == 1);
    desk.openSession("dave", "R1");
    CHECK((desk.patronsOf("R1") == Ids{"alice", "carol", "dave"}));
    CHECK_THROWS(desk.closeSession("bob", "R1"), LendingError);     // already closed
    desk.openSession("alice", "E1");                                // derived electronic classes too
    CHECK_THROWS(desk.openSession("bob", "E1"), UnavailableError);  // a single seat

    // Each kind of resource has its own verbs
    CHECK_THROWS(desk.borrowCopy("erin", "R1"), LendingError);
    CHECK_THROWS(desk.borrowCopy("erin", "E1"), LendingError);
    CHECK_THROWS(desk.openSession("erin", "B2"), LendingError);
    CHECK_THROWS(desk.returnCopy("alice", "R1"), LendingError);
    CHECK_THROWS(desk.closeSession("bob", "B1"), LendingError);

    // Unknown resources and blank patron names
    CHECK_THROWS(desk.borrowCopy("alice", "NOPE"), NotFoundError);
    CHECK_THROWS(desk.openSession("alice", "NOPE"), NotFoundError);
    CHECK_THROWS(desk.borrowCopy("", "B1"), std::invalid_argument);
    CHECK_THROWS(desk.openSession("  ", "R1"), std::invalid_argument);
    CHECK_THROWS(desk.available("NOPE"), NotFoundError);

    // The catalogue keeps the numbers consistent
    CHECK_THROWS(c.addHoldings("B1", -2), std::invalid_argument);   // 2 copies are out
    CHECK(c.holdings("B1") == 2);
    CHECK_THROWS(c.addInUse("B1", 1), std::invalid_argument);       // more than the holdings
    CHECK_THROWS(c.addInUse("B2", -5), std::invalid_argument);
    CHECK_THROWS(c.remove("B1"), std::invalid_argument);            // on loan
    c.addHoldings("B1", 3);                                         // buying more is fine
    desk.borrowCopy("dave", "B1");
    CHECK(desk.available("B1") == 2);
    c.addHoldings("B1", -2);                                        // the free copies can go
    CHECK(c.holdings("B1") == 3 && desk.available("B1") == 0);

    // The report lists who has what
    std::ostringstream os;
    desk.printReport(os);
    CHECK(os.str().find("B1  One: 3 of 3 copies on loan: bob, carol, dave") != std::string::npos);
    CHECK(os.str().find("R1  Database: 3 of 3 seats in use: alice, carol, dave") != std::string::npos);
    std::ostringstream none;
    LendingDesk empty(c);
    empty.printReport(none);
    CHECK(none.str() == "Nothing is on loan and no session is open\n");

    // Lending and purchasing together: bought copies can be lent, and copies on loan
    // cannot be removed by cancelling the purchase
    Catalog c2;
    c2.emplace<Book>("B1", "One", au, "i1", "Pub", 2020, Money::of(100));
    Budget budget(Money::of(100000));
    AcquisitionManager acq(c2, budget);
    LendingDesk desk2(c2);
    CHECK_THROWS(desk2.borrowCopy("alice", "B1"), UnavailableError);  // nothing bought yet
    const int order = acq.purchase("B1", 3).orderNo;
    desk2.borrowCopy("alice", "B1");
    desk2.borrowCopy("bob", "B1");
    CHECK(desk2.available("B1") == 1);
    CHECK_THROWS(acq.cancel(order), CancellationError);               // only 1 of the 3 is free
    CHECK(budget.spent() == Money::of(300) && c2.holdings("B1") == 3);  // nothing changed
    desk2.returnCopy("alice", "B1");
    desk2.returnCopy("bob", "B1");
    acq.cancel(order);
    CHECK(c2.holdings("B1") == 0 && budget.spent() == Money{});
    CHECK_THROWS(desk2.borrowCopy("alice", "B1"), UnavailableError);
}

static void testMoneyCurrency() {
    // The default currency
    CHECK(std::string(kDefaultCurrency) == "INR");
    CHECK(Money::of(5).currency() == "INR" && Money::of(5).hasCurrency());
    CHECK(Money::fromMinor(1250).currency() == "INR");
    CHECK(Money{}.currency().empty() && !Money{}.hasCurrency());

    // Naming a currency
    const Money usd = Money::of(12, 50, "USD");
    CHECK(usd.currency() == "USD" && usd.minorUnits() == 1250);
    CHECK(Money::fromMinor(1250, "USD") == usd);
    CHECK(Money::of(1, 0, "usd").currency() == "USD");          // stored in upper case
    CHECK_THROWS(Money::of(1, 0, "US"), std::invalid_argument);
    CHECK_THROWS(Money::of(1, 0, "USDX"), std::invalid_argument);
    CHECK_THROWS(Money::of(1, 0, "U5D"), std::invalid_argument);
    CHECK_THROWS(Money::of(1, 0, ""), std::invalid_argument);
    CHECK_THROWS(Money::fromMinor(1, "??"), std::invalid_argument);

    // Printing: the default currency is printed without its code, others with it
    CHECK(Money::of(450).toString() == "450.00");
    CHECK(usd.toString() == "12.50 USD");
    CHECK(Money::of(-3, 5, "EUR").toString() == "-3.05 EUR");
    CHECK(Money{}.toString() == "0.00");
    std::ostringstream os;
    os << usd << " / " << Money::of(3);
    CHECK(os.str() == "12.50 USD / 3.00");

    // Same currency: everything works and the currency is kept
    CHECK(Money::of(10, 0, "USD") + Money::of(5, 0, "USD") == Money::of(15, 0, "USD"));
    CHECK(Money::of(10, 0, "USD") - Money::of(15, 0, "USD") == Money::of(-5, 0, "USD"));
    CHECK((usd + usd).currency() == "USD" && (usd - usd).currency() == "USD");
    CHECK((usd * 3) == Money::of(37, 50, "USD") && (3 * usd).currency() == "USD");
    Money acc = Money::of(1, 0, "EUR");
    acc += Money::of(2, 0, "EUR");
    acc -= Money::of(1, 50, "EUR");
    CHECK(acc == Money::of(1, 50, "EUR"));
    CHECK(Money::of(1, 0, "USD") < Money::of(2, 0, "USD") && Money::of(2, 0, "USD") >= Money::of(2, 0, "USD"));
    CHECK(Money::of(1, 0, "USD") != Money::of(2, 0, "USD") && Money::of(3, 0, "USD") > Money::of(2, 0, "USD"));
    CHECK(Money::of(2, 0, "USD") <= Money::of(2, 0, "USD"));

    // Different currencies: adding, subtracting and comparing throw
    const Money inr = Money::of(10);
    const Money eur = Money::of(10, 0, "EUR");
    CHECK_THROWS(static_cast<void>(inr + usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr - usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(usd + eur), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr == usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr != usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr < usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr <= usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr > usd), CurrencyMismatchError);
    CHECK_THROWS(static_cast<void>(inr >= usd), CurrencyMismatchError);
    Money target = inr;
    CHECK_THROWS(target += usd, CurrencyMismatchError);
    CHECK_THROWS(target -= usd, CurrencyMismatchError);
    CHECK(target == inr && target.currency() == "INR");            // a failed operation changes nothing
    CHECK_THROWS(static_cast<void>(Money::of(0) == Money::of(0, 0, "USD")), CurrencyMismatchError);  // even zero
    try {
        static_cast<void>(inr + usd);
    } catch (const LibraryError& e) {                               // it is a library exception
        const std::string message = e.what();
        CHECK(message.find("INR") != std::string::npos && message.find("USD") != std::string::npos);
    }

    // A default-constructed Money is a zero that takes the currency of the other amount
    CHECK(Money{} + usd == usd && (Money{} + usd).currency() == "USD");
    CHECK((Money{} - usd).minorUnits() == -1250 && (Money{} - usd).currency() == "USD");
    CHECK(Money{} == Money{} && Money{}.isZero());
    CHECK(Money{} == Money::of(0, 0, "USD") && Money{} == Money::of(0));
    CHECK(Money{} < usd && usd > Money{} && Money{} != usd);
    Money sum;
    sum += usd;
    sum += usd;
    CHECK(sum == Money::of(25, 0, "USD") && sum.currency() == "USD");
    CHECK_THROWS(sum += inr, CurrencyMismatchError);               // once it has a currency, it keeps it

    // withMinorUnits keeps the currency
    CHECK(usd.withMinorUnits(7) == Money::fromMinor(7, "USD"));
    CHECK(Money{}.withMinorUnits(7) == Money::fromMinor(7));       // no currency: the default
    CHECK(!Money{}.withMinorUnits(0).hasCurrency());               // a zero stays unspecified
}

static void testCurrencyInTheLibrary() {
    const std::vector<std::string> au{"A"};
    const auto usd = [](std::int64_t major, int minor = 0) { return Money::of(major, minor, "USD"); };
    Catalog c;
    c.emplace<Book>("H1", "Hard", au, "i1", "Pub", 2020, usd(1000), 1, Binding::Hardcover);
    c.emplace<ElectronicResource>("R1", "DB", "Pub", 2026, usd(100), "url",
                                  LicenseModel::AnnualSubscription, usd(2000));
    c.emplace<Journal>("J1", "Nature", "1234-5678", "Springer", 2024, usd(80), 12, 3);
    c.emplace<Thesis>("T1", "Free thesis", "IITD", "PhD", "Sup", 2023);   // free: no currency
    c.emplace<Book>("B1", "Book in rupees", au, "i2", "Pub", 2020, Money::of(450));

    // The pricing rules keep the currency
    CHECK(c.get("H1").costFor(1) == usd(1200));                  // 1000 + 20%
    CHECK(c.get("H1").costFor(10) == usd(10800));                // then -10% for 10 copies
    CHECK(c.get("H1").costFor(10).toString() == "10800.00 USD");
    CHECK(c.get("R1").costFor(60) == usd(7500));                 // 2000 + 50 * 100 + 10 * 50
    CHECK(c.get("J1").costFor(2) == usd(480));                   // 80 * 2 copies * 3 years
    CHECK(c.get("B1").costFor(10) == Money::of(4050));
    CHECK(c.get("T1").costFor(3) == usd(0) && c.get("T1").costFor(3) == Money{});

    // Tax keeps the currency
    const TaxPolicy tax(500, 1800);
    CHECK(tax.apply(c.get("H1"), usd(1200)).tax == usd(60));
    CHECK(tax.apply(c.get("H1"), usd(1200)).total() == usd(1260));
    CHECK(tax.apply(c.get("R1"), usd(100)).tax.currency() == "USD");
    CHECK(tax.apply(c.get("B1"), Money::of(100)).tax == Money::of(5));

    // A budget in USD: purchases, quotas, tax, cancellation and rollover all stay in USD
    Budget budget(usd(50000));
    budget.setQuota(ResourceCategory::Book, {20, usd(20000), 2});
    AcquisitionManager acq(c, budget);
    acq.setTaxPolicy(tax);
    const PurchaseRecord rec = acq.purchase("H1", 2);            // 2400 + 5% = 2520
    CHECK(rec.cost == usd(2520) && rec.preTaxCost == usd(2400) && rec.taxAmount == usd(120));
    CHECK(budget.spent() == usd(2520) && budget.remaining() == usd(47480));
    CHECK(*budget.spendRemaining(ResourceCategory::Book) == usd(17480));
    CHECK(acq.totalSpent() == usd(2520));
    std::ostringstream report;
    acq.printReport(report);
    CHECK(report.str().find("2520.00 USD") != std::string::npos);

    CHECK(budget.carryForward(50) == usd(23740));                // half of the unspent 47480
    const Budget nextYear = budget.rollover(50);
    CHECK(nextYear.total() == usd(73740) && nextYear.spent() == Money{});
    CHECK(nextYear.quotaFor(ResourceCategory::Book)->maxSpend == usd(20000));

    acq.setVendorPrice("H1", "V", usd(900));
    CHECK(acq.quote("H1", 1) == usd(1134));                      // 900 + 20% = 1080, + 5%
    CHECK_THROWS(acq.setVendorPrice("H1", "X", Money::of(500)), CurrencyMismatchError);  // INR vendor price
    CHECK(acq.cheapestVendor("H1") == "V");

    // A free item (no currency) can be bought with a budget in any currency
    CHECK(acq.purchase("T1", 2).approved);
    CHECK(budget.spent() == usd(2520));

    const PurchaseRecord cancelRec = acq.cancel(rec.orderNo);
    CHECK(cancelRec.cost == usd(-2520));
    CHECK(budget.spent() == Money{} && budget.spent().currency() == "USD");

    // A cost in a different currency than the budget is rejected, and nothing changes
    std::string why;
    CHECK(!acq.canPurchase("B1", 1, &why));
    CHECK(why.find("currency mismatch") != std::string::npos);
    CHECK_THROWS(acq.purchase("B1", 1), CurrencyMismatchError);
    CHECK(c.holdings("B1") == 0 && budget.spent() == Money{});
    const auto res = acq.processBatch({{"B1", 1}, {"J1", 1}});   // J1 costs 240 USD (3 years)
    CHECK(!res[0].approved && res[0].reason.find("currency mismatch") != std::string::npos);
    CHECK(res[1].approved && budget.spent() == usd(252));        // 240 + 5%
    const auto atomic = acq.processBatch({{"B1", 1}, {"J1", 1}}, true);
    CHECK(!atomic[0].approved && !atomic[1].approved && budget.spent() == usd(252));

    // A quota in a different currency than the budget is refused
    CHECK_THROWS(budget.setQuota(ResourceCategory::Journal, {5, Money::of(100)}), CurrencyMismatchError);
    CHECK(!budget.quotaFor(ResourceCategory::Journal).has_value());
    CHECK_THROWS(static_cast<void>(budget.rollover(50, Money::of(1000))), CurrencyMismatchError);

    // Departments may use different currencies; totals over all of them cannot be added up
    Catalog c2;
    c2.emplace<Book>("H1", "Hard", au, "i1", "Pub", 2020, usd(1000), 1, Binding::Hardcover);
    c2.emplace<Book>("B1", "Book in rupees", au, "i2", "Pub", 2020, Money::of(450));
    Budget rupees(Money::of(100000));
    Budget dollars(usd(5000));
    AcquisitionManager acq2(c2, rupees);
    acq2.addDepartment("US office", dollars);
    acq2.purchase("B1", 2);                                      // default (rupee) budget
    acq2.purchase("H1", 1, "US office");                         // dollar budget
    CHECK(acq2.totalSpent("") == Money::of(900));
    CHECK(acq2.totalSpent("US office") == usd(1200));
    CHECK_THROWS(static_cast<void>(acq2.totalSpent()), CurrencyMismatchError);
    CHECK_THROWS(acq2.purchase("H1", 1), CurrencyMismatchError); // a dollar book on the rupee budget
    CHECK_THROWS(acq2.purchase("B1", 1, "US office"), CurrencyMismatchError);
}

int main() {
    testMoney();
    testResourcesAndCost();
    testCatalog();
    testBudget();
    testAcquisition();
    testJournal();
    testEBook();
    testAudioBook();
    testThesis();
    testBookHardcover();
    testBulkDiscounts();
    testTaxes();
    testTitleQuota();
    testCancellation();
    testDepartments();
    testRollover();
    testAtomicBatch();
    testVendors();
        testVendorPricingRules();
    testAtomicBatchSharedBudget();
    testSearches();
    testLending();
    testMoneyCurrency();
    testCurrencyInTheLibrary();
    /*std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    return g_failures == 0 ? 0 : 1;*/

    std::cout << "All tests passed\n";
return 0;
}
