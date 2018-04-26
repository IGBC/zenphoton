#!/usr/bin/env coffee
#
# Modification of sine.coffee, to act as a benchmark scene.
# This has many small objecs, a lot of ray bouncing, a large scene,
# and random variables as object positions.
#
# Micah Elizabeth Scott <micah@scanlime.org>
# Creative Commons BY-SA license:
# http://creativecommons.org/licenses/by-sa/3.0/
#

RAYS = 100000

TAU = Math.PI * 2
deg = (degrees) -> degrees * TAU / 360
plot = require './plot'

lerp = (frame, length, a, b) ->
    a + (b - a) * frame / length

sunlight = (frame) ->
    # Warm point source
    x = lerp frame, 600, 3440, -200
    [ 1.0, x, -20, 0, 0, [90, 180], [5000, 'K'] ]

sealight = (frame) ->
    # Big diffuse light, with a blueish color
    x = lerp frame, 600, 3440, -800
    [ 0.2, [0, 3440], 1500, 0, 0, [235, 315], [10000, 'K'] ]

sineFunc = (frame, seed, x0, y0, w, h, angle) ->
    (t) ->
        e = lerp frame, 600, 300, 50
        u = lerp frame, 600, 20, 10
        scale = Math.pow(e, t)
        x = w * t
        y = h * Math.sin(t*(u + scale)) / (1 + scale)
        dx = Math.cos angle
        dy = Math.sin angle
        [ x0 + dx*x + dy*y, y0 + dy*x - dx*y ]

fuzzify = (objs) ->
    # Use random variables for line position, to test AABBs
    for o in objs
        o[1] = [ o[1] - 5, o[1] + 5 ]
        o[2] = [ o[2] - 5, o[2] + 5 ]
    return objs    

scene = (frame) ->
    resolution: [3440, 1440]
    rays: RAYS
    exposure: 0.60
    gamma: 1.4

    viewport: [0, 0, 3440, 1440]
    seed: frame * RAYS / 20

    lights: [
        sunlight frame
        sealight frame
    ]

    materials: [
        [ [0.0, "d"], [1.0, "r"], [0.0, "t"] ]
    ]

    objects: [
        # No fixed objects
    ].concat(
        fuzzify plot
            material: 0
            sineFunc frame, '1', -140, 720, 5000, 1200, deg -5 
    )

console.log JSON.stringify scene 0
