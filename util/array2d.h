//
//  wings: web interface for graphics applications
//
//  Copyright 2023 - 2026 Philip Claude Caplan
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
#pragma once

#include <fmt/format.h>

#include <iostream>
#include <vector>

#include "log.h"

namespace wings {

enum LayoutCategory {
  Layout_Jagged,      // size along second dimension is not constant
  Layout_Rectangular  // size along second dimension is constant
};

/**
 * \brief Represents a two-dimensional array, but is stored using
 *        one-dimensional arrays. Can represent rectangular arrays,
 *        in which the size along the second dimension is constant,
 *        or jagged arrays, in which the size along the second dimension
 *        is different for each element.
 */
template <typename T, typename F = uint64_t>
class array2d {
 public:
  /**
   * \brief Constructs a 2d array, setting the layout and stride
   *
   * \param[in] stride - number of elements in along the second dimension
   *                     stride > 0 for rectangular arrays
   *                     stride < 0 for jagged arrays
   */
  array2d(int stride)
      : layout_((stride) > 0 ? Layout_Rectangular : Layout_Jagged),
        stride_(stride) {}

  /**
   * \brief Constructs a 2d array, setting the layout and stride and
   *        allocating the array, while setting all elements to a value.
   *
   * \param[in] stride - number of elements in along the second dimension
   *                     stride > 0 for rectangular arrays
   *                     stride < 0 for jagged arrays
   * \param[in] N - number of elements along the first dimension
   * \param[in] value - the value all (N x stride) components will be set to.
   */
  array2d(int stride, int N, T value)
      : layout_(Layout_Rectangular), stride_(stride) {
    ASSERT(stride > 0);
    std::vector<T> x(stride, value);
    for (int k = 0; k < N; k++) add(x.data());
  }

  void allocate(size_t n) {
    ASSERT(stride_ > 0);
    data_.resize(n * stride_);
  }

  /**
   * \brief Prevent copy constructor, i.e. array2d<int> a2 = a1;
   */
  array2d<T>& operator=(const array2d<T>&) = delete;

  /**
   * \brief Prevent copy assignment operator, i.e. array2d<int> a2 = a1;
   */
  array2d(const array2d<T>&) = delete;

  /**
   * \brief Sets the array layout
   *
   * \param[in] layout - either Layout_Jagged or Layout_Rectangular
   */
  void set_layout(LayoutCategory layout) { layout_ = layout; }

  /**
   * \brief Sets the array stride
   *
   * \param[in] stride - number of elements along second dimension
   *                     stride > 0 for rectangular arrays
   *                     stride < 0 for jagged arrays
   */
  void set_stride(int stride) { stride_ = stride; }

  /**
   * \brief Returns the number of elements along the first dimension.
   *
   * \return number of elements (such as number of vertices, or triangles).
   */
  size_t n() const {
    if (layout_ == Layout_Rectangular) {
      ASSERT(stride_ > 0);
      return data_.size() / stride_;
    }
    return length_.size();
  }

  /**
   * \brief Read access of a value at component (k,j)
   *
   * \param[in] k: 0 <= k < n()
   * \param[in] j: 0 <= j < length(j) or 0 <= j < stride
   *
   * \return const reference to component (k,j)
   */
  const T& operator()(size_t k, uint32_t j) const {
    ASSERT(k < n()) << fmt::format(
        "attempt to access element ({}, {}), but n = {}", k, j, n());
    if (layout_ == Layout_Rectangular) return data_[k * stride_ + j];
    ASSERT(layout_ == Layout_Jagged);
    ASSERT(j < length_[k]) << fmt::format(
        "attempt to access item {} but length({}) = {}", j, k, length_[k]);
    return data_[first_[k] + j];
  }

  /**
   * \brief Read/write access of a value at component (k,j)
   *
   * \param[in] k: 0 <= k < n()
   * \param[in] j: 0 <= j < length(j) or 0 <= j < stride
   *
   * \return reference to component (k,j)
   */
  T& operator()(size_t k, uint32_t j) {
    ASSERT(k < n());
    if (layout_ == Layout_Rectangular) return data_[k * stride_ + j];
    ASSERT(layout_ == Layout_Jagged);
    ASSERT(j < length_[k]);
    return data_[first_[k] + j];
  }

  /**
   * \brief Read access of pointer to element k
   *
   * \param[in] k: 0 <= k < n()
   *
   * \return const pointer to data at element k
   */
  const T* operator[](size_t k) const {
    ASSERT(k < n()) << fmt::format("attempt to access element {}, but n = {}",
                                   k, n());
    if (layout_ == Layout_Rectangular) return &data_[k * stride_];
    ASSERT(layout_ == Layout_Jagged);
    return &data_[first_[k]];
  }

  /**
   * \brief Read/write access of pointer to element k
   *
   * \param[in] k: 0 <= k < n()
   *
   * \return pointer to data at element k
   */
  T* operator[](size_t k) {
    ASSERT(k < n());
    if (layout_ == Layout_Rectangular) return &data_[k * stride_];
    ASSERT(layout_ == Layout_Jagged);
    return &data_[first_[k]];
  }

  /**
   * \brief Adds an element to a rectangular array.
   *
   * \param[in] x - values to add (there must be stride elements)
   */
  template <typename R>
  void add(const R* x) {
    ASSERT(layout_ == Layout_Rectangular);
    for (int j = 0; j < stride_; j++) data_.push_back(x[j]);
  }

  /**
   * \brief Adds an element to a jagged array.
   *
   * \param[in] x - values to add (there must be stride elements)
   * \param[in] n - number of values to add
   */
  template <typename R>
  void add(const R* x, int m) {
    if (layout_ == Layout_Rectangular) {
      add(x);
      return;
    }
    ASSERT(layout_ == Layout_Jagged);
    first_.push_back(data_.size());
    for (int j = 0; j < m; j++) data_.push_back(x[j]);
    length_.push_back(m);
  }

  /**
   * \brief Returns the number of items at element k
   *
   * \param[in] k: 0 <= k < n()
   *
   * \return number of items at k
   */
  int length(size_t k) const {
    ASSERT(k < n());
    if (layout_ == Layout_Rectangular) return stride_;
    return length_[k];
  }

  /**
   * \brief Returns all the data (read-only)
   *
   * \return data vector
   */
  const auto& data() const { return data_; }
  const auto& first() const { return first_; }
  const auto& length() const { return length_; }

  void reserve(uint64_t n, int max_item = 10) {
    if (layout_ == Layout_Rectangular) {
      data_.reserve(n * stride_);
    } else {
      first_.reserve(n);
      length_.reserve(n);
      data_.reserve(max_item * n);
    }
  }

  /**
   * \brief Clears all the data in this array.
   *
   * \param[in] reset_stride - whether stride should be set to 0.
   *            e.g. with 3d vertices, we still want the stride to be 3.
   */
  void clear(bool reset_stride = false) {
    // do not reset layout because this should not change
    if (reset_stride) stride_ = 0;
    data_.clear();
    first_.clear();
    length_.clear();
  }

  /**
   * @brief Frees the memory used by the arrays.
   *
   */
  void free() {
    std::vector<T>().swap(data_);
    std::vector<F>().swap(first_);
    std::vector<uint16_t>().swap(length_);
  }

  /**
   * \brief Prints out the array as a table.
   *
   * \param[in] label (optional) - prefix for array entries.
   */
  void print(const std::string& label = std::string()) const {
    for (int k = 0; k < n(); k++) {
      std::cout << (label.empty() ? "entry" : label) << "[";
      std::cout << k << "] = (";
      int m = length(k);
      for (int j = 0; j < length(k); j++) {
        std::cout << (*this)(k, j);
        if (j + 1 == m)
          std::cout << ")" << std::endl;
        else
          std::cout << ", ";
      }
    }
  }

  /**
   * \brief Returns the stride - only derived classes have access.
   *
   * \return stride (either -1 or > 0).
   */
  int stride() const { return stride_; }

  LayoutCategory layout() const { return layout_; }

 private:
  LayoutCategory layout_;         // either Layout_Jagged or Layout_Rectangular
  int stride_;                    // number of entries along second dimension
  std::vector<T> data_;           // the 1d data array
  std::vector<F> first_;          // index of first entry in jagged array
  std::vector<uint16_t> length_;  // number of entries for each element in
                                  // jagged array
};

}  // namespace wings
