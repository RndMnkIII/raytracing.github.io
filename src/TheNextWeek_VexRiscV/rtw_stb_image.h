#ifndef RTW_STB_IMAGE_H
#define RTW_STB_IMAGE_H
//==============================================================================================
// VexRiscV / openfpgaOS port
//
// Changes vs. original (TheNextWeek):
//   • Guarded by OF_PC / embedded split:
//     - OF_PC build: full stb_image implementation, identical to original.
//     - Embedded build: stub rtw_image that always returns height == 0.
//       This causes image_texture::value() to return its built-in fallback
//       color (cyan — the existing "no image data" debug path in texture.h).
//       No std::string, no file I/O, no stb_image needed.
//
// Rationale: the VexRiscV target has no filesystem accessible at runtime.
// Image textures therefore fall back to the cyan debug color on embedded.
// All other scene types (checker, noise, solid color, lights) work normally.
//==============================================================================================

#ifdef OF_PC
/* ── PC / host build: full stb_image implementation ─────────────────────── */

// Disable strict warnings for this header from the Microsoft Visual C++ compiler.
#ifdef _MSC_VER
    #pragma warning (push, 0)
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "../external/stb_image.h"

#include <cstdlib>
#include <iostream>


class rtw_image {
  public:
    rtw_image() {}

    rtw_image(const char* image_filename) {
        auto filename = std::string(image_filename);
        auto imagedir = getenv("RTW_IMAGES");

        if (imagedir && load(std::string(imagedir) + "/" + image_filename)) return;
        if (load(filename))                           return;
        if (load("images/" + filename))               return;
        if (load("../images/" + filename))            return;
        if (load("../../images/" + filename))         return;
        if (load("../../../images/" + filename))      return;
        if (load("../../../../images/" + filename))   return;
        if (load("../../../../../images/" + filename)) return;
        if (load("../../../../../../images/" + filename)) return;

        std::cerr << "ERROR: Could not load image file '" << image_filename << "'.\n";
    }

    ~rtw_image() {
        delete[] bdata;
        STBI_FREE(fdata);
    }

    bool load(const std::string& filename) {
        auto n = bytes_per_pixel;
        fdata = stbi_loadf(filename.c_str(), &image_width, &image_height, &n, bytes_per_pixel);
        if (fdata == nullptr) return false;

        bytes_per_scanline = image_width * bytes_per_pixel;
        convert_to_bytes();
        return true;
    }

    int width()  const { return (fdata == nullptr) ? 0 : image_width; }
    int height() const { return (fdata == nullptr) ? 0 : image_height; }

    const unsigned char* pixel_data(int x, int y) const {
        static unsigned char magenta[] = { 255, 0, 255 };
        if (bdata == nullptr) return magenta;

        x = clamp(x, 0, image_width);
        y = clamp(y, 0, image_height);

        return bdata + y*bytes_per_scanline + x*bytes_per_pixel;
    }

  private:
    const int      bytes_per_pixel = 3;
    float         *fdata = nullptr;
    unsigned char *bdata = nullptr;
    int            image_width  = 0;
    int            image_height = 0;
    int            bytes_per_scanline = 0;

    static int clamp(int x, int low, int high) {
        if (x < low)  return low;
        if (x < high) return x;
        return high - 1;
    }

    static unsigned char float_to_byte(float value) {
        if (value <= 0.0f) return 0;
        if (1.0f <= value) return 255;
        return static_cast<unsigned char>(256.0f * value);
    }

    void convert_to_bytes() {
        int total_bytes = image_width * image_height * bytes_per_pixel;
        bdata = new unsigned char[total_bytes];

        auto *bptr = bdata;
        auto *fptr = fdata;
        for (auto i = 0; i < total_bytes; i++, fptr++, bptr++)
            *bptr = float_to_byte(*fptr);
    }
};

#ifdef _MSC_VER
    #pragma warning (pop)
#endif

#else /* embedded VexRiscV build ─────────────────────────────────────────── */
/*
 * Stub rtw_image: no filesystem on the VexRiscV target.
 * width() and height() always return 0, so image_texture::value() returns
 * the built-in cyan fallback color (existing "no image data" debug path).
 */
class rtw_image {
  public:
    rtw_image() {}
    rtw_image(const char* /* filename */) {}

    int width()  const { return 0; }
    int height() const { return 0; }

    const unsigned char* pixel_data(int /*x*/, int /*y*/) const {
        static unsigned char magenta[] = { 255, 0, 255 };
        return magenta;
    }
};

#endif /* OF_PC */
#endif /* RTW_STB_IMAGE_H */
