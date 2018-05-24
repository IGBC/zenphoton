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

#include "loadjson.h"
#include "lodepng.h"
#include "zrender.h"
#include <signal.h>
#include <unistd.h>
#include <cstdio>
#include <vector>
#include <thread>

static ZRender *interruptibleRenderer = 0;

typedef struct {
    std::thread *thread;
    ZRender *zr;
} ThreadRuntime;

void handleSigint(int)
{
    static const char message[] = "\nInterrupted! Finishing up...\n";

    if (interruptibleRenderer) {
        // Immediately write a message to stderr.
        // Stdio probably not signal-safe, but syscalls should be...
        write(2, message, sizeof message);

        // Tell the renderer to finish up!
        interruptibleRenderer->interrupt();
        interruptibleRenderer = 0;
    }
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr,
            "\n"
            "High Quality Zen: The batch renderer for Zen photon garden\n"
            "\n"
            "usage: hqz <scene.json> <output.png>\n"
            "  (Either may be \"-\" for stdin/stdout)\n"
            "\n"
            "Copyright (c) 2013 Micah Elizabeth Scott <micah@scanlime.org>\n"
            "https://github.com/scanlime/zenphoton\n"
            "\n");
        return 1;
    }

    FILE *sceneF = argv[1][0] == '-' ? stdin : fopen(argv[1], "r");
    if (!sceneF) {
        perror("Error opening scene file");
        return 2;
    }

    FILE *outputF = argv[2][0] == '-' ? stdout : fopen(argv[2], "wb");
    if (!outputF) {
        perror("Error opening output file");
        return 3;
    }

    std::vector<ThreadRuntime> trv;
    int n = 10; //std::thread::hardware_concurrency();
    ZScene scene = parseJson(sceneF);
    int rays = scene.rays;
    int seed = scene.seed;
    int rays_remaining = rays;

    printf("Main: seed: %i, raycount: %d\n", seed, rays);
    for (int i = 0; i < n; i++) {
        int r = std::min<int>(rays/n, rays_remaining);
        //ZScene st = copy_scene(scene);
        ZRender *zr = new ZRender(scene, seed, r);
        
        if (zr->hasError()) {
            fprintf(stderr, "Scene errors:\n%s", zr->errorText());
            return 5;
        }

        trv.push_back({ NULL, zr });

        rays_remaining -= r;
        seed += r;
    }

    for (int i = 0; i < n; i++) {
        std::thread *t = new std::thread(trv[i].zr->static_render, trv[i].zr);
        trv[i].thread = t;
    }

    while (!trv.empty()) {
        std::thread *t = trv.back().thread;
        t->join();
        if (trv.back().zr->hasError()) {
            fprintf(stderr, "Renderer errors:\n%s", trv.back().zr->errorText());
            return 7;
        }   
        trv.pop_back();
    }

    // // Render, and allow Ctrl-C to interrupt at any time.
    // interruptibleRenderer = &zr;
    // signal(SIGINT, handleSigint);
    // zr.render(pixels);
    // interruptibleRenderer = 0;

    // std::vector<unsigned char> png;
    // lodepng::encode(png, pixels, zr.width(), zr.height(), LCT_RGB);
    // if (1 != fwrite(&png[0], png.size(), 1, outputF)) {
    //     perror("Error writing output file");
    //     return 6;
    // }

    return 0;
}
