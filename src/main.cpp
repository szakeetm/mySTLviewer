#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <nfd.h>
#include <iostream>
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
                    m_bgVAO(0), m_bgVBO(0), m_bgShaderProgram(0) {}
    
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
            // positions        // colors (dark blue at top, black at bottom)
            -1.0f,  1.0f, 0.0f,  0.0f, 0.05f, 0.15f,  // top-left
             1.0f,  1.0f, 0.0f,  0.0f, 0.05f, 0.15f,  // top-right
            -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f,    // bottom-left
             1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 0.0f     // bottom-right
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
    
    void renderBackground() {
        glDepthMask(GL_FALSE); // Don't write to depth buffer
        glUseProgram(m_bgShaderProgram);
        glBindVertexArray(m_bgVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE); // Re-enable depth writing
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
                                break;
                            default:
                                break;
                        }
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_MOTION:
                    if (event.motion.state & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)) {
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
        model = glm::rotate(model, glm::radians(m_rotationX), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(m_rotationY), glm::vec3(0.0f, 1.0f, 0.0f));
        
        // Center the model
        if (m_renderer.getMesh()) {
            glm::vec3 center = m_renderer.getMesh()->getCenter();
            model = glm::translate(model, -center);
        }
        
        m_renderer.render(projection, view, model);
        
        SDL_GL_SwapWindow(m_window);
    }
    
    void cleanup() {
        if (!SDL_StopTextInput(m_window)) {
            // not fatal
        }
        if (m_bgVAO) glDeleteVertexArrays(1, &m_bgVAO);
        if (m_bgVBO) glDeleteBuffers(1, &m_bgVBO);
        if (m_bgShaderProgram) glDeleteProgram(m_bgShaderProgram);
        
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
};

int main(int argc, char* argv[]) {
    std::string stlFile;
    if (argc >= 2) {
        stlFile = argv[1];
    }
    
    // Display controls
    std::cout << "\nControls:" << std::endl;
    std::cout << "  Left Mouse + Drag: Rotate model" << std::endl;
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
