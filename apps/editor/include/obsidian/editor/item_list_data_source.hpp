#pragma once

#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>
#include <vector>

namespace obsidian::editor {

template <typename ValueType> class ItemListDataSource {
public:
  void setValues(std::vector<ValueType> values) {
    _values = std::move(values);

    _valueStringPtrs.resize(_values.size() + 1);
    _valueStringPtrs[0] = "none";

    std::transform(_values.cbegin(), _values.cend(),
                   _valueStringPtrs.begin() + 1,
                   [](ValueType const& v) { return v.c_str(); });
  }

  // returns size and the pointer to the first string pointer in the array
  std::tuple<std::size_t, char const* const*>
  getValueStrings(bool includeNone) const {
    std::size_t const firstInd = includeNone || _values.empty() ? 0 : 1;

    return {_valueStringPtrs.size() - firstInd,
            _valueStringPtrs.data() + firstInd};
  }

  ValueType at(std::size_t listItemInd, bool includeNone) {
    if (includeNone && !listItemInd) {
      return {};
    }

    std::size_t const indWithOffset = listItemInd - (includeNone ? 1 : 0);
    return _values.at(indWithOffset);
  }

  int listItemInd(ValueType const& val, bool includeNone) {
    auto const resultIter = std::find(_values.cbegin(), _values.cend(), val);

    if (resultIter == _values.cend()) {
      assert(includeNone);
      return 0;
    }

    std::size_t const indOffset = includeNone ? 1 : 0;

    return std::distance(_values.cbegin(), resultIter) + indOffset;
  }

  bool exists(ValueType const& val) const {
    return std::any_of(_values.cbegin(), _values.cend(),
                       [&val](ValueType const& value) { return val == value; });
  }

  std::size_t valuesSize() const { return _values.size(); }

private:
  std::vector<ValueType> _values;
  std::vector<char const*> _valueStringPtrs;
};

} /*namespace obsidian::editor*/
