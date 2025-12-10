/**
 * Storage Commands - File System Management
 */

#include "command_registry.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TAG = "CMD_STORAGE";

// ls [path]
static esp_err_t cmd_ls(int argc, char **argv)
{
    const char *path = (argc > 1) ? argv[1] : "/littlefs";

    DIR *dir = opendir(path);
    if (!dir)
    {
        printf("Failed to open directory: %s\n", path);
        return ESP_FAIL;
    }

    printf("\n");
    printf("Directory: %s\n", path);
    printf("----------------------------\n");

    struct dirent *entry;
    int file_count = 0;
    int dir_count = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        // Get full path (larger buffer to avoid truncation warning)
        char full_path[512];
        int len = snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (len >= sizeof(full_path))
        {
            printf("  [SKIP] %s (path too long)\n", entry->d_name);
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                printf("  [DIR]  %s\n", entry->d_name);
                dir_count++;
            }
            else
            {
                printf("  [FILE] %s (%ld bytes)\n", entry->d_name, st.st_size);
                file_count++;
            }
        }
    }

    closedir(dir);

    printf("----------------------------\n");
    printf("Total: %d files, %d directories\n", file_count, dir_count);
    printf("\n");

    return ESP_OK;
}

// cat <file>
static esp_err_t cmd_cat(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: cat <file>\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *filename = argv[1];

    FILE *f = fopen(filename, "r");
    if (!f)
    {
        printf("Failed to open file: %s\n", filename);
        return ESP_FAIL;
    }

    printf("\n");
    printf("--- %s ---\n", filename);

    char line[256];
    while (fgets(line, sizeof(line), f))
    {
        printf("%s", line);
    }

    printf("\n--- End of file ---\n");
    printf("\n");

    fclose(f);
    return ESP_OK;
}

// echo <text> <file>
static esp_err_t cmd_echo(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: echo <text> <file>\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *text = argv[1];
    const char *filename = argv[2];

    FILE *f = fopen(filename, "w");
    if (!f)
    {
        printf("Failed to open file: %s\n", filename);
        return ESP_FAIL;
    }

    fprintf(f, "%s\n", text);
    fclose(f);

    printf("Written to: %s\n", filename);
    return ESP_OK;
}

// rm <file>
static esp_err_t cmd_rm(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: rm <file>\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *filename = argv[1];

    if (unlink(filename) == 0)
    {
        printf("Deleted: %s\n", filename);
        return ESP_OK;
    }
    else
    {
        printf("Failed to delete: %s\n", filename);
        return ESP_FAIL;
    }
}

// mkdir <path>
static esp_err_t cmd_mkdir(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("Usage: mkdir <path>\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *path = argv[1];

    if (mkdir(path, 0755) == 0)
    {
        printf("Created directory: %s\n", path);
        return ESP_OK;
    }
    else
    {
        printf("Failed to create directory: %s\n", path);
        return ESP_FAIL;
    }
}

// format
static esp_err_t cmd_format(int argc, char **argv)
{
    printf("\n");
    printf("WARNING: This will erase ALL data!\n");
    printf("Type 'yes' to confirm: ");
    fflush(stdout);

    char confirm[10];
    if (fgets(confirm, sizeof(confirm), stdin) == NULL)
    {
        printf("\nCancelled.\n");
        return ESP_OK;
    }

    // Remove newline
    confirm[strcspn(confirm, "\r\n")] = 0;

    if (strcmp(confirm, "yes") != 0)
    {
        printf("Cancelled.\n");
        return ESP_OK;
    }

    printf("\nFormatting LittleFS...\n");

    // Unmount first
    esp_vfs_littlefs_unregister("littlefs");

    // Format
    esp_err_t ret = esp_littlefs_format("littlefs");

    // Remount
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "littlefs", // ← ZURÜCK zu "littlefs"
        .format_if_mount_failed = false,
        .dont_mount = false,
    };

    esp_vfs_littlefs_register(&conf);

    if (ret == ESP_OK)
    {
        printf("Format complete.\n");
    }
    else
    {
        printf("Format failed: %s\n", esp_err_to_name(ret));
    }

    printf("\n");
    return ret;
}

// df (disk free)
static esp_err_t cmd_df(int argc, char **argv)
{
    size_t total = 0, used = 0;
    esp_err_t ret = esp_littlefs_info("littlefs", &total, &used);

    if (ret != ESP_OK)
    {
        printf("Failed to get storage info\n");
        return ret;
    }

    printf("\n");
    printf("Storage Information:\n");
    printf("  Total:     %zu KB\n", total / 1024);
    printf("  Used:      %zu KB\n", used / 1024);
    printf("  Free:      %zu KB\n", (total - used) / 1024);
    printf("  Usage:     %d%%\n", (int)((used * 100) / total));
    printf("\n");

    return ESP_OK;
}

// Main command handler
static esp_err_t cmd_storage_execute(int argc, char **argv)
{
    if (argc < 1)
    {
        printf("Unknown storage command\n");
        return ESP_ERR_INVALID_ARG;
    }

    const char *cmd = argv[0];

    if (strcmp(cmd, "ls") == 0)
    {
        return cmd_ls(argc, argv);
    }
    else if (strcmp(cmd, "cat") == 0)
    {
        return cmd_cat(argc, argv);
    }
    else if (strcmp(cmd, "echo") == 0)
    {
        return cmd_echo(argc, argv);
    }
    else if (strcmp(cmd, "rm") == 0)
    {
        return cmd_rm(argc, argv);
    }
    else if (strcmp(cmd, "mkdir") == 0)
    {
        return cmd_mkdir(argc, argv);
    }
    else if (strcmp(cmd, "format") == 0)
    {
        return cmd_format(argc, argv);
    }
    else if (strcmp(cmd, "df") == 0)
    {
        return cmd_df(argc, argv);
    }
    else
    {
        printf("Unknown storage command: %s\n", cmd);
        return ESP_ERR_INVALID_ARG;
    }
}

// Command registration
static const command_t cmd_ls_def = {
    .name = "ls",
    .description = "List directory contents",
    .usage = "ls [path]",
    .execute = cmd_storage_execute};

static const command_t cmd_cat_def = {
    .name = "cat",
    .description = "Display file contents",
    .usage = "cat <file>",
    .execute = cmd_storage_execute};

static const command_t cmd_echo_def = {
    .name = "echo",
    .description = "Write text to file",
    .usage = "echo <text> <file>",
    .execute = cmd_storage_execute};

static const command_t cmd_rm_def = {
    .name = "rm",
    .description = "Remove file",
    .usage = "rm <file>",
    .execute = cmd_storage_execute};

static const command_t cmd_mkdir_def = {
    .name = "mkdir",
    .description = "Create directory",
    .usage = "mkdir <path>",
    .execute = cmd_storage_execute};

static const command_t cmd_format_def = {
    .name = "format",
    .description = "Format filesystem (WARNING: erases all data)",
    .usage = "format",
    .execute = cmd_storage_execute};

static const command_t cmd_df_def = {
    .name = "df",
    .description = "Show storage usage",
    .usage = "df",
    .execute = cmd_storage_execute};

void register_storage_commands(void)
{
    command_register(&cmd_ls_def);
    command_register(&cmd_cat_def);
    command_register(&cmd_echo_def);
    command_register(&cmd_rm_def);
    command_register(&cmd_mkdir_def);
    command_register(&cmd_format_def);
    command_register(&cmd_df_def);
}