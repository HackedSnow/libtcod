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
#include "context_init.h"

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "console_etc.h"
#include "globals.h"
#include "libtcod_int.h"
#include "renderer_gl1.h"
#include "renderer_gl2.h"
#include "renderer_sdl2.h"
#include "tileset_fallback.h"

static TCOD_Error ensure_tileset(TCOD_Tileset** tileset) {
  if (*tileset) {
    return TCOD_E_OK;
  }
  if (!TCOD_ctx.tileset) {
    TCOD_console_set_custom_font("terminal.png", TCOD_FONT_LAYOUT_ASCII_INCOL, 0, 0);
  }
  if (!TCOD_ctx.tileset) {
    TCOD_set_default_tileset(TCOD_tileset_load_fallback_font_(0, 12));
  }
  if (!TCOD_ctx.tileset) {
    TCOD_set_errorv("No font loaded and couldn't load a fallback font!");
    return TCOD_E_ERROR;
  }
  *tileset = TCOD_ctx.tileset;
  return TCOD_E_OK;
}
/**
    Return the renderer from a string object.  Returns -1 on failure.
 */
static int get_renderer_from_str(const char* string) {
  if (!string) {
    return -1;
  } else if (strcmp(string, "sdl") == 0) {
    return TCOD_RENDERER_SDL;
  } else if (strcmp(string, "opengl") == 0) {
    return TCOD_RENDERER_OPENGL;
  } else if (strcmp(string, "glsl") == 0) {
    return TCOD_RENDERER_GLSL;
  } else if (strcmp(string, "sdl2") == 0) {
    return TCOD_RENDERER_SDL2;
  } else if (strcmp(string, "opengl2") == 0) {
    return TCOD_RENDERER_OPENGL2;
  } else {
    return -1;
  }
}
/**
 *  Set `renderer` from the TCOD_RENDERER environment variable if it exists.
 */
static void get_env_renderer(int* renderer_type) {
  const char* value = getenv("TCOD_RENDERER");
  if (get_renderer_from_str(value) >= 0) {
    *renderer_type = get_renderer_from_str(value);
  }
}
/**
 *  Set `vsync` from the TCOD_VSYNC environment variable if it exists.
 */
static void get_env_vsync(int* vsync) {
  const char* value = getenv("TCOD_VSYNC");
  if (!value) {
    return;
  }
  if (strcmp(value, "0") == 0) {
    *vsync = 0;
  } else if (strcmp(value, "1") == 0) {
    *vsync = 1;
  }
}
/**
    The message displayed when "-help" is given.
 */
static const char TCOD_help_msg[] =
    "Command line options:\n\
-help : Show this help message.\n\
-windowed : Open in a resizeable window.\n\
-fullscreen : Open a borderless fullscreen window.\n\
-exclusive-fullscreen : Open an exclusive fullscreen window.\n\
-resolution <width>x<height> : Sets the desired pixel resolution.\n\
-width <pixels> : Set the desired pixel width.\n\
-height <pixels> : Set the desired pixel height.\n\
-renderer <sdl|sdl2|opengl|opengl2> : Change the active libtcod renderer.\n\
-vsync : Enable Vsync when possible.\n\
-no-vsync : Disable Vsync.\n\
";
/**
    Return true if an arg variable is "-name" or "--name".
 */
#define TCOD_CHECK_ARGUMENT(arg, name) (strcmp((arg), ("-" name)) == 0 || strcmp((arg), ("--" name)) == 0)
/**
    Create a new context while filling incomplete values as needed and
    unpacking values from the envrioment and the command line.
 */
TCOD_Error TCOD_context_new(const TCOD_ContextParams* params, TCOD_Context** out) {
  if (!params) {
    TCOD_set_errorv("params must not be NULL.");
    return TCOD_E_INVALID_ARGUMENT;
  }
  if (!out) {
    TCOD_set_errorv("Output must not be NULL.");
    return TCOD_E_INVALID_ARGUMENT;
  }
  // These values may be modified.
  int pixel_width = params->pixel_width;
  int pixel_height = params->pixel_height;
  int columns = params->columns;
  int rows = params->rows;
  int renderer_type = params->renderer_type;
  int vsync = params->vsync;
  int sdl_window_flags = params->sdl_window_flags;
  TCOD_Tileset* tileset = params->tileset;

  get_env_renderer(&renderer_type);
  get_env_vsync(&vsync);

  // Parse CLI arguments.
  for (int i = 0; i < params->argc; ++i) {
    if (strcmp(params->argv[i], "-h") == 0 || TCOD_CHECK_ARGUMENT(params->argv[i], "help")) {
      TCOD_set_error(TCOD_help_msg);
      return TCOD_E_COMMAND_OUT;
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "windowed")) {
      sdl_window_flags &= ~(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
      sdl_window_flags |= SDL_WINDOW_RESIZABLE;
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "exclusive-fullscreen")) {
      sdl_window_flags &= ~(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
      sdl_window_flags |= SDL_WINDOW_FULLSCREEN;
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "fullscreen")) {
      sdl_window_flags &= ~(SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
      sdl_window_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "vsync")) {
      vsync = 1;
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "no-vsync")) {
      vsync = 0;
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "width")) {
      if (++i < params->argc) {
        pixel_width = atoi(params->argv[i]);
      } else {
        TCOD_set_error("Width must be given a number.");
        return TCOD_E_COMMAND_OUT;
      }
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "height")) {
      if (++i < params->argc) {
        pixel_height = atoi(params->argv[i]);
      } else {
        TCOD_set_error("Height must be given a number.");
        return TCOD_E_COMMAND_OUT;
      }
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "renderer")) {
      if (++i < params->argc && get_renderer_from_str(params->argv[i]) >= 0) {
        renderer_type = get_renderer_from_str(params->argv[i]);
      } else {
        TCOD_set_error("Renderer should be one of [sdl|sdl2|opengl|opengl2]");
        return TCOD_E_COMMAND_OUT;
      }
    } else if (TCOD_CHECK_ARGUMENT(params->argv[i], "resolution")) {
      if (++i < params->argc && sscanf(params->argv[i], "%dx%d", &pixel_width, &pixel_height) == 2) {
      } else {
        TCOD_set_error("Resolution argument should be in the format: <width>x<height>");
        return TCOD_E_COMMAND_OUT;
      }
    }
  }
  TCOD_Error err = ensure_tileset(&tileset);
  if (err < 0) {
    return err;
  }
  if (pixel_width < 0 || pixel_height < 0) {
    TCOD_set_errorvf("Width and height must be non-negative. Not %i,%i", pixel_width, pixel_height);
    return TCOD_E_INVALID_ARGUMENT;
  }
  if (columns <= 0) {
    columns = 80;
  }
  if (rows <= 0) {
    rows = 24;
  }
  if (pixel_width <= 0) {
    pixel_width = columns * tileset->tile_width;
  }
  if (pixel_height <= 0) {
    pixel_height = rows * tileset->tile_height;
  }

  // Initilize the renderer.
  int renderer_flags = SDL_RENDERER_PRESENTVSYNC * vsync;
  err = TCOD_E_OK;
  switch (renderer_type) {
    case TCOD_RENDERER_SDL:
      renderer_flags |= SDL_RENDERER_SOFTWARE;
      *out = TCOD_renderer_init_sdl2(
          params->x,
          params->y,
          pixel_width,
          pixel_height,
          params->window_title,
          sdl_window_flags,
          renderer_flags,
          tileset);
      if (!*out) {
        return TCOD_E_ERROR;
      }
      return TCOD_E_OK;
    case TCOD_RENDERER_GLSL:
    case TCOD_RENDERER_OPENGL2:
      *out = TCOD_renderer_new_gl2(
          params->x, params->y, pixel_width, pixel_height, params->window_title, sdl_window_flags, vsync, tileset);
      if (*out) {
        return err;
      }
      err = TCOD_E_WARN;
      //@fallthrough@
    case TCOD_RENDERER_OPENGL:
      *out = TCOD_renderer_init_gl1(
          params->x, params->y, pixel_width, pixel_height, params->window_title, sdl_window_flags, vsync, tileset);
      if (*out) {
        return err;
      }
      err = TCOD_E_WARN;
      //@fallthrough@
    default:
    case TCOD_RENDERER_SDL2:
      *out = TCOD_renderer_init_sdl2(
          params->x,
          params->y,
          pixel_width,
          pixel_height,
          params->window_title,
          sdl_window_flags,
          renderer_flags,
          tileset);
      if (!*out) {
        return TCOD_E_ERROR;
      }
      return err;
  }
}
