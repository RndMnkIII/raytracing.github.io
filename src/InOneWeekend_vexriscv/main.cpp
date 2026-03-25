//==============================================================================================
// Ray Tracing in One Weekend — VexRiscV / openfpgaOS port
//
// Target: VexRiscV RV32IMF core running openfpgaOS (Analogue Pocket)
// Build:  See Makefile in this directory
//
// Changes vs. original main.cc:
//   • Floating-point literals: 'f' suffix added throughout
//   • Scene simplified to 4 spheres (3 featured + ground) for embedded target.
//     The full random-sphere scene (~490 objects) can be restored by enabling
//     the code block guarded by FULL_SCENE — it fits within HITTABLE_LIST_MAX
//     (512) but will be very slow on the embedded CPU.
//   • Camera settings reduced for embedded target:
//     image_width 1200 → 160, samples_per_pixel 10 → 4, max_depth 20 → 8
//==============================================================================================

#include "rtweekend.h"

#include "camera.h"
#include "hittable.h"
#include "hittable_list.h"
#include "material.h"
#include "sphere.h"


int main() {
    hittable_list world;

    // Ground
    auto ground_material = make_shared<lambertian>(color(0.5f, 0.5f, 0.5f));
    world.add(make_shared<sphere>(point3(0,-1000,0), 1000.0f, ground_material));

#ifdef FULL_SCENE
    // Random sphere field — ~484 additional spheres.
    // Enable with -DFULL_SCENE.  Will be very slow on VexRiscV; good for
    // correctness validation on the PC target (make app_pc CXXFLAGS=-DFULL_SCENE).
    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_double();
            point3 center(a + 0.9f*random_double(), 0.2f, b + 0.9f*random_double());

            if ((center - point3(4, 0.2f, 0)).length() > 0.9f) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8f) {
                    auto albedo = color::random() * color::random();
                    sphere_material = make_shared<lambertian>(albedo);
                } else if (choose_mat < 0.95f) {
                    auto albedo = color::random(0.5f, 1.0f);
                    auto fuzz   = random_double(0.0f, 0.5f);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                } else {
                    sphere_material = make_shared<dielectric>(1.5f);
                }
                world.add(make_shared<sphere>(center, 0.2f, sphere_material));
            }
        }
    }
#endif /* FULL_SCENE */

    // Three feature spheres
    auto material1 = make_shared<dielectric>(1.5f);
    world.add(make_shared<sphere>(point3(0, 1, 0), 1.0f, material1));

    auto material2 = make_shared<lambertian>(color(0.4f, 0.2f, 0.1f));
    world.add(make_shared<sphere>(point3(-4, 1, 0), 1.0f, material2));

    auto material3 = make_shared<metal>(color(0.7f, 0.6f, 0.5f), 0.0f);
    world.add(make_shared<sphere>(point3(4, 1, 0), 1.0f, material3));

    camera cam;

    // Reduced settings for embedded VexRiscV target.
    // For PC validation use higher values (e.g. image_width=800, samples=10).
    cam.aspect_ratio      = 16.0f / 9.0f;
    cam.image_width       = 160;  // ~90 lines high
    cam.samples_per_pixel = 4;
    cam.max_depth         = 8;

    cam.vfov     = 20.0f;
    cam.lookfrom = point3(13, 2, 3);
    cam.lookat   = point3(0, 0, 0);
    cam.vup      = vec3(0, 1, 0);

    cam.defocus_angle = 0.6f;
    cam.focus_dist    = 10.0f;

    cam.render(world);

    return 0;
}
