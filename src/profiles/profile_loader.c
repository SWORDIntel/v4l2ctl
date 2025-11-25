/*
 * DSV4L2 Profile Loader
 *
 * Simple YAML-like parser for device profile files.
 * Loads role, classification, TEMPEST controls, and device configuration.
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>

#define MAX_LINE 1024
#define MAX_PROFILES 64

/* Profile structure */
typedef struct {
    char id[32];                    /* USB VID:PID or identifier */
    char vendor[64];
    char model[128];
    char role[32];
    char classification[32];
    uint32_t layer;
    int tempest_ctrl_id;

    /* Format settings */
    char pixel_format[8];
    uint32_t width;
    uint32_t height;
    uint32_t fps;

    /* Profile metadata */
    char filename[256];
} dsv4l2_device_profile_t;

/* Global profile cache */
static dsv4l2_device_profile_t g_profiles[MAX_PROFILES];
static size_t g_profile_count = 0;
static int g_profiles_loaded = 0;

/* Forward declarations */
static int load_profile_file(const char *path, dsv4l2_device_profile_t *profile);
static void trim_whitespace(char *str);
static int parse_key_value(const char *line, char *key, char *value);

/**
 * Load all profiles from profiles/ directory
 */
static void load_all_profiles(void)
{
    DIR *dir;
    struct dirent *entry;
    char path[512];

    if (g_profiles_loaded) {
        return;  /* Already loaded */
    }

    dir = opendir("profiles");
    if (!dir) {
        dir = opendir("../profiles");  /* Try parent directory */
        if (!dir) {
            dir = opendir("/etc/dsv4l2/profiles");  /* Try system directory */
        }
    }

    if (!dir) {
        /* No profiles directory found */
        g_profiles_loaded = 1;
        return;
    }

    while ((entry = readdir(dir)) != NULL && g_profile_count < MAX_PROFILES) {
        /* Only load .yaml files */
        if (strstr(entry->d_name, ".yaml") == NULL) {
            continue;
        }

        snprintf(path, sizeof(path), "profiles/%s", entry->d_name);

        dsv4l2_device_profile_t profile;
        if (load_profile_file(path, &profile) == 0) {
            memcpy(&g_profiles[g_profile_count], &profile, sizeof(profile));
            strncpy(g_profiles[g_profile_count].filename, entry->d_name,
                    sizeof(g_profiles[g_profile_count].filename) - 1);
            g_profile_count++;
        }
    }

    closedir(dir);
    g_profiles_loaded = 1;
}

/**
 * Find a profile by device ID (USB VID:PID)
 *
 * @param id Device ID to match
 * @return Profile pointer or NULL if not found
 */
const dsv4l2_device_profile_t *dsv4l2_find_profile(const char *id)
{
    size_t i;

    if (!id) {
        return NULL;
    }

    /* Ensure profiles are loaded */
    load_all_profiles();

    /* Search for matching profile */
    for (i = 0; i < g_profile_count; i++) {
        if (strcmp(g_profiles[i].id, id) == 0) {
            return &g_profiles[i];
        }
    }

    return NULL;
}

/**
 * Find a profile by role
 *
 * @param role Device role
 * @return Profile pointer or NULL if not found
 */
const dsv4l2_device_profile_t *dsv4l2_find_profile_by_role(const char *role)
{
    size_t i;

    if (!role) {
        return NULL;
    }

    /* Ensure profiles are loaded */
    load_all_profiles();

    /* Search for matching profile */
    for (i = 0; i < g_profile_count; i++) {
        if (strcmp(g_profiles[i].role, role) == 0) {
            return &g_profiles[i];
        }
    }

    return NULL;
}

/**
 * Get profile count
 */
size_t dsv4l2_get_profile_count(void)
{
    load_all_profiles();
    return g_profile_count;
}

/**
 * Get profile by index
 */
const dsv4l2_device_profile_t *dsv4l2_get_profile(size_t index)
{
    load_all_profiles();

    if (index >= g_profile_count) {
        return NULL;
    }

    return &g_profiles[index];
}

/**
 * Load a single profile file
 */
static int load_profile_file(const char *path, dsv4l2_device_profile_t *profile)
{
    FILE *fp;
    char line[MAX_LINE];
    char key[128], value[512];

    if (!path || !profile) {
        return -EINVAL;
    }

    memset(profile, 0, sizeof(*profile));

    /* Set defaults */
    profile->layer = 3;  /* L3 = sensor layer */
    profile->tempest_ctrl_id = 0x9a0902;  /* Default TEMPEST control ID */
    strncpy(profile->classification, "UNCLASSIFIED", sizeof(profile->classification) - 1);

    fp = fopen(path, "r");
    if (!fp) {
        return -errno;
    }

    while (fgets(line, sizeof(line), fp)) {
        /* Skip comments and empty lines */
        trim_whitespace(line);
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }

        /* Parse key: value */
        if (parse_key_value(line, key, value) == 0) {
            /* Device identification */
            if (strcmp(key, "id") == 0) {
                strncpy(profile->id, value, sizeof(profile->id) - 1);
            } else if (strcmp(key, "vendor") == 0) {
                strncpy(profile->vendor, value, sizeof(profile->vendor) - 1);
            } else if (strcmp(key, "model") == 0) {
                strncpy(profile->model, value, sizeof(profile->model) - 1);
            }
            /* Role & classification */
            else if (strcmp(key, "role") == 0) {
                strncpy(profile->role, value, sizeof(profile->role) - 1);
            } else if (strcmp(key, "classification") == 0) {
                strncpy(profile->classification, value, sizeof(profile->classification) - 1);
            } else if (strcmp(key, "layer") == 0) {
                profile->layer = atoi(value);
            }
            /* Video format */
            else if (strcmp(key, "pixel_format") == 0) {
                strncpy(profile->pixel_format, value, sizeof(profile->pixel_format) - 1);
            } else if (strcmp(key, "width") == 0) {
                profile->width = atoi(value);
            } else if (strcmp(key, "height") == 0) {
                profile->height = atoi(value);
            } else if (strcmp(key, "fps") == 0) {
                profile->fps = atoi(value);
            }
            /* TEMPEST control */
            else if (strcmp(key, "tempest_ctrl_id") == 0) {
                if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
                    profile->tempest_ctrl_id = strtol(value, NULL, 16);
                } else {
                    profile->tempest_ctrl_id = atoi(value);
                }
            }
        }
    }

    fclose(fp);

    /* Validate required fields */
    if (profile->id[0] == '\0' || profile->role[0] == '\0') {
        return -EINVAL;
    }

    return 0;
}

/**
 * Trim leading and trailing whitespace
 */
static void trim_whitespace(char *str)
{
    char *start, *end;

    if (!str || *str == '\0') {
        return;
    }

    /* Trim leading */
    start = str;
    while (isspace((unsigned char)*start)) {
        start++;
    }

    /* Trim trailing */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';

    /* Move trimmed string to beginning */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/**
 * Parse "key: value" line
 */
static int parse_key_value(const char *line, char *key, char *value)
{
    const char *colon;
    size_t key_len, value_len;

    if (!line || !key || !value) {
        return -EINVAL;
    }

    /* Find colon separator */
    colon = strchr(line, ':');
    if (!colon) {
        return -EINVAL;
    }

    /* Extract key */
    key_len = colon - line;
    if (key_len >= 128) {
        return -EINVAL;
    }
    strncpy(key, line, key_len);
    key[key_len] = '\0';
    trim_whitespace(key);

    /* Extract value */
    value_len = strlen(colon + 1);
    if (value_len >= 512) {
        return -EINVAL;
    }
    strcpy(value, colon + 1);
    trim_whitespace(value);

    /* Remove quotes if present */
    if (value[0] == '"' || value[0] == '\'') {
        size_t len = strlen(value);
        if (len > 1 && value[len - 1] == value[0]) {
            memmove(value, value + 1, len - 2);
            value[len - 2] = '\0';
        }
    }

    return 0;
}
