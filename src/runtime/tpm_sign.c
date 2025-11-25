/*
 * DSV4L2 Runtime - TPM2 Signing Support
 *
 * Provides TPM2-based cryptographic signing for event chunks
 * to ensure forensic integrity and non-repudiation.
 *
 * Requires: tpm2-tss library (libtss2-esys, libtss2-rc, libtss2-mu)
 */

#include "dsv4l2rt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_TPM2
#include <tss2/tss2_esys.h>
#include <tss2/tss2_rc.h>
#include <tss2/tss2_mu.h>
#include <openssl/sha.h>
#endif

/* TPM2 context (persistent across signing operations) */
static struct {
#ifdef HAVE_TPM2
    ESYS_CONTEXT *esys_ctx;
    ESYS_TR       sign_key_handle;
#endif
    int           initialized;
} tpm_ctx = { .initialized = 0 };

/**
 * Initialize TPM2 context and load signing key.
 *
 * @param key_handle TPM2 key handle (default: 0x81010001 for persistent key)
 * @return 0 on success, negative errno on failure
 */
int dsv4l2_tpm_init(uint32_t key_handle)
{
#ifdef HAVE_TPM2
    TSS2_RC rc;

    if (tpm_ctx.initialized) {
        return 0; /* Already initialized */
    }

    /* Initialize ESAPI context */
    rc = Esys_Initialize(&tpm_ctx.esys_ctx, NULL, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        fprintf(stderr, "TPM2: Esys_Initialize failed: 0x%x\n", rc);
        return -EIO;
    }

    /* Load persistent signing key */
    rc = Esys_TR_FromTPMPublic(tpm_ctx.esys_ctx,
                                key_handle,
                                ESYS_TR_NONE,
                                ESYS_TR_NONE,
                                ESYS_TR_NONE,
                                &tpm_ctx.sign_key_handle);
    if (rc != TSS2_RC_SUCCESS) {
        fprintf(stderr, "TPM2: Failed to load key 0x%x: 0x%x\n", key_handle, rc);
        Esys_Finalize(&tpm_ctx.esys_ctx);
        return -ENOENT;
    }

    tpm_ctx.initialized = 1;
    return 0;
#else
    (void)key_handle;
    fprintf(stderr, "TPM2: Not compiled with HAVE_TPM2 support\n");
    return -ENOSYS;
#endif
}

/**
 * Cleanup TPM2 context.
 */
void dsv4l2_tpm_cleanup(void)
{
#ifdef HAVE_TPM2
    if (tpm_ctx.initialized) {
        if (tpm_ctx.esys_ctx) {
            Esys_Finalize(&tpm_ctx.esys_ctx);
        }
        tpm_ctx.initialized = 0;
    }
#endif
}

/**
 * Sign event chunk with TPM2.
 *
 * @param events Array of events to sign
 * @param count Number of events
 * @param signature Output buffer for signature (must be 256 bytes)
 * @return 0 on success, negative errno on failure
 */
int dsv4l2_tpm_sign_events(const dsv4l2_event_t *events, size_t count, uint8_t signature[256])
{
#ifdef HAVE_TPM2
    TSS2_RC rc;
    TPM2B_DIGEST digest = { .size = SHA256_DIGEST_LENGTH };
    TPMT_SIGNATURE *sig_out = NULL;
    SHA256_CTX sha_ctx;
    size_t i;

    if (!tpm_ctx.initialized) {
        /* Auto-initialize with default key */
        if (dsv4l2_tpm_init(0x81010001) != 0) {
            return -EIO;
        }
    }

    if (!events || count == 0 || !signature) {
        return -EINVAL;
    }

    /* Compute SHA-256 hash of event data */
    SHA256_Init(&sha_ctx);
    for (i = 0; i < count; i++) {
        SHA256_Update(&sha_ctx, &events[i], sizeof(dsv4l2_event_t));
    }
    SHA256_Final(digest.buffer, &sha_ctx);

    /* Sign the hash with TPM2 */
    TPMT_TK_HASHCHECK validation = {
        .tag = TPM2_ST_HASHCHECK,
        .hierarchy = TPM2_RH_NULL
    };

    TPM2B_DATA qualifier = { .size = 0 };

    TPMT_SIG_SCHEME scheme = {
        .scheme = TPM2_ALG_RSASSA,
        .details.rsassa.hashAlg = TPM2_ALG_SHA256
    };

    rc = Esys_Sign(tpm_ctx.esys_ctx,
                   tpm_ctx.sign_key_handle,
                   ESYS_TR_PASSWORD,
                   ESYS_TR_NONE,
                   ESYS_TR_NONE,
                   &digest,
                   &scheme,
                   &validation,
                   &sig_out);

    if (rc != TSS2_RC_SUCCESS) {
        fprintf(stderr, "TPM2: Esys_Sign failed: 0x%x\n", rc);
        return -EIO;
    }

    /* Extract signature bytes */
    if (sig_out->signature.rsassa.sig.size > 256) {
        Esys_Free(sig_out);
        return -E2BIG;
    }

    memset(signature, 0, 256);
    memcpy(signature, sig_out->signature.rsassa.sig.buffer,
           sig_out->signature.rsassa.sig.size);

    Esys_Free(sig_out);
    return 0;
#else
    (void)events;
    (void)count;
    (void)signature;
    return -ENOSYS;
#endif
}

/**
 * Verify TPM2 signature (for forensic validation).
 *
 * @param events Array of events that were signed
 * @param count Number of events
 * @param signature Signature to verify
 * @return 0 if valid, -EBADMSG if invalid, other negative errno on error
 */
int dsv4l2_tpm_verify_signature(const dsv4l2_event_t *events, size_t count,
                                 const uint8_t signature[256])
{
#ifdef HAVE_TPM2
    TSS2_RC rc;
    TPM2B_DIGEST digest = { .size = SHA256_DIGEST_LENGTH };
    SHA256_CTX sha_ctx;
    size_t i;

    if (!tpm_ctx.initialized) {
        if (dsv4l2_tpm_init(0x81010001) != 0) {
            return -EIO;
        }
    }

    if (!events || count == 0 || !signature) {
        return -EINVAL;
    }

    /* Compute SHA-256 hash of event data */
    SHA256_Init(&sha_ctx);
    for (i = 0; i < count; i++) {
        SHA256_Update(&sha_ctx, &events[i], sizeof(dsv4l2_event_t));
    }
    SHA256_Final(digest.buffer, &sha_ctx);

    /* Prepare signature structure */
    TPMT_SIGNATURE tpm_sig = {
        .sigAlg = TPM2_ALG_RSASSA,
        .signature.rsassa = {
            .hash = TPM2_ALG_SHA256,
            .sig.size = 256
        }
    };
    memcpy(tpm_sig.signature.rsassa.sig.buffer, signature, 256);

    /* Verify signature with TPM2 */
    TPMT_TK_VERIFIED *validation = NULL;

    rc = Esys_VerifySignature(tpm_ctx.esys_ctx,
                               tpm_ctx.sign_key_handle,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               ESYS_TR_NONE,
                               &digest,
                               &tpm_sig,
                               &validation);

    if (rc != TSS2_RC_SUCCESS) {
        if (rc == TPM2_RC_SIGNATURE) {
            return -EBADMSG; /* Invalid signature */
        }
        fprintf(stderr, "TPM2: VerifySignature failed: 0x%x\n", rc);
        return -EIO;
    }

    Esys_Free(validation);
    return 0; /* Signature valid */
#else
    (void)events;
    (void)count;
    (void)signature;
    return -ENOSYS;
#endif
}
