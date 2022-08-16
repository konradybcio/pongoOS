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
#include <stdlib.h>
#include <pongo.h>
struct task* command_task;
char command_buffer[0x200];
int command_buffer_idx = 0;

struct command {
    const char* name;
    const char* desc;
    void (*cb)(const char* cmd, char* args);
} commands[64];
char is_masking_autoboot;
static lock command_lock;

static int cmp_cmd(const void *a, const void *b)
{
    const struct command *x = a, *y = b;
    if(!x->name && !y->name) return 0;
    if(!x->name) return 1;
    if(!y->name) return -1;
    return strcmp(x->name, y->name);
}

void command_unregister(const char* name) {
    lock_take(&command_lock);
    for (int i=0; i<64; i++) {
        if (commands[i].name && strcmp(commands[i].name, name) == 0) {
            commands[i].name = 0;
            commands[i].desc = 0;
            commands[i].cb = 0;
        }
    }
    qsort(commands, 64, sizeof(struct command), &cmp_cmd);
    lock_release(&command_lock);
}
void command_register(const char* name, const char* desc, void (*cb)(const char* cmd, char* args)) {
    if (is_masking_autoboot && strcmp(name,"autoboot") == 0) return;
    lock_take(&command_lock);
    for (int i=0; i<64; i++) {
        if (!commands[i].name || strcmp(commands[i].name, name) == 0) {
            commands[i].name = name;
            commands[i].desc = desc;
            commands[i].cb = cb;
            qsort(commands, 64, sizeof(struct command), &cmp_cmd);
            lock_release(&command_lock);
            return;
        }
    }
    lock_release(&command_lock);
    panic("too many commands");
}

char* command_tokenize(char* str, uint32_t strbufsz) {
    char* bound = &str[strbufsz];
    while (*str) {
        if (str > bound) return NULL;
        if (*str == ' ') {
            *str++ = 0;
            while (*str) {
                if (str > bound) return NULL;
                if (*str == ' ') {
                    str++;
                } else
                    break;
            }
            if (str > bound) return NULL;
            if (!*str) return "";
            return str;
        }
        str++;
    }
    return "";
}

char is_executing_command;
uint32_t command_flags;
#define COMMAND_NOTFOUND 1
void command_execute(char* cmd) {
    char* arguments = command_tokenize(cmd, 0x1ff);
    if (arguments) {
        lock_take(&command_lock);
        for (int i=0; i<64; i++) {
            if (commands[i].name && !strcmp(cmd, commands[i].name)) {
                void (*cb)(const char* cmd, char* args) = commands[i].cb;
                lock_release(&command_lock);
                cb(command_buffer, arguments);
                return;
            }
        }
        lock_release(&command_lock);
    }
    if(cmd[0] != '\0')
    {
        iprintf("Bad command: %s\n", cmd);
    }
    if (*cmd)
        command_flags |= COMMAND_NOTFOUND;
}

extern uint32_t uart_should_drop_rx;
char command_handler_ready = 0;
volatile uint8_t command_in_progress = 0;
struct event command_handler_iter;

static inline void put_serial_modifier(const char* str) {
    while (*str) serial_putc(*str++);
}

void command_main() {
    while (1) {
        if (!uart_should_drop_rx) {
            fflush(stdout);
            putchar('\r');
            if (command_flags & COMMAND_NOTFOUND) {
                put_serial_modifier("\x1b[31m");
            }
            iprintf("pongoOS> ");
            fflush(stdout);
            if (command_flags & COMMAND_NOTFOUND) {
                put_serial_modifier("\x1b[0m");
                command_flags &= ~COMMAND_NOTFOUND;
            }
        }
        fflush(stdout);
        event_fire(&command_handler_iter);
        command_handler_ready = 1;
        command_in_progress = 0;
        fgets(command_buffer,512,stdin);
        command_in_progress = 1;
        char* cmd_end = command_buffer + strlen(command_buffer);
        while (cmd_end != command_buffer) {
            cmd_end --;
            if (cmd_end[0] == '\n' || cmd_end[0] == '\r')
                cmd_end[0] = 0;
        }
        command_execute(command_buffer);
    }
}

void help(const char * cmd, char* arg) {
    lock_take(&command_lock);
    for (int i=0; i<64; i++) {
        if (commands[i].name) {
            iprintf("%16s | %s\n", commands[i].name, commands[i].desc ? commands[i].desc : "no description");
        }
    }
    lock_release(&command_lock);
}

void dump_system_regs() {
    lock_take(&command_lock);

    iprintf("hello there 123 123 testing\n");

    unsigned long long buf;

    iprintf("-------- Feature registers --------\n");

    asm volatile ("mrs %0, ID_AA64ISAR0_EL1" : "=r"(buf) ::);
    iprintf("1. ID_AA64ISAR0_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, ID_AA64PFR0_EL1" : "=r"(buf) ::);
    iprintf("2. ID_AA64PFR0_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, ID_AA64PFR1_EL1" : "=r"(buf) ::);
    iprintf("3. ID_AA64PFR1_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, MIDR_EL1" : "=r"(buf) ::);
    iprintf("4. MIDR_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, ID_AA64ISAR1_EL1" : "=r"(buf) ::);
    iprintf("5. ID_AA64ISAR1_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, ID_AA64MMFR0_EL1" : "=r"(buf) ::);
    iprintf("6. ID_AA64MMFR0_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, ID_AA64MMFR1_EL1" : "=r"(buf) ::);
    iprintf("7. ID_AA64MMFR1_EL1: 0x%llx\n", buf);

    asm volatile ("mrs %0, ID_AA64MMFR2_EL1" : "=r"(buf) ::);
    iprintf("8. ID_AA64MMFR2_EL1: 0x%llx\n", buf);

    //apple clang doesnt like it probably because armv8.0 has no business having sve lol
    // asm volatile ("mrs %0, ID_AA64ZFR0_EL1" : "=r"(buf) ::);
    // iprintf("8. ID_AA64ZFR0_EL1: %llx\n", buf);


    iprintf("-------- Other regsiters --------\n");

    asm volatile ("mrs %0, CNTFRQ_EL0" : "=r"(buf) ::);
    iprintf("CNTFRQ_EL0: 0x%llx\n", buf);

    asm volatile ("mrs %0, CurrentEL" : "=r"(buf) ::);
    iprintf("CurrentEL [2:3]: 0x%llx\n", buf >> 2);

    asm volatile ("mrs %0, CLIDR_EL1" : "=r"(buf) ::);
    iprintf("CLIDR_EL1: 0x%llx\n", buf);

    lock_release(&command_lock);
}

extern uint64_t gSynopsysBase;
#define DWC2_REG_VAL(x) *(volatile uint32_t *)(gSynopsysBase + x)

void dwc2_force_host_mode(bool want_host_mode) {
    unsigned long long GUSBCFG = DWC2_REG_VAL(0xc);

    int set = want_host_mode ? 29 : 30;
    int clear = want_host_mode ? 30 : 29;

    GUSBCFG &= ~(1ULL << clear);
    GUSBCFG |= (1ULL << set);

    *(volatile uint32_t *)(gSynopsysBase + 0xc) = GUSBCFG;

    sleep(1); // life is going so fast, take a single second to think about your choices and let dwc2 do the same.
}

void dump_usb_regs() {
    lock_take(&command_lock);

    unsigned long long GRXFSIZ = DWC2_REG_VAL(0x24);
    unsigned long long GNPTXFSIZ = DWC2_REG_VAL(0x28);
    unsigned long long GHWCFG1 = DWC2_REG_VAL(0x44);
    unsigned long long GHWCFG2 = DWC2_REG_VAL(0x48);
    unsigned long long GHWCFG3 = DWC2_REG_VAL(0x4c);
    unsigned long long GHWCFG4 = DWC2_REG_VAL(0x50);
    unsigned long long HPTXFSIZ = DWC2_REG_VAL(0x100);
    unsigned long long GSNPSID = DWC2_REG_VAL(0x400);

    // The order matches the one in /sys/kernel/debug/usb/devicename/hw_params
    iprintf("-------- DWC2 hw_params --------\n");

    iprintf("op_mode = 0x%llx\n", (GHWCFG2 & 0x7) >> 0);

    iprintf("arch = 0x%llx\n", (GHWCFG2 & (0x3 << 3)) >> 3);

    iprintf("dma_desc_enable = 0x%llx\n", GHWCFG4 & (1 << 30));

    iprintf("enable_dynamic_fifo = 0x%llx\n", GHWCFG2 & (1 << 19));

    iprintf("en_multiple_tx_fifo = 0x%llx\n", GHWCFG4 & (1 << 25));

    iprintf("rx_fifo_size (dont mistake with something_rx_fifo_size) = %llu\n", GRXFSIZ & 0xffff);

    /* force host mode to make the readout sane */
    dwc2_force_host_mode(true);
    iprintf("host_nperio_tx_fifo_size = %llu\n", (GNPTXFSIZ & (0xffff << 16)) >> 16);

    /* force dev mode to make the readout sane */
    dwc2_force_host_mode(false);
    iprintf("dev_nperio_tx_fifo_size = %llu\n", (GNPTXFSIZ & (0xffff << 16)) >> 16);

    iprintf("host_perio_tx_fifo_size = %llu\n", (HPTXFSIZ & (0xffff << 16)) >> 16);

    iprintf("nperio_tx_q_depth = %llu\n", (GHWCFG2 & (0x3 << 22)) >> 22 << 1);

    iprintf("host_perio_tx_q_depth = %llu\n", (GHWCFG2 & (0x3 << 24)) >> 24 << 1);

    iprintf("dev_token_q_depth = %llu\n", (GHWCFG2 & (0x1f << 26)) >> 26);

    unsigned long long width = (GHWCFG3 & 0xf);

    iprintf("max_transfer_size = %d\n", (1 << (width + 11)) - 1);

    width = ((GHWCFG3 & (0x7 << 4)) >> 4);

    iprintf("max_packet_size = %d\n", (1 << (width + 4)) - 1);

    iprintf("host_channels = %llu\n", 1 + ((GHWCFG2 & (0xf << 14)) >> 14));

    iprintf("hs_phy_type = %llu\n", ((GHWCFG2 & (0x3 << 6)) >> 6));

    iprintf("fs_phy_type = %llu\n", ((GHWCFG2 & (0x3 << 8)) >> 8));

    iprintf("i2c_enable = %llu\n", (GHWCFG3 & (1 << 8)));

    iprintf("num_dev_ep = %llu\n", ((GHWCFG2 & (0xf << 10))) >> 10);

    iprintf("num_dev_perio_in_ep = %llu\n", (GHWCFG4 & (0xf)));

    iprintf("total_fifo_size = %llu\n", ((GHWCFG3 & (0xffff << 16))) >> 16);

    iprintf("power_optimized = %llu\n", GHWCFG4 & (1 << 4));

    iprintf("utmi_phy_data_width = %llu\n", (GHWCFG4 & (0x3 << 14)) >> 14);

    iprintf("snpsid = 0x%llx\n", GSNPSID);

    iprintf("dev_ep_dirs = 0x%llx\n", GHWCFG1);

    lock_release(&command_lock);
}

extern bool has_ecores;

bool is_ecore() {
    uint64_t val;
    __asm__ volatile("mrs\t%0, MPIDR_EL1" : "=r"(val));
    return !(val & (1 << 16));
}

void fix_apple_common_ecore() {
    __asm__ volatile(
        // "unlock the core for debugging"
        "msr OSLAR_EL1, xzr\n"

        /* Common to all Apple targets */
            "mrs    x28, S3_0_C15_C4_1\n"
            "orr    x28, x28, #0x800\n" //ARM64_REG_HID4_DisDcMVAOps
            "orr    x28, x28, #0x100000000000\n" //ARM64_REG_HID4_DisDcSWL2Ops
            "msr    S3_0_C15_C4_1, x28\n"
            "isb    sy\n"

            /* dont die in wfi kthx */
            "mrs     x28, S3_5_C15_C5_0\n"
            "bic     x28, x28, #0x3000000\n"
            "orr     x28, x28, #0x2000000\n"
            "msr     S3_5_C15_C5_0, x28\n"

            "isb sy\n"
            "dsb sy\n"
    );
}

void fix_apple_common() {
    if(is_ecore() && has_ecores) {
        fix_apple_common_ecore();
        return;
    }

    __asm__ volatile(
        // "unlock the core for debugging"
        "msr OSLAR_EL1, xzr\n"

        /* Common to all Apple targets */
            "mrs    x28, S3_0_C15_C4_0\n"
            "orr    x28, x28, #0x800\n" //ARM64_REG_HID4_DisDcMVAOps
            "orr    x28, x28, #0x100000000000\n" //ARM64_REG_HID4_DisDcSWL2Ops
            "msr    S3_0_C15_C4_0, x28\n"
            "isb    sy\n"

            /* dont die in wfi kthx */
            "mrs     x28, S3_5_C15_C5_0\n"
            "bic     x28, x28, #0x3000000\n"
            "orr     x28, x28, #0x2000000\n"
            "msr     S3_5_C15_C5_0, x28\n"

            "isb sy\n"
            "dsb sy\n"
    );
}

void fix_a7() {
    __asm__ volatile(
        // "unlock the core for debugging"
        "msr OSLAR_EL1, xzr\n"

        /* Common to all Apple targets */
            "mrs    x28, S3_0_C15_C4_0\n"
            "orr    x28, x28, #0x800\n" //ARM64_REG_HID4_DisDcMVAOps
            "orr    x28, x28, #0x100000000000\n" //ARM64_REG_HID4_DisDcSWL2Ops
            "msr    S3_0_C15_C4_0, x28\n"
            "isb    sy\n"

        /* Cyclone / typhoon specific init thing */
            "mrs     x28, S3_0_C15_C0_0\n"
            "orr     x28, x28, #0x100000\n"//ARM64_REG_HID0_LoopBuffDisb
            "msr     S3_0_C15_C0_0, x28\n"

            "mrs     x28, S3_0_C15_C1_0\n"
            "orr     x28, x28, #0x1000000\n"//ARM64_REG_HID1_rccDisStallInactiveIexCtl
    //cyclone
            "orr     x28, x28, #0x2000000\n"//ARM64_REG_HID1_disLspFlushWithContextSwitch
            "msr     S3_0_C15_C1_0, x28\n"

            "mrs     x28, S3_0_C15_C3_0\n"
            "orr     x28, x28, #0x40000000000000\n"//ARM64_REG_HID3_DisXmonSnpEvictTriggerL2StarvationMode
            "msr     S3_0_C15_C3_0, x28\n"

            "mrs     x28, S3_0_C15_C5_0\n"
            "and     x28, x28, #0xffffefffffffffff\n" //(~ARM64_REG_HID5_DisHwpLd)
            "and     x28, x28, #0xffffdfffffffffff\n"//(~ARM64_REG_HID5_DisHwpSt)
            "msr     S3_0_C15_C5_0, x28\n"

            "mrs     x28, S3_0_C15_C8_0\n"
            "orr     x28, x28, #0xff0\n" // ARM64_REG_HID8_DataSetID0_VALUE | ARM64_REG_HID8_DataSetID1_VALUE
            "msr     S3_0_C15_C8_0, x28\n"
        /* Cyclone / typhoon specific init thing end */

            /* dont die in wfi kthx */
            "mrs     x28, S3_5_C15_C5_0\n"
            "bic     x28, x28, #0x3000000\n"
            "orr     x28, x28, #0x2000000\n"
            "msr     S3_5_C15_C5_0, x28\n"

            "isb sy\n"
            "dsb sy\n"
    );
}

void fix_a10() {
    __asm__ volatile(
        // "unlock the core for debugging"
        "msr OSLAR_EL1, xzr\n"

        /* Common to all Apple targets */
            "mrs    x28, S3_0_C15_C4_0\n"
            "orr    x28, x28, #0x800\n" //ARM64_REG_HID4_DisDcMVAOps
            "orr    x28, x28, #0x100000000000\n" //ARM64_REG_HID4_DisDcSWL2Ops
            "msr    S3_0_C15_C4_0, x28\n"
            "isb    sy\n"

        /* Hurricane specific init thing */
            // Increase Snoop reservation in EDB to reduce starvation risk
            // Needs to be done before MMU is enabled
            "mrs     x28, S3_0_C15_C5_0\n"
            "bic     x28, x28, #0xc000\n"//ARM64_REG_HID5_CrdEdbSnpRsvd_mask
            "orr     x28, x28, #0x8000\n"//ARM64_REG_HID5_CrdEdbSnpRsvd_VALUE
            "msr     S3_0_C15_C5_0, x28\n"
        /* Hurricane specific init thing end */

            /* dont die in wfi kthx */
            "mrs     x28, S3_5_C15_C5_0\n"
            "bic     x28, x28, #0x3000000\n"
            "orr     x28, x28, #0x2000000\n"
            "msr     S3_5_C15_C5_0, x28\n"

            "isb sy\n"
            "dsb sy\n"
    );
}

void command_init() {
    command_task = task_create("command", command_main);
    command_task->flags |= TASK_RESTART_ON_EXIT;
    command_task->flags &= ~TASK_CAN_EXIT;
    command_register("help", "shows this help message", help);
    command_register("dumpusb", "dumps various dwc2 registers for lee noocks", dump_usb_regs);
    command_register("dump", "dumps various system registers", dump_system_regs);
    command_register("fix", "tries to fix a7..", fix_a7);
}
