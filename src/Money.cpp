#include "bookmgmt/Money.h"

#include <cctype>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
// "USD" -> a number holding the three letters (upper case); invalid codes throw.
std::uint32_t packCurrency(const std::string& code) {
    if (code.size() != 3) throw std::invalid_argument("currency code must be three letters: " + code);
    std::uint32_t packed = 0;
    for (unsigned char ch : code) {
        if (!std::isalpha(ch))
            throw std::invalid_argument("currency code must be three letters: " + code);
        packed = (packed << 8) | static_cast<std::uint32_t>(std::toupper(ch));
    }
    return packed;
}

std::string unpackCurrency(std::uint32_t packed) {
    if (packed == 0) return std::string();
    std::string code(3, ' ');
    for (int i = 2; i >= 0; --i) {
        code[static_cast<std::size_t>(i)] = static_cast<char>(packed & 0xFF);
        packed >>= 8;
    }
    return code;
}
}  // namespace

Money Money::fromMinor(std::int64_t minor, const std::string& currency) {
    return Money(minor, packCurrency(currency));
}

Money Money::of(std::int64_t major, int minor, const std::string& currency) {
    if (minor < 0 || minor > 99)
        throw std::invalid_argument("minor part must be in 0..99");
    const std::int64_t sign = major < 0 ? -1 : 1;
    return Money(major * 100 + sign * minor, packCurrency(currency));
}

std::string Money::currency() const { return unpackCurrency(currency_); }

Money Money::withMinorUnits(std::int64_t minor) const {
    if (currency_ != 0) return Money(minor, currency_);
    // No currency yet: a zero stays unspecified, any other amount gets the default currency.
    return Money(minor, minor == 0 ? 0 : packCurrency(kDefaultCurrency));
}

void Money::unify(Money o, const char* operation) {
    if (!sameCurrencyAs(o))
        throw CurrencyMismatchError(std::string("cannot ") + operation + " amounts in " +
                                    currency() + " and " + o.currency());
    if (currency_ == 0) currency_ = o.currency_;
}

int Money::compareTo(Money o) const {
    if (!sameCurrencyAs(o))
        throw CurrencyMismatchError("cannot compare amounts in " + currency() + " and " +
                                    o.currency());
    return minor_ < o.minor_ ? -1 : (minor_ > o.minor_ ? 1 : 0);
}

std::string Money::toString() const {
    const std::int64_t absMinor = minor_ < 0 ? -minor_ : minor_;
    std::string s = std::to_string(absMinor / 100);
    const auto frac = absMinor % 100;
    s += '.';
    if (frac < 10) s += '0';
    s += std::to_string(frac);
    if (minor_ < 0) s = "-" + s;
    if (currency_ != 0 && currency_ != packCurrency(kDefaultCurrency)) s += " " + currency();
    return s;
}

std::ostream& operator<<(std::ostream& os, Money m) { return os << m.toString(); }

}  // namespace bookmgmt