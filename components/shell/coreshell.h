/**
 * CoreShell - Main Shell Interface
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start CoreShell (blocking)
 * 
 * This function will run the shell loop forever.
 * It will only return if shell is disabled or critical error.
 */
void coreshell_start(void);

#ifdef __cplusplus
}
#endif