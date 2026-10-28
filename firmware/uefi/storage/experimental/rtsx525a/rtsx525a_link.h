/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef IFS_RTSX525A_LINK_H
#define IFS_RTSX525A_LINK_H

#include <efi.h>

/* Best-effort, volatile PCIe ASPM workaround for the exact Dell/RTS525A
 * hardware. EFI_NOT_FOUND means unrelated hardware; OFS otherwise
 * continues with the standard firmware block transport. */
EFI_STATUS ifs_rtsx525a_disable_aspm(void);

#endif
