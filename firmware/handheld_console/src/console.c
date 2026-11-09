#include "console.h"
#include "hal_board.h"

#include <limits.h>
#include <string.h>

/*
 * Static allocation avoids heap fragmentation and gives the board port the
 * option to place these buffers in DMA-capable or external RAM sections.
 */
static uint16_t g_framebuffers[2][CONSOLE_FRAME_PIXELS];
static uint8_t g_render_index;
static uint32_t g_input_scan_time;
static uint32_t g_raw_previous;
static uint32_t g_debounced;
static uint8_t g_debounce_count[8];
static console_input_t g_input_state;

static int16_t g_audio_chunk[CONSOLE_AUDIO_CHUNK_FRAMES * 2u];
static const int16_t *g_pcm_source;
static size_t g_pcm_total_frames;
static size_t g_pcm_position;

static const uint32_t g_button_masks[8] =
{
    HAL_BUTTON_UP, HAL_BUTTON_DOWN, HAL_BUTTON_LEFT, HAL_BUTTON_RIGHT,
    HAL_BUTTON_A, HAL_BUTTON_B, HAL_BUTTON_START, HAL_BUTTON_SELECT
};

/*
 * Optional PCM stream setup point for game code. The source must remain valid
 * until playback completes. Call only from the main thread.
 */
void console_audio_play_pcm(const int16_t *stereo_samples, size_t frames)
{
    if ((stereo_samples == NULL) || (frames == 0u))
    {
        g_pcm_source = NULL;
        g_pcm_total_frames = 0u;
        g_pcm_position = 0u;
        return;
    }

    g_pcm_source = stereo_samples;
    g_pcm_total_frames = frames;
    g_pcm_position = 0u;
}

console_canvas_t *console_canvas(void)
{
    static console_canvas_t canvas;

    canvas.pixels = g_framebuffers[g_render_index];
    canvas.width = (uint16_t)CONSOLE_WIDTH;
    canvas.height = (uint16_t)CONSOLE_HEIGHT;
    return &canvas;
}

console_input_t console_input_state(void)
{
    return g_input_state;
}

void console_input_poll(void)
{
    uint32_t const now = hal_time_us();

    if ((uint32_t)(now - g_input_scan_time) < CONSOLE_INPUT_SCAN_US)
    {
        return;
    }
    g_input_scan_time = now;

    uint32_t const raw = hal_input_read_raw();
    uint32_t pressed = 0u;
    uint32_t released = 0u;

    for (size_t i = 0u; i < 8u; ++i)
    {
        uint32_t const mask = g_button_masks[i];
        bool const raw_pressed = ((raw & mask) != 0u);
        bool const previous_raw_pressed = ((g_raw_previous & mask) != 0u);

        if (raw_pressed != previous_raw_pressed)
        {
            g_debounce_count[i] = 0u;
        }
        else if (g_debounce_count[i] < CONSOLE_DEBOUNCE_SCANS)
        {
            ++g_debounce_count[i];
        }

        if (g_debounce_count[i] >= CONSOLE_DEBOUNCE_SCANS)
        {
            bool const was_pressed = ((g_debounced & mask) != 0u);

            if (raw_pressed && !was_pressed)
            {
                g_debounced |= mask;
                pressed |= mask;
            }
            else if (!raw_pressed && was_pressed)
            {
                g_debounced &= ~mask;
                released |= mask;
            }
        }
    }

    g_raw_previous = raw;
    g_input_state.held = g_debounced;
    g_input_state.pressed |= pressed;
    g_input_state.released |= released;
}

bool console_display_present(void)
{
    uint8_t const submitted_index = g_render_index;

    if (hal_display_dma_busy())
    {
        return false;
    }

    if (hal_display_submit_rgb565(g_framebuffers[submitted_index],
                                  CONSOLE_FRAME_PIXELS) != HAL_OK)
    {
        return false;
    }

    /*
     * The submitted buffer is now owned by the display DMA. Render into the
     * other buffer until the DMA reports completion and another submit occurs.
     */
    g_render_index ^= 1u;
    return true;
}

void console_draw_pixel(console_canvas_t *canvas,
                        int32_t x, int32_t y, uint16_t rgb565)
{
    if ((canvas == NULL) || (canvas->pixels == NULL) ||
        (x < 0) || (y < 0) ||
        (x >= (int32_t)canvas->width) ||
        (y >= (int32_t)canvas->height))
    {
        return;
    }

    size_t const offset = ((size_t)y * (size_t)canvas->width) + (size_t)x;
    canvas->pixels[offset] = rgb565;
}

void console_draw_bitmap(console_canvas_t *canvas,
                         int32_t x, int32_t y,
                         uint16_t width, uint16_t height,
                         const uint16_t *rgb565_pixels)
{
    if ((canvas == NULL) || (canvas->pixels == NULL) ||
        (rgb565_pixels == NULL))
    {
        return;
    }

    for (uint32_t row = 0u; row < height; ++row)
    {
        int32_t const dst_y = y + (int32_t)row;

        if ((dst_y < 0) || (dst_y >= (int32_t)canvas->height))
        {
            continue;
        }

        for (uint32_t col = 0u; col < width; ++col)
        {
            int32_t const dst_x = x + (int32_t)col;

            if ((dst_x >= 0) && (dst_x < (int32_t)canvas->width))
            {
                size_t const src =
                    ((size_t)row * (size_t)width) + (size_t)col;
                size_t const dst =
                    ((size_t)dst_y * (size_t)canvas->width) +
                    (size_t)dst_x;

                canvas->pixels[dst] = rgb565_pixels[src];
            }
        }
    }
}

void console_audio_poll(void)
{
    if ((g_pcm_source == NULL) || hal_audio_dma_busy())
    {
        return;
    }

    size_t remaining = g_pcm_total_frames - g_pcm_position;
    size_t frames = remaining;

    if (frames > CONSOLE_AUDIO_CHUNK_FRAMES)
    {
        frames = CONSOLE_AUDIO_CHUNK_FRAMES;
    }

    if (frames == 0u)
    {
        g_pcm_source = NULL;
        return;
    }

    size_t const sample_count = frames * 2u;
    (void)memcpy(g_audio_chunk,
                 &g_pcm_source[g_pcm_position * 2u],
                 sample_count * sizeof(g_audio_chunk[0]));

    if (hal_audio_submit(g_audio_chunk, frames) == HAL_OK)
    {
        g_pcm_position += frames;

        if (g_pcm_position >= g_pcm_total_frames)
        {
            g_pcm_source = NULL;
        }
    }
}

void console_fatal_error(void)
{
    hal_enter_safe_state();

    /* Intentionally idle on fatal hardware initialization failure. */
    for (;;)
    {
        /* A board may replace this with a watchdog-safe fault handler. */
    }
}

void console_run(console_update_fn update, console_render_fn render)
{
    uint32_t next_frame = hal_time_us();
    uint32_t previous_frame = next_frame;

    for (;;)
    {
        hal_display_poll();
        hal_audio_poll();
        console_input_poll();

        uint32_t const now = hal_time_us();

        if ((int32_t)(now - next_frame) >= 0)
        {
            uint32_t const elapsed = now - previous_frame;
            previous_frame = now;
            next_frame = now + CONSOLE_FRAME_PERIOD_US;

            /*
             * pressed/released are edge events; clear after the game update.
             */
            g_input_state = console_input_state();

            if (update != NULL)
            {
                update(&g_input_state, elapsed);
            }

            g_input_state.pressed = 0u;
            g_input_state.released = 0u;

            if (render != NULL)
            {
                render(console_canvas());
            }

            (void)console_display_present();
        }
    }
}
