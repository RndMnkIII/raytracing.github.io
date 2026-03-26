#ifndef RTWEEKEND_H
#define RTWEEKEND_H
//==============================================================================================
// VexRiscV / openfpgaOS port — "Ray Tracing: The Next Week"
//
// Target: VexRiscV RV32IMF core running openfpgaOS (Analogue Pocket)
// Build:  See Makefile in this directory
//
// Changes vs. original:
//   • double → float  (VexRiscV RV32IMF has hardware 32-bit FP only)
//   • <cmath>  → compat/cmath   (std:: math wrappers via SDK math.h)
//   • <limits> → compat/limits  (minimal numeric_limits)
//   • <memory> → compat/memory  (shared_ptr/make_shared without libstdc++)
//   • <cstdlib>→ <stdlib.h>     (rand/srand/RAND_MAX via SDK stdlib.h)
//   • std::rand() → rand()      (global namespace, from SDK stdlib.h)
//   • random_int() added (TheNextWeek addition)
//==============================================================================================

#include "compat/cmath"    /* std::sqrt, std::tan, std::fabs, … → SDK float ops */
#include <stdlib.h>        /* rand(), RAND_MAX, malloc/free (SDK stdlib.h)       */
#include <iostream>        /* std::cout, std::cerr (SDK iostream)                */
#include "compat/limits"   /* std::numeric_limits<float>::infinity()             */
#include "compat/memory"   /* std::shared_ptr, std::make_shared                  */


// C++ Std Usings

using std::make_shared;
using std::shared_ptr;


// Constants

const float infinity = std::numeric_limits<float>::infinity();
const float pi = 3.14159265f;


// Utility Functions

inline float degrees_to_radians(float degrees) {
    return degrees * pi / 180.0f;
}

inline float random_double() {
    /* Returns a random real in [0,1). */
    return (float)rand() / (RAND_MAX + 1.0f);
}

inline float random_double(float min, float max) {
    /* Returns a random real in [min,max). */
    return min + (max - min) * random_double();
}

inline int random_int(int min, int max) {
    /* Returns a random integer in [min,max]. */
    return int(random_double((float)min, (float)(max + 1)));
}


// Common Headers

#include "color.h"
#include "interval.h"
#include "ray.h"
#include "vec3.h"


#endif
