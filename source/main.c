#include <stdio.h>
#include <SDL.h>

/* Constants */
const int OS_FAILURE_RETURN_CODE = -1;
const char WINDOW_TITLE[] = "SDL2 rendering without graphics API";
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int WINDOW_WIDTH_VIRTUAL = 160;
const int WINDOW_HEIGHT_VIRTUAL = 144;

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
      > things to show in post:
          - examples of pipes and letterbox
          - other things above
          - what the virtual texture means
          - which calls are optional and what effect this has: Clearing; logical sized rendering; ... ?
          - texture locking
          - set the window to be resizeable, handle the resizing to demonstrate the piping and letterboxed rendering
*/

/* Function prototypes */
void cleanup(
  SDL_Window * const p_window,
  SDL_Renderer * const p_renderer,
  SDL_Texture * const p_texture
);

/* Entry point */
int main(int argc, char * argv[])
{
  /* Initialize SDL2 video and events subsystems */
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
  {
    fprintf(stderr, "\nRequired SDL2 subsystems could not be initialized - Error: %s", SDL_GetError());
    cleanup(NULL, NULL, NULL);
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
    cleanup(NULL, NULL, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 window created successfully - Now create the renderer */
  SDL_Renderer * const p_renderer = SDL_CreateRenderer(p_window, -1, SDL_RENDERER_PRESENTVSYNC);
  if (p_renderer == NULL)
  {
    fprintf(stderr, "\nSDL2 renderer could not be created - Error: %s", SDL_GetError());
    cleanup(p_window, NULL, NULL);
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
    cleanup(p_window, p_renderer, NULL);
    return OS_FAILURE_RETURN_CODE;
  }

  /* SDL2 window texture created successfully */
  const int logical_size_set = SDL_RenderSetLogicalSize(p_renderer, WINDOW_WIDTH_VIRTUAL, WINDOW_HEIGHT_VIRTUAL);
  if (logical_size_set != 0)
  {
    fprintf(stderr, "\nSDL2 logical render size could not be set - Error: %s", SDL_GetError());
    cleanup(p_window, p_renderer, p_window_texture);
    return OS_FAILURE_RETURN_CODE;
  }

  /* Set renderer draw and clear color in case the renderer is to be cleared */
  const int set_render_draw_color_successful = SDL_SetRenderDrawColor(p_renderer, 0x32, 0x96, 0xFF, 0xFF);
  if (set_render_draw_color_successful != 0)
  {
    fprintf(stderr, "\nSDL2 renderer draw color could not be set - Error: %s", SDL_GetError());
  }

  /* SDL2 renderer logical size set successfully - Now start the window loop */
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

    /* Update texture color data before rendering it into the (hidden) renderer surface */
    /* Lock the texture for WRITING ONLY */
    void * p_texture_pixels = NULL;
    int texture_bytes_per_row;
    const int lock_texture_successful = SDL_LockTexture(p_window_texture, NULL, &p_texture_pixels, &texture_bytes_per_row);
    if (lock_texture_successful != 0)
    {
      fprintf(stderr, "\nSDL2 texture could not be locked - %s", SDL_GetError());
    }

    /* Write into the texture - TODO-GS: Use the texture pitch here i.e. is RGBA8888 padded in some way? */
    /*
        Use solution stated in 'https://gamedev.stackexchange.com/questions/98641/how-do-i-modify-textures-in-sdl-with-direct-pixel-access'
        to do things safely even when texture are padded
    */
    unsigned int * const p_pixels = (unsigned int *)p_texture_pixels;
    for (int i = 0; i < (WINDOW_WIDTH_VIRTUAL * WINDOW_HEIGHT_VIRTUAL); i++)
    {
      unsigned int random_color = (unsigned int) rand() % 256;
      random_color <<= 8;
      random_color |= (unsigned int) rand() % 256;
      random_color <<= 8;
      random_color |= (unsigned int) rand() % 256;
      random_color <<= 8;
      random_color |= 0xFF;

      p_pixels[i] = random_color;
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
  cleanup(p_window, p_renderer, p_window_texture);

  /* Return to operating system successfully */
  return 0;
}

/* Function definitions */
void cleanup(
  SDL_Window * const p_window,
  SDL_Renderer * const p_renderer,
  SDL_Texture * const p_texture
)
{
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
