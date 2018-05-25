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

#include <float.h>
#include <time.h>
#include <thread>
#include "zrender.h"
#include "zmaterial.h"

#include <stdio.h>

ZRender::ZRender(ZScene scene, int seed, int rays)
    : mScene(scene),
    mLightPower(0.0),
    mBatches(),
    mBatchesMutex()
{
    // Optional iteger values
    mSeed = seed; //checkInteger(mScene["seed"], "seed");
    mDebug = mScene.debug;

    // Integer resolution values
    mImage.resize(mScene.r_width, mScene.r_height);

    // Check stopping conditions
    mRayLimit = rays; //checkNumber(mScene["rays"], "rays");
    mTimeLimit = mScene.timelimit;
    if (mRayLimit <= 0.0 && mTimeLimit <= 0.0) {
        mError << "No stopping conditions set. Expected a ray limit and/or time limit.\n";
    }

    printf("seed: %i, raycount: %d\n", mSeed, mRayLimit);
}

void ZRender::static_render(ZRender *zr)
{
    std::vector<unsigned char> pixels;
    return zr->render(pixels);
}

void ZRender::render(std::vector<unsigned char> &pixels)
{
    mQuadtree.build(mScene.objects);

    /*
     * Debug flags
     */

    if (mDebug & kDebugQuadtree) {
        ZQuadtree::Visitor v = ZQuadtree::Visitor::root(&mQuadtree);
        renderDebugQuadtree(v);
    }

    /*
     * Trace rays!
     */

    uint64_t numRays = traceRays();

    /*
     * Optional gamma correction. Defaults to linear, for compatibility with zenphoton.
     */

    double gamma = mScene.gamma;
    if (gamma <= 0.0)
        gamma = 1.0;

    /* 
     * Exposure calculation as a backward-compatible generalization of zenphoton.com.
     * We need to correct for differences due to resolution and due to the higher
     * fixed-point resolution we use during histogram rendering.
     */

    double exposure = mScene.exposure;
    double areaScale = sqrt(double(width()) * height() / (1024 * 576));
    double intensityScale = mLightPower / (255.0 * 8192.0);
    double scale = exp(1.0 + 10.0 * exposure) * areaScale * intensityScale / numRays;

    mImage.render(pixels, scale, 1.0 / gamma);
}

ZLight &ZRender::chooseLight(Sampler &s)
{
    // Pick a random light, using the light power as a probability weight.
    // Fast path for scenes with only one light.

    unsigned i = 0;
    unsigned last = mScene.lights.size() - 1;

    if (i != last) {
        double r = s.uniform(0, mLightPower);
        double sum = 0;

        // Check all lights except the last
        do {
            ZLight &light = mScene.lights[i++];
            sum += s.value(light.power);
            if (r <= sum)
                return light;
        } while (i != last);
    }

    // Default, last light.
    ZLight &l = mScene.lights[last];
    return l;
}

void ZRender::interrupt()
{
    /*
     * Interrupt traceRays() in progress. The render will return as
     * soon as the next batch of rays finishes and the histogram is rendered.
     */

    mRayLimit = -1;
    while(!mBatches.empty()) {
        mBatches.pop_back();
    }
}

uint64_t ZRender::traceRays()
{
    /*
     * Keep tracing rays until a stopping condition is hit.
     * Returns the total number of rays traced.
     */

    uint64_t rayCount = 0;
    uint32_t seed = mSeed;
    double startTime = (double)time(0);

    //mBatches.reserve(mRayLimit / batch);

    while (1) {
        // Minimum frequency for checking stopping conditions
        int batch = 1000;

        if (mRayLimit) {
            // Set batch size according to remaining rays.
            batch = std::min<double>(batch, mRayLimit - rayCount);
            if (batch <= 0)
                break;
        }

        if (mTimeLimit) {
            // Check time limit
            double now = (double)time(0);
            if (now > startTime + mTimeLimit)
                break;
        }

        traceRayBatch(seed, batch);

        seed += batch;
        rayCount += batch;            
    }

    return rayCount;
}

void ZRender::worker(ZRender *zr) {
    while (!zr->mBatches.empty()) {
        zr->mBatchesMutex.lock();
        Batch b = zr->mBatches.back();
        zr->mBatches.pop_back();
        zr->mBatchesMutex.unlock();
        zr->traceRayBatch(b.seed, b.size);
    }
}

void ZRender::traceRayBatch(uint32_t seed, uint32_t count)
{
    /*
     * Trace a batch of rays, starting with ray number "start", and
     * tracing "count" rays.
     *
     * Note that each ray is seeded separately, so that rays are independent events
     * with respect to the PRNG sequence. This helps keep our noise pattern stationary,
     * which is a nice effect to have during animation.
     */

    while (count--) {
        Sampler s(seed++);
        traceRay(s);
    }
}

void ZRender::traceRay(Sampler &s)
{
    IntersectionData d;
    d.zobject_id = 0;

    double w = width();
    double h = height();

    // Initialize the ray by sampling a light
    if (!initRay(s, d.ray, chooseLight(s)))
        return;

    // Sample the viewport once per ray. (e.g. for camera motion blur)
    ViewportSample v;
    initViewport(s, v);

    // Look for a large but bounded number of bounces
    for (unsigned bounces = 1000; bounces; --bounces) {

        // Intersect with an object or the edge of the viewport
        bool hit = rayIntersect(d, s, v);

        // Draw a line from d.ray.origin to d.point
        mImage.line( d.ray.color,
            v.xScale(d.ray.origin.x, w),
            v.yScale(d.ray.origin.y, h),
            v.xScale(d.point.x, w),
            v.yScale(d.point.y, h));

        if (!hit) {
            // Ray exited the scene after this.
            break;
        }

        if (!rayMaterial(d, s)) {
            // Ray was absorbed by material
            break;
        }
    }
}

bool ZRender::initRay(Sampler &s, Ray &r, ZLight &light)
{
    double cartesianX = s.value(light.x);
    double cartesianY = s.value(light.y);
    double polarAngle = s.value(light.pol_angle) * (M_PI / 180.0);
    double polarDistance = s.value(light.pol_distance);
    r.origin.x = cartesianX + cos(polarAngle) * polarDistance;
    r.origin.y = cartesianY + sin(polarAngle) * polarDistance;

    double rayAngle = s.value(light.ray_angle) * (M_PI / 180.0);
    r.setAngle(rayAngle);

    /*
     * Try to discard rays for invisible wavelengths without actually
     * counting them as real rays. (If we count them without tracing them,
     * our scene will render as unexpectedly dark since some of the
     * photons will not be visible on the rendered image.)
     */

    unsigned tries = 1000;
    for (;;) {
        double wavelength = s.value(light.wavelength);
        r.color.setWavelength(wavelength);
        if (r.color.isVisible()) {
            // Success
            break;
        } else {
            // Can we keep looking?
            if (!--tries)
                return false;
        }
    }

    return true;
}

void ZRender::initViewport(Sampler &s, ViewportSample &v)
{
    // Sample the viewport. We do this once per ray.

    v.origin.x = s.value(mScene.viewport.x);
    v.origin.y = s.value(mScene.viewport.y);
    v.size.x = s.value(mScene.viewport.width);
    v.size.y = s.value(mScene.viewport.height);
}

bool ZRender::rayIntersect(IntersectionData &d, Sampler &s, const ViewportSample &v)
{
    /*
     * Sample all objects in the scene that d.ray might hit. If we hit an object,
     * updates 'd' and returns true. If we don't, d.point will be clipped to the
     * edge of the image by rayIntersectBounds() and we return 'false'.
     */

    if (mQuadtree.rayIntersect(d, s)) {
        // Quadtree found an intersection
        return true;
    }

    // Nothing. Intersect with the viewport bounds.
    rayIntersectBounds(d, v);
    return false;
}

void ZRender::rayIntersectBounds(IntersectionData &d, const ViewportSample &v)
{
    /*
     * This ray is never going to hit an object. Update d.point with the furthest away
     * viewport intersection, if any. If this ray never intersects the viewport, d.point
     * will equal d.ray.origin.
     */

    AABB viewport = {
        v.origin.x,
        v.origin.y,
        v.origin.x + v.size.x,
        v.origin.y + v.size.y
    };

    d.point = d.ray.pointAtDistance(d.ray.intersectFurthestAABB(viewport));
}

bool ZRender::rayMaterial(IntersectionData &d, Sampler &s)
{
    /*
     * Sample the material indicated by the intersection 'd' and update the ray.
     * Returns 'true' if the ray continues propagating, or 'false' if it's abosrbed.
     */

    // Lookup in our material database
    ZObject object = mScene.objects[d.zobject_id];
    unsigned id = object.material_id;
    ZMaterial material = mScene.materials[id];
    return material.rayOutcome(d, s);
}

void ZRender::renderDebugQuadtree(ZQuadtree::Visitor &v)
{
    /*
     * For debugging, draw an outline around each quadtree AABB.
     */

    ViewportSample vp;
    Sampler s(mSeed);
    initViewport(s, vp);

    Color c = { 0x7FFFFFFF, 0x7FFFFFFF, 0 };
    double w = width();
    double h = height();

    double left   = vp.xScale(v.bounds.left,   w);
    double top    = vp.yScale(v.bounds.top,    h);
    double right  = vp.xScale(v.bounds.right,  w);
    double bottom = vp.yScale(v.bounds.bottom, h);

    mImage.line(c, left, top, right, top);
    mImage.line(c, right, top, right, bottom);
    mImage.line(c, right, bottom, left, bottom);
    mImage.line(c, left, bottom, left, top);

    ZQuadtree::Visitor first = v.first();
    if (first) renderDebugQuadtree(first);

    ZQuadtree::Visitor second = v.second();
    if (second) renderDebugQuadtree(second);
}
