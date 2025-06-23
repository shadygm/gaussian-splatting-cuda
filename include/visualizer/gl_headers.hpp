#pragma once

// This file ensures proper OpenGL header inclusion order
// Always include this instead of individual OpenGL headers

// clang-format off
#ifndef GL_HEADERS_INCLUDED
#define GL_HEADERS_INCLUDED

// Include GLAD first, then GLFW
#include <glad/glad.h>
// Ensure GLFW is included after GLAD to avoid conflicts
#include <GLFW/glfw3.h>

#endif // GL_HEADERS_INCLUDED
// clang-format on