#ifndef ARITHMETICTYPES_HPP
#define ARITHMETICTYPES_HPP

#include <cstdint>
#include <string>
namespace ddsolve::expression {

enum class ComparisonType : uint8_t {
  kEquality = 0,
  kInequality,
  kLessThan,
  kLessThanOrEqual,
  kGreaterThan,
  kGreaterThanOrEqual,
};

enum class SmtResult : uint8_t {
  kUnsat = 0,
  kSat,
  kUnknown,
};

constexpr SmtResult boolToResult(const bool b) {
  return b ? SmtResult::kSat : SmtResult::kUnsat;
}

constexpr SmtResult flip(const SmtResult result) {
  switch (result) {
    case SmtResult::kUnsat:
      return SmtResult::kSat;
    case SmtResult::kSat:
      return SmtResult::kUnsat;
    default:
      return SmtResult::kUnknown;
  }
}

constexpr std::string smtResultToStr(const SmtResult result) {
  switch(result) {
    case SmtResult::kUnsat:
      return "Unsat";
    case SmtResult::kSat:
      return "Sat";
    default:
      return "Unknown";
  }
}

}  // namespace ddsolve::expression

#endif /* ARITHMETICTYPES_HPP */
