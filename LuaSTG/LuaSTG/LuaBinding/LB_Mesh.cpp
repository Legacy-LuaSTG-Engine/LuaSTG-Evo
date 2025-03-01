﻿#include "LuaBinding/LuaWrapper.hpp"
#include "LuaBinding/lua_utility.hpp"

namespace LuaSTGPlus::LuaWrapper
{
    std::string_view const MeshBinding::ClassID = "lstg.Mesh";

    Mesh* MeshBinding::Cast(lua_State* L, int idx)
    {
        return static_cast<Mesh*>(luaL_checkudata(L, idx, ClassID.data()));
    }

    Mesh* MeshBinding::Create(lua_State* L)
    {
        Mesh* p = static_cast<Mesh*>(lua_newuserdata(L, sizeof(Mesh))); // udata
        new(p) Mesh();
        luaL_getmetatable(L, ClassID.data()); // udata mt
        lua_setmetatable(L, -2); // udata
        return p;
    }

    inline uint32_t to_color32(lua_State* L, int idx)
    {
        if (lua_type(L, idx) == LUA_TNUMBER)
        {
            return luaL_checkf_uint32(L, idx); // lua_Number 才能容纳下 32 位无符号整数
        }
        else
        {
            return ColorWrapper::Cast(L, idx)->color();
        }
    }

    void MeshBinding::Register(lua_State* L)
    {
        struct Binding
        {
            static int resize(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const vertex_count = luaL_checki_uint32(L, 2);
                uint32_t const index_count = luaL_checki_uint32(L, 3);
                bool const result = self->resize(vertex_count, index_count);
                lua_pushboolean(L, result);
                return 1;
            }
            static int getVertexCount(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const result = self->getVertexCount();
                lua_pushi_uint32(L, result);
                return 1;
            }
            static int getIndexCount(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const result = self->getIndexCount();
                lua_pushi_uint32(L, result);
                return 1;
            }
            static int setAllVertexColor(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                Core::Color4B const color(to_color32(L, 2));
                self->setAllVertexColor(color);
                return 0;
            }
            static int setIndex(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const index = luaL_checki_uint32(L, 2);
                Core::Graphics::IRenderer::DrawIndex const value = (Core::Graphics::IRenderer::DrawIndex)luaL_checkinteger(L, 3);
                self->setIndex(index, value);
                return 0;
            }
            static int setVertex(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const index = luaL_checki_uint32(L, 2);
                float const x = luaL_check_float(L, 3);
                float const y = luaL_check_float(L, 4);
                float const z = luaL_check_float(L, 5);
                float const u = luaL_check_float(L, 6);
                float const v = luaL_check_float(L, 7);
                Core::Color4B const color(to_color32(L, 8));
                self->setVertex(index, x, y, z, u, v, color);
                return 0;
            }
            static int setVertexPosition(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const index = luaL_checki_uint32(L, 2);
                float const x = luaL_check_float(L, 3);
                float const y = luaL_check_float(L, 4);
                float const z = luaL_check_float(L, 5);
                self->setVertexPosition(index, x, y, z);
                return 0;
            }
            static int setVertexCoords(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const index = luaL_checki_uint32(L, 2);
                float const u = luaL_check_float(L, 3);
                float const v = luaL_check_float(L, 4);
                self->setVertexCoords(index, u, v);
                return 0;
            }
            static int setVertexColor(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                uint32_t const index = luaL_checki_uint32(L, 2);
                Core::Color4B const color(to_color32(L, 3));
                self->setVertexColor(index, color);
                return 0;
            }

            static int __gc(lua_State* L)
            {
                Mesh* self = Cast(L, 1);
                self->~Mesh();
                return 0;
            }
            static int __tostring(lua_State* L)
            {
                std::ignore = Cast(L, 1);
                lua_push_string_view(L, "lstg.Mesh");
                return 1;
            }

            static int create(lua_State* L)
            {
                Mesh* self = Create(L);
                uint32_t const vertex_count = luaL_checki_uint32(L, 1);
                uint32_t const index_count = luaL_checki_uint32(L, 2);
                if (self->resize(vertex_count, index_count))
                    return 1;
                else
                    return luaL_error(L, "create lstg.Mesh object failed");
            }
        };

        luaL_Reg const lib[] = {
            { "resize", &Binding::resize },
            { "getVertexCount", &Binding::getVertexCount },
            { "getIndexCount", &Binding::getIndexCount },
            { "setAllVertexColor", &Binding::setAllVertexColor },
            { "setIndex", &Binding::setIndex },
            { "setVertex", &Binding::setVertex },
            { "setVertexPosition", &Binding::setVertexPosition },
            { "setVertexCoords", &Binding::setVertexCoords },
            { "setVertexColor", &Binding::setVertexColor },
            { NULL, NULL },
        };

        luaL_Reg const mt[] = {
            { "__gc", &Binding::__gc },
            { "__tostring", &Binding::__tostring },
            { NULL, NULL },
        };

        luaL_Reg const api[] = {
            { "MeshData", &Binding::create },
            { NULL, NULL },
        };

        luaL_newmetatable(L, ClassID.data()); // ... mt
        luaL_register(L, NULL, mt);           // ... mt
        lua_push_string_view(L, "__index");   // ... mt '__index'
        lua_newtable(L);                      // ... mt '__index' lib
        luaL_register(L, NULL, lib);          // ... mt '__index' lib
        lua_rawset(L, -3);                    // ... mt
        lua_pop(L, 1);                        // ... 

        luaL_register(L, LUASTG_LUA_LIBNAME, api); // ... lstg
        lua_pop(L, 1);                             // ... 
    }
}
