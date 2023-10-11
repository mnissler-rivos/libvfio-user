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

#ifndef LIB_VFIO_USER_PCI_CAPS_PASID_H
#define LIB_VFIO_USER_PCI_CAPS_PASID_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pasidcap_capability {
    unsigned int reserved1:1;
    unsigned int execute_permission_supported:1;
    unsigned int privileged_mode_supported:1;
    unsigned int translate_with_pasid_supported:1;
    unsigned int reserved2:4;
    unsigned int max_pasid_width:5;
    unsigned int reserved3:3;
} __attribute__ ((packed));
_Static_assert(sizeof(struct pasidcap_capability) == 2, "bad pasid_capability "
                                                        "size");

struct pasidcap_control {
    unsigned int enable:1;
    unsigned int execute_permission_enable:1;
    unsigned int privileged_mode_enable:1;
    unsigned int translate_with_pasid_enable:1;
    unsigned int reserved:12;
} __attribute__ ((packed));
_Static_assert(sizeof(struct pasidcap_control) == 2, "bad pasid_control size");

#define VFIO_USER_PCI_EXT_CAP_PASID_SIZEOF (8)

struct pasidcap {
    struct pcie_ext_cap_hdr hdr;
    struct pasidcap_capability capability;
    struct pasidcap_control control;
}  __attribute__ ((packed));
_Static_assert(sizeof(struct pasidcap) == VFIO_USER_PCI_EXT_CAP_PASID_SIZEOF,
               "bad PASID cap size");

#ifdef __cplusplus
}
#endif

#endif /* LIB_VFIO_USER_PCI_CAPS_PASID_H */

/* ex: set tabstop=4 shiftwidth=4 softtabstop=4 expandtab: */
