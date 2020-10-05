/* BSD 3-Clause License
 *
 * Copyright © 2008-2020, Jice and the libtcod contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef LIBTCOD_CONTEXT_INIT_H_
#define LIBTCOD_CONTEXT_INIT_H_
#include <stdbool.h>

#include "config.h"
#include "context.h"
#include "error.h"
#include "tileset.h"
/**
    A struct of paramers used to create a new context.

    `version` should always be set to the constant: `TCOD_HEXVERSION`.

    `x` and `y` is the starting position of the window.  These are SDL
    parameters so values like `SDL_WINDOWPOS_UNDEFINED` and
    `SDL_WINDOWPOS_CENTERED` are acceptable.
    If unsure then use `SDL_WINDOWPOS_UNDEFINED` for both, even when you don't
    desire a window.

    `pixel_width` and `pixel_height` are the desired size of the window in
    pixels.
    If these are zero then they'll be derived from `columns`, `rows`, and the
    `tileset`.

    `columns` and `rows` are the desired size of the terminal window.
    Usually you'll set either these or the pixel resolution.

    `renderer_type` is a `TCOD_renderer_t` type.

    `tileset` is an optional pointer to a tileset object.

    If `vsync` is True, then vertical sync will be enabled whenever possible.
    A value of true is recommended.

    `sdl_window_flags` is a bitfield of SDL_WindowFlags flags.
    For a window, a value of ``SDL_WINDOW_RESIZABLE`` is recommended.
    For fullscreen, a value of
    ``SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN_DESKTOP`` is recommended.
    You should avoid the ``SDL_WINDOW_FULLSCREEN`` flag whenever possible.

    `window_title` will be the title of the opened window.

    `argc` and `argv` are optional CLI parameters.
    You can pass `0` and `NULL` repecfuly to ignore them.
    If user attention is required then TCOD_E_COMMAND_OUT will be returned and
    on this error code you should print TCOD_get_error to stdout and exit.
 */
typedef struct TCOD_ContextParams {
  int version;  // Should be `TCOD_HEXVERSION`.
  int x;
  int y;
  int pixel_width;
  int pixel_height;
  int columns;
  int rows;
  int renderer_type;
  TCOD_Tileset* tileset;
  int vsync;
  int sdl_window_flags;
  const char* window_title;
  int argc;
  const char* const* argv;
} TCOD_ContextParams;
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus
/**
    Create a new context with the given parameters.

    `params` is a non-NULL pointer to a TCOD_ContextParams struct.
    See its documetnation for info on the parameters.

    `out` is the output for the `TCOD_Context`, must not be NULL.

    \rst
    .. versionadded:: 1.16
    \endrst
 */
TCOD_PUBLIC TCOD_NODISCARD TCOD_Error TCOD_context_new(const TCOD_ContextParams* params, TCOD_Context** out);
/**
    Create a new context with a window of the given size.

    `columns` and `rows` parameters are ignored.

    `out` is the output for the `TCOD_Context`, must not be NULL.

    Returns a negative error code on failure, `out` will unlikely be set in this
    case.

    \rst
    .. versionadded:: 1.16
    \endrst
 */
TCOD_PUBLIC TCOD_NODISCARD TCOD_Error TCOD_context_new_window(const TCOD_ContextParams* params, TCOD_Context** out);
#ifdef __cplusplus
}  // extern "C"
namespace tcod {
TCOD_NODISCARD
inline auto new_context(const TCOD_ContextParams& params) -> ContextPtr {
  struct TCOD_Context* context = nullptr;
  check_throw_error(TCOD_context_new(&params, &context));
  return ContextPtr{context};
}
}  // namespace tcod
#endif  // __cplusplus
#endif  // LIBTCOD_CONTEXT_INIT_H_
