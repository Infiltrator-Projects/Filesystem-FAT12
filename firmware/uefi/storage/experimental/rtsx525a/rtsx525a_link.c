/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Narrow Dell Latitude 5550 / RTS525A firmware transport workaround.
 *
 * Only the observed 10ec:525a / 1028:0cb9 PCIe SD endpoint and its
 * Intel upstream bridge are eligible. Do not touch other PCIe links.
 * Modify volatile PCI configuration only (no BIOS settings or NVRAM).
 *
 * PCIe ASPM L1 is active on both link partners in the Dell diagnostic.
 * The OFS firmware BlockIo path intermittently times out for 30 seconds
 * while Linux rtsx_pci reads the same SD sectors normally. Disable
 * ASPM on this exact link during firmware boot as a controlled hardware
 * compatibility experiment. This does not bypass BlockIo or alter OFS.
 */
#include <efi.h>
#include <efilib.h>
#include <efipciio.h>
#include "rtsx525a_link.h"

#define DELL_RTS525A_PCI_ID    0x525a10ecU
#define DELL_RTS525A_SUBSYS_ID 0x0cb91028U
#define DELL_INTEL_ROOT_ID     0x7e398086U
#define PCI_SUBSYSTEM          0x2cU
#define PCI_SECONDARY_BUS      0x19U
#define PCI_CLASS_SUBCLASS     0x0aU
#define PCI_STATUS             0x06U
#define PCI_STATUS_CAP_LIST    0x0010U
#define PCI_CAP_POINTER        0x34U
#define PCI_CAP_ID_EXPRESS     0x10U
#define PCI_EXP_LNKCTL         0x10U
#define PCI_EXP_ASPM_MASK      0x0003U

/* EFI_PCI_IO_PROTOCOL is a type name in GNU-EFI 3.0.15, not a GUID. */
static EFI_GUID pci_io_guid = {
    0x4cf5b200U, 0x68b8U, 0x4ca5U,
    {0x9eU, 0xecU, 0xb2U, 0x3eU, 0x3fU, 0x50U, 0x02U, 0x9aU}
};

static EFI_STATUS pci_get8(EFI_PCI_IO *pci, UINT32 address, UINT8 *value)
{
    return pci->Pci.Read(pci, EfiPciIoWidthUint8, address, 1U, value);
}

static EFI_STATUS pci_get16(EFI_PCI_IO *pci, UINT32 address, UINT16 *value)
{
    return pci->Pci.Read(pci, EfiPciIoWidthUint16, address, 1U, value);
}

static EFI_STATUS pci_get32(EFI_PCI_IO *pci, UINT32 address, UINT32 *value)
{
    return pci->Pci.Read(pci, EfiPciIoWidthUint32, address, 1U, value);
}

static EFI_STATUS pci_put16(EFI_PCI_IO *pci, UINT32 address, UINT16 value)
{
    return pci->Pci.Write(pci, EfiPciIoWidthUint16, address, 1U, &value);
}

/* Walk standard capabilities only; every offset must be within config
 * header space. PCIe Link Control is at Express capability + 0x10. */
static UINT32 pcie_link_control_offset(EFI_PCI_IO *pci)
{
    UINT16 status = 0U;
    UINT8 capability = 0U;
    UINTN hops;

    if (EFI_ERROR(pci_get16(pci, PCI_STATUS, &status)) ||
        !(status & PCI_STATUS_CAP_LIST) ||
        EFI_ERROR(pci_get8(pci, PCI_CAP_POINTER, &capability)))
        return 0U;

    for (hops = 0U; hops < 48U; ++hops) {
        UINT8 kind = 0U, next = 0U;
        if (capability < 0x40U || capability > 0xe0U ||
            (capability & 3U) != 0U ||
            EFI_ERROR(pci_get8(pci, (UINT32)capability, &kind)) ||
            EFI_ERROR(pci_get8(pci, (UINT32)capability + 1U, &next)))
            break;
        if (kind == PCI_CAP_ID_EXPRESS)
            return (UINT32)capability + PCI_EXP_LNKCTL;
        if (!next || next == capability)
            break;
        capability = next;
    }
    return 0U;
}

/* Write no more than the ASPM control bits; verify the resulting register. */
static EFI_STATUS disable_aspm(EFI_PCI_IO *pci, UINT32 address,
                               UINT16 original)
{
    UINT16 check;
    UINT16 adjusted = (UINT16)(original & (UINT16)~PCI_EXP_ASPM_MASK);
    EFI_STATUS status;

    if (adjusted == original)
        return EFI_SUCCESS;
    status = pci_put16(pci, address, adjusted);
    if (EFI_ERROR(status))
        return status;
    status = pci_get16(pci, address, &check);
    if (EFI_ERROR(status) || check != adjusted)
        return EFI_DEVICE_ERROR;
    return EFI_SUCCESS;
}

/* A firmware PCI enumeration can differ from Linux's bus numbering:
 * match the actual parent via its secondary bus and PCI segment, not
 * hard-coded 57:00.0 or 00:1c.0 BDF addresses. */
EFI_STATUS ifs_rtsx525a_disable_aspm(void)
{
    EFI_HANDLE *handles = NULL;
    EFI_PCI_IO *endpoint = NULL, *bridge = NULL;
    UINTN count = 0U, i;
    UINTN endpoint_segment = 0U, endpoint_bus = 0U;
    UINTN endpoint_count = 0U, bridge_count = 0U;
    UINT32 endpoint_offset, bridge_offset;
    UINT16 endpoint_ctl, bridge_ctl;
    EFI_STATUS status;

    status = BS->LocateHandleBuffer(ByProtocol, &pci_io_guid, NULL,
                                    &count, &handles);
    if (EFI_ERROR(status))
        return EFI_NOT_FOUND;

    /* Identify the exact Realtek endpoint and Dell subsystem first. */
    for (i = 0U; i < count; ++i) {
        EFI_PCI_IO *pci = NULL;
        UINT32 device_id = 0U, subsystem_id = 0U;
        UINTN seg = 0U, bus = 0U, device = 0U, function = 0U;
        if (EFI_ERROR(BS->HandleProtocol(handles[i], &pci_io_guid,
                                         (VOID **)&pci)) || !pci ||
            EFI_ERROR(pci_get32(pci, 0U, &device_id)) ||
            device_id != DELL_RTS525A_PCI_ID ||
            EFI_ERROR(pci_get32(pci, PCI_SUBSYSTEM, &subsystem_id)) ||
            subsystem_id != DELL_RTS525A_SUBSYS_ID ||
            EFI_ERROR(pci->GetLocation(pci, &seg, &bus, &device, &function)))
            continue;
        endpoint = pci;
        endpoint_segment = seg;
        endpoint_bus = bus;
        ++endpoint_count;
    }
    if (endpoint_count != 1U) {
        status = endpoint_count ? EFI_UNSUPPORTED : EFI_NOT_FOUND;
        goto done;
    }

    /* Select exactly one observed Intel root port serving that endpoint. */
    for (i = 0U; i < count; ++i) {
        EFI_PCI_IO *pci = NULL;
        UINT32 device_id = 0U;
        UINT16 class_code = 0U;
        UINT8 secondary = 0U;
        UINTN seg = 0U, bus = 0U, device = 0U, function = 0U;
        if (EFI_ERROR(BS->HandleProtocol(handles[i], &pci_io_guid,
                                         (VOID **)&pci)) || !pci ||
            EFI_ERROR(pci_get32(pci, 0U, &device_id)) ||
            device_id != DELL_INTEL_ROOT_ID ||
            EFI_ERROR(pci_get16(pci, PCI_CLASS_SUBCLASS, &class_code)) ||
            class_code != 0x0604U ||
            EFI_ERROR(pci_get8(pci, PCI_SECONDARY_BUS, &secondary)) ||
            EFI_ERROR(pci->GetLocation(pci, &seg, &bus, &device, &function)) ||
            seg != endpoint_segment || bus == endpoint_bus ||
            (UINTN)secondary != endpoint_bus)
            continue;
        bridge = pci;
        ++bridge_count;
    }
    if (bridge_count != 1U) {
        status = EFI_UNSUPPORTED;
        goto done;
    }

    endpoint_offset = pcie_link_control_offset(endpoint);
    bridge_offset = pcie_link_control_offset(bridge);
    if (!endpoint_offset || !bridge_offset ||
        EFI_ERROR(pci_get16(endpoint, endpoint_offset, &endpoint_ctl)) ||
        EFI_ERROR(pci_get16(bridge, bridge_offset, &bridge_ctl))) {
        status = EFI_UNSUPPORTED;
        goto done;
    }

    /* PCIe requires disabling L1 at the upstream component (SD
     * endpoint) before the downstream component (root port).
     * Do not touch L1 substate timing or any other configuration bits. */
    status = disable_aspm(endpoint, endpoint_offset, endpoint_ctl);
    if (EFI_ERROR(status)) {
        (void)pci_put16(endpoint, endpoint_offset, endpoint_ctl);
        goto done;
    }
    status = disable_aspm(bridge, bridge_offset, bridge_ctl);
    if (EFI_ERROR(status)) {
        /* Do not leave half of the deliberately modified pair behind. */
        (void)pci_put16(bridge, bridge_offset, bridge_ctl);
        (void)pci_put16(endpoint, endpoint_offset, endpoint_ctl);
        goto done;
    }
    status = EFI_SUCCESS;

done:
    BS->FreePool(handles);
    return status;
}
