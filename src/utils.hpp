#ifndef UTILS_HPP
#define UTILS_HPP

#include <bits/ranges_base.h>
#include <xxhash.h>

#include <concepts>
#include <cstddef>
#include <functional>
#include <iterator>
#include <ranges>
#include <span>
#include <utility>

namespace ddsolve {

struct indirection {
  template <typename T>
  constexpr decltype(auto) operator()(T&& t) const {
    return *std::forward<T>(t);
  }
};

struct indirection_less {
  template <typename T>
  constexpr bool operator()(const T& lhs, const T& rhs) const {
    return *lhs < *rhs;
  }
};

struct indirection_equal_to {
  template <typename T>
  constexpr bool operator()(const T& lhs, const T& rhs) const {
    return *lhs == *rhs;
  }
};

template <typename T>
concept MemberHashable = requires(T a) {
  { a.hash() } -> std::convertible_to<size_t>;
};

template <typename T>
concept StdHashable = requires(T a) {
  { std::hash<T>{}(a) } -> std::convertible_to<size_t>;
};

template <typename T>
concept Hashable = MemberHashable<T> || StdHashable<T>;

template <Hashable T>
struct IndirectHash {
  template <typename Ptr>
    requires MemberHashable<T> && requires(const Ptr& p) {
      { *p } -> std::convertible_to<const T&>;
    }
  [[nodiscard]] constexpr size_t operator()(const Ptr& ptr) const {
    return (*ptr).hash();
  }

  template <typename Ptr>
    requires(!MemberHashable<T>) && StdHashable<T> && requires(const Ptr& p) {
      { *p } -> std::convertible_to<const T&>;
    }
  [[nodiscard]] constexpr size_t operator()(const Ptr& ptr) const {
    return std::hash<T>{}(*ptr);
  }
};

struct MemberHash {
  template <MemberHashable T>
  [[nodiscard]] constexpr size_t operator()(const T& a) const {
    return a.hash();
  }
};

struct SpanHash {
  template <typename T>
  [[nodiscard]] size_t operator()(const std::span<T> a) const {
    const auto bytes{std::as_bytes(a)};

    return XXH3_64bits(bytes.data(), bytes.size());
  }

  template <typename T>
  [[nodiscard]] size_t operator()(const std::span<T> a,
                                  const uint64_t seed) const {
    const auto bytes{std::as_bytes(a)};

    return XXH3_64bits_withSeed(bytes.data(), bytes.size(), seed);
  }
};

}  // namespace ddsolve

namespace ddsolve::views {

template <std::forward_iterator It1, std::sentinel_for<It1> S1,
          std::forward_iterator It2, std::sentinel_for<It2> S2,
          class Comp = std::ranges::less>
  requires std::same_as<std::iter_value_t<It1>, std::iter_value_t<It2>> &&
           std::same_as<std::iter_reference_t<It1>,
                        std::iter_reference_t<It2>> &&
           std::indirect_strict_weak_order<Comp, It1, It2>
class MergeIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::iter_value_t<It1>;

  template <std::ranges::forward_range R1, std::ranges::forward_range R2>
  constexpr MergeIterator(const R1& r1, const R2& r2, const Comp& comp = {})
      : a_begin_{std::ranges::cbegin(r1)},
        a_end_{r1.cend()},
        b_begin_{r2.cbegin()},
        b_end_{r2.cend()},
        comp_{comp} {};

  constexpr MergeIterator() = default;
  constexpr ~MergeIterator() = default;

  constexpr MergeIterator(const MergeIterator&) = default;
  constexpr MergeIterator(MergeIterator&&) = default;

  constexpr MergeIterator& operator=(const MergeIterator&) = default;
  constexpr MergeIterator& operator=(MergeIterator&&) = default;

  constexpr std::iter_reference_t<It1> operator*() const {
    if (a_begin_ == a_end_) {
      return *b_begin_;
    }
    if (b_begin_ == b_end_) {
      return *a_begin_;
    }
    return std::invoke(comp_, *a_begin_, *b_begin_) ? *a_begin_ : *b_begin_;
  }

  constexpr MergeIterator& operator++() {
    if (a_begin_ == a_end_) {
      ++b_begin_;
    } else if (b_begin_ == b_end_) {
      ++a_begin_;
    } else {
      if (std::invoke(comp_, *a_begin_, *b_begin_)) {
        ++a_begin_;
      } else {
        ++b_begin_;
      }
    }

    return *this;
  }

  constexpr MergeIterator operator++(int) {
    auto tmp = *this;
    ++*this;
    return tmp;
  }

  constexpr difference_type operator-(const MergeIterator& other) const {
    return std::ranges::distance(other.a_begin_, a_begin_) +
           std::ranges::distance(other.b_begin_, b_begin_);
  }

  constexpr bool operator==(const MergeIterator& other) const {
    return std::forward_as_tuple(a_begin_, b_begin_) ==
           std::forward_as_tuple(other.a_begin_, other.b_begin_);
  }

  [[nodiscard]] constexpr MergeIterator end() const {
    MergeIterator rval{*this};
    rval.a_begin_ = a_end_;
    rval.b_begin_ = b_end_;
    return rval;
  }

 private:
  It1 a_begin_;
  S1 a_end_;
  It2 b_begin_;
  S2 b_end_;

  Comp comp_;
};

template <class R1, class R2, class Comp = std::ranges::less>
MergeIterator(const R1&, const R2&, Comp = {})
    -> MergeIterator<std::ranges::const_iterator_t<const R1&>,
                     std::ranges::const_sentinel_t<const R1&>,
                     std::ranges::const_iterator_t<const R2&>,
                     std::ranges::const_sentinel_t<const R2&>, Comp>;

static_assert(std::forward_iterator<
              decltype(MergeIterator(std::vector<int>{}, std::vector<int>{}))>);

template <std::ranges::forward_range R1, std::ranges::forward_range R2,
          class Comp = std::ranges::less>
class MergeView : public std::ranges::view_interface<MergeView<R1, R2, Comp>> {
 public:
  using const_iterator =
      MergeIterator<std::ranges::const_iterator_t<const R1&>,
                    std::ranges::const_sentinel_t<const R1&>,
                    std::ranges::const_iterator_t<const R2&>,
                    std::ranges::const_sentinel_t<const R2&>, Comp>;

  constexpr MergeView(const R1& r1, const R2& r2, const Comp& comp = {})
      : begin_{MergeIterator(r1, r2, comp)} {}

  [[nodiscard]]
  constexpr const_iterator begin() const {
    return begin_;
  }

  [[nodiscard]]
  constexpr const_iterator end() const {
    return begin_.end();
  }

 private:
  const_iterator begin_;
};

static_assert(std::ranges::forward_range<
              decltype(MergeView(std::vector<int>{}, std::vector<int>{}))>);

}  // namespace ddsolve::views

#endif /* UTILS_HPP */
