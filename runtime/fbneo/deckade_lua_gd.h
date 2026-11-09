// Built-in replacement for the Lua-GD module used by fbneo-training-mode.
//
// The training scripts decode PNG icons with gd and hand gui.gdoverlay a string
// in GD's image format. The real module needs a system libgd, which is absent
// on a Steam Deck and has that format compiled out on current distributions,
// and either case stops the training script. This covers the calls the scripts
// make, with no external library. Included once, from luaengine.cpp.
#pragma once

#include "../intf/video/win32/stb_image.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace deckade_gd {

const char *const kMetatable = "deckade.gd.image";

struct Image {
    int width;
    int height;
    unsigned char *rgba;
};

Image *pushImage(lua_State *L, int width, int height, unsigned char *rgba)
{
    Image *image = static_cast<Image *>(lua_newuserdata(L, sizeof(Image)));
    image->width = width;
    image->height = height;
    image->rgba = rgba;
    luaL_getmetatable(L, kMetatable);
    lua_setmetatable(L, -2);
    return image;
}

Image *checkImage(lua_State *L, int index)
{
    return static_cast<Image *>(luaL_checkudata(L, index, kMetatable));
}

// stb_image and FBNeo release this buffer with the same allocator.
unsigned char *allocatePixels(int width, int height)
{
    return static_cast<unsigned char *>(std::calloc(static_cast<size_t>(width) * height, 4));
}

int pushDecoded(lua_State *L, unsigned char *rgba, int width, int height)
{
    if (!rgba) {
        lua_pushnil(L);
        return 1;
    }
    pushImage(L, width, height, rgba);
    return 1;
}

int createFromPng(lua_State *L)
{
    int width = 0, height = 0, channels = 0;
    unsigned char *rgba = stbi_load(luaL_checkstring(L, 1), &width, &height, &channels, 4);
    return pushDecoded(L, rgba, width, height);
}

int createFromPngStr(lua_State *L)
{
    size_t length = 0;
    const char *data = luaL_checklstring(L, 1, &length);
    int width = 0, height = 0, channels = 0;
    unsigned char *rgba = stbi_load_from_memory(reinterpret_cast<const unsigned char *>(data),
                                                static_cast<int>(length), &width, &height, &channels, 4);
    return pushDecoded(L, rgba, width, height);
}

int create(lua_State *L)
{
    const int width = luaL_checkint(L, 1);
    const int height = luaL_checkint(L, 2);
    if (width <= 0 || height <= 0) return luaL_error(L, "gd.create: invalid size");
    pushImage(L, width, height, allocatePixels(width, height));
    return 1;
}

// GD format: 0xFFFE, big-endian size, truecolor flag, transparent index, then
// ARGB pixels where alpha runs 0 (opaque) to 127 (transparent).
const size_t kGdHeaderSize = 11;

int createFromGdStr(lua_State *L)
{
    size_t length = 0;
    const unsigned char *data = reinterpret_cast<const unsigned char *>(luaL_checklstring(L, 1, &length));
    if (length < kGdHeaderSize || data[0] != 0xFF || data[1] != 0xFE) {
        lua_pushnil(L);
        return 1;
    }
    const int width = (data[2] << 8) | data[3];
    const int height = (data[4] << 8) | data[5];
    if (width <= 0 || height <= 0 || length < kGdHeaderSize + size_t(width) * height * 4) {
        lua_pushnil(L);
        return 1;
    }
    unsigned char *rgba = allocatePixels(width, height);
    const unsigned char *source = data + kGdHeaderSize;
    for (int i = 0; i < width * height; ++i, source += 4) {
        rgba[i * 4] = source[1];
        rgba[i * 4 + 1] = source[2];
        rgba[i * 4 + 2] = source[3];
        rgba[i * 4 + 3] = static_cast<unsigned char>(255 - (source[0] * 255) / 127);
    }
    pushImage(L, width, height, rgba);
    return 1;
}

int gdStr(lua_State *L)
{
    const Image *image = checkImage(L, 1);
    std::string out;
    out.resize(kGdHeaderSize + size_t(image->width) * image->height * 4);
    unsigned char *target = reinterpret_cast<unsigned char *>(&out[0]);
    *target++ = 0xFF;
    *target++ = 0xFE;
    *target++ = (image->width >> 8) & 0xFF;
    *target++ = image->width & 0xFF;
    *target++ = (image->height >> 8) & 0xFF;
    *target++ = image->height & 0xFF;
    *target++ = 1;
    *target++ = 0xFF;
    *target++ = 0xFF;
    *target++ = 0xFF;
    *target++ = 0xFF;
    const unsigned char *source = image->rgba;
    for (int i = 0; i < image->width * image->height; ++i, source += 4) {
        *target++ = static_cast<unsigned char>(127 - (source[3] >> 1));
        *target++ = source[0];
        *target++ = source[1];
        *target++ = source[2];
    }
    lua_pushlstring(L, out.data(), out.size());
    return 1;
}

int sizeX(lua_State *L)
{
    lua_pushinteger(L, checkImage(L, 1)->width);
    return 1;
}

int sizeY(lua_State *L)
{
    lua_pushinteger(L, checkImage(L, 1)->height);
    return 1;
}

// Averages the source area behind each destination pixel, weighted by alpha so
// transparent neighbours do not darken icon edges.
int copyResampled(lua_State *L)
{
    Image *destination = checkImage(L, 1);
    const Image *source = checkImage(L, 2);
    const int dstX = luaL_checkint(L, 3);
    const int dstY = luaL_checkint(L, 4);
    const int srcX = luaL_checkint(L, 5);
    const int srcY = luaL_checkint(L, 6);
    const int dstW = luaL_checkint(L, 7);
    const int dstH = luaL_checkint(L, 8);
    const int srcW = luaL_checkint(L, 9);
    const int srcH = luaL_checkint(L, 10);
    if (dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0) return 0;

    for (int y = 0; y < dstH; ++y) {
        const int targetY = dstY + y;
        if (targetY < 0 || targetY >= destination->height) continue;
        const int fromY = srcY + (y * srcH) / dstH;
        const int toY = std::max(fromY + 1, srcY + ((y + 1) * srcH) / dstH);
        for (int x = 0; x < dstW; ++x) {
            const int targetX = dstX + x;
            if (targetX < 0 || targetX >= destination->width) continue;
            const int fromX = srcX + (x * srcW) / dstW;
            const int toX = std::max(fromX + 1, srcX + ((x + 1) * srcW) / dstW);
            unsigned red = 0, green = 0, blue = 0, alpha = 0, samples = 0;
            for (int sy = std::max(fromY, 0); sy < std::min(toY, source->height); ++sy) {
                for (int sx = std::max(fromX, 0); sx < std::min(toX, source->width); ++sx) {
                    const unsigned char *pixel = source->rgba + (size_t(sy) * source->width + sx) * 4;
                    red += pixel[0] * pixel[3];
                    green += pixel[1] * pixel[3];
                    blue += pixel[2] * pixel[3];
                    alpha += pixel[3];
                    ++samples;
                }
            }
            if (alpha == 0) continue;
            unsigned char *target = destination->rgba + (size_t(targetY) * destination->width + targetX) * 4;
            target[0] = static_cast<unsigned char>(red / alpha);
            target[1] = static_cast<unsigned char>(green / alpha);
            target[2] = static_cast<unsigned char>(blue / alpha);
            target[3] = static_cast<unsigned char>(alpha / samples);
        }
    }
    return 0;
}

// Only the scrolling-input frame recorder saves PNGs, and it is off by default.
int png(lua_State *L)
{
    lua_pushboolean(L, 0);
    return 1;
}

int collect(lua_State *L)
{
    Image *image = checkImage(L, 1);
    std::free(image->rgba);
    image->rgba = nullptr;
    return 0;
}

const luaL_Reg kModule[] = {
    {"createFromPng", createFromPng},
    {"createFromPngStr", createFromPngStr},
    {"createFromGdStr", createFromGdStr},
    {"create", create},
    {"createTrueColor", create},
    {"copyResampled", copyResampled},
    {nullptr, nullptr},
};

const luaL_Reg kMethods[] = {
    {"gdStr", gdStr},
    {"sizeX", sizeX},
    {"sizeY", sizeY},
    {"copyResampled", copyResampled},
    {"png", png},
    {nullptr, nullptr},
};

} // namespace deckade_gd

// luaL_register also fills package.loaded, so `require "gd"` finds this.
static void deckadeOpenLuaGd(lua_State *L)
{
    luaL_newmetatable(L, deckade_gd::kMetatable);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, deckade_gd::collect);
    lua_setfield(L, -2, "__gc");
    luaL_register(L, nullptr, deckade_gd::kMethods);
    lua_pop(L, 1);
    luaL_register(L, "gd", deckade_gd::kModule);
    lua_pop(L, 1);
}
