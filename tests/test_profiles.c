/*
 * DSV4L2 Profile Loading Test
 *
 * Tests YAML profile loading and matching
 */

#include "dsv4l2_profiles.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    size_t count, i;
    const dsv4l2_device_profile_t *profile;

    printf("DSV4L2 Profile Loading Test\n");
    printf("============================\n\n");

    /* Test 1: Get total profile count */
    printf("Test 1: Profile count\n");
    count = dsv4l2_get_profile_count();
    printf("  Loaded %zu profile(s)\n\n", count);

    /* Test 2: List all profiles */
    printf("Test 2: List all profiles\n");
    for (i = 0; i < count; i++) {
        profile = dsv4l2_get_profile(i);
        if (profile) {
            printf("  Profile %zu:\n", i + 1);
            printf("    ID:             %s\n", profile->id);
            printf("    Vendor:         %s\n", profile->vendor);
            printf("    Model:          %s\n", profile->model);
            printf("    Role:           %s\n", profile->role);
            printf("    Classification: %s\n", profile->classification);
            printf("    Layer:          L%u\n", profile->layer);
            printf("    TEMPEST Ctrl:   0x%x\n", profile->tempest_ctrl_id);
            if (profile->width && profile->height) {
                printf("    Resolution:     %ux%u@%u\n",
                       profile->width, profile->height, profile->fps);
            }
            printf("    File:           %s\n", profile->filename);
            printf("\n");
        }
    }

    /* Test 3: Find profile by role */
    printf("Test 3: Find profile by role\n");
    profile = dsv4l2_find_profile_by_role("iris_scanner");
    if (profile) {
        printf("  Found iris_scanner profile:\n");
        printf("    ID:    %s\n", profile->id);
        printf("    Model: %s\n", profile->model);
        printf("    Class: %s\n", profile->classification);
    } else {
        printf("  iris_scanner profile not found\n");
    }
    printf("\n");

    /* Test 4: Find profile by ID */
    printf("Test 4: Find profile by ID\n");
    profile = dsv4l2_find_profile("046d:0825");
    if (profile) {
        printf("  Found profile 046d:0825:\n");
        printf("    Role:  %s\n", profile->role);
        printf("    Model: %s\n", profile->model);
    } else {
        printf("  Profile 046d:0825 not found\n");
    }
    printf("\n");

    printf("All profile tests completed!\n");

    return 0;
}
