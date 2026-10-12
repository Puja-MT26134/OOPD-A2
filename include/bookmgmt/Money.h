#pragma once
// Money: fixed-point amount in a currency, stored in minor units (e.g. paise/cents).
// Using an integer avoids floating-point rounding errors in cost/budget sums.
//
// Q15: every amount has a currency code (three letters, such as "INR" or "USD").
// Adding, subtracting or comparing amounts in different currencies throws
// CurrencyMismatchError. A default-constructed Money is a zero with no currency yet
// ("unspecified"): it takes the currency of whatever it is combined with, so the
// zero used to start a sum, or to test "is this zero?", works in every currency.
// There is no currency conversion.

#include <cstdint>
#include <iosfwd>
#include <string>

namespace bookmgmt {

// The currency of amounts created without naming one. Amounts in this currency are
// printed without the code ("450.00"); amounts in any other currency with it ("450.00 USD").
constexpr const char* kDefaultCurrency = "INR";

class Money {
public:
    // Zero with no currency yet (see above).
    constexpr Money() = default;

    // Construct from minor units: Money::fromMinor(12550) == 125.50
    // `currency` must be three letters (any case; stored in upper case), otherwise
    // std::invalid_argument is thrown.
    static Money fromMinor(std::int64_t minor, const std::string& currency = kDefaultCurrency);
    // Construct from major + minor parts: Money::of(125, 50) == 125.50,
    // Money::of(125, 50, "USD") == 125.50 USD
    static Money of(std::int64_t major, int minor = 0,
                    const std::string& currency = kDefaultCurrency);

    constexpr std::int64_t minorUnits() const { return minor_; }
    double toDouble() const { return static_cast<double>(minor_) / 100.0; }
    std::string toString() const;  // "1234.50" / "-3.05" / "1234.50 USD"

    // The currency code, or "" for an unspecified zero.
    std::string currency() const;
    constexpr bool hasCurrency() const { return currency_ != 0; }
    // True if the two amounts can be added and compared: same currency, or at least
    // one of them has no currency yet.
    constexpr bool sameCurrencyAs(Money o) const {
        return currency_ == 0 || o.currency_ == 0 || currency_ == o.currency_;
    }
    // An amount of `minor` minor units in this amount's currency. If this amount has no
    // currency yet, a zero stays unspecified and any other amount gets the default
    // currency. Used for results such as a rounded discount or tax.
    Money withMinorUnits(std::int64_t minor) const;

    constexpr bool isZero() const { return minor_ == 0; }
    constexpr bool isNegative() const { return minor_ < 0; }

    Money& operator+=(Money o) { unify(o, "add"); minor_ += o.minor_; return *this; }
    Money& operator-=(Money o) { unify(o, "subtract"); minor_ -= o.minor_; return *this; }
    Money& operator*=(std::int64_t k) { minor_ *= k; return *this; }

    friend Money operator+(Money a, Money b) { return a += b; }
    friend Money operator-(Money a, Money b) { return a -= b; }
    friend Money operator*(Money a, std::int64_t k) { return a *= k; }
    friend Money operator*(std::int64_t k, Money a) { return a *= k; }

    // Comparisons throw CurrencyMismatchError for different currencies.
    friend bool operator==(Money a, Money b) { return a.compareTo(b) == 0; }
    friend bool operator!=(Money a, Money b) { return a.compareTo(b) != 0; }
    friend bool operator<(Money a, Money b) { return a.compareTo(b) < 0; }
    friend bool operator<=(Money a, Money b) { return a.compareTo(b) <= 0; }
    friend bool operator>(Money a, Money b) { return a.compareTo(b) > 0; }
    friend bool operator>=(Money a, Money b) { return a.compareTo(b) >= 0; }

private:
    constexpr Money(std::int64_t minor, std::uint32_t currency)
        : minor_(minor), currency_(currency) {}
    // Throws CurrencyMismatchError if the currencies differ; an amount with no currency
    // takes the other's.
    void unify(Money o, const char* operation);
    int compareTo(Money o) const;  // -1, 0 or 1; throws CurrencyMismatchError

    std::int64_t minor_ = 0;
    std::uint32_t currency_ = 0;  // the three letters packed into one number; 0 = none
};

std::ostream& operator<<(std::ostream& os, Money m);

}  // namespace bookmgmt