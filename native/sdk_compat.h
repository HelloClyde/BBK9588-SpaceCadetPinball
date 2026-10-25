/* SPDX-License-Identifier: Apache-2.0
 * BBK 9588 Native BDA SDK, Copyright 2026 HelloClyde.
 * The sole SDK addition used by this application, retained locally so the
 * published SDK commit remains sufficient. See licenses/Apache-2.0.txt.
 * Evidence: SDK docs/verified/fs_flush_api.md in the development workspace;
 * the same call was used by the already verified pinball build.
 */
#ifndef SCP_SDK_COMPAT_H
#define SCP_SDK_COMPAT_H
#include "bda_filesystem.h"
#ifndef BDA_SDK_INTERNAL_FS_FLUSH_ALL
static inline void bda_fs_flush_all(void) {
    typedef int (*fn_t)(void);
    fn_t fn = (fn_t)bda_sdk_internal_api(bda_sdk_internal_fs(), 0x074u);
    (void)fn();
}
#endif
#endif
