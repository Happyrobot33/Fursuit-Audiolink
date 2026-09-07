#pragma once

#include <memory>
#include "render_target.h"

/** Constructs and initializes the render target for whichever device is compiled in. */
std::unique_ptr<IRenderTarget> create_render_target();
