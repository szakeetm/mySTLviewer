#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nfd.h>
#include <iostream>
#include <limits>
#include "STLLoader.h"
#include "Renderer.h"

const int WINDOW_WIDTH = 1024;
const int WINDOW_HEIGHT = 768;

class Application {
public:
    Application() : m_window(nullptr), m_glContext(nullptr), m_running(false), 
                    m_rotationX(30.0f), m_rotationY(45.0f), m_zoom(2.0f),
                    m_pan(0.0f, 0.0f),
                    m_frameCount(0), m_lastFpsTime(0), m_fps(0.0f),
                    m_bgVAO(0), m_bgVBO(0), m_bgShaderProgram(0),
                    m_pivotActive(false), m_pivotModel(0.0f),
                    m_axesVAO(0), m_axesVBO(0), m_axesProgram(0), m_axisLength(0.0f) {}
    
    ~Application() {
        cleanup();
    }
    
    bool initialize(const std::string& stlFile) {
        // On macOS, ensure this is a foreground app (not background-only)
        SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "0");
        // Initialize SDL
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    // Request MSAA 4x for antialiased wireframe edges
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
        
        // Create window
        m_window = SDL_CreateWindow(
            "STL Viewer",
            WINDOW_WIDTH, WINDOW_HEIGHT,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );
        
        if (!m_window) {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Show and raise window to bring it to the front (important on macOS)
        SDL_ShowWindow(m_window);
        SDL_RaiseWindow(m_window);
        SDL_SetWindowKeyboardGrab(m_window, false);
        if (!SDL_StartTextInput(m_window)) { // enable text input events as a fallback
            std::cerr << "Warning: failed to start text input: " << SDL_GetError() << std::endl;
        }
        // Give the OS a brief moment to grant keyboard focus to our window
        for (int attempt = 0; attempt < 50; ++attempt) { // ~500ms total
            Uint32 flags = SDL_GetWindowFlags(m_window);
            if (flags & SDL_WINDOW_INPUT_FOCUS) {
                break;
            }
            SDL_PumpEvents();
            SDL_RaiseWindow(m_window);
            SDL_Delay(10);
        }
        
        // Create OpenGL context
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext) {
            std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Enable VSync
        if (!SDL_GL_SetSwapInterval(1)) {
            std::cerr << "Warning: VSync not supported: " << SDL_GetError() << std::endl;
        } else {
            std::cout << "VSync enabled" << std::endl;
        }
        
        // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }
        
        // Verify OpenGL version >= 3.3
        GLint glMajor = 0, glMinor = 0;
        glGetIntegerv(GL_MAJOR_VERSION, &glMajor);
        glGetIntegerv(GL_MINOR_VERSION, &glMinor);
        std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
        if (glMajor < 3 || (glMajor == 3 && glMinor < 3)) {
            std::cerr << "Error: OpenGL 3.3 or newer is required. Detected "
                      << glMajor << "." << glMinor << std::endl;
            std::cerr << "Please update your graphics drivers or run on a system with OpenGL 3.3+ support." << std::endl;
            return false;
        }
        
    // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    // Enable multisampling if available
    glEnable(GL_MULTISAMPLE);
        
    // Disable dithering to avoid greyish tones on some drivers
    glDisable(GL_DITHER);
        
    // Enable back-face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        
        // Set clear color to black (gradient will cover it)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        
    // Initialize background gradient
        initBackgroundGradient();
        // Initialize axes renderer
        initAxesRenderer();
        
        // Initialize renderer
        if (!m_renderer.initialize()) {
            std::cerr << "Failed to initialize renderer" << std::endl;
            return false;
        }
        
        // If no file path was provided, open file dialog AFTER window is active
        std::string selectedFile = stlFile;
        if (selectedFile.empty()) {
            if (!NFD_Init()) {
                std::cerr << "Error initializing file dialog: " << NFD_GetError() << std::endl;
                return false;
            }
            nfdchar_t* outPath = nullptr;
            nfdfilteritem_t filters[1] = { { "STL Files", "stl" } };
            nfdresult_t result = NFD_OpenDialog(&outPath, filters, 1, nullptr);
            if (result == NFD_OKAY) {
                selectedFile = outPath;
                std::cout << "Selected file: " << selectedFile << std::endl;
                NFD_FreePath(outPath);
            } else if (result == NFD_CANCEL) {
                std::cout << "User cancelled file selection" << std::endl;
                NFD_Quit();
                return false; // treat cancel as a clean exit by caller
            } else {
                std::cerr << "Error opening file dialog: " << NFD_GetError() << std::endl;
                NFD_Quit();
                return false;
            }
            NFD_Quit();
        }

        // Load STL file
        if (!selectedFile.empty()) {
            auto mesh = STLLoader::load(selectedFile);
            if (mesh) {
                m_renderer.setMesh(std::move(mesh));
                
                // Adjust zoom based on model size
                if (m_renderer.getMesh()) {
                    float extent = m_renderer.getMesh()->getMaxExtent();
                    m_zoom = extent * 1.5f;
                    m_axisLength = extent * 0.1f; // size of pivot axes
                }
            } else {
                std::cerr << "Failed to load STL file: " << selectedFile << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    void run() {
        m_running = true;
        m_lastFpsTime = SDL_GetTicksNS();
        m_frameCount = 0;
        
        while (m_running) {
            handleEvents();
            render();
            updateFPS();
        }
    }
    
private:
    void initBackgroundGradient() {
        // Fullscreen quad vertices (positions + colors)
        float vertices[] = {
            // positions        // colors (lighter blue at top, black at bottom)
            -1.0f,  1.0f, 0.0f,  0.3f, 0.5f, 0.7f, // top-left (lighter blue)
             1.0f,  1.0f, 0.0f,  0.3f, 0.5f, 0.7f, // top-right (lighter blue)
            -1.0f, -1.0f, 0.0f,  0.0f,  0.0f,  0.0f,  // bottom-left
             1.0f, -1.0f, 0.0f,  0.0f,  0.0f,  0.0f   // bottom-right
        };
        
        glGenVertexArrays(1, &m_bgVAO);
        glGenBuffers(1, &m_bgVBO);
        
        glBindVertexArray(m_bgVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_bgVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Color attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
        
        // Create simple shader for gradient
        const char* vertexShaderSrc = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aColor;
            out vec3 Color;
            void main() {
                Color = aColor;
                gl_Position = vec4(aPos, 1.0);
            }
        )";
        
        const char* fragmentShaderSrc = R"(
            #version 330 core
            in vec3 Color;
            out vec4 FragColor;
            void main() {
                FragColor = vec4(Color, 1.0);
            }
        )";
        
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
        glCompileShader(vertexShader);
        
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
        glCompileShader(fragmentShader);
        
        m_bgShaderProgram = glCreateProgram();
        glAttachShader(m_bgShaderProgram, vertexShader);
        glAttachShader(m_bgShaderProgram, fragmentShader);
        glLinkProgram(m_bgShaderProgram);
        
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }
    
    void initAxesRenderer() {
        // Simple shader for colored lines in world space
        const char* vsrc = R"(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aColor;
            uniform mat4 projection;
            uniform mat4 view;
            out vec3 Color;
            void main(){
                Color = aColor;
                gl_Position = projection * view * vec4(aPos, 1.0);
            }
        )";
        const char* fsrc = R"(
            #version 330 core
            in vec3 Color;
            out vec4 FragColor;
            void main(){
                FragColor = vec4(Color, 1.0);
            }
        )";
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vsrc, nullptr);
        glCompileShader(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fsrc, nullptr);
        glCompileShader(fs);
        m_axesProgram = glCreateProgram();
        glAttachShader(m_axesProgram, vs);
        glAttachShader(m_axesProgram, fs);
        glLinkProgram(m_axesProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);

        glGenVertexArrays(1, &m_axesVAO);
        glGenBuffers(1, &m_axesVBO);
        glBindVertexArray(m_axesVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_axesVBO);
        // allocate dynamic buffer for 6 vertices (pos+color)
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * (3 + 3), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void renderBackground() {
        // Ensure background renders regardless of current GL state
        GLboolean wasCull = glIsEnabled(GL_CULL_FACE);
        GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE); // Don't write to depth buffer

        glUseProgram(m_bgShaderProgram);
        glBindVertexArray(m_bgVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE); // Re-enable depth writing
        if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (wasCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    }
    
    void updateFPS() {
        m_frameCount++;
        Uint64 currentTime = SDL_GetTicksNS();
        Uint64 elapsed = currentTime - m_lastFpsTime;
        
        // Update FPS every second (1 billion nanoseconds)
        if (elapsed >= 1000000000ULL) {
            m_fps = m_frameCount * 1000000000.0f / elapsed;
            
            // Update window title with FPS
            std::string title = "STL Viewer - FPS: " + std::to_string(static_cast<int>(m_fps));
            SDL_SetWindowTitle(m_window, title.c_str());
            
            m_frameCount = 0;
            m_lastFpsTime = currentTime;
        }
    }
    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    m_running = false;
                    break;
                    
                case SDL_EVENT_KEY_DOWN:
                    // Use scancodes for robust key handling across layouts
                    handleKeyPress(event.key.scancode, event.key.mod);
                    break;
                case SDL_EVENT_TEXT_INPUT:
                    if (event.text.text && event.text.text[0] != '\0') {
                        char c = event.text.text[0];
                        switch (c) {
                            case 'q': case 'Q':
                                std::cout << "Q pressed (text) - Quitting" << std::endl;
                                m_running = false;
                                break;
                            case 'w': case 'W':
                                std::cout << "W pressed (text) - Switching to wireframe mode" << std::endl;
                                m_renderer.setRenderMode(RenderMode::WIREFRAME);
                                break;
                            case 's': case 'S':
                                std::cout << "S pressed (text) - Switching to solid mode" << std::endl;
                                m_renderer.setRenderMode(RenderMode::SOLID);
                                break;
                            case 'r': case 'R':
                                std::cout << "R pressed (text) - Resetting view" << std::endl;
                                m_rotationX = 30.0f;
                                m_rotationY = 45.0f;
                                if (m_renderer.getMesh()) {
                                    float extent = m_renderer.getMesh()->getMaxExtent();
                                    m_zoom = extent * 1.5f;
                                }
                                m_pan = glm::vec2(0.0f);
                                m_pivotActive = false;
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_MOTION:
                    if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)) {
                        m_rotationY += event.motion.xrel * 0.25f;
                        m_rotationX += event.motion.yrel * 0.25f;
                        m_rotationX = glm::clamp(m_rotationX, -89.0f, 89.0f);
                    }
                    if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_MIDDLE)) {
                        int width = 1, height = 1;
                        SDL_GetWindowSize(m_window, &width, &height);
                        float aspect = (float)width / (float)height;
                        float orthoSize = m_zoom;
                        // Convert pixel delta to world units based on ortho projection
                        float worldPerPixelY = (2.0f * orthoSize) / (float)height;
                        float worldPerPixelX = (2.0f * orthoSize * aspect) / (float)width;
                        // Update pan so the model follows the mouse (drag right -> model right, drag down -> model down)
                        m_pan.x += event.motion.xrel * worldPerPixelX;
                        m_pan.y -= event.motion.yrel * worldPerPixelY;
                    }
                    break;
                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        int mx = event.button.x;
                        int my = event.button.y;
                        pickPivot(mx, my);
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_WHEEL:
                    {
                        // Scale zoom speed with current zoom level (5% of current zoom)
                        float zoomDelta = event.wheel.y * m_zoom * 0.1f;
                        m_zoom -= zoomDelta;
                        m_zoom = glm::max(0.1f, m_zoom);
                    }
                    break;
                    
                case SDL_EVENT_WINDOW_RESIZED:
                    glViewport(0, 0, event.window.data1, event.window.data2);
                    break;
            }
        }
    }
    
    void handleKeyPress(SDL_Scancode scancode, SDL_Keymod /*mod*/) {
        switch (scancode) {
            case SDL_SCANCODE_ESCAPE:
                std::cout << "ESC pressed - Quitting" << std::endl;
                m_running = false;
                break;
            case SDL_SCANCODE_Q:
                std::cout << "Q pressed - Quitting" << std::endl;
                m_running = false;
                break;
            case SDL_SCANCODE_V: {
                // Toggle VSync at runtime (SDL3: get uses out-param, set returns bool)
                int current = 0;
                if (!SDL_GL_GetSwapInterval(&current)) {
                    std::cerr << "Failed to get current VSync state: " << SDL_GetError() << std::endl;
                    break;
                }
                int desired = (current == 0) ? 1 : 0; // 0=off, 1=on
                if (SDL_GL_SetSwapInterval(desired)) {
                    std::cout << "VSync " << (desired ? "enabled" : "disabled") << std::endl;
                } else {
                    std::cerr << "Failed to toggle VSync: " << SDL_GetError() << std::endl;
                }
                break;
            }
            case SDL_SCANCODE_W:
                std::cout << "W pressed - Switching to wireframe mode" << std::endl;
                m_renderer.setRenderMode(RenderMode::WIREFRAME);
                break;
            case SDL_SCANCODE_S:
                std::cout << "S pressed - Switching to solid mode" << std::endl;
                m_renderer.setRenderMode(RenderMode::SOLID);
                break;
            case SDL_SCANCODE_R:
                std::cout << "R pressed - Resetting view" << std::endl;
                m_rotationX = 30.0f;
                m_rotationY = 45.0f;
                if (m_renderer.getMesh()) {
                    float extent = m_renderer.getMesh()->getMaxExtent();
                    m_zoom = extent * 1.5f;
                }
                m_pan = glm::vec2(0.0f);
                m_pivotActive = false;
                break;
            default:
                // no-op
                break;
        }
    }
    
    void render() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // Render background gradient
        renderBackground();
        
        int width, height;
        SDL_GetWindowSize(m_window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        
        // Orthogonal projection with large depth range
        float orthoSize = m_zoom;
        float maxExtent = m_renderer.getMesh() ? m_renderer.getMesh()->getMaxExtent() : 100.0f;
        glm::mat4 projection = glm::ortho(
            -orthoSize * aspect, orthoSize * aspect,
            -orthoSize, orthoSize,
            -maxExtent * 10.0f, maxExtent * 10.0f  // Much larger near/far planes
        );
        
        // View matrix with panning in XY (ortho)
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(m_pan.x, m_pan.y, 0.0f));
        
        // Model matrix (rotation and centering)
        glm::mat4 model = glm::mat4(1.0f);
        if (m_renderer.getMesh()) {
            glm::vec3 center = m_renderer.getMesh()->getCenter();
            if (m_pivotActive) {
                glm::vec3 pPrime = m_pivotModel - center;
                model = glm::translate(model, pPrime);
                model = glm::rotate(model, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, -pPrime);
                model = glm::translate(model, -center);
            } else {
                model = glm::rotate(model, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::translate(model, -center);
            }
        }
        
        m_renderer.render(projection, view, model);

        // Draw pivot axes if active
        if (m_pivotActive && m_renderer.getMesh()) {
            drawPivotAxes(projection, view, model);
        }
        
        SDL_GL_SwapWindow(m_window);
    }
    
    void cleanup() {
        if (!SDL_StopTextInput(m_window)) {
            // not fatal
        }
    if (m_bgVAO) glDeleteVertexArrays(1, &m_bgVAO);
    if (m_bgVBO) glDeleteBuffers(1, &m_bgVBO);
    if (m_bgShaderProgram) glDeleteProgram(m_bgShaderProgram);
    if (m_axesVAO) glDeleteVertexArrays(1, &m_axesVAO);
    if (m_axesVBO) glDeleteBuffers(1, &m_axesVBO);
    if (m_axesProgram) glDeleteProgram(m_axesProgram);
        
        if (m_glContext) {
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
        }
        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
        SDL_Quit();
    }
    
    SDL_Window* m_window;
    SDL_GLContext m_glContext;
    Renderer m_renderer;
    bool m_running;
    float m_rotationX;
    float m_rotationY;
    float m_zoom;
    glm::vec2 m_pan;
    
    // FPS tracking
    Uint32 m_frameCount;
    Uint64 m_lastFpsTime;
    float m_fps;
    
    // Background gradient
    GLuint m_bgVAO;
    GLuint m_bgVBO;
    GLuint m_bgShaderProgram;

    // Pivot selection and axes rendering
    bool m_pivotActive;
    glm::vec3 m_pivotModel;
    GLuint m_axesVAO;
    GLuint m_axesVBO;
    GLuint m_axesProgram;
    float m_axisLength;

    void pickPivot(int mouseX, int mouseY) {
        if (!m_renderer.getMesh()) return;
        int width = 1, height = 1;
        SDL_GetWindowSize(m_window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        float orthoSize = m_zoom;
        float maxExtent = m_renderer.getMesh() ? m_renderer.getMesh()->getMaxExtent() : 100.0f;
        glm::mat4 projection = glm::ortho(
            -orthoSize * aspect, orthoSize * aspect,
            -orthoSize, orthoSize,
            -maxExtent * 10.0f, maxExtent * 10.0f);
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(m_pan.x, m_pan.y, 0.0f));

        // Model matrix BEFORE changing pivot (respect current state)
        glm::mat4 modelOld = glm::mat4(1.0f);
        glm::vec3 center = m_renderer.getMesh()->getCenter();
        if (m_pivotActive) {
            glm::vec3 pPrime = m_pivotModel - center;
            modelOld = glm::translate(modelOld, pPrime);
            modelOld = glm::rotate(modelOld, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
            modelOld = glm::rotate(modelOld, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
            modelOld = glm::translate(modelOld, -pPrime);
            modelOld = glm::translate(modelOld, -center);
        } else {
            modelOld = glm::rotate(modelOld, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
            modelOld = glm::rotate(modelOld, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
            modelOld = glm::translate(modelOld, -center);
        }

        const auto& verts = m_renderer.getMesh()->vertices;
        float bestDist2 = std::numeric_limits<float>::infinity();
        glm::vec3 bestPos(0.0f);
        for (const auto& v : verts) {
            glm::vec4 clip = projection * view * modelOld * glm::vec4(v.position, 1.0f);
            if (clip.w == 0.0f) continue;
            glm::vec3 ndc = glm::vec3(clip) / clip.w;
            // Convert to screen coords (origin top-left)
            float sx = (ndc.x * 0.5f + 0.5f) * width;
            float sy = (1.0f - (ndc.y * 0.5f + 0.5f)) * height;
            float dx = sx - mouseX;
            float dy = sy - mouseY;
            float d2 = dx*dx + dy*dy;
            if (d2 < bestDist2) {
                bestDist2 = d2;
                bestPos = v.position; // model-space
            }
        }
        if (!(bestDist2 < std::numeric_limits<float>::infinity())) {
            return;
        }

        // If too far from any vertex, disable pivot mode
        const float maxPixelDist = 100.0f;
        if (bestDist2 > maxPixelDist * maxPixelDist) {
            if (m_pivotActive) {
                // Keep the current pivot point fixed on screen when disabling pivot
                glm::vec4 worldBefore4 = modelOld * glm::vec4(m_pivotModel, 1.0f);
                glm::vec3 worldBefore(worldBefore4);
                glm::mat4 modelNoPivot = glm::mat4(1.0f);
                modelNoPivot = glm::rotate(modelNoPivot, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
                modelNoPivot = glm::rotate(modelNoPivot, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
                modelNoPivot = glm::translate(modelNoPivot, -center);
                glm::vec4 worldAfter4 = modelNoPivot * glm::vec4(m_pivotModel, 1.0f);
                glm::vec3 worldAfter(worldAfter4);
                glm::vec3 delta = worldAfter - worldBefore;
                m_pan.x -= delta.x;
                m_pan.y -= delta.y;
                std::cout << "Click far from vertices; pivot disabled" << std::endl;
            }
            m_pivotActive = false;
            return;
        }

        // Prevent view jump: compensate pan so the selected vertex stays in place
        glm::vec4 worldBefore4 = modelOld * glm::vec4(bestPos, 1.0f);
        glm::vec3 worldBefore(worldBefore4);

        // Build model AFTER changing pivot to the new selection
        glm::mat4 modelNew = glm::mat4(1.0f);
        glm::vec3 pPrimeNew = bestPos - center;
        modelNew = glm::translate(modelNew, pPrimeNew);
        modelNew = glm::rotate(modelNew, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        modelNew = glm::rotate(modelNew, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        modelNew = glm::translate(modelNew, -pPrimeNew);
        modelNew = glm::translate(modelNew, -center);

        glm::vec4 worldAfter4 = modelNew * glm::vec4(bestPos, 1.0f);
        glm::vec3 worldAfter(worldAfter4);
        glm::vec3 delta = worldAfter - worldBefore;

        // Adjust pan to counteract the delta
        m_pan.x -= delta.x;
        m_pan.y -= delta.y;

        // Finally, set the pivot
        m_pivotModel = bestPos;
        m_pivotActive = true;
        std::cout << "Pivot selected at model coords: (" << bestPos.x << ", " << bestPos.y << ", " << bestPos.z << ")" << std::endl;
    }

    void drawPivotAxes(const glm::mat4& projection, const glm::mat4& view, const glm::mat4& model) {
        if (!m_axesProgram) return;
        // Compute world-space pivot position and axis directions from model matrix
        glm::vec4 worldPivot4 = model * glm::vec4(m_pivotModel, 1.0f);
        glm::vec3 worldPivot(worldPivot4);
        glm::mat3 rot = glm::mat3(model); // extract rotation+scale; our model has only rotation
        glm::vec3 dirX = glm::normalize(rot * glm::vec3(1,0,0)) * m_axisLength;
        glm::vec3 dirY = glm::normalize(rot * glm::vec3(0,1,0)) * m_axisLength;
        glm::vec3 dirZ = glm::normalize(rot * glm::vec3(0,0,1)) * m_axisLength;

        float data[6 * 6]; // 6 vertices, each pos(3) + color(3)
        auto put = [&](int idx, const glm::vec3& p, const glm::vec3& c){
            data[idx*6 + 0] = p.x; data[idx*6 + 1] = p.y; data[idx*6 + 2] = p.z;
            data[idx*6 + 3] = c.r; data[idx*6 + 4] = c.g; data[idx*6 + 5] = c.b;
        };
        // X axis - red
        put(0, worldPivot, glm::vec3(1,0,0));
        put(1, worldPivot + dirX, glm::vec3(1,0,0));
        // Y axis - green
        put(2, worldPivot, glm::vec3(0,1,0));
        put(3, worldPivot + dirY, glm::vec3(0,1,0));
        // Z axis - blue
        put(4, worldPivot, glm::vec3(0,0,1));
        put(5, worldPivot + dirZ, glm::vec3(0,0,1));

        glBindBuffer(GL_ARRAY_BUFFER, m_axesVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(data), data);

        // Draw with depth disabled so axes are visible
        GLboolean wasDepth = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        glUseProgram(m_axesProgram);
        GLint projLoc = glGetUniformLocation(m_axesProgram, "projection");
        GLint viewLoc = glGetUniformLocation(m_axesProgram, "view");
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glBindVertexArray(m_axesVAO);
        glLineWidth(2.0f);
        glDrawArrays(GL_LINES, 0, 6);
        glBindVertexArray(0);
        if (wasDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    }
};

int main(int argc, char* argv[]) {
    std::string stlFile;
    if (argc >= 2) {
        stlFile = argv[1];
    }
    
    // Display controls
    std::cout << "\nControls:" << std::endl;
    std::cout << "  Right Mouse + Drag: Rotate model" << std::endl;
    std::cout << "  Left Mouse: Pick pivot (draw axes)" << std::endl;
    std::cout << "  Middle Mouse + Drag: Pan view" << std::endl;
    std::cout << "  Mouse Wheel: Zoom in/out" << std::endl;
    std::cout << "  V: Toggle VSync" << std::endl;
    std::cout << "  W: Wireframe mode" << std::endl;
    std::cout << "  S: Solid mode" << std::endl;
    std::cout << "  R: Reset view" << std::endl;
    std::cout << "  Q/ESC: Quit" << std::endl;
    
    Application app;
    
    if (!app.initialize(stlFile)) {
        // If user cancelled the file dialog, treat it as a normal exit
        if (argc < 2) {
            return 0;
        }
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    app.run();
    
    return 0;
}
