/**
 * VGA Mode 13h: 320x200, 256 colors, linear framebuffer at 0xA0000.
 * Set by programming VGA registers (no BIOS). Restore text mode on leave.
 */

#include <kernel/video_mode13.h>
#include <kernel/port.h>
#include <kernel/types.h>

#define SEQ_IDX  0x3C4
#define SEQ_DAT  0x3C5
#define GC_IDX   0x3CE
#define GC_DAT   0x3CF
#define CRTC_IDX 0x3D4
#define CRTC_DAT 0x3D5
#define AC_IDX   0x3C0
#define AC_RD    0x3C1
#define MISC_WR  0x3C2
#define DAC_WR   0x3C8
#define DAC_DAT  0x3C9
#define IN_STAT  0x3DA
#define MISC_RD  0x3CC

static uint8_t saved_sequencer[5];
static uint8_t saved_gc[9];
static uint8_t saved_crtc[25];
static uint8_t saved_attr[21];
static uint8_t saved_misc;
static bool mode13_active;

static void write_sequencer(uint8_t idx, uint8_t val) { outb(SEQ_IDX, idx); outb(SEQ_DAT, val); }
static void write_gc(uint8_t idx, uint8_t val)       { outb(GC_IDX, idx); outb(GC_DAT, val); }
static void write_crtc(uint8_t idx, uint8_t val)     { outb(CRTC_IDX, idx); outb(CRTC_DAT, val); }

static uint8_t read_crtc(uint8_t idx) { outb(CRTC_IDX, idx); return inb(CRTC_DAT); }

static void read_attr(void)
{
    (void)inb(IN_STAT);
    for (int i = 0; i < 21; i++) {
        outb(AC_IDX, i);
        saved_attr[i] = inb(AC_RD);
    }
    (void)inb(IN_STAT);
}

static void write_attr(uint8_t idx, uint8_t val)
{
    (void)inb(IN_STAT);
    outb(AC_IDX, idx);
    outb(AC_IDX, val);
    (void)inb(IN_STAT);
}

static void save_state(void)
{
    saved_misc = inb(MISC_RD);
    for (int i = 0; i < 5; i++)  { outb(SEQ_IDX, i); saved_sequencer[i] = inb(SEQ_DAT); }
    for (int i = 0; i < 9; i++)  { outb(GC_IDX, i);  saved_gc[i] = inb(GC_DAT); }
    for (int i = 0; i < 25; i++) { outb(CRTC_IDX, i); saved_crtc[i] = inb(CRTC_DAT); }
    read_attr();
}

static void restore_state(void)
{
    (void)inb(IN_STAT);
    for (int i = 0; i < 25; i++) write_attr(i, saved_attr[i]);
    outb(AC_IDX, 0x20);
    for (int i = 0; i < 5; i++)  write_sequencer(i, saved_sequencer[i]);
    for (int i = 0; i < 9; i++)  write_gc(i, saved_gc[i]);
    uint8_t crtc_prot = read_crtc(0x11);
    write_crtc(0x11, crtc_prot & 0x7F);
    for (int i = 0; i < 25; i++) write_crtc(i, saved_crtc[i]);
    write_crtc(0x11, crtc_prot);
    outb(MISC_WR, saved_misc);
}

/* Standard Mode 13h register set (VGA 320x200x256 linear) */
static void set_mode13_registers(void)
{
    outb(MISC_WR, 0x63);
    write_sequencer(0x00, 0x03);
    write_sequencer(0x01, 0x01);
    write_sequencer(0x02, 0x0F);
    write_sequencer(0x03, 0x00);
    write_sequencer(0x04, 0x06);  /* Chain 4, 256-color */
    write_gc(0x00, 0x00);
    write_gc(0x01, 0x00);
    write_gc(0x02, 0x00);
    write_gc(0x03, 0x00);
    write_gc(0x04, 0x00);
    write_gc(0x05, 0x40);  /* 256-color shift */
    write_gc(0x06, 0x05);
    write_gc(0x07, 0x0F);
    write_gc(0x08, 0xFF);
    uint8_t cr11 = read_crtc(0x11);
    write_crtc(0x11, cr11 & 0x7F);
    write_crtc(0x00, 0x5F);
    write_crtc(0x01, 0x4F);
    write_crtc(0x02, 0x50);
    write_crtc(0x03, 0x82);
    write_crtc(0x04, 0x54);
    write_crtc(0x05, 0x80);
    write_crtc(0x06, 0xBF);
    write_crtc(0x07, 0x1F);
    write_crtc(0x08, 0x00);
    write_crtc(0x09, 0x41);
    write_crtc(0x10, 0x00);
    write_crtc(0x11, 0x40);
    write_crtc(0x12, 0x00);
    write_crtc(0x13, 0x00);
    write_crtc(0x14, 0x00);
    write_crtc(0x15, 0x00);
    write_crtc(0x16, 0x00);
    write_crtc(0x17, 0x9C);
    write_crtc(0x18, 0x8E);
    write_crtc(0x19, 0x8F);
    write_crtc(0x20, 0x40);
    write_crtc(0x21, 0x00);
    write_crtc(0x22, 0x07);
    write_crtc(0x23, 0x00);
    write_crtc(0x24, 0x00);
    write_crtc(0x11, cr11);
    for (int i = 0; i < 16; i++) write_attr(i, i);
    write_attr(0x10, 0x41);
    write_attr(0x11, 0x00);
    write_attr(0x12, 0x0F);
    write_attr(0x13, 0x00);
    write_attr(0x14, 0x00);
    outb(AC_IDX, 0x20);
}

void video_mode13_enter(void)
{
    if (mode13_active) return;
    save_state();
    set_mode13_registers();
    mode13_active = true;
}

void video_mode13_leave(void)
{
    if (!mode13_active) return;
    restore_state();
    mode13_active = false;
}

uint8_t *video_mode13_framebuffer(void)
{
    return (uint8_t *)MODE13_FB;
}

void video_mode13_set_palette(const uint8_t *rgb768)
{
    for (int i = 0; i < 256; i++) {
        outb(DAC_WR, (uint8_t)i);
        outb(DAC_DAT, rgb768[i * 3 + 0]);
        outb(DAC_DAT, rgb768[i * 3 + 1]);
        outb(DAC_DAT, rgb768[i * 3 + 2]);
    }
}
