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
int gRank1 = -1;
int gRank2 = -1;
int gRanked = 0;
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

std::string rankText(int rank)
{
    if (rank <= 0) return {};
    if (rank == 1) return "E";
    if (rank == 2) return "D";
    if (rank == 3) return "C";
    if (rank == 4) return "B";
    if (rank == 5) return "A";
    if (rank >= 6) return "S";
    return std::to_string(rank);
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
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
    SDL_Rect bar{0, 0, width, 26};
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 115);
    SDL_Rect footer{0, height - 24, width, 24};
    SDL_RenderFillRect(renderer, &footer);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    if (!title.empty()) {
        const std::string p1 = cleanName(gPlayer1.c_str());
        const std::string p2 = cleanName(gPlayer2.c_str());
        const std::string r1 = rankText(gRank1);
        const std::string r2 = rankText(gRank2);
        const std::string score1 = gScore1 >= 0 ? std::to_string(gScore1) : "0";
        const std::string score2 = gScore2 >= 0 ? std::to_string(gScore2) : "0";
        std::string left = p1;
        if (!r1.empty()) left += " [" + r1 + "]";
        left += "  " + score1;
        std::string right = score2;
        if (!r2.empty()) right += " [" + r2 + "]";
        right += "  " + p2;
        const std::string center = gRanked > 1 ? ("FT" + std::to_string(gRanked)) : "VS";
        const int leftWidth = static_cast<int>(left.size()) * 6;
        const int rightWidth = static_cast<int>(right.size()) * 6;
        const int centerWidth = static_cast<int>(center.size()) * 6;
        inprint_shadowed(renderer, left.c_str(), 8, 5);
        inprint_shadowed(renderer, center.c_str(),
                         static_cast<Uint32>(std::max(8, (width - centerWidth) / 2)), 5);
        inprint_shadowed(renderer, right.c_str(),
                         static_cast<Uint32>(std::max(8, width - rightWidth - 10)), 5);
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
    (void)player;
    gRanked = ranked;
    // Names arrive as "name#rank,score" from the Fightcade match-info event.
    auto assign = [](std::string &target, int &rank, const char *value, const char *overrideValue) {
        target = overrideValue && *overrideValue ? overrideValue : (value ? value : "");
        rank = -1;
        const size_t hash = target.find('#');
        if (hash != std::string::npos) {
            const std::string meta = target.substr(hash + 1);
            target.erase(hash);
            const size_t comma = meta.find(',');
            if (comma != std::string::npos) {
                rank = std::atoi(meta.substr(0, comma).c_str());
                const int score = std::atoi(meta.substr(comma + 1).c_str());
                if (score > 0) {
                    if (&rank == &gRank1) gScore1 = score; else gScore2 = score;
                }
            }
        }
    };
    assign(gPlayer1, gRank1, player1, std::getenv("DECKADE_OVERLAY_P1"));
    assign(gPlayer2, gRank2, player2, std::getenv("DECKADE_OVERLAY_P2"));
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
