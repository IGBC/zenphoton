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
 * Utility class for working with materials in HQZ.
 *
 * Understands the various formats of material outcomes, and how each outcome affects rays.
 * A material outcome is a JSON tuple beginning with a probability number. ZRender picks an
 * outcome according to these propabilities, and uses ZMaterial to handle the individual
 * properties of these outcome objects.
 */

struct ZMaterial {
    double t, d, r;

    bool rayOutcome(IntersectionData &id, Sampler &s);
};


inline bool ZMaterial::rayOutcome(IntersectionData &id, Sampler &s)
{
    /*
     * If the ray continues propagating, updates 'd' and returns true.
     * If the ray is absorbed, returns false.
     */

    double r = s.uniform();
    double sum = 0;
    
    // Loop over all material outcomes, pick one according to our random variable.
    if (r <= d) {
        id.ray.origin = id.point;
        id.ray.setAngle(s.uniform(0, M_PI * 2.0));
        return true;
    }

    if (r <= d+r) {
        id.ray.origin = id.point;
        id.ray.reflect(id.normal);
        return true;
    }
    
    if (r <= d+r+t) {
        id.ray.origin = id.point;
        return true;
    }

    // Unknown outcome
    return false;
}
