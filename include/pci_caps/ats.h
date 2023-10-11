/*
 * Copyright (c) 2019 Nutanix Inc. All rights reserved.
 * Copyright (c) 2023 Rivos Inc.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of Nutanix nor the names of its contributors may be
 *        used to endorse or promote products derived from this software without
 *        specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 */

#ifndef LIB_VFIO_USER_PCI_CAPS_ATS_H
#define LIB_VFIO_USER_PCI_CAPS_ATS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct atscap_capability {
    unsigned int invalidate_queue_depth:5;
    unsigned int page_aligned_request:1;
    unsigned int global_invalidate_supported:1;
    unsigned int relaxed_ordering_supported:1;
    unsigned int ats_memory_attributes_supported:1;
    unsigned int reserved:7;
} __attribute__ ((packed));
_Static_assert(sizeof(struct atscap_capability) == 2, "bad ats_capability "
                                                      "size");

struct atscap_control {
    unsigned int smallest_translation_unit:5;
    unsigned int reserved:6;
    unsigned int ats_memory_attributes_default:3;
    unsigned int ats_memory_attributes_enable:1;
    unsigned int enable:1;
} __attribute__ ((packed));
_Static_assert(sizeof(struct atscap_control) == 2, "bad ats_control size");

#define VFIO_USER_PCI_EXT_CAP_ATS_SIZEOF (8)

struct atscap {
    struct pcie_ext_cap_hdr hdr;
    struct atscap_capability capability;
    struct atscap_control control;
}  __attribute__ ((packed));
_Static_assert(sizeof(struct atscap) == VFIO_USER_PCI_EXT_CAP_ATS_SIZEOF,
               "bad ATS cap size");

#ifdef __cplusplus
}
#endif

#endif /* LIB_VFIO_USER_PCI_CAPS_ATS_H */

/* ex: set tabstop=4 shiftwidth=4 softtabstop=4 expandtab: */
