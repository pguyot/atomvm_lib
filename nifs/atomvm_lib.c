//
// Copyright (c) 2020 dushin.net
// All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include <stdlib.h>

#include <atomvm_lib.h>
#include <esp_log.h>
#include <esp_attr.h>
#include <context.h>
#include <defaultatoms.h>
#include <errno.h>
#include <esp32_sys.h>
#include <interop.h>
#include <nifs.h>
#include <term.h>
#include <memory.h>
#include <esp_system.h>
#include <mbedtls/sha1.h>
#include <sys/time.h>

// #define ENABLE_TRACE
#include "trace.h"

#define TAG "atomvm_lib"

RTC_DATA_ATTR size_t data_len = 0;
RTC_DATA_ATTR char *data[CONFIG_RTC_MEMORY_SIZE];

static term nif_set_rtc_memory(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    term binary = argv[0];
    VALIDATE_VALUE(binary, term_is_binary);

    size_t binary_len = term_binary_size(binary);
    if (CONFIG_RTC_MEMORY_SIZE < binary_len) {
        RAISE_ERROR(BADARG_ATOM);
    }
    data_len = binary_len;
    memcpy(data, term_binary_data(binary), binary_len);

    return OK_ATOM;
}

static term nif_get_rtc_memory(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    if (UNLIKELY(memory_ensure_free(ctx, term_binary_heap_size(data_len)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }

    return term_from_literal_binary(data, data_len, &ctx->heap, ctx->global);
}

#define MAC_LENGTH 6

static term nif_get_mac(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    uint8_t mac[MAC_LENGTH];
    esp_efuse_mac_get_default(mac);

    if (UNLIKELY(memory_ensure_free(ctx, term_binary_heap_size(2 * MAC_LENGTH)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    char buf[2 * MAC_LENGTH + 1];
    snprintf(buf, 2 * MAC_LENGTH + 1,
        "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return term_from_literal_binary(buf, 2 * MAC_LENGTH, &ctx->heap, ctx->global);
}

#define SHA1_LEN 20

static term nif_sha1(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);
    term binary = argv[0];
    VALIDATE_VALUE(binary, term_is_binary);

    if (UNLIKELY(memory_ensure_free(ctx, term_binary_heap_size(SHA1_LEN)) != MEMORY_GC_OK)) {
        RAISE_ERROR(OUT_OF_MEMORY_ATOM);
    }
    term ret = term_create_uninitialized_binary(SHA1_LEN, &ctx->heap, ctx->global);
    binary = argv[0];

    int res = mbedtls_sha1_ret((const unsigned char *) term_binary_data(binary), term_binary_size(binary), (unsigned char *) term_binary_data(ret));
    if (res != 0) {
        RAISE_ERROR(BADARG_ATOM);
    }

    return ret;
}

static term nif_set_time_of_day(Context *ctx, int argc, term argv[])
{
    UNUSED(argc);

    VALIDATE_VALUE(argv[0], term_is_any_integer);

    avm_int64_t ms_since_unix_epoch = term_maybe_unbox_int64(argv[0]);

    TRACE("ms_since_unix_epoch: %lli\n", ms_since_unix_epoch);

    struct timeval tp = {
        .tv_sec = ms_since_unix_epoch / 1000,
        .tv_usec = (ms_since_unix_epoch % 1000) * 1000
    };
    struct timezone tz = {
        .tz_minuteswest = 0,
        .tz_dsttime = 0
    };
    int res = settimeofday(&tp, &tz);
    if (res != 0) {
        if (UNLIKELY(memory_ensure_free(ctx, TUPLE_SIZE(2)) != MEMORY_GC_OK)) {
            RAISE_ERROR(OUT_OF_MEMORY_ATOM);
        }
        term error = term_alloc_tuple(2, &ctx->heap);
        term_put_tuple_element(error, 0, ERROR_ATOM);
        term_put_tuple_element(error, 1, term_from_int(errno));
        return RAISE_ERROR(error);
    } else {
        return OK_ATOM;
    }
}

static const struct Nif set_rtc_memory_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_set_rtc_memory
};
static const struct Nif get_rtc_memory_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_get_rtc_memory
};
static const struct Nif get_mac_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_get_mac
};
static const struct Nif sha1_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_sha1
};
static const struct Nif set_time_of_day_nif =
{
    .base.type = NIFFunctionType,
    .nif_ptr = nif_set_time_of_day
};


//
// Component Nif Entrypoints
//

void atomvm_lib_init(GlobalContext *global)
{
    // no-op
}

const struct Nif *atomvm_lib_get_nif(const char *nifname)
{
    TRACE("Locating nif %s ...", nifname);
    if (strcmp("atomvm_lib:set_rtc_memory/1", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &set_rtc_memory_nif;
    }
    if (strcmp("atomvm_lib:get_rtc_memory/0", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &get_rtc_memory_nif;
    }
    if (strcmp("atomvm_lib:get_mac/0", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &get_mac_nif;
    }
    if (strcmp("atomvm_lib:sha1/1", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &sha1_nif;
    }
    if (strcmp("atomvm_lib:set_time_of_day/1", nifname) == 0) {
        TRACE("Resolved platform nif %s ...\n", nifname);
        return &set_time_of_day_nif;
    }
    return NULL;
}

#include <sdkconfig.h>
#ifdef CONFIG_AVM_LIB_ENABLE
REGISTER_NIF_COLLECTION(atomvm_lib, atomvm_lib_init, NULL, atomvm_lib_get_nif)
#endif
