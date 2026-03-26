#ifndef TEXTURE_H
#define TEXTURE_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original (TheNextWeek):
//   • All double members/parameters/return types → float
//   • Floating-point literals: added 'f' suffix
//   • 1.0 / 255.0 → 1.0f / 255.0f
//   • std::floor, std::sin use float overloads from compat/cmath
//
// Program logic: UNCHANGED.
//==============================================================================================

#include "perlin.h"
#include "rtw_stb_image.h"


class texture {
  public:
    virtual ~texture() = default;

    virtual color value(float u, float v, const point3& p) const = 0;
};


class solid_color : public texture {
  public:
    solid_color(const color& albedo) : albedo(albedo) {}

    solid_color(float red, float green, float blue) : solid_color(color(red, green, blue)) {}

    color value(float u, float v, const point3& p) const override {
        return albedo;
    }

  private:
    color albedo;
};


class checker_texture : public texture {
  public:
    checker_texture(float scale, shared_ptr<texture> even, shared_ptr<texture> odd)
      : inv_scale(1.0f / scale), even(even), odd(odd) {}

    checker_texture(float scale, const color& c1, const color& c2)
      : checker_texture(scale, make_shared<solid_color>(c1), make_shared<solid_color>(c2)) {}

    color value(float u, float v, const point3& p) const override {
        auto xInteger = int(std::floor(inv_scale * p.x()));
        auto yInteger = int(std::floor(inv_scale * p.y()));
        auto zInteger = int(std::floor(inv_scale * p.z()));

        bool isEven = (xInteger + yInteger + zInteger) % 2 == 0;

        return isEven ? even->value(u, v, p) : odd->value(u, v, p);
    }

  private:
    float inv_scale;
    shared_ptr<texture> even;
    shared_ptr<texture> odd;
};


class image_texture : public texture {
  public:
    image_texture(const char* filename) : image(filename) {}

    color value(float u, float v, const point3& p) const override {
        // If we have no texture data, then return solid cyan as a debugging aid.
        if (image.height() <= 0) return color(0,1,1);

        // Clamp input texture coordinates to [0,1] x [1,0]
        u = interval(0,1).clamp(u);
        v = 1.0f - interval(0,1).clamp(v);  // Flip V to image coordinates

        auto i = int(u * image.width());
        auto j = int(v * image.height());
        auto pixel = image.pixel_data(i, j);

        auto color_scale = 1.0f / 255.0f;
        return color(color_scale*pixel[0], color_scale*pixel[1], color_scale*pixel[2]);
    }

  private:
    rtw_image image;
};


class noise_texture : public texture {
  public:
    noise_texture(float scale) : scale(scale) {}

    color value(float u, float v, const point3& p) const override {
        return color(.5f, .5f, .5f) * (1.0f + std::sin(scale * p.z() + 10.0f * noise.turb(p, 7)));
    }

  private:
    perlin noise;
    float  scale;
};


#endif
