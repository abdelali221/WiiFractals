#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <string.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>

extern void __exception_setlookupxfb(void *xfb);
extern void __exception_setreload(int s);
extern void usleep(u32 t);

int max_iterations = 0;
int shift = 0;

int Iterations_values [] = {
	0xff,
	0xfff,
	0xffff,
	0xfffff,
	0xffffff
};

char *Iterations_names [] = {
	"low (fast)",
	"medium",
	"good",
	"fancy (slow)",
	"ultra (extremely slow)"
};

void POSCursor(uint8_t X, uint8_t Y) {
	printf("\x1b[%d;%dH", Y, X);
}

char *fractalname[] = {
	"Bifurcation",
	"Mandelbrot"
};

void *xfb = NULL;
static GXRModeObj *rmode = NULL;

u16 WIDTH = 480;
u16 HEIGHT = 480;

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
				printf("%d, ",row);
				if(col < 10) printf("00");
				else if (col < 100) printf("0");
				printf("%d",col);
            }
        }
    }
}

u32 Mandelbrot() {
    int max_size = 4;
	float x_min = -2.0;
    float x_max = 1.0;
    float y_min = -1.25;
    float y_max = 1.25;

    float delta_x = (x_max - x_min) / WIDTH;
    float delta_y = (y_max - y_min) / HEIGHT;
    
    float Q[HEIGHT];
	for(int i = 0;i < HEIGHT;i++) Q[i] = y_max;
    float P[WIDTH];
	for(int i = 0;i < WIDTH;i++) P[i] = x_min;

    for (int row = 1; row < HEIGHT; row++)
        Q[row] = Q[row - 1] - delta_y;
    for (int col = 1; col < WIDTH; col++ )
        P[col] = P[col - 1] + delta_x;

    /*
     * For every pixel calculate resulting value until the number becomes too
     * big, or we run out of iterations
     */
	u32 start_time = ticks_to_millisecs(gettime());
    for (int col = 0; col < WIDTH; col++ ) {
        for (int row = 0; row < HEIGHT; row++ ) {
            float x_square = 0.0;
            float y_square = 0.0;
            float x = 0.0;
            float y = 0.0;

            u32 color = 0;
            while (color < Iterations_values[max_iterations] && x_square + y_square < max_size) {
                x_square = x * x;
                y_square = y * y;
                y = 2 * x * y + Q[row];
                x = x_square - y_square + P[col];
                color++; 
            }
			                
			u8 r = (color * 5) % 255;
            u8 g = (color * 3) % 255;
            u8 b = (color * 1) % 255;
			if (color == Iterations_values[max_iterations]) r = g = b = 0;

			writetoxfb(xfb, (row*WIDTH)+col, 2, RGB2YCBCR(r, g, b));
			putchar('\r');
			if(row < 10) printf("00");
			else if (row < 100) printf("0");
			printf("%d, ",row);
			if(col < 10) printf("00");
			else if (col < 100) printf("0");
			printf("%d",col);
        }
    }
	u32 end_time = ticks_to_millisecs(gettime());
	return end_time - start_time;
}

void Settings() {
	u8 selection = 0;

	printf("\x1b[2J");

	POSCursor(35,7);

	printf("Settings :");

	POSCursor(22,10);

	printf("Iteration (Quality) : ");
	printf("%s", Iterations_names[max_iterations]);

	POSCursor(0, 26);
	printf("HOME : Go back , U/D : Select , A : Change");

	while(1) {
		WPAD_ScanPads();

		int pressed = WPAD_ButtonsDown(0);

		if(pressed & WPAD_BUTTON_DOWN) {
			if (selection < 1) selection++;
		} else if (pressed & WPAD_BUTTON_UP) {
			if (selection > 0) selection--;
		} else if (pressed & WPAD_BUTTON_A) {
			switch(selection) {
				case 0:
					max_iterations++;
					if(max_iterations == 5) max_iterations = 0;
					POSCursor(22,10);
					printf("\x1b[2KIteration (Quality) : ");
					printf("%s", Iterations_names[max_iterations]);
				break;
			}
		} else if (pressed & WPAD_BUTTON_HOME) {
			break;
		} 
		VIDEO_WaitVSync();
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

	WIDTH = rmode->viWidth / 2;
	HEIGHT = rmode->viHeight;

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

	u8 selection = 0;

	printf("\x1b[2J\n\rPlease Select a fractal : %s", fractalname[selection]);
	POSCursor(0, 26);
	printf("HOME : Exit , 1 : Settings , A : Select , L/R : Change");
	POSCursor(0, 0);

	while(1) {
		WPAD_ScanPads();

		int pressed = WPAD_ButtonsDown(0);

		if(pressed & WPAD_BUTTON_RIGHT) {
			if (selection < 1) selection++;
		} else if (pressed & WPAD_BUTTON_LEFT) {
			if (selection > 0) selection--;
		} else if (pressed & WPAD_BUTTON_A) {
			VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
			double time = 0;
			switch(selection) {
				case 0:
					Bifurcation();
				break;

				case 1:
					time = Mandelbrot();
				break;
			}
			if(time) {
				printf("\r%.3fs / Press HOME to go back", time/1000);
			} else {
				printf("\rPress HOME to go back");
			}
			
			while(1) {
				WPAD_ScanPads();
				pressed = WPAD_ButtonsDown(0);
				if (pressed & WPAD_BUTTON_HOME) {
					break;
				}	
			}
			VIDEO_ClearFrameBuffer(rmode, xfb, COLOR_BLACK);
		
		} else if (pressed & WPAD_BUTTON_1) {
			Settings();
			printf("\x1b[2J");
		} else if (pressed & WPAD_BUTTON_HOME) {
			exit(0);
		}
		if(pressed) {
			printf("\n\x1b[2K\rPlease Select a fractal : %s", fractalname[selection]);
			POSCursor(0, 26);
			printf("HOME : Exit , 1 : Settings , A : Select , L/R : Change");
			POSCursor(0, 0);
		}
		VIDEO_WaitVSync();
	}
	return 0;
}
