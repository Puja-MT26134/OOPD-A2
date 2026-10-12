#pragma once
// Exception hierarchy. Catch LibraryError to handle any library-specific failure.

#include <stdexcept>
#include <string>

namespace bookmgmt {

class LibraryError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class NotFoundError : public LibraryError {
public:
    explicit NotFoundError(const std::string& id)
        : LibraryError("resource not found: " + id) {}
};

class DuplicateIdError : public LibraryError {
public:
    explicit DuplicateIdError(const std::string& id)
        : LibraryError("duplicate resource id: " + id) {}
};

// Thrown when a purchase would exceed a per-category quota (units or spend).
class QuotaExceededError : public LibraryError {
public:
    using LibraryError::LibraryError;
};

// Thrown when a purchase would exceed the overall budget.
class BudgetExceededError : public LibraryError {
public:
    using LibraryError::LibraryError;
};

// Thrown when a purchase names a department that has not been registered.
class UnknownDepartmentError : public LibraryError {
public:
    explicit UnknownDepartmentError(const std::string& name)
        : LibraryError("unknown department: " + name) {}
};

// Thrown when an order cannot be cancelled (unknown, rejected, already cancelled, ...).
class CancellationError : public LibraryError {
public:
    using LibraryError::LibraryError;
};

// Thrown when a lending request breaks a rule (wrong kind of resource, not borrowed
// by that patron, already borrowed, ...).
class LendingError : public LibraryError {
public:
    using LibraryError::LibraryError;
};

// Thrown when every copy (print) or seat (electronic) is already in use.
class UnavailableError : public LendingError {
public:
    using LendingError::LendingError;
};

// Thrown when amounts in different currencies are added, subtracted or compared.
class CurrencyMismatchError : public LibraryError {
public:
    using LibraryError::LibraryError;
};

}  // namespace bookmgmt
