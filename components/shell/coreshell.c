#include "coreshell.h"
#include "command_registry.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char* TAG = "SHELL";

void coreshell_start(void) {
    ESP_LOGI(TAG, "CoreShell starting...");
    
    char input_buffer[512];
    int pos = 0;
    
    printf("\n");
    printf("CorESP32 > ");
    fflush(stdout);
    
    while (1) {
        // Watchdog feed
        vTaskDelay(pdMS_TO_TICKS(1));
        
        // Read character (blocking, but with timeout via vTaskDelay)
        int c = fgetc(stdin);
        
        if (c == EOF || c == 0xFF) {
            // No data available
            continue;
        }
        
        // Handle newline (Enter pressed)
        if (c == '\n' || c == '\r') {
            printf("\n");  // Echo newline
            
            if (pos > 0) {
                input_buffer[pos] = '\0';
                
                // Execute command
                esp_err_t ret = command_execute(input_buffer);
                if (ret != ESP_OK) {
                    printf("Error: %s\n", esp_err_to_name(ret));
                }
                
                pos = 0;  // Reset buffer
            }
            
            printf("CorESP32 > ");
            fflush(stdout);
            continue;
        }
        
        // Handle backspace
        if (c == '\b' || c == 127) {
            if (pos > 0) {
                pos--;
                printf("\b \b");  // Erase character on screen
                fflush(stdout);
            }
            continue;
        }
        
        // Handle printable characters
        if (c >= 32 && c < 127 && pos < sizeof(input_buffer) - 1) {
            input_buffer[pos++] = (char)c;
            putchar(c);  // Echo character
            fflush(stdout);
        }
    }
}