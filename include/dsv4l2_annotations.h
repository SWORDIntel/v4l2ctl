\
    #ifndef DSV4L2_ANNOTATIONS_H
    #define DSV4L2_ANNOTATIONS_H

    /*
     * DSLLVM annotation helpers for the dsv4l2 library.
     *
     * These macros wrap DSLLVM-specific attributes such as:
     *   - dsmil_secret
     *   - dsmil_secret_region
     *   - dsmil_meta
     *   - dsmil_tempest
     *   - dsmil_tempest_query
     *   - dsmil_tempest_transition
     *   - dsmil_requires_tempest_check
     *   - dsmil_quantum_candidate
     *
     * On non-DSLLVM compilers, they can be configured to expand to nothing.
     */

    #if defined(__has_attribute)
    #  if __has_attribute(dsmil_secret)
    #    define DSMIL_ATTR(x) __attribute__((x))
    #  else
    #    define DSMIL_ATTR(x)
    #  endif
    #else
    #  define DSMIL_ATTR(x)
    #endif

    #define DSMIL_SECRET(tag)                 DSMIL_ATTR(dsmil_secret(tag))
    #define DSMIL_SECRET_REGION               DSMIL_ATTR(dsmil_secret_region)
    #define DSMIL_META(tag)                   DSMIL_ATTR(dsmil_meta(tag))
    #define DSMIL_TEMPEST                     DSMIL_ATTR(dsmil_tempest)
    #define DSMIL_TEMPEST_QUERY               DSMIL_ATTR(dsmil_tempest_query)
    #define DSMIL_TEMPEST_TRANSITION          DSMIL_ATTR(dsmil_tempest_transition)
    #define DSMIL_REQUIRES_TEMPEST_CHECK      DSMIL_ATTR(dsmil_requires_tempest_check)
    #define DSMIL_QUANTUM_CANDIDATE(tag)      DSMIL_ATTR(dsmil_quantum_candidate(tag))

    #include <stddef.h>
    #include <stdint.h>

    typedef struct {
        uint8_t *data;
        size_t   len;
    } DSMIL_SECRET("biometric_frame") dsv4l2_frame_t;

    typedef struct {
        uint8_t *data;
        size_t   len;
    } DSMIL_META("radiometric") dsv4l2_meta_t;

    typedef enum DSMIL_TEMPEST {
        DSV4L2_TEMPEST_DISABLED = 0,
        DSV4L2_TEMPEST_LOW      = 1,
        DSV4L2_TEMPEST_HIGH     = 2,
        DSV4L2_TEMPEST_LOCKDOWN = 3,
    } dsv4l2_tempest_state_t;

    #endif /* DSV4L2_ANNOTATIONS_H */
