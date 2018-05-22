#include "loadjson.h"

#include "rapidjson/document.h"
#include "rapidjson/reader.h"
#include "rapidjson/filestream.h"

typedef rapidjson::Value Value;

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

    //mError << "'" << noun << "' expected an integer value\n";
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

    //mError << "'" << noun << "' expected a number value\n";
    return 0;
}

bool checkTuple(const Value &v, const char *noun, unsigned expected)
{
    /*
     * A quick way to check for a tuple with an expected length, and set
     * an error if there's any problem. Returns true on success.
     */

    if (!v.IsArray() || v.Size() < expected) {
        //mError << "'" << noun << "' expected an array with at least "
        //    << expected << " item" << (expected == 1 ? "" : "s") << "\n";
        return false;
    }

    return true;
}


ZScene parseJson(FILE *scene_f) {
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

    const Value& resolution = scene["resolution"];    
    if (checkTuple(resolution, "resolution", 2)) {
        output.r_width = checkInteger(resolution[0u], "resolution[0]");
        output.r_height = checkInteger(resolution[1], "resolution[1]");
    }

    output.exposure = checkNumber(scene["exposure"], "exposure");
    output.gamma = checkNumber(scene["gamma"], "gamma");
    output.seed = checkInteger(scene["seed"], "seed");
    output.rays = checkInteger(scene["rays"], "rays");
    output.timelimit = checkInteger(scene["rays"], "rays");
}