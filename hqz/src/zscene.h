#include <vector>

#include "zobject.h"
#include "zmaterial.h"
#include "zlight.h"

struct ZScene {
    int r_width;
    int r_height;
    int viewport[4];
    double exposure;
    double gamma;
    int seed;
    long int rays;
    long int timelimit;
    std::vector<ZMaterial> materials;
    std::vector<ZObject> objects;
    std::vector<ZLight> lights;
};
