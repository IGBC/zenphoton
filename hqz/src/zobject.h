/*
 * This file is part of HQZ, the batch renderer for Zen Photon Garden.
 *
 * Copyright (c) 2013 Micah Elizabeth Scott <micah@scanlime.org>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "ray.h"
#include "sampler.h"


/**
 * Utility class for working with scene objects in HQZ.
 * Understands the various types of scene objects, how to do a ray test,
 # and how to compute their AABB.
 */

class ZObject {
    public:
        int id;
        int material_id;
        bool curve; // true if a0 and da parameters are valide
        Sample x0, y0, a0, dx, dy, da; //coordinates

        bool rayIntersect(IntersectionData &d, Sampler &s);
        void getBounds(AABB &bounds);
};


inline bool ZObject::rayIntersect(IntersectionData &d, Sampler &s)
{
    /*
     * Does this ray intersect a specific object? This samples the object once,
     * filling in the required IntersectionData on success. If this ray and object
     * don't intersect at all, returns false.
     *
     * Does not write to d.object; it is assumed that the caller does this.
     */

    Vec2 origin = { s.value(x0), s.value(y0) };
    Vec2 delta  = { s.value(dx), s.value(dy) };

    if (!curve) {
        // Line segment
        if (d.ray.intersectSegment(origin, delta, d.distance)) {
            d.point = d.ray.pointAtDistance(d.distance);
            d.normal.x = -delta.y;
            d.normal.y = delta.x;
            return true;
        }
    } else {
        // Line segment with trigonometrically interpolated normals
        double alpha;

        if (d.ray.intersectSegment(origin, delta, d.distance, alpha)) {
            double degrees = s.value(a0) + alpha * s.value(da);
            double radians = degrees * (M_PI / 180.0);
            d.point = d.ray.pointAtDistance(d.distance);
            d.normal.x = cos(radians);
            d.normal.y = sin(radians);
            return true;
        }
    }
    return false;
}

inline void ZObject::getBounds(AABB &bounds)
{
    Sampler::Bounds sx0 = Sampler::bounds(x0);
    Sampler::Bounds sy0 = Sampler::bounds(y0);
    Sampler::Bounds sdx = Sampler::bounds(dx);
    Sampler::Bounds sdy = Sampler::bounds(dy);

    bounds.left = std::min( sx0.min + sdx.min, sx0.min );
    bounds.right = std::max( sx0.max + sdx.max, sx0.max );
    bounds.top = std::min( sy0.min + sdy.min, sy0.min );
    bounds.bottom = std::max( sy0.max + sdy.max, sy0.max );
}
