/*
 * pongoOS - https://checkra.in
 *
 * Copyright (C) 2019-2021 checkra1n team
 *
 * This file is part of pongoOS.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <pongo.h>
extern dt_node_t *gDeviceTree;
extern uint64_t gIOBase;
#include <libfdt.h>
#include <lzma/lzmadec.h>

void *fdt = NULL;
bool fdt_initialized = false;
void *ramdisk = NULL;
uint32_t prev_ramdisk_size = 0;
bool ramdisk_initialized = false;

char gLinuxCmdLine[LINUX_CMDLINE_SIZE] = {0};

void linux_dtree_init(void)
{
    if (fdt_initialized)
        free(fdt);
    fdt = malloc(LINUX_DTREE_SIZE);
}

int linux_dtree_overlay(char *boot_args)
{
    int node = 0, node1 = 0, ret = 0;
    char fdt_nodename[64], *key;
    uint64_t fb_size;
    uint32_t width;

    node = fdt_path_offset(fdt, "/chosen");
    if (node < 0)
    {
        iprintf("Failed to find /chosen");
        return -1;
    }

    if (ramdisk != NULL)
    {
        void *rd_start = (void *)(vatophys_static(ramdisk));
        void *rd_end = (void *)((((uint64_t)rd_start) + ramdisk_size + 7ull) & -8ull);

        int ret = fdt_setprop_u64(fdt, node, "linux,initrd-start", (uint64_t) rd_start);
        if (ret < 0) 
        {
            iprintf("Cannot update chosen node [linux,initrd-start]\n");
            return -1;
        }

        ret = fdt_setprop_u64(fdt, node, "linux,initrd-end", (uint64_t) rd_end);
        if (ret < 0) 
        {
            iprintf("Cannot update chosen node [linux,initrd-end]\n");
            return -1;
        }

        iprintf("initrd @ %p-%p\n", rd_start, rd_end);
    }

    if (boot_args)
    {
        ret = fdt_delprop(fdt, node, "bootargs");
        if (ret < 0 && ret != -FDT_ERR_NOTFOUND)
        {
            iprintf("Failed to delete bootargs: %d", ret);
            return -1;
        }

        fdt_appendprop_string(fdt, node, "bootargs", boot_args);
    }

    /* Simple framebuffer node */
    /* Some devices are really stupid */
    key = dt_get_prop("device-tree", "target-type", NULL);
    if (!strcmp(key, "N61") || // 6
        !strcmp(key, "N71") || !strcmp(key, "N71m") || // 6S
        !strcmp(key, "D10") || !strcmp(key, "D101") || // 7
        !strcmp(key, "D20") || !strcmp(key, "D201")) // 8
        width = gBootArgs->Video.v_width + 2;
    else if (!strcmp(key, "N56")  || // 6 Plus
             !strcmp(key, "N66")  || !strcmp(key, "N66m") || // 6S Plus
             !strcmp(key, "D11")  || !strcmp(key, "D111")) // 7 Plus
        width = gBootArgs->Video.v_width + 8;
    else if (!strcmp(key, "D22")  || !strcmp(key, "D221")) // X
        width = gBootArgs->Video.v_width + 11;
    else if (!strcmp(key, "J207") || !strcmp(key, "J208")) // iPad Pro (10.5-inch)
        width = gBootArgs->Video.v_width + 12;
    else
        width = gBootArgs->Video.v_width;

    fb_size = gBootArgs->Video.v_height * width * 4;

    siprintf(fdt_nodename, "/framebuffer@%lx", gBootArgs->Video.v_baseAddr);
    node1 = fdt_add_subnode(fdt, node, fdt_nodename);
    if (node < 0)
    {
        iprintf("Failed to add framebuffer node");
        return -1;
    }

    fdt_appendprop_addrrange(fdt, node, node1, "reg", gBootArgs->Video.v_baseAddr, fb_size);
    fdt_appendprop_cell(fdt, node1, "width", width);
    fdt_appendprop_cell(fdt, node1, "height", gBootArgs->Video.v_height);
    fdt_appendprop_cell(fdt, node1, "stride", width * 4);
    if ((gBootArgs->Video.v_depth & 0xff) == 30) // X is *very* special
        fdt_appendprop_string(fdt, node1, "format", "x2r10g10b10");
    else
        fdt_appendprop_string(fdt, node1, "format", "a8b8g8r8");
    fdt_appendprop_string(fdt, node1, "compatible", "simple-framebuffer");

    /* Reserved memory */
    node = fdt_path_offset(fdt, "/reserved-memory");
    if (node < 0)
    {
        iprintf("Failed to find /reserved-memory");
        return -1;
    }

    /* Reserve the framebuffer (so that Linux doesn't overwrite it) */
    siprintf(fdt_nodename, "/memory@%lx", gBootArgs->Video.v_baseAddr);
    node1 = fdt_add_subnode(fdt, node, fdt_nodename);
    if (node < 0)
    {
        iprintf("Failed to reserve framebuffer region");
        return -1;
    }
    fdt_appendprop_addrrange(fdt, 0, node1, "reg", gBootArgs->Video.v_baseAddr, fb_size);
    fdt_appendprop(fdt, node1, "no-map", "", 0);

    return 0;
}

bool linux_can_boot()
{
    if (!loader_xfer_recv_count)
        return false;
    return true;
}

void *gLinuxStage;
uint32_t gLinuxStageSize;

void fdt_select_dtree(void *fdt)
{
    unsigned char *buf = fdt, *ebuf;
    char *key;
    unsigned len;

    if (memcmp(buf, "Cows", 4)) // pack signature
        return;
    buf += 4;

    key = dt_get_prop("device-tree", "target-type", NULL);
    if (!key)
        return;

    while (buf[0])
    {
        ebuf = buf + strlen((char *)buf) + 1;
        len = ebuf[0];
        len = (len << 8) | ebuf[1];
        len = (len << 8) | ebuf[2];
        len = (len << 8) | ebuf[3];
        ebuf += 4;
        if (!strcmp(key, (char *)buf))
        {
            iprintf("Found device tree for %s (%d bytes).\n", key, len);
            memmove(fdt, ebuf, len);
            return;
        }
        buf = ebuf + len;
    }

    iprintf("Device tree for %s not found.\n", key);
}

void linux_prep_boot()
{
    if (!ramdisk_initialized)
    {
        ramdisk_initialized = true;

        if (ramdisk != NULL) {
            free_contig(ramdisk, prev_ramdisk_size);
            ramdisk = NULL;
            prev_ramdisk_size = 0;
        }

        if (ramdisk_size != 0) {
            // should not return NULL, since pongo panics on OOM.
            ramdisk = alloc_contig(ramdisk_size);
            prev_ramdisk_size = ramdisk_size;

            memcpy(ramdisk, ramdisk_buf, (size_t) ramdisk_size);
        }
    }

    // invoked in sched task with MMU on
    if (!fdt_initialized)
    {
        linux_dtree_init();
    }
    else
    {
        fdt_select_dtree(fdt);
        if (fdt_open_into((void *)fdt, (void *)fdt, LINUX_DTREE_SIZE))
        {
            iprintf("failed to apply overlay to fdt\n");
            return;
        }

        iprintf("Kernel command line: %s\n", gLinuxCmdLine);
        if (linux_dtree_overlay(gLinuxCmdLine) < 0)
        {
            panic("Booting Linux failed!");
        }
    }

#define pixfmt0 (&disp[0x402c / 4])
#define colormatrix_bypass (&disp[0x40b4 / 4])
#define colormatrix_mul_31 (&disp[0x40cc / 4])
#define colormatrix_mul_32 (&disp[0x40d4 / 4])
#define colormatrix_mul_33 (&disp[0x40dc / 4])

    volatile uint32_t *disp = ((uint32_t *)(dt_get_u32_prop("disp0", "reg") + gIOBase));

    *pixfmt0 = (*pixfmt0 & 0xF00FFFFFu) | 0x05200000u;

    *colormatrix_bypass = 0;
    *colormatrix_mul_31 = 4095;
    *colormatrix_mul_32 = 4095;
    *colormatrix_mul_33 = 4095;


    // BIG HACK BIG HACK BIG HACK we'll clean it up.. eventually :P
    gEntryPoint = (void *)(0x803000000);
    uint64_t image_size = loader_xfer_recv_count;
    gLinuxStage = (void *)alloc_contig(image_size + LINUX_DTREE_SIZE);
    size_t dest_size = 0x10000000;
    int res = unlzma_decompress((uint8_t *)gLinuxStage, &dest_size, loader_xfer_recv_data, image_size);
    if (res != SZ_OK)
    {
        puts("Assuming decompressed kernel.");
        image_size = *(uint64_t *)(loader_xfer_recv_data + 16);
        memcpy(gLinuxStage, loader_xfer_recv_data, image_size);
    }
    else
    {
        image_size = *(uint64_t *)(gLinuxStage + 16);
    }
    void *gLinuxDtre = (void *)((((uint64_t)gLinuxStage) + image_size + 7ull) & -8ull);
    memcpy(gLinuxDtre, fdt, LINUX_DTREE_SIZE);
    gLinuxStageSize = image_size + LINUX_DTREE_SIZE;

    gBootArgs = (void *)((((uint64_t)gEntryPoint) + image_size + 7ull) & -8ull);
    iprintf("Booting Linux: %p(%p)\n", gEntryPoint, gBootArgs);
    gLinuxStage = (void *)(((uint64_t)gLinuxStage) - kCacheableView + 0x800000000);
}

void linux_boot()
{
    memcpy(gEntryPoint, gLinuxStage, gLinuxStageSize);
}
