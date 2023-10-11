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

#ifndef LIB_VFIO_USER_PCI_CAPS_PRI_H
#define LIB_VFIO_USER_PCI_CAPS_PRI_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pricap_control {
    unsigned int enable:1;
    unsigned int reset:1;
    unsigned int reserved:14;
} __attribute__ ((packed));
_Static_assert(sizeof(struct pricap_control) == 2, "bad pricap_control size");

struct pricap_status {
    unsigned int response_failure:1;
    unsigned int unexpected_group_index:1;
    unsigned int reserved1:6;
    unsigned int stopped:1;
    unsigned int reserved2:6;
    unsigned int prgr_pasid_required:1;
} __attribute__ ((packed));
_Static_assert(sizeof(struct pricap_status) == 2, "bad pricap_status size");

#define VFIO_USER_PCI_EXT_CAP_PRI_SIZEOF (16)

struct pricap {
    struct pcie_ext_cap_hdr hdr;
    struct pricap_control control;
    struct pricap_status status;
    uint32_t capacity;
    uint32_t allocation;
}  __attribute__ ((packed));
_Static_assert(sizeof(struct pricap) == VFIO_USER_PCI_EXT_CAP_PRI_SIZEOF,
               "bad PRI cap size");

#ifdef __cplusplus
}
#endif

#endif /* LIB_VFIO_USER_PCI_CAPS_PRI_H */

/* ex: set tabstop=4 shiftwidth=4 softtabstop=4 expandtab: */
