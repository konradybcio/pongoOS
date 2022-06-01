#include "font.h"
#include "printf.h"



void draw_horizontal_line(int x1, int x2, int y);
void barcode_space_narrow();
void barcode_space_wide();
void barcode_stripe_narrow();
void barcode_stripe_wide();
extern char *fbp;
unsigned int* framebuffer =  (unsigned int*) 0x87D0E2000;
#define PIXELS_NARROW      3
#define PIXELS_WIDE        6

enum parts {
        BARCODE_STRIPE_NARROW,
        BARCODE_STRIPE_WIDE,
        BARCODE_SPACE_NARROW,
        BARCODE_SPACE_WIDE,
};

enum parts _asterisk[] = {BARCODE_STRIPE_NARROW, BARCODE_SPACE_WIDE, BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW,
                         BARCODE_STRIPE_WIDE,   BARCODE_SPACE_NARROW, BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW };

enum parts _1[] =        {BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_WIDE,
                        BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW, BARCODE_STRIPE_WIDE, };

enum parts _h[] =        {BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW,
                        BARCODE_SPACE_WIDE, BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, };

enum parts _e[] =        {BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW,
                        BARCODE_STRIPE_WIDE, BARCODE_SPACE_WIDE, BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, };

enum parts _l[] =        {BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW, BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW,
                        BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_WIDE, BARCODE_STRIPE_WIDE, };

enum parts _o[] =        {BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_NARROW,
                        BARCODE_STRIPE_WIDE, BARCODE_SPACE_NARROW, BARCODE_STRIPE_NARROW, BARCODE_SPACE_WIDE, BARCODE_STRIPE_NARROW, };

void barcode_space_narrow() {
        framebuffer += 0x600*PIXELS_NARROW;
}

void barcode_space_wide() {
        framebuffer += 0x600*PIXELS_WIDE;
}

void barcode_stripe_wide() {
        int count = 0x600*PIXELS_WIDE;

        for (; count != 0; count--) {
                *framebuffer = 0x00000000;
                framebuffer += 1;
        }
}

void barcode_stripe_narrow() {
        int count = 0x600*PIXELS_NARROW;

        for (; count != 0; count--) {
                *framebuffer = 0xff000000;
                framebuffer += 1;
        }
}

void code39_asterisk() {
        barcode_stripe_narrow();
        barcode_space_wide();
        barcode_stripe_narrow();
        barcode_space_narrow();

        barcode_stripe_wide();
        barcode_space_narrow();
        barcode_stripe_wide();

        barcode_space_narrow();
        barcode_stripe_narrow();
}

void code39_1() {
        barcode_stripe_wide();
        barcode_space_narrow();
        barcode_stripe_narrow();
        barcode_space_wide();
        barcode_stripe_narrow();
        barcode_space_narrow();
        barcode_stripe_narrow();
        barcode_space_narrow();
        barcode_stripe_wide();
}

void printc_code39(char c) {
        enum parts* parts;
        switch (c){
                case '*':
                        parts = _asterisk;
                        break;
                case 'h':
                        parts = _h;
                        break;
                case 'e':
                        parts = _e;
                        break;
                case 'l':
                        parts = _l;
                        break;
                case 'o':
                        parts = _o;
                        break;
        }

        for (int i = 0; i < 9; i++) {
                switch (parts[i]) {
                        case BARCODE_STRIPE_NARROW:
                                barcode_stripe_narrow();
                                break;
                        case BARCODE_STRIPE_WIDE:
                                barcode_stripe_wide();
                                break;
                        case BARCODE_SPACE_NARROW:
                                barcode_space_narrow();
                                break;
                        case BARCODE_SPACE_WIDE:
                                barcode_space_wide();
                                break;
                }
        }
        barcode_space_narrow();
}

void print_code39(char* s, int len) {
        printc_code39('*');
        code39_1();
        barcode_space_narrow();

        code39_1();
        barcode_space_narrow();

        code39_1();
        barcode_space_narrow();

        code39_1();
        barcode_space_narrow();

        printc_code39('h');
        printc_code39('e');
        printc_code39('l');
        printc_code39('l');
        printc_code39('o');
        printc_code39('*');
}

void draw_character() {

}

void draw_text(char *text, int textX, int textY) {
        int i, x, y;
        // loop through all characters in the text string
        for (i = 0; text[i] != '\0'; i++) {
                if(text[i] < 32) { //ignore unprintable characters
                        continue;
                }
                int ix = font_index(text[i]); // get the 'image' index for this character
                unsigned char *img = letters[ix]; // get the font 'image'
                for (y = 0; y < FONTH; y++) { // loop through pixel rows
                        unsigned char b = img[y]; // get the row value
                        for (x = 0; x < FONTW; x++) { // loop through pixel cols
                                
                                //printf("%d\n", (b>>x)&1);
                                if (((b<<x)&0b10000000) > 0) {
                                        *(framebuffer + x + (y*1536)) = 0x00000000;
                                }
                        } // end "for x"
                } // end "for y"
                framebuffer += FONTW;
        } // end "for i"
}

void memset(char* buf, char n, long size);
volatile void memset_r(char* buf, char n, long size){
        for (long i = 0; i < size; i++){
                buf[i] = n;
        }
}

void in_c (void* framebuffer1) {
        char b[256] = {0};

        barcode_space_narrow();
        barcode_stripe_narrow();

        my_sprintf(b, "Hello from EL%d", 3);
        for (;;);

        draw_text(b, 0, 0);
}