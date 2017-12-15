Those are standalone ready-to-build applications to demonstrate ImGui.
Binaries of some of those demos: http://www.miracleworld.net/imgui/binaries

Third party languages and frameworks bindings: https://github.com/ocornut/imgui/wiki/Links
(languages: C, .net, rust, D, Python, Lua..)
(frameworks: DX12, Vulkan, Cinder, OpenGLES, openFrameworks, Cocos2d-x, SFML, Flexium, NanoRT, Irrlicht..)
(extras: RemoteImGui, ImWindow, imgui_wm..)

TL;DR; 
 - Newcomers, read 'PROGRAMMER GUIDE' in imgui.cpp for notes on how to setup ImGui in your codebase.
 - If you are using of the backend provided here, so you can copy the imgui_impl_xxx.cpp/h files
   to your project and use them unmodified.
 - To LEARN how the library is setup, you may refer to 'opengl2_example' because is the simplest one to read.
   However, do NOT USE the 'opengl2_example' if your code is using any modern GL3+ calls.
   Mixing old fixed-pipeline OpenGL2 and modern OpenGL3+ is going to make everything more complicated.
   Read comments below for details. If you are not sure, in doubt, use 'opengl3_example'.
 - If you have your own engine, you probably want to read a few of the examples first then adapt it to
   your engine. Please note that if your engine is based on OpenGL/DirectX you can perfectly use the
   existing rendering backends, don't feel forced to rewrite them with your own engine API, or you can
   do that later when you already got things to work.

ImGui is highly portable and only requires a few things to run:
 - Providing mouse/keyboard inputs
 - Load the font atlas texture into graphics memory
 - Providing a render function to render indexed textured triangles
 - Optional: clipboard support, mouse cursor supports, Windows IME support, etc.
So this is essentially what those examples are doing + the obligatory cruft for portability.

Unfortunately in 2016 it is still tedious to create and maintain portable build files using external 
libraries (the kind we're using here to create a window and render 3D triangles) without relying on 
third party software. For most examples here I choose to provide:
 - Makefiles for Linux/OSX
 - Batch files for Visual Studio 2008+
 - A .sln project file for Visual Studio 2010+ 
Please let me know if they don't work with your setup!
You can probably just import the imgui_impl_xxx.cpp/.h files into your own codebase or compile those
directly with a command-line compiler.

ImGui has zero frame of lag for most behaviors and one frame of lag for some behaviors. 
At 60 FPS your experience should be pleasant. Consider that OS mouse cursors are typically drawn through 
a specific hardware accelerated route and may feel smoother than other GPU rendered contents. You may 
experiment with the io.MouseDrawCursor flag to request ImGui to draw a mouse cursor itself, to visualize 
the lag between a hardware cursor and a software cursor. It might be beneficial to the user experience
to switch to a software rendered cursor when an interactive drag is in progress. 
Also note that some setup or GPU drivers may be causing extra lag (possibly by enforcing triple buffering), 
leaving you with no option but sadness/anger (Intel GPU drivers were reported as such).

opengl2_example/
    *DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL*
    GLFW + OpenGL example (old, fixed graphic pipeline).
    This is mostly provided as a reference to learn how ImGui integration works, because it is easier to read.
    If your code is using GL3+ context or any semi modern OpenGL calls, using this is likely to make everything
    more complicated, will require your code to reset every single OpenGL attributes to their initial state,
    and might confuse your GPU driver. Prefer using opengl3_example.

opengl3_example/
    GLFW + OpenGL example (programmable pipeline, binding modern functions with GL3W).
    This uses more modern OpenGL calls and custom shaders. 
    Prefer using that if you are using modern OpenGL3/4 in your application.

directx9_example/
    DirectX9 example, Windows only.
	
directx10_example/
    DirectX10 example, Windows only.
    This is quite long and tedious, because: DirectX10.

directx11_example/
    DirectX11 example, Windows only.
    This is quite long and tedious, because: DirectX11.
	
apple_example/
    OSX & iOS example.
    On iOS, Using Synergy to access keyboard/mouse data from server computer.
    Synergy keyboard integration is rather hacky.

sdl_opengl2_example/
    *DO NOT USE THIS CODE IF YOUR CODE/ENGINE IS USING MODERN OPENGL*
    SDL2 + OpenGL example (old fixed pipeline).
    This is mostly provided as a reference to learn how ImGui integration works, because it is easier to read.
    If your code is using GL3+ context or any semi modern OpenGL calls, using this is likely to make everything
    more complicated, will require your code to reset every single OpenGL attributes to their initial state,
    and might confuse your GPU driver. Prefer using sdl_opengl3_example.

sdl_opengl3_example/
    SDL2 + OpenGL3 example.

allegro5_example/
    Allegro 5 example.
	 
marmalade_example/
    Marmalade example using IwGx
    
vulkan_example/
	Vulkan example.
	This is quite long and tedious, because: Vulkan.

TODO: Apple, SDL GL/GL3, Allegro, Marmalade, Vulkan examples do not honor the io.WantMoveMouse flag.
