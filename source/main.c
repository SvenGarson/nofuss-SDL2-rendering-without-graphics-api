#include <stdio.h>
#include <SDL.h>

/* Constants */
const int OS_FAILURE_RETURN_CODE = -1;
const char WINDOW_TITLE[] = "SDL2 rendering without graphics API";
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int WINDOW_WIDTH_VIRTUAL = 160;
const int WINDOW_HEIGHT_VIRTUAL = 144;
const int WINDOW_PIXELS_TOTAL_VIRTUAL = WINDOW_WIDTH_VIRTUAL * WINDOW_HEIGHT_VIRTUAL;

/*
    TODO-GS: Improve the following things
      - Single function to call when exiting and cleaning up
      - Explain differences between SDL_RENDERER flags
      - How is SDL2 cleaned up properly: SDL; Subs; Window; Renderer; Textures; ... ?
      - Error messages in cleanup steps
      - Use a client side texture optionally, that can be optimized for fast access
      - Explain actual and virtual terminology
      - SDL texture should be cleared after creating it unless every pixel is overwritten every frame
      - lock only when locking is necessary
      - what the pitch in textures is and how to use it
      - apologize for the verbose nature (no funtions or abstractions), this is for clarity and showing of the sequence + compiler may not like varaibels not listed at top
      - can use unsigned int for most, but that may break on other platforms, stdint is more explicit
      - I believe in handling error
      > things to show in post:
          - examples of pipes and letterbox
          - other things above
          - what the virtual texture means
          - which calls are optional and what effect this has: Clearing; logical sized rendering; ... ?
          - texture locking
          - set the window to be resizeable, handle the resizing to demonstrate the piping and letterboxed rendering
*/

/* Datatypes */
typedef struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
  uint8_t alpha;
} client_pixel_rgba_ts;

/* Function prototypes */
void cleanup(
  SDL_Window * const p_window,
  SDL_Renderer * const p_renderer,
  SDL_Texture * const p_texture,
  SDL_PixelFormat * const p_pixel_format
);

/* Entry point */
int main(int argc, char * argv[])
{
  /* Initialize SDL2 video and events subsystems */
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    fprintf(stderr, "\nRequired SDL2 subsystems could not be initialized - Error: %s", SDL_GetError());
    cleanup(NULL, NULL, NULL, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* Video and events subsystems initialized successfully - Now create the window */
  SDL_Window * const p_window = SDL_CreateWindow(
    WINDOW_TITLE,
    SDL_WINDOWPOS_CENTERED,
    SDL_WINDOWPOS_CENTERED,
    WINDOW_WIDTH,
    WINDOW_HEIGHT,
    SDL_WINDOW_SHOWN
  );

  if (p_window == NULL)
  {
    fprintf(stderr, "\nSDL2 window could not be created - Error: %s", SDL_GetError());
    cleanup(NULL, NULL, NULL, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 window created successfully - Now create the renderer */
  SDL_Renderer * const p_renderer = SDL_CreateRenderer(p_window, -1, SDL_RENDERER_PRESENTVSYNC);
  if (p_renderer == NULL)
  {
    fprintf(stderr, "\nSDL2 renderer could not be created - Error: %s", SDL_GetError());
    cleanup(p_window, NULL, NULL, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 renderer created successfully - Now setup the texture to act as window pixel color buffer */
  SDL_Texture * const p_window_texture = SDL_CreateTexture(
    p_renderer,
    SDL_PIXELFORMAT_RGBA8888,
    SDL_TEXTUREACCESS_STREAMING,
    WINDOW_WIDTH_VIRTUAL,
    WINDOW_HEIGHT_VIRTUAL
  );
  if (p_window_texture == NULL)
  {
    fprintf(stderr, "\nSDL2 texture could not be created - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, NULL, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 window texture created successfully - Now extract the created texture attributes for robust, per-pixel texture manipulation */
  uint32_t window_texture_format;
  int window_texture_width, window_texture_height;
  const int query_texture_successful = SDL_QueryTexture(p_window_texture, &window_texture_format, NULL, &window_texture_width, &window_texture_height);
  if (query_texture_successful != 0)
  {
    fprintf(stderr, "\nSDL2 texture attributes could not be queried - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, p_window_texture, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  SDL_PixelFormat * const p_texture_pixel_format = SDL_AllocFormat(window_texture_format);
  if (p_texture_pixel_format == NULL)
  {
    fprintf(stderr, "\nSDL2 texture pixel format could not be determined - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, p_window_texture, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 texture attributes determined successfully - Now configure the renderer for fixed-ration rendering */
  const int logical_size_set = SDL_RenderSetLogicalSize(p_renderer, WINDOW_WIDTH_VIRTUAL, WINDOW_HEIGHT_VIRTUAL);
  if (logical_size_set != 0)
  {
    fprintf(stderr, "\nSDL2 logical render size could not be set - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, p_window_texture, p_texture_pixel_format);
    return OS_FAILURE_RETURN_CODE;
  }

  /* Set renderer draw and clear color in case the renderer is to be cleared */
  const int set_render_draw_color_successful = SDL_SetRenderDrawColor(p_renderer, 0x32, 0x96, 0xFF, 0xFF);
  if (set_render_draw_color_successful != 0)
  {
    fprintf(stderr, "\nSDL2 renderer draw color could not be set - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, p_window_texture, p_texture_pixel_format);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 related setup and configuration completed successfully - Now allocate a client-side pixel buffer for rendering operations */
  client_pixel_rgba_ts * const p_client_pixels_rgba = malloc(sizeof(client_pixel_rgba_ts) * WINDOW_PIXELS_TOTAL_VIRTUAL);
  if (p_client_pixels_rgba == NULL)
  {
    fprintf(stderr, "\nCould not allocate client-side pixel buffer for offline rendering - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, p_window_texture, p_texture_pixel_format);
    return OS_FAILURE_RETURN_CODE;
  }

  /* All rendering preparations setup successfully - Now start the window loop */
  int window_close_requested = 0;
  while (!window_close_requested)
  {
    /* Process SDL2 window events */
    SDL_Event window_event;
    while (SDL_PollEvent(&window_event) != 0)
    {
      /* Handle and thereby consume required SDL2 window events */
      switch(window_event.type)
      {
        case SDL_QUIT:
          window_close_requested = 1;
          break;
        case SDL_KEYDOWN:
          if (window_event.key.keysym.sym == SDLK_ESCAPE)
          {
            window_close_requested = 1;
          }
          break;
      }
    }

    /* Add SDL2 window events processed - Now render into the client-side pixel buffer */
    for (int texel_y = 0; texel_y < WINDOW_HEIGHT_VIRTUAL; texel_y++)
    {
      for (int texel_x = 0; texel_x < WINDOW_WIDTH_VIRTUAL; texel_x++)
      {
        const int client_texel_index = (WINDOW_WIDTH_VIRTUAL * texel_y) + texel_x;
        client_pixel_rgba_ts * const p_client_pixel_color = p_client_pixels_rgba + client_texel_index;
        
        /* TODO-GS: Test pixel positioning */
        if (client_texel_index == 0) p_client_pixel_color->red = 0xFF;
        else if (client_texel_index == WINDOW_WIDTH_VIRTUAL - 1) p_client_pixel_color->green = 0xFF;
        else if (client_texel_index == (WINDOW_HEIGHT_VIRTUAL - 1) * WINDOW_WIDTH_VIRTUAL) p_client_pixel_color->blue = 0xFF;
        else if (client_texel_index == WINDOW_PIXELS_TOTAL_VIRTUAL - 1)
        {
          p_client_pixel_color->red = p_client_pixel_color->green = p_client_pixel_color->blue = 0x00;
        }
        else 
        {
          p_client_pixel_color->red = p_client_pixel_color->green = p_client_pixel_color->blue = p_client_pixel_color->alpha = 0xFF;
        }
      }
    }

    /*
        Update texture color data before rendering it into the (hidden) renderer surface.

        The pointer to the texture pixel must be used for WRITING ONLY!
    */
    void * p_texture_pixels = NULL;
    int texture_pitch;
    const int lock_texture_successful = SDL_LockTexture(p_window_texture, NULL, (void **)&p_texture_pixels, &texture_pitch);
    if (lock_texture_successful != 0)
    {
      fprintf(stderr, "\nSDL2 texture could not be locked - %s", SDL_GetError());
    }

    /*
        Texture locked - Now copy the client-side pixel data into the texture in one go
    */
    uint32_t * const p_texture_pixels_rgba = (uint32_t *)p_texture_pixels;
    const int padded_texels_per_row = texture_pitch / sizeof(uint32_t);
    for (int texel_y = 0; texel_y < WINDOW_HEIGHT_VIRTUAL; texel_y++)
    {
      for (int texel_x = 0; texel_x < WINDOW_WIDTH_VIRTUAL; texel_x++)
      {
        /* At this point assume that the client-side texture has the same dimensions as the SDL2 texture to excessive work per pixel */
        const int texture_texel_index = (padded_texels_per_row * texel_y) + texel_x;
        const int client_texel_index = (WINDOW_WIDTH_VIRTUAL * texel_y) + texel_x;

        /* Convert the client pixel color based on the texture pixel format */
        const client_pixel_rgba_ts * p_client_pixel_color = p_client_pixels_rgba + client_texel_index;
        const uint32_t formatted_pixel_color = SDL_MapRGB(
          p_texture_pixel_format,
          p_client_pixel_color->red,
          p_client_pixel_color->green,
          p_client_pixel_color->blue
        );

        p_texture_pixels_rgba[texture_texel_index] = formatted_pixel_color;
      }
    }

    /* Unlock the locked texture and upload the changes to video memory if necessary */
    SDL_UnlockTexture(p_window_texture);

    /*
        Clear the entire (hidden) renderer window pixel data to a single color.
        This step is unnecessary since the SDL2 texture is to be drawn into the
        renderer window surface and any uncovered region after stretching is
        blacked out for fixed aspect ratio rendering that was enabled with
        SDL_RenderSetLogicalSize
    */
    const int render_clear_successful = SDL_RenderClear(p_renderer);
    if (render_clear_successful != 0)
    {
      fprintf(stderr, "\nSDL2 Render clear failed - Error: %s", SDL_GetError());
    }

    /* Copy the texture pixel data into the (hidden) renderer window surface */
    const int render_copy_successful = SDL_RenderCopy(p_renderer, p_window_texture, NULL, NULL);
    if (render_copy_successful != 0)
    {
      fprintf(stderr, "\nSDL2 Render copy failed - Error: %s", SDL_GetError());
    }

    /*
        Copy the (hidden) renderer window pixel data into the visible window surface.
        This is similar to swapping the back and front buffers with double-buffered rendering
    */
    SDL_RenderPresent(p_renderer);
  }

  /* Cleanup all resources */
  cleanup(p_window, p_renderer, p_window_texture, p_texture_pixel_format);

  /* Return to operating system successfully */
  return 0;
}

/* Function definitions */
void cleanup(
  SDL_Window * const p_window,
  SDL_Renderer * const p_renderer,
  SDL_Texture * const p_texture,
  SDL_PixelFormat * const p_pixel_format
)
{
  /* Cleanup queried pixel formats */
  SDL_FreeFormat(p_pixel_format);

  /* Cleanup SDL2 texture */
  if (p_texture != NULL)
    SDL_DestroyTexture(p_texture);

  /* Cleanup SDL2 renderer */
  if (p_renderer != NULL)
    SDL_DestroyRenderer(p_renderer);

  /* Cleanup SDL2 window */
  if (p_window != NULL)
    SDL_DestroyWindow(p_window);

  /* Quit all initialized SDL2 subsystems */
  SDL_Quit();
}
