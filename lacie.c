
/* lacie.c is Linux Ansi Console Image Editor for 1-bit or 4-bit pallette, with transparency.
 * By jpka, 2026. Std: ANSI C (C89). License: GPL 2+ or later */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>
#include "sys/param.h" /* MIN(), MAX() */

/* Transparent pixel, Pixel, Transparent Cursor, Cursor; and same yet selected. */
const char* figure[] = {"▀▄", "██", "▒▒", "▒▒", "//", "##", "{}", "()"};

#define DEBUG 0

#define MAX_XSIZE 64
#define MAX_YSIZE 64

#define TEMP_FILENAME "/dev/shm/lacie_tmp.pam"  /* Do not wear out SSD, use RAM instead. */

const char* pam_header[] = {"P7\n",
                            "WIDTH %d\n",
                            "HEIGHT %d\n",
                            "DEPTH 4\n",
                            "MAXVAL 3\n",
                            "TUPLTYPE RGB_ALPHA\n",
                            "ENDHDR\n"};

int xSize     = 16,  ySize     = 16;
int xSelStart = 0,   ySelStart = 0;
int xSelEnd   = 0,   ySelEnd   = 0;
int xCursor   = 0,   yCursor   = 0;

uint8_t color = 0;
uint8_t screen    [MAX_XSIZE][MAX_YSIZE],
        screenUndo[MAX_XSIZE][MAX_YSIZE]; /* Just one. Anybody need more? */

uint8_t  kbdInput[8];
uint64_t key;
int      n;

int   mode_select = 0;
int   mode_include_transparent = 0;
int   mode_toroidal = 0;     /* Endless canvas, cursor rolls over boundary */
int   can_undo = 0;
float cursor_r_ext = 1.0f;
float cursor_r_int = 0.0f;
float cursor_offgrid = 0.0f; /* 0 or 0.5 are useful */
int   cursor_fill = 0;
int   put = 0;

int   quit = 0;

#define MOVE(axis,dir)  axis##Cursor = (axis##Cursor + dir + axis##Size) % axis##Size
#define ERREXIT(...)    { fprintf(stderr, ##__VA_ARGS__); exit(1); }

int x, y;  /* Loops can't be local for ANSI C which is C89 */


void load_image_from_file(char* filename)
{
    FILE *fp = fopen(filename, "rb");
    if (! fp)
        ERREXIT("Can't read %s.\n", filename);

    for (y = 0; y < 7; y++)
        if ((y == 1) ? ((fscanf(fp, pam_header[y], &xSize) != 1)) :
            (y == 2) ? ((fscanf(fp, pam_header[y], &ySize) != 1)) :
                       ((fscanf(fp, pam_header[y]) != 0)))
            ERREXIT("%s:\nHeader mismatch @ line %d, expected %s\n"
                    "Is image created like `magick test._ -depth 2 test.pam` ?\n",
                     filename, y, pam_header[y]);

    if ((xSize < 1) || (xSize > MAX_XSIZE) ||
        (ySize < 1) || (ySize > MAX_YSIZE))
            ERREXIT("Image size %dx%d out of bounds 1x1...%dx%d.\n",
                     xSize, ySize, MAX_XSIZE, MAX_YSIZE);

    unsigned int abgr;
    for (y = 0; y < ySize; y++)
        for (x = 0; x < xSize; x++)
            if (fread(&abgr, sizeof abgr, 1, fp) == 1)

                /* ABGR to indexed, 0..15 or 255 for transparent */
                screen[x][y] = ((abgr & 0xff000000) < 128) ? 255 :
                        ((abgr & 0x30303) >= 0x10101) * 8 +    /* I */
                        (( abgr        & 3) > 1)      * 4 +    /* R */
                        (((abgr >>  8) & 3) > 1)      * 2 +    /* G */
                        (((abgr >> 16) & 3) > 1)      * 1;     /* B */
            else
                break;
    fclose(fp);
}


void save_image_to_file(char* filename)
{
    FILE *fp = fopen(filename, "wb");
    if (! fp)
        ERREXIT("Can't write %s.\n", filename);

    for (y = 0; y < 7; y++)
        (y == 1) ? fprintf(fp, pam_header[y], xSize) :
        (y == 2) ? fprintf(fp, pam_header[y], ySize) :
                   fprintf(fp, pam_header[y]);

    unsigned int abgr;
    for (y = 0; y < ySize; y++)
        for (x = 0; x < xSize; x++)
        {
            int b = screen[x][y];

            /* Indexed, 0..15 or 255 for transparent, to ABGR */
            abgr = (b == 255) ? 0 :
                                0x03000000 +
                        (b > 7) * 0x010101 +
                      !!(b & 4) * 0x000002 +
                      !!(b & 2) * 0x000200 +
                      !!(b & 1) * 0x020000;
            fwrite(&abgr, sizeof abgr, 1, fp);
        }
    fclose(fp);
}


int main(int argc, char *argv[])
{
    if ((argc < 2) || ((argc == 2) && (argv[1][1] == '-')))
        ERREXIT("Usage:  lacie (new)filename.pam\n");

    int is_fbcon = !system("tty | grep tty");

    /* Change EGA16 brown color to dark yellow will work for bare console (fbcon) only.
     * All X11 terminals are already defines this color as dark yellow. */
    if (is_fbcon)
        printf("\x1b]P3AAAA00\n");

    struct termios attr;

    tcgetattr(STDIN_FILENO, &attr);
    attr.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);

    /* memset() is byte based, exactly as we need. */
    memset(screen, (uint8_t) 255, sizeof screen);

    if (access(argv[1], F_OK) == 0)
        load_image_from_file(argv[1]);

    /* Initial preview. */
    save_image_to_file(TEMP_FILENAME);

    /* Please use a and A keys for toggle transparency and antialiasing, + for zoom. */
    system("sxiv -b -g 64x64 "TEMP_FILENAME" &"); /* FIXME it lacks many CLI keys yet, sadly. */

    // printf("\x1b[2J\x1b[H");           /* Clear screen */
    goto draw;

    do
    {
        printf("\x1b[%dA", ySize + 2); /* Move ANSI terminal cursor up N lines */

        if ((n = read(STDIN_FILENO, &key, 8)) > 0)
        {
            key = key & ((1L << (n * 8)) - 1L);

            if (DEBUG)
                printf("Key %lx bytes %d", key, n);

            switch (key)
            {
                /* Cursor movement */

                case 0x415b1b:    MOVE(y, -1);               break; /* Arrows */
                case 0x425b1b:    MOVE(y,  1);               break;
                case 0x445b1b:    MOVE(x, -1);               break;
                case 0x435b1b:    MOVE(x,  1);               break;
                case 0x7e315b1b:                                    /* Home, fbcon */
                case 0x485b1b:    MOVE(x, -1); MOVE(y, -1);  break; /* Home, x11   */
                case 0x7e345b1b:                                    /* End,  fbcon */
                case 0x465b1b:    MOVE(x, -1); MOVE(y,  1);  break; /* End,  x11   */
                case 0x7e355b1b:  MOVE(x,  1); MOVE(y, -1);  break; /* PgUp */
                case 0x7e365b1b:  MOVE(x,  1); MOVE(y,  1);  break; /* PdDn */

                /* Colors */

                case '`':
                case 'k':         color = 0;                 break;
                case 'w':         color = 15;                break;
                case '0'...'9':   color = key - '0';         break;
                case 'a'...'f':   color = key - 'a' + 10;    break;
                case 0x09:        color = (color & 15) ^ 15; break; /* Tab */
                case 0x7e335b1b:  color = 255; put = 1;      break; /* Del for transparent */

                /* Rectangular block select and copy, like with 'mcedit' */

                case 0x435b5b1b:  /* F3, fbcon, */
                case 0x524f1b:    /* F3, x11:   select start & select end */
                    cursor_r_ext = 1.0f;
                    mode_select = ! mode_select;
                    if (mode_select)
                    {
                        xSelStart = xSelEnd = xCursor;
                        ySelStart = ySelEnd = yCursor;
                    }
                    break;

                case 0x455b5b1b:   /* F5, fbcon, */
                case 0x7e35315b1b: /* F5, x11:   block copy to cursor */
                    cursor_r_ext = 1.0f;
                    if ((xSelStart != xSelEnd) && (ySelStart != ySelEnd))
                    {
                        memcpy(screenUndo, screen, sizeof screen);
                        can_undo = 1;

                        int dx = xCursor - xSelStart;
                        int dy = yCursor - ySelStart;

                        for (y = ySelStart; y < ySelEnd; y++)
                            for (x = xSelStart; x < xSelEnd; x++)
                                if (mode_include_transparent || (screenUndo[x][y] != 255))
                                    screen[(x + dx + xSize) % xSize][(y + dy + ySize) % ySize] =
                                                screenUndo[x][y];
                    }
                    break;

                /* Are transparent pixels blockcopied or not? Same as 'paintbrush' switch. */

                case 't':
                    mode_include_transparent = ! mode_include_transparent;
                    break;

                /* Modes:
                 * circle is filled or not;
                 * circle is even or odd size, aka centered to pixel or pixels boundary */

                case 'm':
                    cursor_fill = ! cursor_fill;
                    if (cursor_fill)
                        cursor_offgrid = (cursor_offgrid > 0.0f) ? 0.0f : 0.5f;
                    break;

                /* Circle turn on (r > 1), turn off (r <= 1), and fine shape control */

                case ']':
                case 'r':
                    cursor_r_ext = MIN((floor(cursor_r_ext) + 1.0f), (float)(xSize / 2));
                    cursor_r_int = cursor_r_ext - 1;
                    break;

                case '[':
                case 'R':
                    cursor_r_ext = MAX((ceil(cursor_r_ext) - 1.0f), 1.0f);
                    cursor_r_int = cursor_r_ext - 1;
                    break;

                case '+':
                    cursor_r_ext = MIN((cursor_r_ext + 0.05f), (float)(xSize / 2));
                    break;

                case '-':
                    cursor_r_ext = MAX((cursor_r_ext - 0.05f), 1.0f);
                    break;

                case '*':
                    cursor_r_int = MIN((cursor_r_int + 0.05f), (float)(xSize / 2));
                    break;

                case '/':
                    cursor_r_int = MAX((cursor_r_int - 0.05f), 1.0f);
                    break;

                case 0x1b: /* Esc to return to one-pixel cursor */
                    cursor_r_ext = 1.0f;
                    break;

                /* Draw pixel or circle */

                case ' ':
                    put = 1;
                    memcpy(screenUndo, screen, sizeof screen);
                    can_undo = 1;
                    break;

                /* Undo for block copy */

                case 'u':
                    if (can_undo)
                        memcpy(screen, screenUndo, sizeof screen);
                    can_undo = 0;
                    break;

                /* Save file. There is no auto save at exit.
                 * Please check temp file if you forget to save. */

                case 0x13:     /* Ctrl+S; for Konsole: Profile, Advanced, Turn off flow control */
                case 0x514f1b: /* F2 */
                    save_image_to_file(argv[1]);
                    break;

                case 'q':
                    quit = 1;
                default:
                    break;
            }
        }

        /* FIXME It should be done in shorter & better way. */
        if (mode_select)
        {
            if (xCursor > xSelStart)
                xSelEnd = xCursor;
            else
                xSelStart = xCursor;

            if (yCursor > ySelStart)
                ySelEnd = yCursor;
            else
                ySelStart = yCursor;
        }

draw:
        for (y = 0; y < ySize; y++)
        {
            for (x = 0; x < xSize; x++)
            {
                float distance = sqrt(pow(x - xCursor + cursor_offgrid, 2) +
                                      pow(y - yCursor + cursor_offgrid, 2));

                int is_cursor = (cursor_r_ext <= 1.0f) ? ((x == xCursor) && (y == yCursor)) :
                    (((distance >= cursor_r_int) || cursor_fill) && distance <= cursor_r_ext);

                if (is_cursor && put)
                    screen[x][y] = color;

                int is_transparent = (screen[x][y] == 255);

                int is_selected = ((x >= xSelStart) && (x < xSelEnd) &&
                                   (y >= ySelStart) && (y < ySelEnd));

                int c = screen[x][y];

                /* 4 bit EGA/VGA: 0x0000IRGB, where I is intensity.
                 * Note this is not full range for X11, to save eyes,
                 * and to make cursor brighter than *any* color, include white. */
                uint8_t A = is_fbcon ? 0x55 : 0x33;

                int fg = is_transparent ? (A * 0x010101) :
                         !!(c & 4) * (A * 0x000002) +
                         !!(c & 2) * (A * 0x000200) +
                         !!(c & 1) * (A * 0x020000) +
                         !!(c & 8) * (A * 0x010101);

                int bg = is_transparent ? ((A / 2) * 0x010101) : 0;

                /* With fbcon, there is not possible to have more than 16 colors, sadly.
                 * So one of colors will be not seen under cursor; it will be 7, light gray. */
                if (is_cursor)
                    bg = (!is_fbcon || (c < 7) || (c == 8) || (c == 255)) ? 0xffffff : 0;

                printf("\x1b[38;2;%d;%d;%dm", (fg & 255), ((fg >> 8) & 255), ((fg >> 16) & 255));
                printf("\x1b[48;2;%d;%d;%dm", (bg & 255), ((bg >> 8) & 255), ((bg >> 16) & 255));

                /* Blinking: Will it be helpful ? */
                // if ((is_cursor) /* && (cursor_r_ext <= 1.0f) */)
                //     printf("\x1b[5m");
                    // printf("\x1b[0;5m");

                int index = (is_selected * 4 + is_cursor * 2 + ! is_transparent);
                printf("%s", figure[index]);
                printf("\x1b[0m");
            }
            printf(" %d\n", y);
        }

        /* Auto updated temp image used for preview with some viewer with auto-update of file. */
        if (put)
            save_image_to_file(TEMP_FILENAME);

        put = 0;

        printf("%dx%d  %d, %d  Sel %d  Trans %d  Undo %d  Color %d   \n",
               xSize, ySize, xCursor, yCursor, mode_select, mode_include_transparent, can_undo, color);
        printf("rR[]+-/*Esc: R %4.2f, r %4.2f   m: Offgrid %3.1f, Fill %d   \n",
               cursor_r_ext, cursor_r_int, cursor_offgrid, cursor_fill);

    } while (! quit);

    attr.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &attr);

    // save_image_to_file(argv[1]);

    return 0;
}
