#include "window.h"
#include "logger.h"

namespace nm::window {

GLFWwindow *window = nullptr;
const char *glsl_version;
int display_w, display_h;
float highDPIscaleFactor = 1.0;

void glfw_error_callback(int error, const char *description) {
    LOG_E << "GLFW error " << error << ": " << description << std::endl;
}

int initWindow(int width, int height, const char *title) {
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
        return 1;

        // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100
    glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on Mac
    glfwWindowHintString(GLFW_COCOA_FRAME_NAME, "nyukomatic");
#elif 0
    // GL 3.0 + GLSL 130
    glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#else
    // linux temp
    glsl_version = "#version 120";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    // glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

#ifdef _WIN32
    // if it's a HighDPI monitor, try to scale everything
    GLFWmonitor *monitor = glfwGetPrimaryMonitor();

    float xscale, yscale;

    glfwGetMonitorContentScale(monitor, &xscale, &yscale);
    if (xscale > 1 || yscale > 1) {
        highDPIscaleFactor = xscale;
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
    }
#elif __APPLE__
    // to prevent 1200x800 from becoming 2400x1600
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
#endif

    // Create window with graphics context
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (window == nullptr) {
        LOG_F << "failed to create GLFW window" << std::endl;
        return 1;
    }

    glfwMakeContextCurrent(window);

    if (glewInit() != GLEW_OK) {
        std::cerr << "failed to init GLEW" << std::endl;
        return 1;
    }

    // glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    // glfwSetWindowAspectRatio(window, 352, 280);

    return 0;
}

void deinitWindow() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

/**
 * Cross-platform sleep function for C
 * @param int milliseconds
 */
void sleep_ms(int ms) {
#ifdef WIN32
    Sleep(ms);
#elif _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#else
    usleep(ms * 1000);
#endif
}

void limitFPS(double fps) {
    static double last_time = glfwGetTime();
    static double accumulator = 0;

    double fpsLimit = 1.0 / fps;
    bool done = false;

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        double delta_frame_time = now - last_time;
        last_time = now;

        if (abs(delta_frame_time - fpsLimit * 0.5) < .0002)
            delta_frame_time = fpsLimit * 0.5;

        if (abs(delta_frame_time - fpsLimit) < .0002)
            delta_frame_time = fpsLimit;

        if (abs(delta_frame_time - fpsLimit * 2.0) < .0002)
            delta_frame_time = fpsLimit * 2.0;

        accumulator += delta_frame_time;

        while (accumulator >= fpsLimit) {
            accumulator -= fpsLimit;
            done = true;
        }

        if (done)
            break;

        sleep_ms(1);
    }
}

} // namespace nm::window
