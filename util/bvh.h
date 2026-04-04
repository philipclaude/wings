#pragma once

#include <fmt/format.h>

#include <unordered_set>
#include <vector>

#include "glm.h"
#include "log.h"
#include "util.h"

namespace wings {

class BVHLeafBase {
 public:
  virtual ~BVHLeafBase() {}
  int group() const { return group_; }
  int cell() const { return cell_; }
  virtual vec3f center() const = 0;

 protected:
  BVHLeafBase(int cell, int group) : cell_(cell), group_(group) {}
  int cell_;
  int group_;
};

static float intersect_ray_triangle(const Ray<3>& ray, const vec3d& a,
                                    const vec3d& b, const vec3d& c, float tmin,
                                    float tmax) {
  // vec3d instead of vec3f for more precision
  vec3d d = ray.direction, o = ray.origin;
  auto ab = b - a;
  auto ac = c - a;
  auto h = cross(d, ac);
  double det = dot(ab, h);
  if (std::fabs(det) < 1e-14) return -1;

  auto s = o - a;
  double u = dot(s, h) / det;
  if (u < 0 || u > 1) return -1;

  auto q = cross(s, ab);
  double v = dot(d, q) / det;
  if (v < 0 || u + v > 1) return -1;

  double t = dot(ac, q) / det;
  if (t < tmin || t > tmax) return -1;
  return t;
}

class BVHTriangle : public BVHLeafBase {
 public:
  static constexpr int N = 3;
  static constexpr int dim = 3;
  BVHTriangle(const std::array<vec3f, 3>& triangle, uint32_t cell, int group)
      : BVHLeafBase(cell, group),
        a_(triangle[0]),
        b_(triangle[1]),
        c_(triangle[2]) {
    vec3f min, max;
    for (int d = 0; d < 3; d++) {
      min[d] = std::min(std::min(a_[d], b_[d]), c_[d]);
      max[d] = std::max(std::max(a_[d], b_[d]), c_[d]);
    }
    box_ = AABB<3>(min, max);
  }

  float intersect(const Ray<3>& ray, float tmin, float tmax) const {
    return intersect_ray_triangle(ray, a_, b_, c_, tmin, tmax);
  }

  const AABB<3>& box() const { return box_; }

  vec3f center() const { return (a_ + b_ + c_) / 3.0f; }

 private:
  vec3f a_, b_, c_;
  AABB<3> box_;
};

class BVHTet : public BVHLeafBase {
 public:
  static constexpr int N = 4;
  static constexpr int dim = 4;
  BVHTet(const std::array<vec4f, 4>& tet, uint32_t cell, int group)
      : BVHLeafBase(cell, group) {
    vec4f min, max;
    for (int d = 0; d < dim; d++) {
      min[d] = std::min(std::min(std::min(tet[0][d], tet[1][d]), tet[2][d]),
                        tet[3][d]);
      max[d] = std::max(std::max(std::max(tet[0][d], tet[1][d]), tet[2][d]),
                        tet[3][d]);
    }
    box_ = AABB<dim>(min, max);
  }

  float intersect(const Ray<4>& ray, float tmin, float tmax) const {
    // determine which edges are intersected
    return -1;  // intersect_ray_triangle(ray, a_, b_, c_, tmin, tmax);
  }

  const auto& box() const { return box_; }

  vec3f center() const {
    return {0, 0, 0};
  }  //(a_ + b_ + c_ + d_).xyz() / 3.0f; }

 private:
  // vec4f a_, b_, c_, d_;
  AABB<dim> box_;
};

struct Intersection {
  float t{-1};
  const BVHLeafBase* elem{nullptr};
};

template <typename BVHLeaf>
struct BVHNode {
  static constexpr int dim = BVHLeaf::dim;
  uint32_t left{0};
  uint32_t right{0};
  AABB<dim> box;
};

class BoundingVolumeHierarchyBase {
 public:
  virtual ~BoundingVolumeHierarchyBase() {}
  template <int dim>
  Intersection intersect(const Ray<dim>& ray, const std::vector<bool>& hidden);
  virtual void build() = 0;
  virtual void clear() = 0;
};

template <typename BVHLeaf>
class BoundingVolumeHierarchy : public BoundingVolumeHierarchyBase {
 public:
  static constexpr int dim = BVHLeaf::dim;
  using vecf = vec<dim, float>;
  BoundingVolumeHierarchy() {}

  void add(const std::array<vecf, BVHLeaf::N>& elem, uint32_t cell, int group) {
    leaves_.emplace_back(elem, cell, group);
  }

  void build() {
    LOGF("Building BVH with {} leaves.", leaves_.size());
    nodes_.clear();
    nodes_.resize(2 * leaves_.size());
    root_ = 0;
    root_ = build(root_, 0, leaves_.size());
    assert(root_ == 0);
  }

  int32_t build(size_t& i, size_t m, size_t n) {
    int64_t n_objects = n - m;
    if (n_objects == 0) return 0;
    if (n_objects == 1) return -m - 1;

    size_t mid = std::floor(0.5 * (m + n));
    int axis = std::floor(rand() * dim / double(RAND_MAX));
    const auto compare = [axis](const auto& a, const auto& b) {
      return a.box().min()[axis] < b.box().min()[axis];
    };
    std::nth_element(leaves_.begin() + m, leaves_.begin() + mid,
                     leaves_.begin() + n, compare);
    size_t idx_m = i++;
    auto l = build(i, m, mid);
    auto r = build(i, mid, n);

    nodes_[idx_m].left = l;
    nodes_[idx_m].right = r;
    nodes_[idx_m].box =
        AABB<dim>(l < 0 ? leaves_[-l - 1].box() : nodes_[l].box,
                  r < 0 ? leaves_[-r - 1].box() : nodes_[r].box);
    return idx_m;
  }

  void clear() {
    nodes_.clear();
    leaves_.clear();
  }

  Intersection intersect(const Ray<dim>& ray,
                         const std::vector<bool>& hidden) const {
    return intersect(root_, ray, 1e-6f, 10000.0f, hidden);
  }

  Intersection intersect(int32_t idx, const Ray<dim>& ray, float tmin,
                         float tmax, const std::vector<bool>& hidden) const {
    if (idx < 0) {
      auto& leaf = leaves_[-idx - 1];
      auto group = leaf.group();
      if (group >= 0 && hidden[group]) return Intersection();
      float t = leaf.intersect(ray, tmin, tmax);
      if (t < tmin || t > tmax) return Intersection();
      return {t, &leaf};
    }
    const auto& node = nodes_[idx];
    if (!node.box.intersect(ray, tmin, tmax)) return Intersection();

    auto ixnL = intersect(node.left, ray, tmin, tmax, hidden);
    auto ixnR =
        intersect(node.right, ray, tmin, ixnL.t > 0 ? ixnL.t : tmax, hidden);
    if (ixnR.t > 0) return ixnR;
    if (ixnL.t > 0) return ixnL;
    return Intersection();
  }

 private:
  size_t root_;
  std::vector<BVHNode<BVHLeaf>> nodes_;
  std::vector<BVHLeaf> leaves_;
};

}  // namespace wings