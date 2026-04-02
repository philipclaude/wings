#pragma once

#include <fmt/format.h>

#include <unordered_set>
#include <vector>

#include "glm.h"
#include "log.h"
#include "util.h"

namespace wings {

class BVHTriangle {
 public:
  BVHTriangle(vec3f a, vec3f b, vec3f c, uint32_t cell, int group)
      : a_(a), b_(b), c_(c), cell_(cell), group_(group) {
    vec3f min, max;
    for (int d = 0; d < 3; d++) {
      min[d] = std::min(std::min(a[d], b[d]), c[d]);
      max[d] = std::max(std::max(a[d], b[d]), c[d]);
    }
    box_ = AABB(min, max);
  }

  float intersect(const Ray& ray, float tmin, float tmax) const {
    // vec3d instead of vec3f for more precision
    vec3d a = a_, b = b_, c = c_, d = ray.direction, o = ray.origin;
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

  const AABB& box() const { return box_; }

  int group() const { return group_; }
  int cell() const { return cell_; }

  vec3f center() const { return (a_ + b_ + c_) / 3.0f; }

 private:
  vec3f a_, b_, c_;
  AABB box_;
  int cell_;
  int group_;
};

struct Intersection {
  float t{-1};
  const BVHTriangle* triangle{nullptr};
};

class BVHNode {
 public:
  BVHNode(const BVHTriangle* triangle) : triangle_(triangle) {
    box_ = triangle_->box();
  }
  BVHNode(std::vector<BVHTriangle>& objects, size_t m, size_t n) {
    const int axis = std::floor(rand() * 3 / double(RAND_MAX));
    const auto compare = [axis](const auto& a, const auto& b) {
      return a.box().min()[axis] < b.box().min()[axis];
    };
    int64_t n_objects = n - m;
    if (n_objects == 0) {
      return;
    } else if (n_objects == 1) {
      left_ = std::make_unique<BVHNode>(&objects[m]);
      right_ = std::make_unique<BVHNode>(&objects[m]);
      box_ = objects[m].box();
      return;
    } else if (n_objects == 2) {
      if (compare(objects[m], objects[m + 1])) {
        left_ = std::make_unique<BVHNode>(&objects[m]);
        right_ = std::make_unique<BVHNode>(&objects[m + 1]);
      } else {
        left_ = std::make_unique<BVHNode>(&objects[m + 1]);
        right_ = std::make_unique<BVHNode>(&objects[m]);
      }
    } else {
      std::sort(objects.begin() + m, objects.begin() + n, compare);
      const size_t mid = std::floor(0.5 * (m + n));
      left_ = std::make_unique<BVHNode>(objects, m, mid);
      right_ = std::make_unique<BVHNode>(objects, mid, n);
    }
    box_ = AABB(left_->box(), right_->box());
  }

  Intersection intersect(const Ray& ray, float tmin, float tmax,
                         const std::vector<bool>& hidden) {
    if (!box_.intersect(ray, tmin, tmax)) return Intersection();
    if (triangle_) {
      auto group = triangle_->group();
      if (group >= 0 && hidden[group]) return Intersection();
      float t = triangle_->intersect(ray, tmin, tmax);
      if (t < tmin || t > tmax) return Intersection();
      return {t, triangle_};
    }

    auto ixnL = left_->intersect(ray, tmin, tmax, hidden);
    auto ixnR =
        right_->intersect(ray, tmin, ixnL.t > 0 ? ixnL.t : tmax, hidden);
    if (ixnR.t > 0) return ixnR;
    if (ixnL.t > 0) return ixnL;
    return Intersection();
  }

  const AABB& box() const { return box_; }

  BVHNode* left() const { return left_.get(); }
  BVHNode* right() const { return right_.get(); }

 private:
  std::unique_ptr<BVHNode> left_;
  std::unique_ptr<BVHNode> right_;
  const BVHTriangle* triangle_{nullptr};
  AABB box_;
};

class BoundingVolumeHierarchy {
 public:
  BoundingVolumeHierarchy() {}

  void add(vec3f a, vec3f b, vec3f c, uint32_t cell, int group) {
    triangles_.emplace_back(a, b, c, cell, group);
  }

  void build() {
    root_ = std::make_unique<BVHNode>(triangles_, 0, triangles_.size());
  }

  void clear() {
    triangles_.clear();
    root_ = nullptr;
  }

  Intersection intersect(const Ray& ray, const std::vector<bool>& hidden) {
    if (!root_) return Intersection();
    return root_->intersect(ray, 1e-6f, 10000.0f, hidden);
#if 0
    float tmin = 10000.0f;
    const BVHTriangle* tri{nullptr};
    for (const auto& triangle : triangles_) {
      auto t = triangle.intersect(ray, 0, 10000.0f);
      if (t > 0 && t < tmin) {
        tmin = t;
        tri = &triangle;
      }
    }
    if (tmin < 100) return {tmin, tri};
    return Intersection();
#endif
  }

 private:
  std::unique_ptr<BVHNode> root_;
  std::vector<BVHTriangle> triangles_;
};

}  // namespace wings