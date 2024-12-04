#include "myignore.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fnmatch.h>
#include <ctype.h>

// Helper function to trim leading and trailing whitespace
char *trim_whitespace(char *str)
{
    char *end;

    // Trim leading space
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

void load_myignore(const char *path, ignore_list_t *ignore_list)
{
    char ignore_path[PATH_MAX];
    snprintf(ignore_path, sizeof(ignore_path), "%s/.myignore", path);

    FILE *file = fopen(ignore_path, "r");
    if (!file)
    {
        printf("Warning: No .myignore file found at '%s'\n", ignore_path);
        return;
    }

    char line[PATH_MAX];
    ignore_list->count = 0;
    while (fgets(line, sizeof(line), file) && ignore_list->count < MAX_PATTERNS)
    {
        // Remove newline character
        line[strcspn(line, "\n")] = 0;

        // Trim leading and trailing whitespace
        char *trimmed_line = trim_whitespace(line);

        // Skip comments and empty lines
        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0')
            continue;

        // Add pattern to ignore list
        strcpy(ignore_list->patterns[ignore_list->count], trimmed_line);
        ignore_list->count++;
    }

    fclose(file);
}
// myignore.c
int is_ignored(const char *filepath, ignore_list_t *ignore_list)
{
    if (!ignore_list || !filepath)
        return 0;

    // Never ignore the .myignore file itself
    if (strcmp(filepath, ".myignore") == 0)
        return 0;

    for (int i = 0; i < ignore_list->count; i++) {
        const char *pattern = ignore_list->patterns[i];
        
        // Skip empty patterns
        if (strlen(pattern) == 0)
            continue;

        // Handle directory-specific patterns (ending with /)
        int is_dir_pattern = pattern[strlen(pattern) - 1] == '/';
        
        // Remove trailing slash for matching
        char clean_pattern[PATH_MAX];
        strncpy(clean_pattern, pattern, sizeof(clean_pattern) - 1);
        if (is_dir_pattern)
            clean_pattern[strlen(clean_pattern) - 1] = '\0';

        // Check direct match
        if (fnmatch(clean_pattern, filepath, FNM_PATHNAME | FNM_LEADING_DIR) == 0)
            return 1;

        // Check if any parent directory matches
        char parent_path[PATH_MAX];
        strncpy(parent_path, filepath, sizeof(parent_path));
        
        char *slash = strrchr(parent_path, '/');
        while (slash) {
            *slash = '\0';
            if (fnmatch(clean_pattern, parent_path, FNM_PATHNAME) == 0)
                return 1;
            slash = strrchr(parent_path, '/');
        }
    }
    
    return 0;
}