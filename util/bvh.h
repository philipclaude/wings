#pragma once

#include <fmt/format.h>

#include <random>
#include <thread>
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

template <int dim>
struct BVHNode {
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
class BoundingVolumeHierarchy;
template <typename BVHLeaf>
void build_level(BoundingVolumeHierarchy<BVHLeaf>* tree, size_t idx_m,
                 size_t idx_l, size_t idx_r, int level) {
  auto l = tree->build(idx_l, idx_m, ++level);
  auto r = tree->build(idx_m, idx_r, level);

  auto& nodes = tree->nodes();
  auto& leaves = tree->leaves();
  nodes[idx_m].left = l;
  nodes[idx_m].right = r;
  nodes[idx_m].box =
      AABB<BVHLeaf::dim>(l < 0 ? leaves[-l - 1].box() : nodes[l].box,
                         r < 0 ? leaves[-r - 1].box() : nodes[r].box);
}

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
    size_t n_threads = std::thread::hardware_concurrency();
    threads_.reserve(n_threads);
    paralevel_ = std::ceil(std::log(n_threads) / std::log(2));
    nodes_.resize(2 * leaves_.size());
    Timer timer;
    timer.start();
    root_ = build(0, leaves_.size(), 0);
    for (auto& t : threads_) t.join();
    update_boxes(root_, 0);
    timer.stop();
    LOGF("Built BVH in {} seconds.", timer.seconds());
  }

  void update_boxes(int32_t idx, int level) {
    if (idx < 0 || level >= paralevel_) return;
    update_boxes(nodes_[idx].left, level + 1);
    update_boxes(nodes_[idx].right, level + 1);
    auto l = nodes_[idx].left;
    auto r = nodes_[idx].right;
    nodes_[idx].box = AABB<dim>(l < 0 ? leaves_[-l - 1].box() : nodes_[l].box,
                                r < 0 ? leaves_[-r - 1].box() : nodes_[r].box);
  }

  int32_t build(size_t idx_l, size_t idx_r, int level) {
    int64_t n_objects = idx_r - idx_l;
    if (n_objects == 0) return 0;
    if (n_objects == 1) return -idx_l - 1;

    size_t idx_m = std::floor(0.5 * (idx_l + idx_r));
    int axis = std::floor(rand() * dim / double(RAND_MAX));
    const auto compare = [axis](const auto& a, const auto& b) {
      return a.box().min()[axis] < b.box().min()[axis];
    };
    std::nth_element(leaves_.begin() + idx_l, leaves_.begin() + idx_m,
                     leaves_.begin() + idx_r, compare);

    if (level != paralevel_) {
      build_level<BVHLeaf>(this, idx_m, idx_l, idx_r, level);
    } else {
      threads_.push_back(
          std::thread(build_level<BVHLeaf>, this, idx_m, idx_l, idx_r, level));
    }
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

  auto& nodes() { return nodes_; }
  auto& leaves() { return leaves_; }

 private:
  size_t root_;
  std::vector<BVHNode<BVHLeaf::dim>> nodes_;
  std::vector<BVHLeaf> leaves_;
  int paralevel_{-1};
  std::vector<std::thread> threads_;
};

}  // namespace wings