#include "macade_embedded.h"
#include "sdl2_inprint.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Deckade Linux bridge. The macOS embedded framebuffer is intentionally inert;
// this file only keeps the overlay state and draws it into the normal SDL
// window. It is compiled into the FBNeo runtime instead of Macade's macOS
// shared-memory bridge.

namespace {
struct ChatLine {
    std::string name;
    std::string text;
    int frames = 0;
};

std::string gPlayer1;
std::string gPlayer2;
std::string gSystemMessage;
int gSystemFrames = 0;
int gConnectionPhase = 0;
int gScore1 = -1;
int gScore2 = -1;
int gSpectators = 0;
int gPing = 0;
int gDelay = 0;
std::vector<ChatLine> gChat;

std::string cleanName(const char *name)
{
    std::string value = name ? name : "";
    const size_t hash = value.find('#');
    if (hash != std::string::npos) {
        value.erase(hash);
    }
    if (value == "Player1" || value == "Player2") {
        return {};
    }
    return value;
}

std::string matchTitle()
{
    const std::string p1 = cleanName(gPlayer1.c_str());
    const std::string p2 = cleanName(gPlayer2.c_str());
    if (!p1.empty() && !p2.empty()) {
        return p1 + "  vs  " + p2;
    }
    if (!p1.empty()) return p1;
    if (!p2.empty()) return p2;
    return {};
}

void drawOverlay(SDL_Renderer *renderer)
{
    if (!renderer) return;

    int width = 0;
    int height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    if (width <= 0 || height <= 0) return;

    const std::string title = matchTitle();
    if (title.empty() && gSystemFrames <= 0 && gChat.empty()) return;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 155);
    SDL_Rect bar{0, 0, width, 28};
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 115);
    SDL_Rect footer{0, height - 24, width, 24};
    SDL_RenderFillRect(renderer, &footer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    if (!title.empty()) {
        inprint_shadowed(renderer, title.c_str(), 8, 6);
    }

    if (gScore1 >= 0 && gScore2 >= 0) {
        char score[64];
        std::snprintf(score, sizeof(score), "%d - %d", gScore1, gScore2);
        const int scoreWidth = static_cast<int>(std::strlen(score)) * 6;
        inprint_shadowed(renderer, score, static_cast<Uint32>(std::max(8, (width - scoreWidth) / 2)), 6);
    }

    std::string right;
    if (gSpectators > 0) {
        right += std::to_string(gSpectators) + " watching";
    }
    if (gDelay > 0) {
        if (!right.empty()) right += "  ";
        right += "delay " + std::to_string(gDelay);
    }
    if (!right.empty()) {
        const int rightWidth = static_cast<int>(right.size()) * 6;
        inprint_shadowed(renderer, right.c_str(), static_cast<Uint32>(std::max(8, width - rightWidth - 10)), 6);
    }

    if (gSystemFrames > 0 && !gSystemMessage.empty()) {
        inprint_shadowed(renderer, gSystemMessage.c_str(), 8, static_cast<Uint32>(std::max(30, height - 48)));
        --gSystemFrames;
    }

    int y = height - 22;
    for (int i = static_cast<int>(gChat.size()) - 1; i >= 0 && i >= static_cast<int>(gChat.size()) - 3; --i) {
        if (gChat[i].frames <= 0) continue;
        const std::string line = gChat[i].name + ": " + gChat[i].text;
        inprint_shadowed(renderer, line.c_str(), 8, static_cast<Uint32>(std::max(20, y)));
        y -= 12;
        --gChat[i].frames;
    }
    while (!gChat.empty() && gChat.front().frames <= 0) {
        gChat.erase(gChat.begin());
    }
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
    (void)ranked;
    (void)player;
    // Local direct/quark-loopback runs have no Fightcade server to supply
    // names. Allow the launcher/test harness to pass display names explicitly.
    const char *override1 = std::getenv("DECKADE_OVERLAY_P1");
    const char *override2 = std::getenv("DECKADE_OVERLAY_P2");
    gPlayer1 = override1 && *override1 ? override1 : (player1 ? player1 : "");
    gPlayer2 = override2 && *override2 ? override2 : (player2 ? player2 : "");
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
    gChat.push_back({name ? name : "Fightcade", text, 300});
    if (gChat.size() > 5) gChat.erase(gChat.begin());
}

void MacadeEmbeddedSetReplayStatus(int, int, int, int, int, int) {}
int MacadeEmbeddedConsumeChatSubmit(char *, int) { return 0; }
extern "C" int MacadeEmbeddedKeyState(int) { return 0; }

void MacadeEmbeddedRenderOverlay(SDL_Renderer *renderer)
{
    drawOverlay(renderer);
}
