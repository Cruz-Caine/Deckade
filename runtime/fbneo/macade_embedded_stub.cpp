#include "macade_embedded.h"

#include <SDL.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WARNINGS
#include "../intf/video/win32/stb_image.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// Deckade Linux bridge. The macOS embedded framebuffer is intentionally inert;
// this file keeps the overlay state and draws it into the normal SDL window.
// It is compiled into the FBNeo runtime instead of Macade's macOS bridge.
//
// The match overlay follows Fightcade's own vid_overlay.cpp: same art and font,
// positions in fractions of the game picture's width, and the same rules for
// when scores and rank badges appear.

void DetectorGetState(int &state, int &score1, int &score2, int &start1, int &start2);

namespace {

const float kFontMedium = 0.20f;
const float kFontChat = 0.24f;
const float kFontSeparation = 0.09f;
const float kFontSpacing = 0.6f;
const float kBarScale = 0.17f;
const int kChatLines = 7;
const int kChatFrames = 200;
const int kRankBadges = 7;

const Uint32 kWhite = 0xFFFFFFFF;
const Uint32 kScoreCyan = 0xFF33F1E5;
const Uint32 kFirstToGold = 0xFFFFB200;
const Uint32 kVersusGrey = 0xFF808080;
const Uint32 kChatOwnBlue = 0xFF00B2FF;
const Uint32 kChatOtherGold = 0xFFFFB200;
const Uint32 kChatSystem = 0xFFFF4D00;

enum Align { AlignLeft, AlignCenter, AlignRight };
enum Anchor { AnchorHCenter = 1, AnchorBottom = 2 };

struct Texture {
    SDL_Texture *handle = nullptr;
    int width = 0;
    int height = 0;
};

struct Glyph {
    int x = 0, y = 0, width = 0, height = 0;
    int offsetX = 0, offsetY = 0, advance = 0;
};

struct BitmapFont {
    std::map<uint32_t, Glyph> glyphs;
    std::map<uint64_t, int> kerning;
    Texture page;

    bool valid() const { return page.handle != nullptr && !glyphs.empty(); }
};

struct ChatLine {
    std::string name;
    std::string text;
    Uint32 nameColor = kWhite;
};

// The game picture on screen, and the unit system Fightcade positions against.
struct Frame {
    float left = 0, top = 0, width = 1, height = 1;
    float scale = 1, adjust = 1;
    float unitHeight = 1;
    float rendererScaleX = 1, rendererScaleY = 1;
};

SDL_Renderer *gRenderer = nullptr;
BitmapFont gFont;
Texture gBar;
Texture gSpectatorBadge;
Texture gRankBadge[kRankBadges];
Frame gFrame;

std::string gPlayer1;
std::string gPlayer2;
std::string gSystemMessage;
int gSystemFrames = 0;
int gConnectionPhase = 0;
int gScore1 = -1;
int gScore2 = -1;
int gRank1 = -1;
int gRank2 = -1;
int gRanked = 0;
int gLocalPlayer = 0;
int gSpectators = 0;
int gPing = 0;
int gDelay = 0;
int gChatTimer = 0;
std::vector<ChatLine> gChat;
int gDumpCounter = 0;

std::string assetPath(const char *name)
{
    const char *runtime = std::getenv("MACADE_FIGHTCADE_RUNTIME");
    const std::string root = runtime && *runtime ? std::string(runtime) + "/ui/" : std::string("ui/");
    return root + name;
}

Texture loadTexture(SDL_Renderer *renderer, const char *name)
{
    Texture result;
    const std::string path = assetPath(name);
    int channels = 0;
    unsigned char *pixels = stbi_load(path.c_str(), &result.width, &result.height, &channels, 4);
    if (!pixels) {
        std::fprintf(stderr, "Deckade overlay: could not load %s\n", path.c_str());
        return Texture{};
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, result.width, result.height, 32, result.width * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface) {
        result.handle = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
    stbi_image_free(pixels);
    if (result.handle) {
        SDL_SetTextureBlendMode(result.handle, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(result.handle, SDL_ScaleModeLinear);
    }
    return result;
}

uint32_t readU32(const std::vector<unsigned char> &d, size_t p)
{
    return d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | (uint32_t(d[p + 3]) << 24);
}

int readU16(const std::vector<unsigned char> &d, size_t p) { return d[p] | (d[p + 1] << 8); }

int readS16(const std::vector<unsigned char> &d, size_t p)
{
    return static_cast<int16_t>(readU16(d, p));
}

// AngelCode BMFont binary format, the layout of Fightcade's font.fnt.
bool loadFontMetrics(BitmapFont &font)
{
    const std::string path = assetPath("font.fnt");
    FILE *file = std::fopen(path.c_str(), "rb");
    if (!file) {
        std::fprintf(stderr, "Deckade overlay: could not load %s\n", path.c_str());
        return false;
    }
    std::vector<unsigned char> data;
    unsigned char buffer[4096];
    size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof(buffer), file)) > 0) {
        data.insert(data.end(), buffer, buffer + got);
    }
    std::fclose(file);

    if (data.size() < 4 || std::memcmp(data.data(), "BMF", 3) != 0) return false;
    size_t pos = 4;
    while (pos + 5 <= data.size()) {
        const int type = data[pos];
        const size_t size = readU32(data, pos + 1);
        pos += 5;
        if (pos + size > data.size()) return false;
        if (type == 4) {
            for (size_t p = pos; p + 20 <= pos + size; p += 20) {
                Glyph glyph;
                glyph.x = readU16(data, p + 4);
                glyph.y = readU16(data, p + 6);
                glyph.width = readU16(data, p + 8);
                glyph.height = readU16(data, p + 10);
                glyph.offsetX = readS16(data, p + 12);
                glyph.offsetY = readS16(data, p + 14);
                glyph.advance = readS16(data, p + 16);
                font.glyphs[readU32(data, p)] = glyph;
            }
        } else if (type == 5) {
            for (size_t p = pos; p + 10 <= pos + size; p += 10) {
                const uint64_t key = (uint64_t(readU32(data, p)) << 32) | readU32(data, p + 4);
                font.kerning[key] = readS16(data, p + 8);
            }
        }
        pos += size;
    }
    return !font.glyphs.empty();
}

// Textures belong to one renderer, and FBNeo recreates it on video re-init.
void loadAssets(SDL_Renderer *renderer)
{
    gRenderer = renderer;
    gFont = BitmapFont{};
    if (loadFontMetrics(gFont)) gFont.page = loadTexture(renderer, "font_0.png");
    gBar = loadTexture(renderer, "background.png");
    gSpectatorBadge = loadTexture(renderer, "spectators.png");
    for (int i = 0; i < kRankBadges; ++i) {
        const std::string name = "rank" + std::to_string(i) + ".png";
        gRankBadge[i] = loadTexture(renderer, name.c_str());
    }
}

void setFrameSize(float size)
{
    gFrame.scale = gFrame.height / size;
    gFrame.adjust = 1.f;
    if (gFrame.scale < 1.5f) {
        gFrame.adjust = 1.5f / gFrame.scale;
        gFrame.scale = 1.5f;
    }
    gFrame.unitHeight = gFrame.height / gFrame.width;
}

float toPixelX(float x) { return x * gFrame.width + gFrame.left; }
float toPixelY(float y) { return y * gFrame.width + gFrame.top; }
float fontSeparation() { return kFontSeparation * gFrame.adjust; }

// FBNeo keeps an SDL logical size such as 304x228 active while this draws, so
// real-pixel positions are converted into that space rather than changing it.
SDL_FRect toRendererRect(float x, float y, float width, float height)
{
    return SDL_FRect{(x - gFrame.left) / gFrame.rendererScaleX, (y - gFrame.top) / gFrame.rendererScaleY,
                     width / gFrame.rendererScaleX, height / gFrame.rendererScaleY};
}

std::vector<uint32_t> decodeUtf8(const std::string &text)
{
    std::vector<uint32_t> result;
    for (size_t i = 0; i < text.size();) {
        const unsigned char lead = text[i];
        uint32_t code = lead;
        size_t length = 1;
        if (lead >= 0xF0) { code = lead & 0x07; length = 4; }
        else if (lead >= 0xE0) { code = lead & 0x0F; length = 3; }
        else if (lead >= 0xC0) { code = lead & 0x1F; length = 2; }
        if (i + length > text.size()) break;
        for (size_t k = 1; k < length; ++k) code = (code << 6) | (text[i + k] & 0x3F);
        result.push_back(code);
        i += length;
    }
    return result;
}

const Glyph *findGlyph(uint32_t code)
{
    auto it = gFont.glyphs.find(code);
    if (it != gFont.glyphs.end()) return &it->second;
    it = gFont.glyphs.find('?');
    return it != gFont.glyphs.end() ? &it->second : nullptr;
}

float kerningBetween(uint32_t first, uint32_t second, float scale)
{
    auto it = gFont.kerning.find((uint64_t(first) << 32) | second);
    return it != gFont.kerning.end() ? it->second * scale : 0.f;
}

float textWidthPixels(const std::vector<uint32_t> &codes, float scale)
{
    float width = 0;
    for (size_t i = 0; i < codes.size(); ++i) {
        const Glyph *glyph = findGlyph(codes[i]);
        if (glyph) width += glyph->advance * scale;
        if (i + 1 < codes.size()) {
            if (codes[i] != ' ') width += kFontSpacing;
            width += kerningBetween(codes[i], codes[i + 1], scale);
        }
    }
    return width;
}

float textWidth(const std::string &text, float size)
{
    if (!gFont.valid()) return 0;
    return textWidthPixels(decodeUtf8(text), size * gFrame.scale) / gFrame.width;
}

void writeText(const std::string &text, float x, float y, Uint32 color, float size, Align align)
{
    if (!gFont.valid() || text.empty()) return;
    const std::vector<uint32_t> codes = decodeUtf8(text);
    const float scale = size * gFrame.scale;
    float px = toPixelX(x);
    const float py = toPixelY(y);
    const float width = textWidthPixels(codes, scale);
    if (align == AlignCenter) px -= width / 2;
    if (align == AlignRight) px -= width;

    SDL_SetTextureColorMod(gFont.page.handle, (color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF);
    SDL_SetTextureAlphaMod(gFont.page.handle, (color >> 24) & 0xFF);
    for (size_t i = 0; i < codes.size(); ++i) {
        const Glyph *glyph = findGlyph(codes[i]);
        if (!glyph) continue;
        const SDL_Rect source{glyph->x, glyph->y, glyph->width, glyph->height};
        const SDL_FRect target = toRendererRect(px + glyph->offsetX * scale, py + glyph->offsetY * scale,
                                                glyph->width * scale, glyph->height * scale);
        SDL_RenderCopyF(gRenderer, gFont.page.handle, &source, &target);
        px += glyph->advance * scale;
        if (codes[i] != ' ') px += kFontSpacing;
        if (i + 1 < codes.size()) px += kerningBetween(codes[i], codes[i + 1], scale);
    }
}

void drawSprite(const Texture &texture, float x, float y, float scale, int anchor)
{
    if (!texture.handle) return;
    const float width = texture.width * scale * gFrame.scale;
    const float height = texture.height * scale * gFrame.scale;
    float px = toPixelX(x);
    float py = toPixelY(y);
    if (anchor & AnchorHCenter) px -= width * 0.5f;
    if (anchor & AnchorBottom) py -= height;
    const SDL_FRect target = toRendererRect(px, py, width, height);
    SDL_RenderCopyF(gRenderer, texture.handle, nullptr, &target);
}

void drawRankBadge(int rank, float x, float y)
{
    if (rank >= 0 && rank < kRankBadges) drawSprite(gRankBadge[rank], x, y, kBarScale, AnchorHCenter);
}

void drawMatchBar(bool detectorEnabled)
{
    if (gPlayer1.empty() && gPlayer2.empty()) return;

    const float adjust = gFrame.adjust;
    const float x = 0.5f;
    const float scoreOffset = adjust * 0.025f;
    const float nameOffset = adjust * (detectorEnabled ? 0.05f : 0.019f);
    const float textY = adjust * 0.002f;
    const float badgeY = adjust * 0.003f;

    drawSprite(gBar, x, 0, kBarScale, AnchorHCenter);

    if (gRanked > 1) {
        writeText("FT" + std::to_string(gRanked), x, textY, kFirstToGold, kFontMedium, AlignCenter);
    } else {
        writeText("VS", x, textY, kVersusGrey, kFontMedium, AlignCenter);
    }

    if (!gPlayer1.empty()) {
        writeText(gPlayer1, x - nameOffset, textY, kWhite, kFontMedium, AlignRight);
        if (detectorEnabled) {
            if (gScore1 >= 0) writeText(std::to_string(gScore1), x - scoreOffset, textY, kScoreCyan, kFontMedium, AlignRight);
            if (gRanked && gRank1 != -1) {
                drawRankBadge(gRank1, x - nameOffset - textWidth(gPlayer1, kFontMedium) - 0.019f, badgeY);
            }
        }
    }
    if (!gPlayer2.empty()) {
        writeText(gPlayer2, x + nameOffset, textY, kWhite, kFontMedium, AlignLeft);
        if (detectorEnabled) {
            if (gScore2 >= 0) writeText(std::to_string(gScore2), x + scoreOffset, textY, kScoreCyan, kFontMedium, AlignLeft);
            if (gRanked && gRank2 != -1) {
                drawRankBadge(gRank2, x + nameOffset + textWidth(gPlayer2, kFontMedium) + 0.019f, badgeY);
            }
        }
    }

    if (gSpectators > 1) {
        drawSprite(gSpectatorBadge, x, gFrame.unitHeight, kBarScale, AnchorHCenter | AnchorBottom);
        writeText(std::to_string(gSpectators - 1), x + 0.012f,
                  gFrame.unitHeight - 0.007f - kFontMedium * 0.8f * fontSeparation(),
                  kWhite, kFontMedium, AlignCenter);
    }
}

void drawChat()
{
    if (gChatTimer <= 0) return;
    --gChatTimer;
    const float x = 0.005f;
    float y = gFrame.unitHeight - 0.005f - kFontMedium * fontSeparation() * 1.1f;
    for (int i = static_cast<int>(gChat.size()) - 1; i >= 0; --i) {
        y -= kFontChat * fontSeparation();
        writeText(gChat[i].name, x, y, gChat[i].nameColor, kFontChat, AlignLeft);
        writeText(gChat[i].text, x + textWidth(gChat[i].name, kFontChat) + 0.006f, y, kWhite, kFontChat, AlignLeft);
    }
}

void drawStats()
{
    const char *enabled = std::getenv("DECKADE_OVERLAY_STATS");
    if (!enabled || std::strcmp(enabled, "1") != 0) return;
    const std::string line = "ping " + std::to_string(gPing) + "  delay " + std::to_string(gDelay);
    writeText(line, 0.9965f, 0.003f, kWhite, kFontMedium * 0.9f, AlignRight);
}

// Test hook: writes the composited game picture, including this overlay.
void maybeDumpWindow()
{
    const char *path = std::getenv("DECKADE_OVERLAY_DUMP");
    if (!path || !*path) return;
    const char *frames = std::getenv("DECKADE_OVERLAY_DUMP_FRAME");
    if (gDumpCounter++ != (frames ? std::atoi(frames) : 90)) return;
    const int width = static_cast<int>(gFrame.width);
    const int height = static_cast<int>(gFrame.height);
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
    if (SDL_RenderReadPixels(gRenderer, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), width * 4) != 0) return;
    FILE *file = std::fopen(path, "wb");
    if (!file) return;
    const int header[5] = {0x44464D52, width, height, 32, width * 4};
    std::fwrite(header, sizeof(int), 5, file);
    std::fwrite(pixels.data(), 1, pixels.size(), file);
    std::fclose(file);
}

void drawOverlay(SDL_Renderer *renderer)
{
    if (!renderer) return;
    if (renderer != gRenderer) loadAssets(renderer);

    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight);
    if (outputWidth <= 0 || outputHeight <= 0) return;

    int logicalWidth = 0;
    int logicalHeight = 0;
    SDL_RenderGetLogicalSize(renderer, &logicalWidth, &logicalHeight);
    SDL_Rect viewport{};
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetScale(renderer, &gFrame.rendererScaleX, &gFrame.rendererScaleY);
    if (logicalWidth > 0 && logicalHeight > 0) {
        gFrame.left = viewport.x * gFrame.rendererScaleX;
        gFrame.top = viewport.y * gFrame.rendererScaleY;
        gFrame.width = viewport.w * gFrame.rendererScaleX;
        gFrame.height = viewport.h * gFrame.rendererScaleY;
    } else {
        gFrame.left = 0;
        gFrame.top = 0;
        gFrame.width = static_cast<float>(outputWidth);
        gFrame.height = static_cast<float>(outputHeight);
    }

    setFrameSize(260.f);

    int state = 0;
    int score1 = 0;
    int score2 = 0;
    int start1 = 0;
    int start2 = 0;
    DetectorGetState(state, score1, score2, start1, start2);

    drawMatchBar(gRanked != 0 && state != 0);

    if (gSystemFrames > 0 && !gSystemMessage.empty()) {
        writeText(gSystemMessage, 0.5f, gFrame.unitHeight * 0.2f, kWhite, kFontMedium * 1.8f, AlignCenter);
        --gSystemFrames;
    }
    drawChat();
    drawStats();
    maybeDumpWindow();
}

// Names arrive as "name#rank,score,country". Names can contain '#', so split
// at the last one, as Fightcade does. Live scores come from the game detector.
void parsePlayer(const char *value, const char *overrideValue, std::string &name, int &rank)
{
    name = overrideValue && *overrideValue ? overrideValue : (value ? value : "");
    rank = -1;
    const size_t hash = name.rfind('#');
    if (hash != std::string::npos) {
        rank = std::atoi(name.c_str() + hash + 1);
        name.erase(hash);
    }
    if (name == "Player1" || name == "Player2") name.clear();
}

} // namespace

bool MacadeEmbeddedEnabled() { return false; }
bool MacadeEmbeddedWindowHidden() { return false; }
void MacadeEmbeddedShutdown() {}
void MacadeEmbeddedPumpInput() {}
void MacadeEmbeddedPublishFrame(const void *, int, int, int, int, int) {}

void MacadeEmbeddedSetOverlayConnectionPhase(int phase)
{
    gConnectionPhase = phase;
}

void MacadeEmbeddedSetOverlaySystemMessage(const char *message, int frames)
{
    gSystemMessage = message ? message : "";
    gSystemFrames = frames;
}

void MacadeEmbeddedSetOverlayGameInfo(const char *player1, const char *player2, int spectator, int ranked, int player)
{
    (void)spectator;
    gRanked = ranked;
    gLocalPlayer = player;
    parsePlayer(player1, std::getenv("DECKADE_OVERLAY_P1"), gPlayer1, gRank1);
    parsePlayer(player2, std::getenv("DECKADE_OVERLAY_P2"), gPlayer2, gRank2);
}

void MacadeEmbeddedSetOverlayScores(int score1, int score2)
{
    gScore1 = score1;
    gScore2 = score2;
}

void MacadeEmbeddedSetOverlaySpectators(int spectators)
{
    gSpectators = spectators;
}

void MacadeEmbeddedSetOverlayStats(int ping, int delay)
{
    gPing = ping;
    gDelay = delay;
}

void MacadeEmbeddedSetOverlayMatchEnded()
{
    gSystemMessage = "Match ended";
    gSystemFrames = 240;
}

void MacadeEmbeddedAddOverlayChatLine(const char *name, const char *text)
{
    if (!text || !*text) return;
    ChatLine line;
    line.name = name ? name : "Fightcade";
    line.text = text;
    if (line.name == gPlayer1) {
        line.nameColor = gLocalPlayer == 0 ? kChatOwnBlue : kChatOtherGold;
    } else if (line.name == gPlayer2) {
        line.nameColor = gLocalPlayer == 1 ? kChatOwnBlue : kChatOtherGold;
    } else {
        line.nameColor = kChatSystem;
    }
    gChat.push_back(line);
    if (static_cast<int>(gChat.size()) > kChatLines) gChat.erase(gChat.begin());
    gChatTimer = kChatFrames;
}

void MacadeEmbeddedSetReplayStatus(int, int, int, int, int, int) {}
int MacadeEmbeddedConsumeChatSubmit(char *, int) { return 0; }
extern "C" int MacadeEmbeddedKeyState(int) { return 0; }

void MacadeEmbeddedRenderOverlay(SDL_Renderer *renderer)
{
    drawOverlay(renderer);
}
