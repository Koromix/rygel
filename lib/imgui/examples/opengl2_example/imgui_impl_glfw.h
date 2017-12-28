// ImGui GLFW binding with OpenGL (legacy, fixed pipeline)
// In this binding, ImTextureID is used to store an OpenGL 'GLuint' texture identifier. Read the FAQ about ImTextureID in imgui.cpp.
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan graphics context creation, etc.)

// **DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL (SHADERS, VBO, VAO, etc.)**
// **Prefer using the code in the opengl3_example/ folder**
// See imgui_impl_glfw.cpp for details.

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 4 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXXX_NewFrame(), ImGui::Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

struct GLFWwindow;

IMGUI_API bool        ImGui_ImplGlfwGL2_Init(GLFWwindow* window, bool install_callbacks);
IMGUI_API void        ImGui_ImplGlfwGL2_Shutdown();
IMGUI_API void        ImGui_ImplGlfwGL2_NewFrame();

// Use if you want to reset your rendering device without losing ImGui state.
IMGUI_API void        ImGui_ImplGlfwGL2_InvalidateDeviceObjects();
IMGUI_API bool        ImGui_ImplGlfwGL2_CreateDeviceObjects();

// GLFW callbacks (registered by default to GLFW if you enable 'install_callbacks' during initialization)
// Provided here if you want to chain callbacks yourself. You may also handle inputs yourself and use those as a reference.
IMGUI_API void        ImGui_ImplGlfwGL2_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
IMGUI_API void        ImGui_ImplGlfwGL2_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
IMGUI_API void        ImGui_ImplGlfwGL2_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
IMGUI_API void        ImGui_ImplGlfwGL2_CharCallback(GLFWwindow* window, unsigned int c);
