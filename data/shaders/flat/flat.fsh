// SPDX-License-Identifier: MIT
#version 420

// Ideally this would be done via define injection when importing the shader source code when compiling with
// Shader::compile(), however this would mean deep changes to glow which are avoided at this point in time.
#define INSTANCED 0

#include "flat_template.fsh"
