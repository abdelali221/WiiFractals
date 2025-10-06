#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <ogc/machine/asm.h>
#include <ogc/machine/processor.h>
#include <string.h>
#include <wiiuse/wpad.h>

extern void __exception_setlookupxfb(void *xfb);
extern void __exception_setreload(int s);
extern void usleep(u32 t);

char *fractalname[] = {
	"Bifurcation",
	"Mandelbrot"
};

void *xfb = NULL;
static GXRModeObj *rmode = NULL;

#define WIDTH 480
#define HEIGHT 480

const u32 RGB2YCBCR(u8 r1, u8 g1, u8 b1) {
	u8 r2 = r1; u8 g2 = g1; u8 b2 = b1;
	if (r1 < 16) r1 = 16;
	if (g1 < 16) g1 = 16;
	if (b1 < 16) b1 = 16;
	if (r2 < 16) r2 = 16;
	if (g2 < 16) g2 = 16;
	if (b2 < 16) b2 = 16;

	if (r1 > 240) r1 = 240;
	if (g1 > 240) g1 = 240;
	if (b1 > 240) b1 = 240;
	if (r2 > 240) r2 = 240;
	if (g2 > 240) g2 = 240;
	if (b2 > 240) b2 = 240;

	u8 Y1 = ( 77 * r1 + 150 * g1 + 29 * b1) / 256;
	u8 Y2 = ( 77 * r2 + 150 * g2 + 29 * b2) / 256;
	u8 Cb = (112 * (b1 + b2) -  74 * (g1 + g2) - 38 * (r1 + r2)) / 512 + 128;
	u8 Cr = (112 * (r1 + r2) - 94 * (g1 + g2) - 18 * (b1 + b2)) / 512 + 128;

	return Y1 << 24 | Cb << 16 | Y2 << 8 | Cr;
}

void writetoxfb(void* videoBuffer, u32 offset, u32 length, u32 color)
{
	u32 *p = ((u32*)videoBuffer) + offset;
	for(u32 i = 0; i < length; i++) {
		*p++ = color;
	}
}

void Bifurcation() {
	float r = .95, x_init = .5;
    float delta_r = 0.00958;
    for (int col = 0; col < 639; col++) {
        float x = x_init;
        r += delta_r;
        for (int i = 0; i < 256; ++i) {
            x = r * x * (1 - x);
            if ((x > 1000000) || (x < -1000000))
                break;

            int row = 320 - (x * 300);
            if (i > 64 && row < 349 && row >= 0 && col >= 0 && col < 639) {
                writetoxfb(xfb, (row*320)+col, 1, COLOR_WHITE);
				putchar('\r');
				if(row < 10) printf("00");
				else if (row < 100) printf("0");
				printf("%d,",row);
				if(col < 10) printf("00");
				else if (col < 100) printf("0");
				printf(" %d",col);
            }
        }
    }
}

void Mandelbrot() {
	int max_iterations = 655;
    int max_size = 4;
	float x_min = -2.0;
    float x_max = 2.0;
    float y_min = -1.0;
    float y_max = 1.0;

    float delta_x = (x_max - x_min) / WIDTH;
    float delta_y = (y_max - y_min) / HEIGHT;

    float Q[HEIGHT] = { y_max };
    float P[WIDTH] = { x_min };
    for (int row = 1; row < HEIGHT; row++)
        Q[row] = Q[row - 1] - delta_y;
    for (int col = 1; col < WIDTH; col++ )
        P[col] = P[col - 1] + delta_x;

    /*
     * For every pixel calculate resulting value until the number becomes too
     * big, or we run out of iterations
     */
    for (int col = 0; col < WIDTH-(WIDTH/3); col++ ) {
        for (int row = 0; row < HEIGHT; row++ ) {
            float x_square = 0.0;
            float y_square = 0.0;
            float x = 0.0;
            float y = 0.0;

            int color = 1;
            while (color < max_iterations && x_square + y_square < max_size) {
                x_square = x * x;
                y_square = y * y;
                y = 2 * x * y + Q[row];
                x = x_square - y_square + P[col];
                color++;
            }
			writetoxfb(xfb, (row*320)+col, 1, color);
			putchar('\r');
			if(row < 10) printf("00");
			else if (row < 100) printf("0");
			printf("%d,",row);
			if(col < 10) printf("00");
			else if (col < 100) printf("0");
			printf(" %d",col);
        }
    }
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	// Initialise the video system
	VIDEO_Init();
	WPAD_Init();
	// Obtain the preferred video mode from the system
	// This will correspond to the settings in the Wii menu
	rmode = VIDEO_GetPreferredMode(NULL);

	// Allocate memory for the display in the uncached region
	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	__exception_setlookupxfb(xfb);
	__exception_setreload(30);

	// Initialise the console, required for printf
	console_init(xfb,20,20,rmode->fbWidth-20,rmode->xfbHeight-20,rmode->fbWidth*VI_DISPLAY_PIX_SZ);
	//SYS_STDIO_Report(true);

	// Set up the video registers with the chosen mode
	VIDEO_Configure(rmode);

	// Tell the video hardware where our display memory is
	VIDEO_SetNextFramebuffer(xfb);

	// Clear the framebuffer
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	// Make the display visible
	VIDEO_SetBlack(false);

	// Flush the video register changes to the hardware
	VIDEO_Flush();

	// Wait for Video setup to complete
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

	printf("WiiFractals / V1.0\nBy Abdelali221");
	usleep(2000000);
	VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);

	u8 selection = 0;

	printf("\rPlease Select a fractal : %s", fractalname[selection]);
	
	while(1) {
		WPAD_ScanPads();

		int pressed = WPAD_ButtonsDown(0);

		if(pressed & WPAD_BUTTON_RIGHT) {
			if (selection < 1) selection++;
		} else if (pressed & WPAD_BUTTON_LEFT) {
			if (selection > 0) selection--;
		} else if (pressed & WPAD_BUTTON_A) {
			VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
			switch(selection) {
				case 0:
					Bifurcation();
				break;

				case 1:
					Mandelbrot();
				break;
			}
			printf("\rPress HOME to go back");
			while(1) {
				WPAD_ScanPads();
				pressed = WPAD_ButtonsDown(0);
				if (pressed & WPAD_BUTTON_HOME) {
					break;
				}	
			}
			VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
		
		} else if (pressed & WPAD_BUTTON_HOME) {
			exit(0);
		}
		if(pressed) {
			printf("\x1b[2K\rPlease Select a fractal : %s", fractalname[selection]);
		}
		VIDEO_WaitVSync();
	}
	return 0;
}
