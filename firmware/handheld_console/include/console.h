#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONSOLE_WIDTH              240u
#define CONSOLE_HEIGHT             240u
#define CONSOLE_FRAME_PIXELS       (CONSOLE_WIDTH * CONSOLE_HEIGHT)
#define CONSOLE_FRAME_PERIOD_US    16667u
#define CONSOLE_INPUT_SCAN_US      1000u
#define CONSOLE_DEBOUNCE_SCANS     5u
#define CONSOLE_AUDIO_RATE_HZ      22050u
#define CONSOLE_AUDIO_CHUNK_FRAMES 256u

typedef struct
{
    uint32_t held;
    uint32_t pressed;
    uint32_t released;
} console_input_t;

typedef struct
{
    uint16_t *pixels;
    uint16_t width;
    uint16_t height;
} console_canvas_t;

typedef void (*console_update_fn)(const console_input_t *input,
                                  uint32_t elapsed_us);
typedef void (*console_render_fn)(console_canvas_t *canvas);

/* Call once per main-loop iteration; both functions are non-blocking. */
void console_input_poll(void);
void console_audio_poll(void);

/* Read the latest debounced input state. */
console_input_t console_input_state(void);

/* Call once per frame. Returns false if the previous frame is still in DMA. */
bool console_display_present(void);
console_canvas_t *console_canvas(void);

void console_draw_pixel(console_canvas_t *canvas,
                        int32_t x, int32_t y, uint16_t rgb565);
void console_draw_bitmap(console_canvas_t *canvas,
                         int32_t x, int32_t y,
                         uint16_t width, uint16_t height,
                         const uint16_t *rgb565_pixels);

void console_run(console_update_fn update, console_render_fn render);
void console_fatal_error(void);

#endif
