#pragma once

/*
 * Android/Linux Speex integer type config.
 * Added during SRC-like cleanup because vendor/speex/speex_types.h includes
 * speex_config_types.h on non-special platforms.
 */
#include <stdint.h>

typedef int16_t  spx_int16_t;
typedef uint16_t spx_uint16_t;
typedef int32_t  spx_int32_t;
typedef uint32_t spx_uint32_t;
