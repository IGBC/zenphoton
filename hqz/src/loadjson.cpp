#include "loadjson.h"

#include "rapidjson/document.h"
#include "rapidjson/reader.h"
#include "rapidjson/filestream.h"


#include <sstream>

typedef rapidjson::Value Value;

std::ostringstream mError;

int checkInteger(const Value &v, const char *noun)
{
    /*
     * Convenience method to evaluate an integer. If it's Null, returns zero quietly.
     * If it's a valid integer, returns it quietly. Otherwise returns zero and logs an error.
     */

    if (v.IsNull())
        return 0;

    if (v.IsInt())
        return v.GetInt();

    mError << "'" << noun << "' expected an integer value\n";
    return 0;
}

double checkNumber(const Value &v, const char *noun)
{
    /*
     * Convenience method to evaluate a number. If it's Null, returns zero quietly.
     * If it's a valid number, returns it quietly. Otherwise returns zero and logs an error.
     */

    if (v.IsNull())
        return 0;

    if (v.IsNumber())
        return v.GetDouble();

    mError << "'" << noun << "' expected a number value\n";
    return 0;
}

bool checkTuple(const Value &v, const char *noun, unsigned expected)
{
    /*
     * A quick way to check for a tuple with an expected length, and set
     * an error if there's any problem. Returns true on success.
     */

    if (!v.IsArray() || v.Size() < expected) {
        mError << "'" << noun << "' expected an array with at least "
               << expected << " item" << (expected == 1 ? "" : "s") << "\n";
        return false;
    }

    return true;
}

bool checkMaterialID(const Value &v, int numMaterials)
{
    // Check for a valid material ID.

    if (!v.IsUint()) {
        mError << "Material ID must be an unsigned integer\n";
        return false;
    }

    if (v.GetUint() >= numMaterials) {
        mError << "Material ID (" << v.GetUint() << ") out of range\n";
        return false;
    }

    return true;
}

bool checkMaterialValue(const Value &v)
{
    if (!v.IsArray()) {
        //mError << "Material #" << index << " is not an array\n";
        return false;
    }

    bool result = true;
    for (unsigned i = 0, e = v.Size(); i != e; ++i) {
        const Value& outcome = v[i];

        if (!outcome.IsArray() || outcome.Size() < 1 || !outcome[0u].IsNumber()) {
            //mError << "Material #" << index << " outcome #" << i << "is not an array starting with a number\n";
            result = false;
        }
    }

    return result;
}

Sample new_sample(const Value &v) {
    Sample s = {
        STYPE_CONST,
        0,
        0,
    };

    if (v.IsNumber()) {
            // Constant
        s.lower = v.GetDouble();
        s.type = STYPE_CONST;
        return s;
    }

    if (v.IsArray() && v.Size() == 2 && v[0u].IsNumber()) {
        // 2-tuples starting with a number

        if (v[1].IsNumber()) {
            double v0 = v[0u].GetDouble();
            double v1 = v[1].GetDouble();
            if (v0 > v1) {
                std::swap(v0, v1);
            }
            s.lower = v0;
            s.upper = v1;
            s.type = STYPE_LINIER;
            return s;
        }

        if (v[1].IsString() && v[1].GetStringLength() == 1 && v[1].GetString()[0] == 'K') {
            double v0 = v[0u].GetDouble();
            s.lower = v0;
            s.type = STYPE_LINIER;
            return s;
        }
    }

    // Unknown
    return s; //represents null
}

ZScene parseJson(FILE *scene_f) {
    fprintf(stderr, "Opening Scene\n");
    rapidjson::FileStream istr(scene_f);
    rapidjson::Document scene;
    scene.ParseStream<0>(istr);
    if (scene.HasParseError()) {
        fprintf(stderr, "Parse error at character %ld: %s\n",
            scene.GetErrorOffset(), scene.GetParseError());
        //TODO: ERROR OUT.
    }

    // Create Output
    ZScene output;

    fprintf(stderr, "Reading Scene Globals\n");
    const Value& resolution = scene["resolution"];    
    if (checkTuple(resolution, "resolution", 2)) {
        output.r_width = checkInteger(resolution[0u], "resolution[0]");
        output.r_height = checkInteger(resolution[1], "resolution[1]");
    }

    output.exposure = checkNumber(scene["exposure"], "exposure");
    output.gamma = checkNumber(scene["gamma"], "gamma");
    output.debug = checkInteger(scene["debug"], "debug");
    output.seed = checkInteger(scene["seed"], "seed");
    output.rays = checkInteger(scene["rays"], "rays");
    output.timelimit = checkInteger(scene["rays"], "rays");

    // Other cached tuples
    if (checkTuple(scene["viewport"], "viewport", 4)) {
        output.viewport.x = new_sample(scene["viewport"][0u]);
        output.viewport.y = new_sample(scene["viewport"][1]);
        output.viewport.width = new_sample(scene["viewport"][2]);
        output.viewport.height = new_sample(scene["viewport"][3]);
    }

    fprintf(stderr, "Adding Lights\n");
    // Add up the total light power in the scene, and check all lights.
    double mLightPower = 0.0;
    const Value &mLights = scene["lights"];
    if (checkTuple(mLights, "viewport", 1)) {
        for (unsigned i = 0; i < mLights.Size(); ++i) {
            const Value &light = mLights[i];
            if (checkTuple(light, "light", 7)) {
                ZLight l;
                l.power = new_sample(light[0u]);
                l.x = new_sample(light[1]);
                l.y = new_sample(light[2]);
                l.pol_angle = new_sample(light[3]);
                l.pol_distance = new_sample(light[4]);
                l.ray_angle = new_sample(light[5]);
                l.wavelength = new_sample(light[6]);
                output.lights.push_back(l);
                mLightPower += light[0u].GetDouble();
            }
        }
    }
    if (mLightPower <= 0.0) {
        //mError << "Total light power (" << mLightPower << ") must be positive.\n";
    }
    output.lightPower = mLightPower;

    // Check all materials
    fprintf(stderr, "Computing Materials\n");
    const Value &mMaterials = scene["materials"];
    if (checkTuple(mMaterials, "materials", 0)) {
        for (unsigned i = 0; i < mMaterials.Size(); ++i) {
            
            fprintf(stderr, "Material %i\n", i);
            const Value &mat = mMaterials[i];
            if (checkMaterialValue(mat)) {
                fprintf(stderr, "Material %i Passes\n", i);
                ZMaterial m = {0,0,0};
                fprintf(stderr, "Material %i has size %i\n", i, mat.Size());
                for (unsigned j = 0; j < mat.Size(); ++j) {
                    char key = mat[j][1].GetString()[0];
                    double val = mat[j][0u].GetDouble();
                    fprintf(stderr, "Material %i %c %lf\n", i, key, val);
                    switch (key) {
                        case 'd': m.d = val; break;
                        case 'r': m.r = val; break;
                        case 't': m.t = val; break;
                    }
                }
                output.materials.push_back(m);
            }
        }
    }

    // Check all objects
    fprintf(stderr, "Adding Objects\n");
    const Value &mObjects = scene["objects"];
    if (checkTuple(mObjects, "objects", 0)) {
        for (unsigned i = 0; i < mObjects.Size(); ++i) {
            const Value &object = mObjects[i];
            if (checkTuple(object, "object", 5)) {
                checkMaterialID(object[0u], mMaterials.Size());
                ZObject o;
                o.material_id = object[0u].GetDouble();
                o.x0 = new_sample(object[1]);
                o.y0 = new_sample(object[2]);
                if (object.Size() == 7) {
                    o.curve = true;
                    o.a0 = new_sample(object[3]);
                    o.dx = new_sample(object[4]);
                    o.dy = new_sample(object[5]);
                    o.da = new_sample(object[6]);
                } else {
                    o.curve = false;
                    o.dx = new_sample(object[3]);
                    o.dy = new_sample(object[4]);
                }
                o.id = i;
                output.objects.push_back(o);
            }
        }
    }

    fprintf(stderr, "Scene errors:\n%s", mError.str().c_str());

    return output;
}