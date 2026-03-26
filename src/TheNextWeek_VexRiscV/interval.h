#ifndef INTERVAL_H
#define INTERVAL_H
//==============================================================================================
// VexRiscV / openfpgaOS port — float version
//
// Changes vs. original (TheNextWeek):
//   • All double members/parameters/return types → float
//   • Floating-point literals: added 'f' suffix
//   • Spanning constructor interval(a, b) — same logic, float
//   • expand(delta) — same logic, float
//   • operator+ for interval displacement — float displacement
//
// NOTE: infinity is defined in rtweekend.h and must be in scope before this
// header is processed.  interval.h is included from rtweekend.h after the
// infinity constant is defined.
//==============================================================================================

class interval {
  public:
    float min, max;

    interval() : min(+infinity), max(-infinity) {} // Default interval is empty

    interval(float min, float max) : min(min), max(max) {}

    interval(const interval& a, const interval& b) {
        // Create the interval tightly enclosing the two input intervals.
        min = a.min <= b.min ? a.min : b.min;
        max = a.max >= b.max ? a.max : b.max;
    }

    float size() const {
        return max - min;
    }

    bool contains(float x) const {
        return min <= x && x <= max;
    }

    bool surrounds(float x) const {
        return min < x && x < max;
    }

    float clamp(float x) const {
        if (x < min) return min;
        if (x > max) return max;
        return x;
    }

    interval expand(float delta) const {
        auto padding = delta / 2.0f;
        return interval(min - padding, max + padding);
    }

    static const interval empty, universe;
};

const interval interval::empty    = interval(+infinity, -infinity);
const interval interval::universe = interval(-infinity, +infinity);

inline interval operator+(const interval& ival, float displacement) {
    return interval(ival.min + displacement, ival.max + displacement);
}

inline interval operator+(float displacement, const interval& ival) {
    return ival + displacement;
}


#endif
