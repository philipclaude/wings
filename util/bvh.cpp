#include "bvh.h"

#include "mesh.h"

namespace wings {

template <>
Intersection BoundingVolumeHierarchyBase::intersect(
    const Ray<3>& ray, const std::vector<bool>& hidden) {
  return static_cast<BoundingVolumeHierarchy<BVHTriangle>*>(this)->intersect(
      ray, hidden);
}

template <>
Intersection BoundingVolumeHierarchyBase::intersect(
    const Ray<4>& ray, const std::vector<bool>& hidden) {
  return static_cast<BoundingVolumeHierarchy<BVHTet>*>(this)->intersect(ray,
                                                                        hidden);
}

BVHTet::BVHTet(const Mesh& mesh, uint32_t cell, int group)
    : BVHLeafBase(cell, group) {
  vec4f min{1e20f, 1e20f, 1e20f, 1e20f}, max = -1.0f * min;
  const auto& vertices = mesh.vertices();
  const auto& tet = mesh.tetrahedra();
  for (int d = 0; d < dim; d++) {
    for (int i = 0; i < 4; i++) {
      min[d] = std::min(min[d] * 1.0, vertices[tet[cell][i]][d]);
      max[d] = std::max(max[d] * 1.0, vertices[tet[cell][i]][d]);
    }
  }
  box_ = AABB<dim>(min, max);
}

Intersection BVHTet::intersect(const Mesh* mesh, const Ray<4>& ray, float tmin,
                               float tmax) const {
  ASSERT(mesh);
  mat4f m;
  vec4f a(mesh->vertices()[mesh->tetrahedra()(cell_, 0)]);
  for (int d = 0; d < 4; d++) {
    m(d, 0) = ray.direction[d];
    m(d, 1) = a[d] - mesh->vertices()[mesh->tetrahedra()(cell_, 1)][d];
    m(d, 2) = a[d] - mesh->vertices()[mesh->tetrahedra()(cell_, 2)][d];
    m(d, 3) = a[d] - mesh->vertices()[mesh->tetrahedra()(cell_, 3)][d];
  }
  // float detm = glm::det(m);
  //  LOGF("det = {}", detm);
  //  if (std::fabs(detm) < 1e-12) return -1;
  vec4f sol = glm::inverse(m) * (a - ray.origin);
  float t = sol[0], u = sol[1], v = sol[2], w = sol[3];
  float eps = 0;
  if (u < -eps || v < -eps || w < -eps || u + v + w > 1 + eps)
    return Intersection();
  // if (u < 0 || v < 0 || w < 0 || u + v + w > 1) return Intersection();
  if (t < tmin || t > tmax) return Intersection();
  return {this, t};
}

template class BoundingVolumeHierarchy<BVHTriangle>;
template class BoundingVolumeHierarchy<BVHTet>;

}  // namespace wings