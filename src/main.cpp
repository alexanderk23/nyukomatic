const char *logo{R"#(
               |                     |    o
,---.,   ..   .|__/ ,---.,--.--.,---.|--- .,---.
|   ||   ||   ||  \ |   ||  |  |,---||    ||
`   '`---|`---'`   ``---'`  '  '`---^`---'``---'
     `---'             :.by..stardust..2025.:
)#"};

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <misc/cpp/imgui_stdlib.h>
#include <misc/freetype/imgui_freetype.h>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <json.h>
#include <rang.hpp>
#include <xxhash.hpp>

#include "TextEditor.h"
#include "compiler.h"
#include "default_text.h"
#include "emulator.h"
#include "fonts/font_iosevka.h"
#include "fonts/font_monaco.h"
#include "logger.h"
#include "options.h"
#include "render.h"
#include "style.h"
#include "version.h"
#include "window.h"

using namespace nm::render;
using namespace nm::window;

emulator e;
bool showDemoWindow = false;

using nm::options::fps;
using nm::options::fpsLock;
using nm::options::fullBorder;
using nm::options::isSender;
using nm::options::vsync;

bool forceSend = false;
bool forceUpdate = false;
bool optionsVisible = false;

TextEditor editor;
bool editorVisible = true;
bool doCompile = false;
bool showWhitespaces = false;
bool somethingReceived = false;

ImFont *font;
ImFont *editorFont;

ImVec4 editorBgColor{0.0f, 0.0f, 0.0f, 0.6f};
ImVec4 clearColor{0.05f, 0.05f, 0.05f, 0.05f};

jt::Json json;
ix::WebSocket webSocket;
std::mutex mtx;

int frame_w = 352, frame_h = 280;

void updateTransform() {
    vec4<float> xform{1.0f, 1.0f, 1.0f, 1.0f};
    float aspect = 1.0f * (frame_h * display_w) / (frame_w * display_h);
    if (aspect > 1.0f)
        xform.x = (aspect > 0) ? 1.0f / aspect : 1.0f;
    else
        xform.y = aspect;
    setTransform(xform);
}

void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
    display_w = width;
    display_h = height;
    glViewport(0, 0, display_w, display_h);
    updateTransform();
}

void renderFrame();

void windowRefreshCallback(GLFWwindow *window) {
    renderFrame();
}

void setFixedWindowSize(GLFWwindow *window, int size) {
    glfwSetWindowSize(window, frame_w * size, frame_h * size);
}

void updateWindowTitle() {
    glfwSetWindowTitle(window, isSender ? "nyukomatic"
                                        : "nyukomatic – grabber");
}

void setBorderSize(bool full) {
    if (full) {
        setBorderOffsets(0, 0, 0, 0);
    } else {
        const int border_x = 24;
        const int border_y = 24;
        setBorderOffsets(
            e.border_width - border_x, e.top_border_height - border_y, //
            e.border_width - border_x, e.bottom_border_height - border_y);
    }
    frame_w = (256 + e.border_width + e.border_width) - borderOffsets.left -
              borderOffsets.right; // 352
    frame_h = (192 + e.top_border_height + e.bottom_border_height) -
              borderOffsets.top - borderOffsets.bottom; // 280
    updateTransform();
    glfwSetWindowSizeLimits(window, frame_w, frame_h, GLFW_DONT_CARE,
                            GLFW_DONT_CARE);
}

void renderFrame() {
    renderEmulatorImage();

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (editorVisible) {
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        const bool use_work_area = true;
        ImGui::SetNextWindowPos(use_work_area ? viewport->WorkPos
                                              : viewport->Pos);
        ImGui::SetNextWindowSize(use_work_area ? viewport->WorkSize
                                               : viewport->Size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, editorBgColor);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,
                              ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,
                              ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_NavHighlight,
                              ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

        if (ImGui::Begin("Editor", nullptr,
                         ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoBringToFrontOnFocus)) {
            if (!optionsVisible)
                ImGui::SetNextWindowFocus();
            ImGui::PushFont(editorFont);
            editor.Render("Editor");
            ImGui::PopFont();
        }

        ImGui::End();
        ImGui::PopStyleColor(5);
        ImGui::PopStyleVar(4);
    }

    if (optionsVisible) {
#ifndef NDEBUG
        // editor.ImGuiDebugPanel("EditorDebug");
        if (showDemoWindow)
            ImGui::ShowDemoWindow(&showDemoWindow);
#endif
        ImGuiIO &io = ImGui::GetIO();

        static bool initialPosSet = false;
        if (!initialPosSet) {
            initialPosSet = true;
            ImGui::SetNextWindowSize(ImVec2(700, -1));
            ImGui::SetNextWindowPos(
                ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        }

        if (ImGui::Begin("options", &optionsVisible,
                         ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoResize)) {
            ImGui::InputTextWithHint("server URL", "enter websocket server url",
                                     &nm::options::url);

            if (webSocket.getReadyState() == ix::ReadyState::Open) {
                if (ImGui::Button("disconnect"))
                    webSocket.stop();
            } else {
                if (ImGui::Button("connect")) {
                    webSocket.stop();
                    webSocket.setUrl(nm::options::url);
                    webSocket.start();
                }
            }

            ImGui::SameLine();
            ImGui::BeginDisabled(webSocket.getReadyState() !=
                                 ix::ReadyState::Closed);
            if (ImGui::Checkbox("sender mode", &isSender)) {
                LOG_I << "switched to " << (isSender ? "sender" : "grabber")
                      << " mode" << std::endl;
                updateWindowTitle();
            }
            ImGui::EndDisabled();

            ImGui::Dummy(ImVec2(0.0f, 20.0f));

            if (ImGui::Checkbox("full border", &fullBorder))
                setBorderSize(fullBorder);

            ImGui::SameLine();
            if (ImGui::Checkbox("vsync", &vsync))
                glfwSwapInterval((int)vsync);

            ImGui::SameLine();
            ImGui::Checkbox("lock fps", &fpsLock);

            ImGui::SliderInt("fps limit", (int *)&fps, 10.0, 100.0, "%d fps");

            ImGui::ColorEdit3("editor bg color", (float *)&editorBgColor);
            ImGui::SliderFloat("editor bg opacity", (float *)&editorBgColor + 3,
                               0.0f, 1.0f);

            ImGui::ColorEdit3("bg color", (float *)&bgColor);

#ifndef NDEBUG
            ImGui::Checkbox("imgui tools", &showDemoWindow);
#endif
            ImGui::Dummy(ImVec2(0.0f, 20.0f));

            ImGui::Text("%s %s", glGetString(GL_RENDERER),
                        glGetString(GL_VERSION));

            ImGui::Text("GLSL %s, %.1f FPS",
                        glGetString(GL_SHADING_LANGUAGE_VERSION), io.Framerate);

            auto textWidth = ImGui::CalcTextSize("v" FULL_VERSION).x;
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - textWidth);
            ImGui::PushStyleColor(ImGuiCol_TextLink,
                                  ImVec4(0.5f, 0.5f, 0.5f, 0.3f));
            ImGui::TextLinkOpenURL("v" FULL_VERSION, PROJECT_URL);
            ImGui::PopStyleColor();

            ImGui::End();
        }
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);
}

int main(int argc, char **argv) {
    std::cerr << rang::style::bold << rang::fgB::yellow << logo
              << rang::style::reset << std::endl;

    std::cerr << rang::style::bold << rang::fgB::green << PROJECT_NAME
              << rang::style::reset << " v" FULL_VERSION " (" PROJECT_URL ")"
              << std::endl;

    nm::options::parseOptions(argc, argv);

    ix::initNetSystem(); // Required on Windows

    webSocket.setOnMessageCallback([](const ix::WebSocketMessagePtr &msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            mtx.lock();
            jt::Json::Status status;
            std::tie(status, json) = jt::Json::parse(msg->str);
            mtx.unlock();

            if (status != jt::Json::success || !json.isObject()) {
                LOG_W << "ws: invalid json received" << std::endl;
                return;
            }

            somethingReceived = true;
        } else if (msg->type == ix::WebSocketMessageType::Open) {
            LOG_I << "ws: connected to " << nm::options::url << std::endl;
            forceSend = true;
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            LOG_E << "ws: connection error: " << msg->errorInfo.reason
                  << std::endl;
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            LOG_I << "ws: disconnected (" << msg->closeInfo.code << ")"
                  << std::endl;
        }
    });

    if (initWindow(frame_w * 3, frame_h * 3, "nyukomatic") != 0) {
        ix::uninitNetSystem();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    // io.IniFilename = "nyukomatic.ini";
    io.IniFilename = nullptr;
    // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    // io.ConfigInputTrickleEventQueue = false;
    io.KeyRepeatDelay = 0.25f;
    io.KeyRepeatRate = 0.035f;

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    ImFontConfig config;
    font = io.Fonts->AddFontFromMemoryCompressedTTF(
        font_iosevka_compressed_data, font_iosevka_compressed_size,
        22.0f * highDPIscaleFactor, &config);

#if IMGUI_VERSION_NUM > 19180
    config.GlyphExtraAdvanceX = -1.0f;
#else
    config.GlyphExtraSpacing.x = -1.0f;
#endif
    config.GlyphOffset.y = 2.0f;
    config.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Bold;
    editorFont = io.Fonts->AddFontFromMemoryCompressedTTF(
        font_monaco_compressed_data, font_monaco_compressed_size,
        20.0f * highDPIscaleFactor, &config);
    editor.SetLineSpacing(1.2f);

    editor.SetLanguageDefinition(TextEditor::LanguageDefinition::Z80Asm());
    editor.SetCompletePairedGlyphs(true);
    editor.SetShowWhitespacesEnabled(false);
    editor.SetTabSize(8);
    editor.SetPalette(textEditorPalette);

    webSocket.setUrl(nm::options::url);

    if (isSender) {
        editor.SetText(defaultHeader + defaultText);
        doCompile = true;
    } else {
        LOG_I << "running in grabber mode" << std::endl;
        webSocket.start();
        updateWindowTitle();
        editor.SetText(defaultHeader +
                       std::string("\n; waiting for sender ..."));
        editor.changed = false;
    }

    // Setup Dear ImGui style
    setImGuiStyle(highDPIscaleFactor);

    glfwSwapInterval((int)vsync);
    setBorderSize(fullBorder);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetWindowRefreshCallback(window, windowRefreshCallback);

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        auto forceCompile = isSender && ImGui::IsKeyPressed(ImGuiKey_F5);
        forceSend |= forceCompile;
        forceCompile |= forceUpdate && !isSender;
        forceUpdate = false;

        editor.SetReadOnlyEnabled(!isSender);
        bool doSend = forceSend;
        static int lastRow = 0;
        static int lastCol = 0;

        jt::Json outJson;
        if (isSender) {
            if (forceSend)
                outJson["forceUpdate"] = true;

            int row = 0, col = 0;

            editor.GetCursorPosition(row, col);

            if (row != lastRow || col != lastCol || forceSend) {
                outJson["cursor"]["row"] = row;
                outJson["cursor"]["col"] = col;

                lastRow = row;
                lastCol = col;
                doSend = true;
            }

            if (editor.changed || forceSend) {
                outJson["source"] = editor.GetText();
                doSend = true;
            }
        }

        if (somethingReceived && mtx.try_lock()) {
            somethingReceived = false;

            if (!isSender) {
                if (json["hideCode"].isBool())
                    editorVisible = !json["hideCode"].getBool();

                if (json["forceUpdate"].isBool())
                    forceUpdate = json["forceUpdate"].getBool();

                if (json["source"].isString())
                    editor.SetText(json["source"].getString());
                else if (json["code"].isString())
                    editor.SetText(json["code"].getString());

                if (json["cursor"]["row"].isNumber() &&
                    json["cursor"]["col"].isNumber()) {
                    int row = json["cursor"]["row"].getNumber();
                    int col = json["cursor"]["col"].getNumber();
                    editor.SetCursorPositionAbs(row, col);
                }
            }

            if (json["ports"].isArray()) {
                auto &objs = json["ports"].getArray();
                for (auto &obj : objs) {
                    if (!obj.isArray() || !obj[0].isNumber())
                        continue;
                    fast_u16 port = obj[0].getNumber();
                    auto &val = obj[1];
                    if (val.isNumber()) {
                        e.set_port_value(port, val.getNumber());
                    } else if (val.isArray()) {
                        for (auto &item : val.getArray()) {
                            e.set_port_value(port,
                                             item.isNumber() ? item.getNumber()
                                                             : 0x00);
                            port += 0x100;
                        }
                    }
                }
            }

            mtx.unlock();
        }

        static double lastChangedAt = 0;
        double now = glfwGetTime();
        if (editor.changed) {
            editor.changed = false;
            lastChangedAt = now;
        }

        if ((lastChangedAt != 0) && (now > lastChangedAt + 0.02)) { // 0.08
            doCompile = true;
            lastChangedAt = 0;
        }

        if (isSender && ImGui::IsKeyPressed(ImGuiKey_F12)) {
            e.on_reset(false);
            doCompile = false;
            lastChangedAt = 0;
            editor.changed = false;
        }

        if (doCompile || forceCompile) {
            doCompile = false;
            lastChangedAt = 0;

            static xxh::hash32_t prev_hash = 0;
            nm::compiler::input_t source = editor.GetText();
            nm::compiler::output_t output;
            nm::compiler::errors_t errors;

            if (nm::compiler::compile(source, output, errors)) {
                if (!forceCompile) {
                    auto hash = xxh::xxhash<32>(output);
                    if (hash != prev_hash) {
                        prev_hash = hash;
                        forceCompile = true;
                    }
                }
                if (forceCompile) {
                    e.on_reset(false);
                    e.set_sp(0x5fff);
                    auto data = output.data();
                    auto len = output.length();
                    e.write_ram(0x4000, data, len);
                    unsigned int start_addr = 0x6000;
                    while (!data[start_addr - 0x4000] && start_addr <= 0xffff)
                        start_addr++;
                    e.set_pc(start_addr);
                }
            }

            editor.SetErrorMarkers(errors);
        }

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        auto alt = io.KeyAlt;
        auto ctrl = io.KeyCtrl;
        auto shift = io.KeyShift;
        auto super = io.KeySuper;

        if (ImGui::IsKeyPressed(ImGuiKey_F3)) {
            showWhitespaces ^= true;
            editor.SetShowWhitespacesEnabled(showWhitespaces);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F6)) {
            editorVisible ^= true;
            doSend = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            if (optionsVisible)
                optionsVisible = false;
            else {
                editorVisible ^= true;
                doSend = true;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
            optionsVisible ^= true;
        }

        if (ctrl && io.ConfigMacOSXBehaviors &&
            ImGui::IsKeyPressed(ImGuiKey_Comma)) {
            optionsVisible = true;
        }

        uploadEmulatorImage(e.process_frame(), e.frame_width, e.frame_height);
        renderFrame();

        if (isSender && doSend &&
            webSocket.getReadyState() == ix::ReadyState::Open) {
            outJson["hideCode"] = !editorVisible;
            webSocket.send(outJson.toString());
            forceSend = false;
        }

        if (fpsLock)
            limitFPS(fps);

        if (ctrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_1))
                setFixedWindowSize(window, 1);
            else if (ImGui::IsKeyPressed(ImGuiKey_2))
                setFixedWindowSize(window, 2);
            else if (ImGui::IsKeyPressed(ImGuiKey_3))
                setFixedWindowSize(window, 3);
            else if (ImGui::IsKeyPressed(ImGuiKey_4))
                setFixedWindowSize(window, 4);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    deinitWindow();
    ix::uninitNetSystem();

    return 0;
}
