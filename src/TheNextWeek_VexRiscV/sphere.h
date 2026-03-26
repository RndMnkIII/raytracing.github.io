#ifndef SPHERE_H
#define SPHERE_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original (TheNextWeek):
//   • All double members/parameters/return types → float
//   • std::fmax(0, radius) → std::fmax(0.0f, radius)
//   • std::sqrt, std::acos, std::atan2 use float overloads from compat/cmath
//   • Floating-point literals: added 'f' suffix
//   • 2*pi → 2.0f*pi
//
// Program logic: UNCHANGED  (stationary + moving sphere, UV mapping, bbox).
//==============================================================================================

#include "hittable.h"


class sphere : public hittable {
  public:
    // Stationary Sphere
    sphere(const point3& static_center, float radius, shared_ptr<material> mat)
      : center(static_center, vec3(0,0,0)), radius(std::fmax(0.0f, radius)), mat(mat)
    {
        auto rvec = vec3(radius, radius, radius);
        bbox = aabb(static_center - rvec, static_center + rvec);
    }

    // Moving Sphere
    sphere(const point3& center1, const point3& center2, float radius,
           shared_ptr<material> mat)
      : center(center1, center2 - center1), radius(std::fmax(0.0f, radius)), mat(mat)
    {
        auto rvec = vec3(radius, radius, radius);
        aabb box1(center.at(0.0f) - rvec, center.at(0.0f) + rvec);
        aabb box2(center.at(1.0f) - rvec, center.at(1.0f) + rvec);
        bbox = aabb(box1, box2);
    }

    bool hit(const ray& r, interval ray_t, hit_record& rec) const override {
        point3 current_center = center.at(r.time());
        vec3 oc = current_center - r.origin();
        auto a = r.direction().length_squared();
        auto h = dot(r.direction(), oc);
        auto c = oc.length_squared() - radius*radius;

        auto discriminant = h*h - a*c;
        if (discriminant < 0.0f)
            return false;

        auto sqrtd = std::sqrt(discriminant);

        // Find the nearest root that lies in the acceptable range.
        auto root = (h - sqrtd) / a;
        if (!ray_t.surrounds(root)) {
            root = (h + sqrtd) / a;
            if (!ray_t.surrounds(root))
                return false;
        }

        rec.t = root;
        rec.p = r.at(rec.t);
        vec3 outward_normal = (rec.p - current_center) / radius;
        rec.set_face_normal(r, outward_normal);
        get_sphere_uv(outward_normal, rec.u, rec.v);
        rec.mat = mat;

        return true;
    }

    aabb bounding_box() const override { return bbox; }

  private:
    ray   center;
    float radius;
    shared_ptr<material> mat;
    aabb bbox;

    static void get_sphere_uv(const point3& p, float& u, float& v) {
        // p: a given point on the sphere of radius one, centered at the origin.
        // u: returned value [0,1] of angle around the Y axis from X=-1.
        // v: returned value [0,1] of angle from Y=-1 to Y=+1.

        auto theta = std::acos(-p.y());
        auto phi   = std::atan2(-p.z(), p.x()) + pi;

        u = phi   / (2.0f * pi);
        v = theta / pi;
    }
};


#endif
