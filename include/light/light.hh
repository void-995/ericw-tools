/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#pragma once

#include <common/cmdlib.hh>
#include <common/mathlib.hh>
#include <common/bspfile.hh>
#include <common/log.hh>
#include <common/threads.hh>
#include <common/polylib.hh>
#include <common/imglib.hh>
#include <common/settings.hh>
#include <common/bitflags.hh>

#include <light/litfile.hh>
#include <light/trace.hh>

#include <vector>
#include <map>
#include <set>
#include <string>
#include <cassert>
#include <limits>
#include <sstream>

#include <common/qvec.hh>
#include <common/bsputils.hh>

constexpr vec_t LIGHT_ON_EPSILON = 0.1;
constexpr vec_t LIGHT_ANGLE_EPSILON = 0.001;
constexpr vec_t LIGHT_EQUAL_EPSILON = 0.001;

// FIXME: use maximum dimension of level
constexpr vec_t MAX_SKY_DIST = 1000000;

struct lightsample_t
{
    qvec3f color;
    qvec3d direction;
};

// CHECK: isn't average a bad algorithm for color brightness?
template<typename T>
constexpr float LightSample_Brightness(const T &color)
{
    return ((color[0] + color[1] + color[2]) / 3.0);
}

/**
 * A directional light, emitted from "sky*" textured faces.
 */
class sun_t
{
public:
    qvec3d sunvec;
    vec_t sunlight;
    qvec3d sunlight_color;
    bool dirt;
    float anglescale;
    int style;
    std::string suntexture;
    const img::texture *suntexture_value;
};

class modelinfo_t;
namespace settings
{
class worldspawn_keys;
};

class lightmap_t
{
public:
    int style;
    std::vector<lightsample_t> samples;
};

using lightmapdict_t = std::vector<lightmap_t>;

struct lightsurf_t
{
    const settings::worldspawn_keys *cfg;
    const modelinfo_t *modelinfo;
    const mbsp_t *bsp;
    const mface_t *face;
    /* these take precedence the values in modelinfo */
    vec_t minlight, maxlight, lightcolorscale = 1.0;
    qvec3d minlight_color;
    bool nodirt, minlightMottle;

    qplane3d plane;
    qvec3d snormal;
    qvec3d tnormal;

    /* 16 in vanilla. engines will hate you if this is not power-of-two-and-at-least-one */
    float lightmapscale;
    bool curved; /*normals are interpolated for smooth lighting*/

    faceextents_t extents, vanilla_extents;

    // width * height sample points in world space
    std::vector<qvec3d> points;
    std::vector<qvec3d> normals;
    std::vector<bool> occluded;
    std::vector<int> realfacenums;

    /*
     raw ambient occlusion amount per sample point, 0-1, where 1 is
     fully occluded. dirtgain/dirtscale are not applied yet
     */
    std::vector<float> occlusion;

    /*
     pvs for the entire light surface. generated by ORing together
     the pvs at each of the sample points
     */
    std::vector<uint8_t> pvs;

    // output width * extra
    int width;
    // output height * extra
    int height;

    /* for lit water. receive light from either front or back. */
    bool twosided;

    // ray batch stuff
    raystream_occlusion_t occlusion_stream;
    raystream_intersection_t intersection_stream;

    lightmapdict_t lightmapsByStyle;
};

/* debug */

enum class debugmodes
{
    none = 0,
    phong,
    phong_obj,
    dirt,
    bounce,
    bouncelights,
    debugoccluded,
    debugneighbours,
    phong_tangents,
    phong_bitangents,
    mottle
};

enum class lightfile
{
    none = 0,
    external = 1,
    bspx = 2,
    both = external | bspx,
    lit2 = 4
};

/* tracelist is a std::vector of pointers to modelinfo_t to use for LOS tests */
extern std::vector<const modelinfo_t *> tracelist;
extern std::vector<const modelinfo_t *> selfshadowlist;
extern std::vector<const modelinfo_t *> shadowworldonlylist;
extern std::vector<const modelinfo_t *> switchableshadowlist;

extern int numDirtVectors;

// other flags

extern bool dirt_in_use; // should any dirtmapping take place? set in SetupDirt

constexpr qvec3d vec3_white{255};

extern int dump_facenum;
extern int dump_vertnum;

class modelinfo_t : public settings::setting_container
{
public:
    static constexpr vec_t DEFAULT_PHONG_ANGLE = 89.0;

public:
    const mbsp_t *bsp;
    const dmodelh2_t *model;
    float lightmapscale;
    qvec3d offset;
    
    settings::setting_scalar minlight;
    // zero will apply no clamping; use lightignore instead to do that.
    // above zero, this controls the clamp value on the light, default 255
    settings::setting_scalar maxlight;
    settings::setting_bool minlightMottle;
    settings::setting_scalar shadow;
    settings::setting_scalar shadowself;
    settings::setting_scalar shadowworldonly;
    settings::setting_scalar switchableshadow;
    settings::setting_int32 switchshadstyle;
    settings::setting_scalar dirt;
    settings::setting_scalar phong;
    settings::setting_scalar phong_angle;
    settings::setting_scalar alpha;
    settings::setting_color minlight_color;
    settings::setting_bool lightignore;
    settings::setting_scalar lightcolorscale;

    float getResolvedPhongAngle() const;
    bool isWorld() const;

    modelinfo_t(const mbsp_t *b, const dmodelh2_t *m, float lmscale);
};

enum class visapprox_t
{
    NONE,
    AUTO,
    VIS,
    RAYS
};

//
// worldspawn keys / command-line settings
//

enum
{
    // Q1-style surface light copies
    SURFLIGHT_Q1 = 0,
    // Q2/Q3-style radiosity
    SURFLIGHT_RAD = 1
};

namespace settings
{
extern setting_group worldspawn_group;

class worldspawn_keys : public virtual setting_container
{
public:
    setting_scalar scaledist;
    setting_scalar rangescale;
    setting_scalar global_anglescale;
    setting_scalar lightmapgamma;
    setting_bool addminlight;
    setting_scalar minlight;
    setting_scalar maxlight;
    setting_color minlight_color;
    setting_bool spotlightautofalloff;
    // start index for switchable light styles, default 32
    setting_int32 compilerstyle_start;
    // max index for switchable light styles, default 64
    setting_int32 compilerstyle_max;

    /* dirt */
    // apply dirt to all lights (unless they override it) + sunlight + minlight?
    setting_bool globalDirt;
    setting_scalar dirtMode;
    setting_scalar dirtDepth;
    setting_scalar dirtScale;
    setting_scalar dirtGain;
    setting_scalar dirtAngle;
    setting_bool minlightDirt;

    /* phong */
    setting_bool phongallowed;
    setting_scalar phongangle;

    /* bounce */
    setting_bool bounce;
    setting_bool bouncestyled;
    setting_scalar bouncescale;
    setting_scalar bouncecolorscale;
    setting_scalar bouncelightsubdivision;

    /* Q2 surface lights (mxd) */
    setting_scalar surflightscale;
    setting_scalar surflightskyscale;
    // "choplight" - arghrad3 name
    setting_scalar surflightsubdivision;

    /* sunlight */
    /* sun_light, sun_color, sun_angle for http://www.bspquakeeditor.com/arghrad/ compatibility */
    setting_scalar sunlight;
    setting_color sunlight_color;
    setting_scalar sun2;
    setting_color sun2_color;
    setting_scalar sunlight2;
    setting_color sunlight2_color;
    setting_scalar sunlight3;
    setting_color sunlight3_color;
    setting_scalar sunlight_dirt;
    setting_scalar sunlight2_dirt;
    /* defaults to straight down */
    setting_mangle sunvec;
    /* defaults to straight down */
    setting_mangle sun2vec;
    setting_scalar sun_deviance;
    setting_color sky_surface;
    setting_int32 surflight_radiosity;

    worldspawn_keys();
};

extern setting_group output_group;
extern setting_group debug_group;
extern setting_group postprocessing_group;
extern setting_group experimental_group;

class light_settings : public common_settings, public worldspawn_keys
{
public:
    // slight modification to setting_numeric that supports
    // a default value if a non-number is supplied after parsing
    class setting_soft : public setting_int32
    {
    public:
        using setting_int32::setting_int32;

        bool parse(const std::string &settingName, parser_base_t &parser, source source) override;
        std::string format() const override;
    };

    class setting_extra : public setting_value<int32_t>
    {
    public:
        using setting_value::setting_value;

        bool parse(const std::string &settingName, parser_base_t &parser, source source) override;
        std::string stringValue() const override;
        std::string format() const override;
    };

    void CheckNoDebugModeSet();

    setting_bool surflight_dump;
    setting_scalar surflight_subdivide;
    setting_bool onlyents;
    setting_bool write_normals;
    setting_bool novanilla;
    setting_scalar gate;
    setting_int32 sunsamples;
    setting_bool arghradcompat;
    setting_bool nolighting;
    setting_vec3 debugface;
    setting_vec3 debugvert;
    setting_bool highlightseams;
    setting_soft soft;
    setting_set radlights;
    setting_int32 lightmap_scale;
    setting_extra extra;
    setting_bool fastbounce;
    setting_enum<visapprox_t> visapprox;
    setting_func lit;
    setting_func lit2;
    setting_func bspxlit;
    setting_func lux;
    setting_func bspxlux;
    setting_func bspxonly;
    setting_func bspx;
    setting_scalar world_units_per_luxel;
    setting_bool litonly;
    setting_bool nolights;
    setting_int32 facestyles;
    setting_bool exportobj;
    setting_int32 lmshift;

    setting_func dirtdebug;
    setting_func bouncedebug;
    setting_func bouncelightsdebug;
    setting_func phongdebug;
    setting_func phongdebug_obj;
    setting_func debugoccluded;
    setting_func debugneighbours;
    setting_func debugmottle;

    light_settings();

    fs::path sourceMap;

    bitflags<lightfile> write_litfile = lightfile::none;
    bitflags<lightfile> write_luxfile = lightfile::none;
    debugmodes debugmode = debugmodes::none;

    void setParameters(int argc, const char **argv) override;
    void initialize(int argc, const char **argv) override;
    void postinitialize(int argc, const char **argv) override;
    void reset() override;
};
}; // namespace settings

extern settings::light_settings light_options;

extern std::vector<uint8_t> filebase;
extern std::vector<uint8_t> lit_filebase;
extern std::vector<uint8_t> lux_filebase;

bool IsOutputtingSupplementaryData();

std::vector<std::unique_ptr<lightsurf_t>> &LightSurfaces();

extern std::vector<surfflags_t> extended_texinfo_flags;

// public functions

void FixupGlobalSettings(void);
void GetFileSpace(uint8_t **lightdata, uint8_t **colordata, uint8_t **deluxdata, int size);
void GetFileSpace_PreserveOffsetInBsp(uint8_t **lightdata, uint8_t **colordata, uint8_t **deluxdata, int lightofs);
const modelinfo_t *ModelInfoForModel(const mbsp_t *bsp, int modelnum);
/**
 * returns nullptr for "skip" faces
 */
const modelinfo_t *ModelInfoForFace(const mbsp_t *bsp, int facenum);
const img::texture *Face_Texture(const mbsp_t *bsp, const mface_t *face);
const qvec3b &Face_LookupTextureColor(const mbsp_t *bsp, const mface_t *face);
const qvec3d &Face_LookupTextureBounceColor(const mbsp_t *bsp, const mface_t *face);
void light_reset();
int light_main(int argc, const char **argv);
int light_main(const std::vector<std::string> &args);
