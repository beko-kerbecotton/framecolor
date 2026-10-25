// Implementation of the OpenVR connection and overlay.
#include "vr_overlay.h"

#include "openvr.h"

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace {

constexpr const char* kDashboardKey = "local.framecolor";
constexpr const char* kDashboardName = "Display";
// The panel image is shown 2.8 m wide (about the same px-to-meter ratio as v2's 1024px / 2.4m;
// height works out to roughly 1.52m)
constexpr float kDashboardWidthM = 2.8f;
// On shutdown, how long to wait after clearing the overlay before VR_Shutdown (about 36 frames at 90Hz)
constexpr int kShutdownWaitMs = 400;

/**
 * Return the overlay error name.
 * @param error the error
 * @return the name (e.g. VROverlayError_None)
 */
const char* overlayErrorName(vr::EVROverlayError error) {
    return vr::VROverlay()->GetOverlayErrorNameFromEnum(error);
}

/**
 * Log an overlay error to stderr unless it's success.
 * @param what what was being done
 * @param error the error
 * @return true on success
 */
bool checkOverlay(const char* what, vr::EVROverlayError error) {
    if (error == vr::VROverlayError_None) return true;
    std::fprintf(stderr, "[VR] %s failed: %s\n", what, overlayErrorName(error));
    return false;
}

/**
 * Log one shutdown step's result, whether it succeeded or failed (to verify order and outcome later).
 * @param what what was done
 * @param error the return value
 */
void logShutdownStep(const char* what, vr::EVROverlayError error) {
    std::fprintf(stderr, "[VR] shutdown: %s -> %s\n", what, overlayErrorName(error));
}

}  // namespace

VrOverlay::VrOverlay() = default;

VrOverlay::~VrOverlay() {
    shutdown();
}

int VrOverlay::findVrserverPid() {
    DIR* proc = ::opendir("/proc");
    if (proc == nullptr) return -1;
    int found = -1;
    while (const dirent* entry = ::readdir(proc)) {
        const char* name = entry->d_name;
        if (name[0] < '0' || name[0] > '9') continue;
        const std::string commPath = std::string("/proc/") + name + "/comm";
        const int fd = ::open(commPath.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd < 0) continue;
        char comm[64] = {};
        const ssize_t n = ::read(fd, comm, sizeof(comm) - 1);
        ::close(fd);
        if (n > 0 && std::strncmp(comm, "vrserver\n", 9) == 0) {
            found = std::atoi(name);
            break;
        }
    }
    ::closedir(proc);
    return found;
}

VrOverlay::ConnectResult VrOverlay::connect(int width, int height, std::string& message) {
    if (connected_) return ConnectResult::Ok;

    // 1) Check with a Background-type init whether SteamVR is running (don't launch it if not)
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);
    if (error != vr::VRInitError_None) {
        message = vr::VR_GetVRInitErrorAsEnglishDescription(error);
        return error == vr::VRInitError_Init_NoServerForBackgroundApp ? ConnectResult::NotRunning
                                                                        : ConnectResult::Error;
    }
    vr::VR_Shutdown();

    // 2) Reconnect as an overlay app
    vr::VR_Init(&error, vr::VRApplication_Overlay);
    if (error != vr::VRInitError_None) {
        message = vr::VR_GetVRInitErrorAsEnglishDescription(error);
        return ConnectResult::Error;
    }
    connected_ = true;

    // 3) Vulkan (built with the extensions OpenVR requires, on the HMD's GPU)
    std::string vkMessage;
    if (!vulkan_.init(vkMessage)) {
        message = "Vulkan setup failed: " + vkMessage;
        shutdown();
        return ConnectResult::Error;
    }

    // 4) Dashboard panel and thumbnail
    vr::IVROverlay* overlay = vr::VROverlay();
    vr::VROverlayHandle_t main = vr::k_ulOverlayHandleInvalid;
    vr::VROverlayHandle_t thumbnail = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError createError =
        overlay->CreateDashboardOverlay(kDashboardKey, kDashboardName, &main, &thumbnail);
    std::fprintf(stderr, "[VR] CreateDashboardOverlay(%s) -> %s\n", kDashboardKey, overlayErrorName(createError));
    if (createError != vr::VROverlayError_None) {
        message = std::string("CreateDashboardOverlay: ") + overlayErrorName(createError);
        shutdown();
        return ConnectResult::Error;
    }
    dashboardHandle_ = main;
    thumbnailHandle_ = thumbnail;
    panelHeight_ = height;

    checkOverlay("SetOverlayWidthInMeters", overlay->SetOverlayWidthInMeters(main, kDashboardWidthM));
    checkOverlay("SetOverlayInputMethod", overlay->SetOverlayInputMethod(main, vr::VROverlayInputMethod_Mouse));
    // Align mouse coordinates with the image's px
    const vr::HmdVector2_t scale = {{static_cast<float>(width), static_cast<float>(height)}};
    checkOverlay("SetOverlayMouseScale", overlay->SetOverlayMouseScale(main, &scale));
    // Show "close" when hovering the dashboard's icon bar. Pressing it delivers VREvent_OverlayClosed
    const vr::EVROverlayError closeError =
        overlay->SetOverlayFlag(main, vr::VROverlayFlags_EnableControlBarClose, true);
    bool closeEnabled = false;
    const vr::EVROverlayError readError =
        overlay->GetOverlayFlag(main, vr::VROverlayFlags_EnableControlBarClose, &closeEnabled);
    std::fprintf(stderr, "[VR] SetOverlayFlag(EnableControlBarClose) -> %s (read back: %s, %s)\n",
                 overlayErrorName(closeError), overlayErrorName(readError), closeEnabled ? "true" : "false");

    if (!panelTexture_.create(vulkan_, width, height, vkMessage)) {
        message = "Failed to create the panel texture: " + vkMessage;
        shutdown();
        return ConnectResult::Error;
    }

    vrserverPid_ = findVrserverPid();
    lastPanelError_.clear();
    return ConnectResult::Ok;
}

void VrOverlay::shutdown() {
    if (!connected_) return;
    vr::IVROverlay* overlay = vr::VROverlay();
    std::fprintf(stderr, "[VR] starting shutdown\n");

    // 1) Clear the texture (so the compositor stops referencing our image)
    if (dashboardHandle_ != 0) {
        logShutdownStep("ClearOverlayTexture(panel)", overlay->ClearOverlayTexture(dashboardHandle_));
        logShutdownStep("ClearOverlayTexture(thumbnail)", overlay->ClearOverlayTexture(thumbnailHandle_));
    }
    // 2) Destroy the overlay (the thumbnail goes away along with the panel)
    if (dashboardHandle_ != 0) logShutdownStep("DestroyOverlay(panel)", overlay->DestroyOverlay(dashboardHandle_));
    dashboardHandle_ = 0;
    thumbnailHandle_ = 0;

    // 3) Wait a few compositor frames for it to drop the cleared texture
    std::this_thread::sleep_for(std::chrono::milliseconds(kShutdownWaitMs));
    std::fprintf(stderr, "[VR] shutdown: waited %dms\n", kShutdownWaitMs);

    // 4) Close OpenVR (OpenVR requires that Vulkan images are destroyed only after this)
    vr::VR_Shutdown();
    connected_ = false;
    std::fprintf(stderr, "[VR] shutdown: VR_Shutdown done\n");

    // 5) Destroy the Vulkan images and device
    thumbnailTexture_.destroy();
    panelTexture_.destroy();
    vulkan_.destroy();
    std::fprintf(stderr, "[VR] shutdown: Vulkan cleaned up\n");
}

VrEvents VrOverlay::pollEvents() {
    VrEvents result;
    if (!connected_) return result;
    vr::VREvent_t event {};
    while (vr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
        if (event.eventType == vr::VREvent_Quit) result.quit = true;
    }
    if (dashboardHandle_ != 0) {
        while (vr::VROverlay()->PollNextOverlayEvent(dashboardHandle_, &event, sizeof(event))) {
            // Mouse coordinates have their origin at the bottom-left, so flip to put it at the top
            const double x = event.data.mouse.x;
            const double y = panelHeight_ - event.data.mouse.y;
            switch (event.eventType) {
                case vr::VREvent_MouseMove: result.pointer.push_back({PointerInput::Type::Move, x, y}); break;
                case vr::VREvent_MouseButtonDown:
                    if (event.data.mouse.button == vr::VRMouseButton_Left) {
                        result.pointer.push_back({PointerInput::Type::Down, x, y});
                    }
                    break;
                case vr::VREvent_MouseButtonUp:
                    if (event.data.mouse.button == vr::VRMouseButton_Left) {
                        result.pointer.push_back({PointerInput::Type::Up, x, y});
                    }
                    break;
                case vr::VREvent_FocusLeave: result.pointer.push_back({PointerInput::Type::Leave, 0, 0}); break;
                // The dashboard icon bar's "close" (VROverlayFlags_EnableControlBarClose).
                // This is distinct from SteamVR's own shutdown (VRSystem's VREvent_Quit)
                case vr::VREvent_OverlayClosed:
                    std::fprintf(stderr, "[VR] dashboard \"close\" was pressed\n");
                    result.closeRequested = true;
                    break;
                case vr::VREvent_OverlayShown: std::fprintf(stderr, "[VR] panel shown\n"); break;
                case vr::VREvent_OverlayHidden: std::fprintf(stderr, "[VR] panel hidden\n"); break;
                default: break;
            }
        }
    }
    if (result.quit) {
        std::fprintf(stderr, "[VR] received shutdown notice from SteamVR\n");
        vr::VRSystem()->AcknowledgeQuit_Exiting();
    }
    return result;
}

bool VrOverlay::steamVrAlive() const {
    if (vrserverPid_ <= 0) return true;  // treat as alive if it can't be checked
    return ::kill(vrserverPid_, 0) == 0 || errno == EPERM;
}

bool VrOverlay::panelVisible() const {
    return connected_ && dashboardHandle_ != 0 && vr::VROverlay()->IsOverlayVisible(dashboardHandle_);
}

void VrOverlay::showPanel() {
    if (!connected_ || dashboardHandle_ == 0) return;
    // No return value; whether it actually opened shows up later via IsOverlayVisible / VREvent_OverlayShown
    vr::VROverlay()->ShowDashboard(kDashboardKey);
    std::fprintf(stderr, "[VR] called ShowDashboard(%s)\n", kDashboardKey);
}

bool VrOverlay::submitThumbnail(const uint8_t* rgba, int size) {
    if (!connected_ || thumbnailHandle_ == 0) return false;
    std::string message;
    if (!thumbnailTexture_.ready() && !thumbnailTexture_.create(vulkan_, size, size, message)) {
        std::fprintf(stderr, "[Vulkan] can't create the thumbnail texture: %s\n", message.c_str());
        return false;
    }
    if (!thumbnailTexture_.update(thumbnailHandle_, rgba, message)) {
        std::fprintf(stderr, "[VR] can't send the thumbnail: %s\n", message.c_str());
        return false;
    }
    return true;
}

bool VrOverlay::submitPanel(const uint8_t* rgba) {
    if (!connected_ || dashboardHandle_ == 0 || !panelTexture_.ready()) return false;
    std::string message;
    const bool ok = panelTexture_.update(dashboardHandle_, rgba, message);
    // Don't spam the same error every time
    if (message != lastPanelError_) {
        if (!ok) std::fprintf(stderr, "[VR] can't send the panel: %s\n", message.c_str());
        lastPanelError_ = message;
    }
    return ok;
}

void VrOverlay::setOverlayName(const std::string& name) {
    if (connected_ && dashboardHandle_ != 0 && vr::VROverlay() != nullptr) {
        vr::VROverlay()->SetOverlayName(dashboardHandle_, name.c_str());
    }
}

void VrOverlay::logOverlayState(const char* when) const {
    if (!connected_) return;
    vr::IVROverlay* overlay = vr::VROverlay();
    vr::VROverlayHandle_t found = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError findError = overlay->FindOverlay(kDashboardKey, &found);
    uint32_t w = 0;
    uint32_t h = 0;
    const vr::EVROverlayError sizeError = overlay->GetOverlayTextureSize(dashboardHandle_, &w, &h);
    uint32_t tw = 0;
    uint32_t th = 0;
    const vr::EVROverlayError thumbError = overlay->GetOverlayTextureSize(thumbnailHandle_, &tw, &th);
    std::fprintf(stderr,
                 "[VR] check (%s): FindOverlay(%s) -> %s (same handle: %s) panel texture %ux%u (%s) "
                 "thumbnail texture %ux%u (%s) panel visible: %s dashboard: %s\n",
                 when, kDashboardKey, overlayErrorName(findError), found == dashboardHandle_ ? "yes" : "no", w, h,
                 overlayErrorName(sizeError), tw, th, overlayErrorName(thumbError),
                 overlay->IsOverlayVisible(dashboardHandle_) ? "yes" : "no",
                 overlay->IsDashboardVisible() ? "open" : "closed");
}

int VrOverlay::probe() {
    vr::EVRInitError error = vr::VRInitError_None;
    vr::VR_Init(&error, vr::VRApplication_Background);
    if (error != vr::VRInitError_None) {
        std::printf("Can't connect to SteamVR: %s\n", vr::VR_GetVRInitErrorAsEnglishDescription(error));
        return 1;
    }
    vr::IVROverlay* overlay = vr::VROverlay();
    int code = 1;
    vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
    const vr::EVROverlayError findError = overlay ? overlay->FindOverlay(kDashboardKey, &handle)
                                                  : vr::VROverlayError_RequestFailed;
    std::printf("FindOverlay(%s) -> %s\n", kDashboardKey, overlay ? overlayErrorName(findError) : "no IVROverlay");
    if (overlay != nullptr && findError == vr::VROverlayError_None) {
        code = 0;
        char name[128] = {};
        overlay->GetOverlayName(handle, name, sizeof(name), nullptr);
        bool closeFlag = false;
        overlay->GetOverlayFlag(handle, vr::VROverlayFlags_EnableControlBarClose, &closeFlag);
        float widthM = 0.0f;
        overlay->GetOverlayWidthInMeters(handle, &widthM);
        std::printf("  name: %s  width: %.2fm  close button: %s  panel visible: %s  dashboard: %s\n", name, widthM,
                    closeFlag ? "yes" : "no", overlay->IsOverlayVisible(handle) ? "yes" : "no",
                    overlay->IsDashboardVisible() ? "open" : "closed");
        uint32_t w = 0;
        uint32_t h = 0;
        const vr::EVROverlayError sizeError = overlay->GetOverlayTextureSize(handle, &w, &h);
        std::printf("  GetOverlayTextureSize -> %s (%ux%u)\n", overlayErrorName(sizeError), w, h);
        // Don't use GetOverlayImageData: as tested on 2026-09-27, calling it on an overlay holding a
        // Vulkan texture crashed the calling process (this one) with SIGSEGV (SteamVR itself was fine).
        // Confirm the image was set by checking its size instead.
    }
    vr::VR_Shutdown();
    return code;
}
