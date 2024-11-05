
local findtable = require("findtable")
local E = findtable.makeEntry

---@type findtable.config
local lstg_Particle2D = {
    cpp_namespace = { "LuaSTGPlus" },
    func_name = "MapParticle2DMember",
    enum_name = "Particle2DMember",
    enum_entry = {
        E("x"         , "x"         ),
        E("y"         , "y"         ),
        E("ax"        , "ax"        ),
        E("ay"        , "ay"        ),
        E("sx"        , "sx"        ),
        E("sy"        , "sy"        ),
        E("vx"        , "vx"        ),
        E("vy"        , "vy"        ),
        E("a"         , "a"         ),
        E("r"         , "r"         ),
        E("g"         , "g"         ),
        E("b"         , "b"         ),
        E("color"     , "color"     ),
        E("rot"       , "rot"       ),
        E("omiga"     , "omiga"     ),
        E("timer"     , "timer"     ),
        E("extra1"    , "extra1"    ),
        E("extra2"    , "extra2"    ),
        E("extra3"    , "extra3"    ),
    },
}

---@type findtable.config
local lstg_Particle3D = {
    cpp_namespace = { "LuaSTGPlus" },
    func_name = "MapParticle3DMember",
    enum_name = "Particle3DMember",
    enum_entry = {
        E("x"         , "x"         ),
        E("y"         , "y"         ),
        E("y"         , "z"         ),
        E("ax"        , "ax"        ),
        E("ay"        , "ay"        ),
        E("ay"        , "az"        ),
        E("ox"        , "ox"        ),
        E("oy"        , "oy"        ),
        E("oy"        , "oz"        ),
        E("rx"        , "rx"        ),
        E("ry"        , "ry"        ),
        E("ry"        , "rz"        ),
        E("sx"        , "sx"        ),
        E("sy"        , "sy"        ),
        E("vx"        , "vx"        ),
        E("vy"        , "vy"        ),
        E("vy"        , "vz"        ),
        E("a"         , "a"         ),
        E("r"         , "r"         ),
        E("g"         , "g"         ),
        E("b"         , "b"         ),
        E("color"     , "color"     ),
        E("timer"     , "timer"     ),
        E("extra1"    , "extra1"    ),
        E("extra2"    , "extra2"    ),
        E("extra3"    , "extra3"    ),
        E("extra4"    , "extra4"    ),
    },
}

---@type findtable.config
local lstg_TexParticle2D = {
    cpp_namespace = { "LuaSTGPlus" },
    func_name = "MapTexParticle2DMember",
    enum_name = "TexParticle2DMember",
    enum_entry = {
        E("x"         , "x"         ),
        E("y"         , "y"         ),
        E("ax"        , "ax"        ),
        E("ay"        , "ay"        ),
        E("sx"        , "sx"        ),
        E("sy"        , "sy"        ),
        E("vx"        , "vx"        ),
        E("vy"        , "vy"        ),
        E("a"         , "a"         ),
        E("r"         , "r"         ),
        E("g"         , "g"         ),
        E("b"         , "b"         ),
        E("color"     , "color"     ),
        E("rot"       , "rot"       ),
        E("omiga"     , "omiga"     ),
        E("timer"     , "timer"     ),
        E("extra1"    , "extra1"    ),
        E("extra2"    , "extra2"    ),
        E("extra3"    , "extra3"    ),
        E("u"         , "u"         ),
        E("v"         , "v"         ),
        E("w"         , "w"         ),
        E("h"         , "h"         ),
    },
}

---@type findtable.config
local lstg_TexParticle3D = {
    cpp_namespace = { "LuaSTGPlus" },
    func_name = "MapTexParticle3DMember",
    enum_name = "TexParticle3DMember",
    enum_entry = {
        E("x"         , "x"         ),
        E("y"         , "y"         ),
        E("y"         , "z"         ),
        E("ax"        , "ax"        ),
        E("ay"        , "ay"        ),
        E("ay"        , "az"        ),
        E("ox"        , "ox"        ),
        E("oy"        , "oy"        ),
        E("oy"        , "oz"        ),
        E("rx"        , "rx"        ),
        E("ry"        , "ry"        ),
        E("ry"        , "rz"        ),
        E("sx"        , "sx"        ),
        E("sy"        , "sy"        ),
        E("vx"        , "vx"        ),
        E("vy"        , "vy"        ),
        E("vy"        , "vz"        ),
        E("a"         , "a"         ),
        E("r"         , "r"         ),
        E("g"         , "g"         ),
        E("b"         , "b"         ),
        E("color"     , "color"     ),
        E("timer"     , "timer"     ),
        E("extra1"    , "extra1"    ),
        E("extra2"    , "extra2"    ),
        E("extra3"    , "extra3"    ),
        E("extra4"    , "extra4"    ),
        E("u"         , "u"         ),
        E("v"         , "v"         ),
        E("w"         , "w"         ),
        E("h"         , "h"         ),
    },
}

findtable.makeSource("lua_particle_hash", {
    lstg_Particle2D,
    lstg_Particle3D,
    lstg_TexParticle2D,
    lstg_TexParticle3D,
})