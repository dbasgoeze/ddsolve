#ifndef SMTTERM_HPP
#define SMTTERM_HPP

#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

#include "ArithmeticTypes.hpp"
#include "utils.hpp"

namespace ddsolve::expression {

class SmtComparison;

class SmtTerm {
 public:
  using Scalar = SmtTerm;
  using Variable = SmtTerm;

  using ScalarType = SmtTerm;
  using CoefficientType = SmtTerm;
  using CoefficientPointer = std::shared_ptr<CoefficientType>;
  using ConstCoefficientPointer = std::shared_ptr<const CoefficientType>;
  using ScalarPointer = std::shared_ptr<ScalarType>;
  using ConstScalarPointer = std::shared_ptr<const ScalarType>;

  using Term = SmtTerm;
  using TermPointer = std::shared_ptr<Term>;
  using ConstTermPointer = std::shared_ptr<const Term>;

  using Comparison = SmtComparison;
  using ComparisonPointer = std::shared_ptr<SmtComparison>;
  using ConstComparisonPointer = std::shared_ptr<const SmtComparison>;

  // TODO(DB) this does not look right
  // The map is broken
  static uint64_t ScalarType_get_id(
      const std::shared_ptr<const ScalarType> &s) {
    static std::shared_mutex map_mutex{};
    static std::map<std::string, uint64_t> map{};

    const std::string str{s->str()};

    {
      std::shared_lock lock{map_mutex};
      const auto it{map.find(str)};
      if (it != map.end()) {
        return it->second;
      }
    }
    std::scoped_lock lock{map_mutex};
    const auto it{map.find(str)};
    if (!map.contains(str)) {
      const auto id = map.size();
      map[str] = id;
      return id;
    }
    return it->second;
  }

  static ScalarType scalarFromString(const std::string &str) {
    return mkReal(str);
  }

  static CoefficientType coefficientFromString(const std::string &str) {
    return mkReal(str);
  }

  SmtTerm() = default;
  explicit SmtTerm(std::string term, bool isSimplified = true)
      : term_{std::move(term)},
        hash_cache_{calc_hash()},
        isSimplified_{isSimplified} {}
  SmtTerm(const SmtTerm &) = default;
  SmtTerm(SmtTerm &&) = default;
  SmtTerm &operator=(const SmtTerm &) = default;
  SmtTerm &operator=(SmtTerm &&) = default;
  ~SmtTerm() = default;

  [[nodiscard]] static SmtTerm mkIntVar(const std::string &name);
  [[nodiscard]] static SmtTerm mkRealVar(const std::string &name);
  [[nodiscard]] static SmtTerm mkReal(const std::string &value);
  [[nodiscard]] static SmtTerm mkInt(const std::string &value);

  [[nodiscard]] static SmtTerm mkRealFunction(const std::string &name,
                                              unsigned int arity);
  [[nodiscard]] static SmtTerm mkIntFunction(const std::string &name,
                                             unsigned int arity);
  [[nodiscard]] SmtTerm applyFunction(
      const std::vector<SmtTerm> &arguments) const;

  [[nodiscard]] static SmtTerm mkIntArray(const std::string &name);
  [[nodiscard]] static SmtTerm mkRealArray(const std::string &name);
  [[nodiscard]] SmtTerm arrayStore(const SmtTerm &index,
                                   const SmtTerm &value) const;
  [[nodiscard]] SmtTerm arraySelect(const SmtTerm &index) const;

  [[nodiscard]] SmtTerm toReal() const;
  [[nodiscard]] SmtTerm toInt() const;

  [[nodiscard]] SmtTerm operator/(const SmtTerm &other) const;
  [[nodiscard]] SmtTerm operator*(const SmtTerm &other) const;
  [[nodiscard]] SmtTerm operator+(const SmtTerm &other) const;
  [[nodiscard]] SmtTerm operator-(const SmtTerm &other) const;

  [[nodiscard]] SmtTerm scale(const Scalar &scalar) const {
    return scalar * (*this);
  }

  [[nodiscard]] SmtTerm fusedScaleAdd(const Scalar &scalar,
                                      const SmtTerm &other) const {
    return scalar * (*this) + other;
  }

  [[nodiscard]] SmtTerm substitute(const Variable &variable,
                                   const SmtTerm &other) const;

  [[nodiscard]] SmtTerm simplify() const;

  [[nodiscard]] bool operator==(const SmtTerm &other) const {
    return term_ == other.term_;
  }

  [[nodiscard]] bool operator<(const SmtTerm &other) const {
    return term_ < other.term_;
  }
  [[nodiscard]] std::string str() const {
    return term_;
  }

  [[nodiscard]] uint64_t hash() const {
    return hash_cache_;
  }

  [[nodiscard]] bool isSimplified() const {
    return isSimplified_;
  }

 private:
  [[nodiscard]] uint64_t calc_hash() const {
    return SpanHash{}(std::span{term_.data(), term_.size()});
  }

  std::string term_;
  uint64_t hash_cache_{};
  bool isSimplified_{};

  friend class SmtComparison;
};

class SmtComparison {
 public:
  SmtComparison();
  explicit SmtComparison(std::string term) : term_{std::move(term)} {}
  explicit SmtComparison(SmtTerm term) : term_{std::move(term)} {}

  SmtComparison(const SmtTerm &term, const ComparisonType &type) {}

  SmtComparison(const SmtTerm &lhs, const SmtTerm &rhs,
                const ComparisonType &type);

  SmtComparison(const SmtComparison &) = default;
  SmtComparison(SmtComparison &&) = default;
  SmtComparison &operator=(const SmtComparison &) = default;
  SmtComparison &operator=(SmtComparison &&) = default;
  ~SmtComparison() = default;

  [[nodiscard]] bool operator==(const SmtComparison &other) const {
    return term_ == other.term_;
  }

  [[nodiscard]] bool operator<(const SmtComparison &other) const {
    return term_ < other.term_;
  }

  [[nodiscard]] SmtComparison substitute(const SmtTerm::Variable &var,
                                         const SmtTerm &other) const;

  struct WithParity {
    bool parity;
    std::shared_ptr<const SmtComparison> comparison;
  };

  [[nodiscard]] SmtResult trivialCheck() const {
    return SmtResult::kUnknown;
  }

  static void pushAssumption(const WithParity &assumption);
  static void popAssumption();

  [[nodiscard]] static SmtResult decide(
      const std::vector<WithParity> &conjunction, bool printModel);

  static void shutdown();

  [[nodiscard]] uint64_t hash() const {
    return term_.hash();
  }

  [[nodiscard]] std::string str() const {
    return term_.str();
  }

  [[nodiscard]] const SmtTerm &getTerm() const {
    return term_;
  }

  [[nodiscard]] SmtComparison simplify() const {
    return SmtComparison{term_.simplify()};
  }

  [[nodiscard]] SmtComparison operator&(const SmtComparison &other) const;
  [[nodiscard]] SmtComparison operator|(const SmtComparison &other) const;
  [[nodiscard]] SmtComparison operator~() const;

  [[nodiscard]] static SmtComparison mkForallInt(const SmtTerm &var, const SmtComparison &expr);
  [[nodiscard]] static SmtComparison mkForallReal(const SmtTerm &var, const SmtComparison &expr);

 private:
  SmtTerm term_;
};

}  // namespace ddsolve::expression

#endif /* SMTTERM_HPP */
