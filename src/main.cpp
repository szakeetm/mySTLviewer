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
                    m_rotationX(30.0f), m_rotationY(45.0f), m_zoom(2.0f) {}
    
    ~Application() {
        cleanup();
    }
    
    bool initialize(const std::string& stlFile) {
        // Initialize SDL
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Set OpenGL attributes
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        
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
        
        // Create OpenGL context
        m_glContext = SDL_GL_CreateContext(m_window);
        if (!m_glContext) {
            std::cerr << "OpenGL context creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }
        
        std::cout << "OpenGL " << glGetString(GL_VERSION) << std::endl;
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        
        // Enable back-face culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        
        // Set clear color
        glClearColor(0.2f, 0.2f, 0.25f, 1.0f);
        
        // Initialize renderer
        if (!m_renderer.initialize()) {
            std::cerr << "Failed to initialize renderer" << std::endl;
            return false;
        }
        
        // Load STL file
        if (!stlFile.empty()) {
            auto mesh = STLLoader::load(stlFile);
            if (mesh) {
                m_renderer.setMesh(std::move(mesh));
                
                // Adjust zoom based on model size
                if (m_renderer.getMesh()) {
                    float extent = m_renderer.getMesh()->getMaxExtent();
                    m_zoom = extent * 1.5f;
                }
            } else {
                std::cerr << "Failed to load STL file: " << stlFile << std::endl;
                return false;
            }
        }
        
        return true;
    }
    
    void run() {
        m_running = true;
        
        while (m_running) {
            handleEvents();
            render();
        }
    }
    
private:
    void handleEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    m_running = false;
                    break;
                    
                case SDL_EVENT_KEY_DOWN:
                    handleKeyPress(event.key.key);
                    break;
                    
                case SDL_EVENT_MOUSE_MOTION:
                    if (event.motion.state & SDL_BUTTON_LMASK) {
                        m_rotationY += event.motion.xrel * 0.5f;
                        m_rotationX += event.motion.yrel * 0.5f;
                        m_rotationX = glm::clamp(m_rotationX, -89.0f, 89.0f);
                    }
                    break;
                    
                case SDL_EVENT_MOUSE_WHEEL:
                    m_zoom -= event.wheel.y * 0.1f;
                    m_zoom = glm::max(0.1f, m_zoom);
                    break;
                    
                case SDL_EVENT_WINDOW_RESIZED:
                    glViewport(0, 0, event.window.data1, event.window.data2);
                    break;
            }
        }
    }
    
    void handleKeyPress(SDL_Keycode key) {
        switch (key) {
            case SDLK_ESCAPE:
            case SDLK_Q:
                m_running = false;
                break;
                
            case SDLK_W:
                m_renderer.setRenderMode(RenderMode::WIREFRAME);
                std::cout << "Wireframe mode" << std::endl;
                break;
                
            case SDLK_S:
                m_renderer.setRenderMode(RenderMode::SOLID);
                std::cout << "Solid mode" << std::endl;
                break;
                
            case SDLK_R:
                m_rotationX = 30.0f;
                m_rotationY = 45.0f;
                if (m_renderer.getMesh()) {
                    float extent = m_renderer.getMesh()->getMaxExtent();
                    m_zoom = extent * 1.5f;
                }
                std::cout << "View reset" << std::endl;
                break;
        }
    }
    
    void render() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        int width, height;
        SDL_GetWindowSize(m_window, &width, &height);
        float aspect = static_cast<float>(width) / static_cast<float>(height);
        
        // Orthogonal projection
        float orthoSize = m_zoom;
        glm::mat4 projection = glm::ortho(
            -orthoSize * aspect, orthoSize * aspect,
            -orthoSize, orthoSize,
            0.1f, 1000.0f
        );
        
        // View matrix (camera looking from distance)
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
        
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
};

int main(int argc, char* argv[]) {
    std::string stlFile;
    
    // If no command line argument, open file dialog
    if (argc < 2) {
        // Initialize NFD
        NFD_Init();
        
        nfdchar_t* outPath = nullptr;
        nfdfilteritem_t filters[1] = { { "STL Files", "stl" } };
        nfdresult_t result = NFD_OpenDialog(&outPath, filters, 1, nullptr);
        
        if (result == NFD_OKAY) {
            stlFile = outPath;
            NFD_FreePath(outPath);
            std::cout << "Selected file: " << stlFile << std::endl;
        } else if (result == NFD_CANCEL) {
            std::cout << "User cancelled file selection" << std::endl;
            NFD_Quit();
            return 0;
        } else {
            std::cerr << "Error opening file dialog: " << NFD_GetError() << std::endl;
            NFD_Quit();
            return 1;
        }
        
        NFD_Quit();
    } else {
        stlFile = argv[1];
    }
    
    // Display controls
    std::cout << "\nControls:" << std::endl;
    std::cout << "  Left Mouse + Drag: Rotate model" << std::endl;
    std::cout << "  Mouse Wheel: Zoom in/out" << std::endl;
    std::cout << "  W: Wireframe mode" << std::endl;
    std::cout << "  S: Solid mode" << std::endl;
    std::cout << "  R: Reset view" << std::endl;
    std::cout << "  Q/ESC: Quit" << std::endl;
    
    Application app;
    
    if (!app.initialize(stlFile)) {
        std::cerr << "Failed to initialize application" << std::endl;
        return 1;
    }
    
    app.run();
    
    return 0;
}
