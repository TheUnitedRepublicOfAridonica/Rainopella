#ifndef HAL_BOARD_H
#define HAL_BOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Implement these functions for the target MCU and its SDK.
 *
 * Timing contract:
 * - hal_time_us() must be monotonic modulo uint32_t wraparound.
 * - DMA submit functions must return promptly and must not retain buffers
 *   beyond the corresponding busy period.
 * - DMA functions must be safe to poll from the main loop.
 */

typedef enum
{
    HAL_OK = 0,
    HAL_ERROR
} hal_status_t;

enum
{
    HAL_BUTTON_UP     = (1u << 0),
    HAL_BUTTON_DOWN   = (1u << 1),
    HAL_BUTTON_LEFT   = (1u << 2),
    HAL_BUTTON_RIGHT  = (1u << 3),
    HAL_BUTTON_A      = (1u << 4),
    HAL_BUTTON_B      = (1u << 5),
    HAL_BUTTON_START  = (1u << 6),
    HAL_BUTTON_SELECT = (1u << 7)
};

hal_status_t hal_display_init(void);
bool hal_display_dma_busy(void);
hal_status_t hal_display_submit_rgb565(const uint16_t *pixels,
                                       size_t pixel_count);
void hal_display_poll(void);

hal_status_t hal_audio_init(uint32_t sample_rate_hz);
bool hal_audio_dma_busy(void);
hal_status_t hal_audio_submit(const int16_t *interleaved_stereo,
                              size_t frames);
void hal_audio_poll(void);

hal_status_t hal_input_init(void);
uint32_t hal_input_read_raw(void);

uint32_t hal_time_us(void);

/* Optional board-specific safe state (e.g. mute audio, disable backlight). */
void hal_enter_safe_state(void);

#endif
