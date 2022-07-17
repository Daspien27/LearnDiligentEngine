# LearnDiligentEngine
Learning Diligent by going through the LearnOpenGL tutorial

Build system: Visual Studio 2019

Platform: Just windows for now...sorry!

Build dependencies:

- clone DiligentEngine (https://github.com/DiligentGraphics/DiligentEngine) and install it through cmake to some directory. Then add an environment variable DILIGENT_ENGINE_INSTALL_DIR pointing to that folder.
- install vcpkg and the following dependencies for the win64 triplet
  - glfw
  - glm
  - vulkan sdk
  
This project is just a sandbox implementing examples from https://learnopengl.com/
