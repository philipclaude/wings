#include "bvh.h"

namespace wings {

template <>
Intersection BoundingVolumeHierarchyBase::intersect(
    const Ray<3>& ray, const std::vector<bool>& hidden) {
  return static_cast<BoundingVolumeHierarchy<BVHTriangle>*>(this)->intersect(
      ray, hidden);
}

template class BoundingVolumeHierarchy<BVHTriangle>;
template class BoundingVolumeHierarchy<BVHTet>;

}  // namespace wings