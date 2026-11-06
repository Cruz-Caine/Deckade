#include "libretro.h"
#include "macade_quark.h"
#include "snapshot.h"

#include <SDL.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

bool bNetRollback = false;

namespace {

constexpr int kExitUsage = 64;
constexpr int kExitRom = 66;
constexpr int kExitUnsupported = 78;

std::atomic_bool stopRequested{false};
retro_pixel_format pixelFormat = RETRO_PIXEL_FORMAT_RGB565;
std::string systemDirectory = ".";
std::string saveDirectory = ".";
std::string romDirectory = ".";
bool gameLoaded = false;
bool suppressVideo = false;
bool fullscreen = true;

SDL_Window *window = nullptr;
SDL_Renderer *renderer = nullptr;
SDL_Texture *texture = nullptr;
SDL_AudioDeviceID audioDevice = 0;
SDL_GameController *controller = nullptr;
int textureWidth = 0;
int textureHeight = 0;
int textureFormat = 0;

bool startsWith(const std::string &value, const char *prefix)
{
    const size_t length = std::strlen(prefix);
    return value.size() >= length && value.compare(0, length, prefix) == 0;
}

bool endsWith(const std::string &value, const char *suffix)
{
    const size_t length = std::strlen(suffix);
    return value.size() >= length && value.compare(value.size() - length, length, suffix) == 0;
}

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool fileExists(const std::string &path)
{
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

bool hasPathSeparator(const std::string &value)
{
    return value.find('/') != std::string::npos || value.find('\\') != std::string::npos;
}

bool hasKnownExtension(const std::string &value)
{
    const std::string lower = lowercase(value);
    return endsWith(lower, ".zip") || endsWith(lower, ".sfc") || endsWith(lower, ".smc")
        || endsWith(lower, ".fig") || endsWith(lower, ".swc");
}

void appendUnique(std::vector<std::string> &values, const std::string &value)
{
    if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::vector<std::string> gameStems(const std::string &game)
{
    std::vector<std::string> stems;
    appendUnique(stems, game);
    const std::string lower = lowercase(game);
    if (startsWith(lower, "snes9x_")) appendUnique(stems, game.substr(7));
    if (startsWith(lower, "snes_")) appendUnique(stems, game.substr(5));
    return stems;
}

std::vector<std::string> romCandidates(const std::string &game)
{
    std::vector<std::string> candidates;
    if (hasPathSeparator(game) || hasKnownExtension(game)) {
        appendUnique(candidates, game);
    }
    for (const std::string &stem : gameStems(game)) {
        if (hasKnownExtension(stem)) {
            appendUnique(candidates, romDirectory + "/" + stem);
            continue;
        }
        appendUnique(candidates, romDirectory + "/" + stem + ".zip");
        appendUnique(candidates, romDirectory + "/" + stem + ".sfc");
        appendUnique(candidates, romDirectory + "/" + stem + ".smc");
    }
    return candidates;
}

std::string resolveRomPath(const std::string &game)
{
    for (const std::string &candidate : romCandidates(game)) {
        if (fileExists(candidate)) return candidate;
    }

    std::error_code error;
    if (std::filesystem::is_directory(romDirectory, error)) {
        const std::string wanted = lowercase(gameStems(game).front()) + ".";
        for (const auto &entry : std::filesystem::recursive_directory_iterator(
                 romDirectory, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (!entry.is_regular_file(error)) continue;
            const std::string filename = lowercase(entry.path().filename().string());
            if (filename.rfind(wanted, 0) == 0 && hasKnownExtension(filename)) {
                return entry.path().string();
            }
        }
    }
    return {};
}

uint32_t buttonBit(unsigned id)
{
    switch (id) {
    case RETRO_DEVICE_ID_JOYPAD_B: return 1u << RETRO_DEVICE_ID_JOYPAD_B;
    case RETRO_DEVICE_ID_JOYPAD_Y: return 1u << RETRO_DEVICE_ID_JOYPAD_Y;
    case RETRO_DEVICE_ID_JOYPAD_SELECT: return 1u << RETRO_DEVICE_ID_JOYPAD_SELECT;
    case RETRO_DEVICE_ID_JOYPAD_START: return 1u << RETRO_DEVICE_ID_JOYPAD_START;
    case RETRO_DEVICE_ID_JOYPAD_UP: return 1u << RETRO_DEVICE_ID_JOYPAD_UP;
    case RETRO_DEVICE_ID_JOYPAD_DOWN: return 1u << RETRO_DEVICE_ID_JOYPAD_DOWN;
    case RETRO_DEVICE_ID_JOYPAD_LEFT: return 1u << RETRO_DEVICE_ID_JOYPAD_LEFT;
    case RETRO_DEVICE_ID_JOYPAD_RIGHT: return 1u << RETRO_DEVICE_ID_JOYPAD_RIGHT;
    case RETRO_DEVICE_ID_JOYPAD_A: return 1u << RETRO_DEVICE_ID_JOYPAD_A;
    case RETRO_DEVICE_ID_JOYPAD_X: return 1u << RETRO_DEVICE_ID_JOYPAD_X;
    case RETRO_DEVICE_ID_JOYPAD_L: return 1u << RETRO_DEVICE_ID_JOYPAD_L;
    case RETRO_DEVICE_ID_JOYPAD_R: return 1u << RETRO_DEVICE_ID_JOYPAD_R;
    default: return 0;
    }
}

uint32_t localJoypadBits()
{
    uint32_t bits = 0;

    if (controller) {
        auto pressed = [](SDL_GameControllerButton button) {
            return controller && SDL_GameControllerGetButton(controller, button);
        };
        auto axis = [](SDL_GameControllerAxis value) {
            return controller ? SDL_GameControllerGetAxis(controller, value) : 0;
        };

        if (pressed(SDL_CONTROLLER_BUTTON_A)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_B);
        if (pressed(SDL_CONTROLLER_BUTTON_B)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_A);
        if (pressed(SDL_CONTROLLER_BUTTON_X)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_Y);
        if (pressed(SDL_CONTROLLER_BUTTON_Y)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_X);
        if (pressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_L);
        if (pressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_R);
        if (pressed(SDL_CONTROLLER_BUTTON_START)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_START);
        if (pressed(SDL_CONTROLLER_BUTTON_BACK)) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_SELECT);
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_UP) || axis(SDL_CONTROLLER_AXIS_LEFTY) < -16000) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_UP);
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || axis(SDL_CONTROLLER_AXIS_LEFTY) > 16000) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_DOWN);
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || axis(SDL_CONTROLLER_AXIS_LEFTX) < -16000) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_LEFT);
        if (pressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || axis(SDL_CONTROLLER_AXIS_LEFTX) > 16000) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_RIGHT);
    }

    const Uint8 *keys = SDL_GetKeyboardState(nullptr);
    if (keys) {
        if (keys[SDL_SCANCODE_UP]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_UP);
        if (keys[SDL_SCANCODE_DOWN]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_DOWN);
        if (keys[SDL_SCANCODE_LEFT]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_LEFT);
        if (keys[SDL_SCANCODE_RIGHT]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_RIGHT);
        if (keys[SDL_SCANCODE_Z]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_B);
        if (keys[SDL_SCANCODE_X]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_A);
        if (keys[SDL_SCANCODE_A]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_Y);
        if (keys[SDL_SCANCODE_S]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_X);
        if (keys[SDL_SCANCODE_Q]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_L);
        if (keys[SDL_SCANCODE_W]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_R);
        if (keys[SDL_SCANCODE_RETURN]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_START);
        if (keys[SDL_SCANCODE_RSHIFT]) bits |= buttonBit(RETRO_DEVICE_ID_JOYPAD_SELECT);
    }
    return bits;
}

void setRollbackMode(bool enabled)
{
    suppressVideo = enabled;
}

int16_t inputState(unsigned port, unsigned device, unsigned, unsigned id)
{
    if (device != RETRO_DEVICE_JOYPAD) return 0;
    if (MacadeSnes9xQuarkActive()) {
        const auto &inputs = MacadeSnes9xQuarkInputs();
        return port < inputs.size() && (inputs[port] & (1u << id)) ? 1 : 0;
    }
    if (port != 0) return 0;
    return (localJoypadBits() & (1u << id)) ? 1 : 0;
}

bool loadRomForGame(const std::string &game)
{
    if (gameLoaded) return true;
    const std::string path = resolveRomPath(game);
    if (path.empty()) {
        std::fprintf(stderr, "Could not find Snes9x ROM for '%s' in %s\n", game.c_str(), romDirectory.c_str());
        return false;
    }

    retro_game_info info{};
    info.path = path.c_str();
    if (!retro_load_game(&info)) {
        std::fprintf(stderr, "Snes9x failed to load ROM: %s\n", path.c_str());
        return false;
    }
    gameLoaded = true;
    return true;
}

void ensureVideo(int width, int height)
{
    if (window && width == textureWidth && height == textureHeight && texture) return;

    if (!window) {
        window = SDL_CreateWindow("Deckade Snes9x", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window) {
            std::fprintf(stderr, "SDL window failed: %s\n", SDL_GetError());
            stopRequested = true;
            return;
        }
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (fullscreen) SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }

    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTexture(renderer, textureFormat, SDL_TEXTUREACCESS_STREAMING, width, height);
    textureWidth = width;
    textureHeight = height;
}

void startAudio(double sampleRate)
{
    if (audioDevice) {
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
    SDL_AudioSpec wanted{};
    wanted.freq = static_cast<int>(sampleRate > 1.0 ? sampleRate : 32040.0);
    wanted.format = AUDIO_S16SYS;
    wanted.channels = 2;
    wanted.samples = 1024;
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
    if (audioDevice) SDL_PauseAudioDevice(audioDevice, 0);
}

void videoRefresh(const void *data, unsigned width, unsigned height, size_t pitch)
{
    if (suppressVideo || !data || width == 0 || height == 0) return;

    const bool is32 = pixelFormat == RETRO_PIXEL_FORMAT_XRGB8888;
    textureFormat = is32 ? SDL_PIXELFORMAT_ARGB8888
                         : pixelFormat == RETRO_PIXEL_FORMAT_0RGB1555 ? SDL_PIXELFORMAT_ARGB1555
                                                                     : SDL_PIXELFORMAT_RGB565;
    ensureVideo(static_cast<int>(width), static_cast<int>(height));
    if (!texture) return;

    SDL_UpdateTexture(texture, nullptr, data, static_cast<int>(pitch));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    int outputWidth = 0;
    int outputHeight = 0;
    SDL_GetRendererOutputSize(renderer, &outputWidth, &outputHeight);
    if (outputWidth <= 0 || outputHeight <= 0) {
        SDL_GetWindowSize(window, &outputWidth, &outputHeight);
    }
    const float scale = std::min(static_cast<float>(outputWidth) / width,
                                 static_cast<float>(outputHeight) / height);
    SDL_Rect destination{
        static_cast<int>((outputWidth - width * scale) / 2.0f),
        static_cast<int>((outputHeight - height * scale) / 2.0f),
        static_cast<int>(width * scale), static_cast<int>(height * scale)
    };
    SDL_RenderCopy(renderer, texture, nullptr, &destination);
    SDL_RenderPresent(renderer);
}

void audioSample(int16_t left, int16_t right)
{
    if (suppressVideo || !audioDevice) return;
    const int16_t samples[2] = {left, right};
    SDL_QueueAudio(audioDevice, samples, sizeof(samples));
}

size_t audioSampleBatch(const int16_t *samples, size_t frames)
{
    if (suppressVideo || !audioDevice || !samples) return frames;
    SDL_QueueAudio(audioDevice, samples, frames * 2 * sizeof(int16_t));
    return frames;
}

void inputPoll()
{
    SDL_PumpEvents();
}

void retroLog(retro_log_level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    std::vfprintf(stderr, format, args);
    va_end(args);
}

bool environmentCallback(unsigned command, void *data)
{
    switch (command) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        if (data) static_cast<retro_log_callback *>(data)->log = retroLog;
        return data != nullptr;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        if (data) *static_cast<const char **>(data) = systemDirectory.c_str();
        return data != nullptr;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        if (data) *static_cast<const char **>(data) = saveDirectory.c_str();
        return data != nullptr;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        if (data) pixelFormat = *static_cast<retro_pixel_format *>(data);
        return data != nullptr;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        if (data) *static_cast<int *>(data) = suppressVideo ? 0 : 3;
        return data != nullptr;
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        if (data) *static_cast<bool *>(data) = false;
        return true;
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS:
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        return true;
    default:
        return false;
    }
}

void configureDirectories()
{
    if (const char *roms = std::getenv("MACADE_ROM_DIR"); roms && roms[0]) {
        romDirectory = roms;
    }
    if (const char *runtime = std::getenv("MACADE_FIGHTCADE_RUNTIME"); runtime && runtime[0]) {
        systemDirectory = runtime;
        saveDirectory = runtime;
    }
}

void openController()
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        controller = SDL_GameControllerOpen(i);
        if (controller) break;
    }
}

} // namespace

int main(int argc, char **argv)
{
    std::string command;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i] ? argv[i] : "";
        if (argument == "-windowed") {
            fullscreen = false;
            continue;
        }
        if (argument == "-fullscreen" || argument == "-joy") continue;
        if (!argument.empty() && argument[0] == '-') continue;
        command = argument;
        break;
    }

    if (command.empty()) {
        std::fprintf(stderr, "usage: %s [-fullscreen|-windowed] <game-id|quark:command>\n", argv[0]);
        return kExitUsage;
    }

    configureDirectories();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "1");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL init failed: %s\n", SDL_GetError());
        return kExitUnsupported;
    }
    openController();

    signal(SIGINT, [](int) { stopRequested = true; });
    signal(SIGTERM, [](int) { stopRequested = true; });

    retro_set_environment(environmentCallback);
    retro_set_video_refresh(videoRefresh);
    retro_set_audio_sample(audioSample);
    retro_set_audio_sample_batch(audioSampleBatch);
    retro_set_input_poll(inputPoll);
    retro_set_input_state(inputState);
    retro_init();

    const bool quark = startsWith(command, "quark:");
    if (quark) {
        MacadeSnes9xQuarkConfigure({loadRomForGame, localJoypadBits, setRollbackMode});
        if (!MacadeSnes9xQuarkStart(command)) {
            std::fprintf(stderr, "Snes9x GGPO start failed: %s\n", command.c_str());
            retro_deinit();
            SDL_Quit();
            return kExitUnsupported;
        }
    } else {
        std::string game = command;
        if (startsWith(game, "macade:training,")) game = game.substr(16);
        if (!loadRomForGame(game)) {
            retro_deinit();
            SDL_Quit();
            return kExitRom;
        }
    }

    retro_system_av_info info{};
    if (gameLoaded) {
        retro_get_system_av_info(&info);
        startAudio(info.timing.sample_rate);
    }
    const double fps = info.timing.fps > 1.0 ? info.timing.fps : 60.0;
    const auto frameDuration = std::chrono::duration<double>(1.0 / fps);
    auto nextFrame = std::chrono::steady_clock::now();

    while (!stopRequested) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) stopRequested = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                fullscreen = !fullscreen;
                if (window) SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
            }
        }

        if (quark) {
            if (!gameLoaded) {
                MacadeSnes9xQuarkIdle(16);
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }
            if (!MacadeSnes9xQuarkSynchronizeInput()) {
                MacadeSnes9xQuarkIdle(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
        }

        retro_run();
        if (quark) {
            MacadeSnes9xQuarkAdvanceFrame();
            MacadeSnes9xQuarkIdle(0);
        }

        nextFrame += std::chrono::duration_cast<std::chrono::steady_clock::duration>(frameDuration);
        std::this_thread::sleep_until(nextFrame);
        if (std::chrono::steady_clock::now() > nextFrame + std::chrono::seconds(1)) {
            nextFrame = std::chrono::steady_clock::now();
        }
    }

    MacadeSnes9xQuarkStop();
    if (gameLoaded) retro_unload_game();
    retro_deinit();
    if (audioDevice) SDL_CloseAudioDevice(audioDevice);
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (controller) SDL_GameControllerClose(controller);
    SDL_Quit();
    return 0;
}
