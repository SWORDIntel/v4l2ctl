/*
 * DSV4L2 DSMIL Policy Bridge
 *
 * Maps DSMIL THREATCON levels to TEMPEST states and enforces
 * layer-specific security policies.
 *
 * THREATCON Levels (0-5):
 *   0 = NORMAL     - No heightened threat
 *   1 = ALPHA      - General threat
 *   2 = BRAVO      - Increased threat
 *   3 = CHARLIE    - Significant threat
 *   4 = DELTA      - Severe threat
 *   5 = EMERGENCY  - Critical threat
 */

#include "dsv4l2_annotations.h"
#include "dsv4l2_policy.h"
#include "dsv4l2_profiles.h"
#include "dsv4l2rt.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

/* DSMIL THREATCON levels */
typedef enum {
    THREATCON_NORMAL    = 0,
    THREATCON_ALPHA     = 1,
    THREATCON_BRAVO     = 2,
    THREATCON_CHARLIE   = 3,
    THREATCON_DELTA     = 4,
    THREATCON_EMERGENCY = 5,
} dsmil_threatcon_t;

/* Layer policy structure */
typedef struct {
    uint32_t layer;                     /* DSMIL layer (0-8) */
    uint32_t max_width;                 /* Max resolution width */
    uint32_t max_height;                /* Max resolution height */
    dsv4l2_tempest_state_t min_tempest; /* Minimum TEMPEST state */
} dsv4l2_layer_policy_t;

/* Global policy state */
static struct {
    dsmil_threatcon_t current_threatcon;
    int initialized;
} g_policy = {
    .current_threatcon = THREATCON_NORMAL,
    .initialized = 0,
};

/* Layer-specific policies */
static const dsv4l2_layer_policy_t g_layer_policies[] = {
    /* L0: Hardware - no direct access */
    { .layer = 0, .max_width = 0,    .max_height = 0,    .min_tempest = DSV4L2_TEMPEST_DISABLED },
    /* L1: Drivers - no direct access */
    { .layer = 1, .max_width = 0,    .max_height = 0,    .min_tempest = DSV4L2_TEMPEST_DISABLED },
    /* L2: HAL - limited resolution */
    { .layer = 2, .max_width = 640,  .max_height = 480,  .min_tempest = DSV4L2_TEMPEST_DISABLED },
    /* L3: Sensors - full resolution */
    { .layer = 3, .max_width = 1280, .max_height = 720,  .min_tempest = DSV4L2_TEMPEST_DISABLED },
    /* L4: Application - full resolution */
    { .layer = 4, .max_width = 1920, .max_height = 1080, .min_tempest = DSV4L2_TEMPEST_LOW },
    /* L5: Policy - enhanced resolution */
    { .layer = 5, .max_width = 1920, .max_height = 1080, .min_tempest = DSV4L2_TEMPEST_LOW },
    /* L6: Data fusion - enhanced resolution */
    { .layer = 6, .max_width = 1920, .max_height = 1080, .min_tempest = DSV4L2_TEMPEST_LOW },
    /* L7: Quantum/accelerator - 4K */
    { .layer = 7, .max_width = 3840, .max_height = 2160, .min_tempest = DSV4L2_TEMPEST_HIGH },
    /* L8: AI orchestration - 4K */
    { .layer = 8, .max_width = 3840, .max_height = 2160, .min_tempest = DSV4L2_TEMPEST_HIGH },
};

/* THREATCON to TEMPEST state mapping */
static const dsv4l2_tempest_state_t g_threatcon_tempest_map[] = {
    DSV4L2_TEMPEST_DISABLED,  /* NORMAL */
    DSV4L2_TEMPEST_LOW,       /* ALPHA */
    DSV4L2_TEMPEST_LOW,       /* BRAVO */
    DSV4L2_TEMPEST_HIGH,      /* CHARLIE */
    DSV4L2_TEMPEST_HIGH,      /* DELTA */
    DSV4L2_TEMPEST_LOCKDOWN,  /* EMERGENCY */
};

/**
 * Initialize DSMIL policy subsystem
 */
void dsv4l2_policy_init(void)
{
    if (g_policy.initialized) {
        return;
    }

    g_policy.current_threatcon = THREATCON_NORMAL;
    g_policy.initialized = 1;
}

/**
 * Get current THREATCON level
 */
dsmil_threatcon_t dsv4l2_get_threatcon(void)
{
    dsv4l2_policy_init();
    return g_policy.current_threatcon;
}

/**
 * Set THREATCON level
 *
 * This would normally integrate with DSMIL fabric to receive
 * THREATCON updates. For now, it's a manual setter.
 */
int dsv4l2_set_threatcon(dsmil_threatcon_t level)
{
    if (level > THREATCON_EMERGENCY) {
        return -EINVAL;
    }

    dsv4l2_policy_init();
    g_policy.current_threatcon = level;

    return 0;
}

/**
 * Apply THREATCON to device (maps to TEMPEST state)
 *
 * @param dev Device handle
 * @return 0 on success, negative errno on error
 */
int dsv4l2_apply_threatcon(dsv4l2_device_t *dev)
{
    dsv4l2_tempest_state_t target_state;

    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_policy_init();

    /* Map THREATCON to TEMPEST state */
    target_state = g_threatcon_tempest_map[g_policy.current_threatcon];

    /* Apply TEMPEST state to device */
    return dsv4l2_set_tempest_state(dev, target_state);
}

/**
 * Get layer policy
 *
 * @param layer DSMIL layer (0-8)
 * @param policy Output policy structure
 * @return 0 on success, -EINVAL if layer invalid
 */
int dsv4l2_get_layer_policy(uint32_t layer, dsv4l2_layer_policy_t **policy)
{
    if (layer > 8 || !policy) {
        return -EINVAL;
    }

    /* Return pointer to const policy (cast away const for API) */
    *policy = (dsv4l2_layer_policy_t *)&g_layer_policies[layer];
    return 0;
}

/**
 * Check if capture is allowed for device
 *
 * Enforces:
 * - Layer-specific resolution limits
 * - Minimum TEMPEST requirements
 * - THREATCON-based restrictions
 *
 * @param dev Device handle
 * @param context Capture context (for logging)
 * @return 0 if allowed, -EPERM if blocked
 */
int dsv4l2_check_capture_allowed(dsv4l2_device_t *dev, const char *context)
{
    dsv4l2_tempest_state_t current_tempest;
    const dsv4l2_layer_policy_t *layer_policy;

    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_policy_init();

    /* Get current TEMPEST state */
    current_tempest = dsv4l2_get_tempest_state(dev);

    /* LOCKDOWN blocks all capture */
    if (current_tempest == DSV4L2_TEMPEST_LOCKDOWN) {
        return -EPERM;
    }

    /* Get layer policy */
    if (dev->layer <= 8) {
        layer_policy = &g_layer_policies[dev->layer];

        /* Check minimum TEMPEST requirement for layer */
        if (current_tempest < layer_policy->min_tempest) {
            return -EPERM;
        }

        /* TODO: Check resolution against layer policy
         * (would need current format from device) */
    }

    /* Context could be used for logging/audit */
    (void)context;

    return 0;
}

/* Clearance level enumeration */
typedef enum {
    CLEARANCE_NONE          = 0,
    CLEARANCE_UNCLASSIFIED  = 1,
    CLEARANCE_CONFIDENTIAL  = 2,
    CLEARANCE_SECRET        = 3,
    CLEARANCE_TOP_SECRET    = 4,
} clearance_level_t;

/* Role-to-minimum-clearance mapping */
typedef struct {
    const char *role;
    clearance_level_t min_clearance;
} role_clearance_map_t;

static const role_clearance_map_t g_role_clearance_map[] = {
    { "generic_webcam", CLEARANCE_UNCLASSIFIED },
    { "ir_sensor",      CLEARANCE_CONFIDENTIAL },
    { "iris_scanner",   CLEARANCE_SECRET },
    { "tempest_cam",    CLEARANCE_TOP_SECRET },
    { NULL, CLEARANCE_NONE }
};

/**
 * Get clearance level from classification string
 */
static clearance_level_t get_clearance_from_classification(const char *classification)
{
    if (!classification) {
        return CLEARANCE_NONE;
    }

    if (strstr(classification, "TOP_SECRET") || strstr(classification, "TOP SECRET")) {
        return CLEARANCE_TOP_SECRET;
    }
    if (strstr(classification, "SECRET")) {
        return CLEARANCE_SECRET;
    }
    if (strstr(classification, "CONFIDENTIAL")) {
        return CLEARANCE_CONFIDENTIAL;
    }
    if (strstr(classification, "UNCLASSIFIED")) {
        return CLEARANCE_UNCLASSIFIED;
    }

    return CLEARANCE_NONE;
}

/**
 * Get user's current clearance level
 *
 * Checks environment variable DSV4L2_CLEARANCE or defaults to UNCLASSIFIED
 */
static clearance_level_t get_user_clearance(void)
{
    const char *env_clearance;
    static clearance_level_t cached_clearance = 0;
    static int cached = 0;

    if (cached) {
        return cached_clearance;
    }

    env_clearance = getenv("DSV4L2_CLEARANCE");
    if (!env_clearance) {
        /* Default to UNCLASSIFIED if not set */
        cached_clearance = CLEARANCE_UNCLASSIFIED;
    } else {
        cached_clearance = get_clearance_from_classification(env_clearance);
    }

    cached = 1;
    return cached_clearance;
}

/**
 * Get minimum clearance required for a role
 */
static clearance_level_t get_role_clearance_requirement(const char *role)
{
    size_t i;

    if (!role) {
        return CLEARANCE_NONE;
    }

    for (i = 0; g_role_clearance_map[i].role != NULL; i++) {
        if (strcmp(g_role_clearance_map[i].role, role) == 0) {
            return g_role_clearance_map[i].min_clearance;
        }
    }

    /* Unknown roles default to UNCLASSIFIED */
    return CLEARANCE_UNCLASSIFIED;
}

/**
 * Check clearance level
 *
 * Verifies user has sufficient clearance for device role and classification
 *
 * @param role Device role
 * @param classification Required classification level
 * @return 0 if authorized, -EPERM if denied
 */
int dsv4l2_check_clearance(const char *role, const char *classification)
{
    clearance_level_t user_clearance;
    clearance_level_t required_clearance;
    clearance_level_t role_clearance;

    if (!role || !classification) {
        return -EINVAL;
    }

    /* Get user's clearance */
    user_clearance = get_user_clearance();

    /* Get classification requirement */
    required_clearance = get_clearance_from_classification(classification);

    /* Get role requirement */
    role_clearance = get_role_clearance_requirement(role);

    /* Use the higher of classification or role requirement */
    if (role_clearance > required_clearance) {
        required_clearance = role_clearance;
    }

    /* Check if user has sufficient clearance */
    if (user_clearance < required_clearance) {
        /* Access denied - insufficient clearance */
        return -EPERM;
    }

    return 0;
}

/**
 * Get THREATCON name (for display/logging)
 */
const char *dsv4l2_threatcon_name(dsmil_threatcon_t level)
{
    switch (level) {
        case THREATCON_NORMAL:    return "NORMAL";
        case THREATCON_ALPHA:     return "ALPHA";
        case THREATCON_BRAVO:     return "BRAVO";
        case THREATCON_CHARLIE:   return "CHARLIE";
        case THREATCON_DELTA:     return "DELTA";
        case THREATCON_EMERGENCY: return "EMERGENCY";
        default:                  return "UNKNOWN";
    }
}
