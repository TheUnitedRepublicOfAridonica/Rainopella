#include "console.h"
#include "hal_board.h"

static void game_update(const console_input_t *input, uint32_t elapsed_us)
{
    (void)input;
    (void)elapsed_us;

    /* Update game/UI state here. Keep this function bounded and non-blocking. */
}

static void game_render(console_canvas_t *canvas)
{
    if ((canvas == NULL) || (canvas->pixels == NULL))
    {
        return;
    }

    /* Example clear to black. Replace with scene rendering. */
    for (size_t i = 0u; i < (size_t)canvas->width * canvas->height; ++i)
    {
        canvas->pixels[i] = 0x0000u;
    }
}

int main(void)
{
    /*
     * Ordered bring-up: display, audio, then input. Each HAL initializer should
     * configure safe pin states and return promptly; no delay loops are needed.
     */
    if (hal_display_init() != HAL_OK)
    {
        console_fatal_error();
    }

    if (hal_audio_init(CONSOLE_AUDIO_RATE_HZ) != HAL_OK)
    {
        console_fatal_error();
    }

    if (hal_input_init() != HAL_OK)
    {
        console_fatal_error();
    }

    console_run(game_update, game_render);
    return 0;
}
