#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define ARRAY_LENGTH 10
#define ARRAY_LIMIT 100

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

#define CUBE_LENGTH 10

volatile int pixel_buffer_start; // global variable

int max_height = 0;
int max_i = 0, max_j = 0;

int array[ARRAY_LENGTH][ARRAY_LENGTH];

// BACK-END FUNCTIONS
void update_max() {
    max_height = 0;

    for (int i = 0; i < ARRAY_LENGTH; ++i) {
        for (int j = 0; j < ARRAY_LENGTH; ++j) {
            if (array[i][j] > max_height) {
                // update the max number
                max_height = array[i][j];
                // update the index of the max number
                max_i = i; max_j = j;
            }
        }
    }
}

int pop_max() {
    // save the max number
    int temp = array[max_i][max_j];
    array[max_i][max_j] = 0;

    // update the max number
    update_max();

    return temp;
}

void populate_array() {
    // seed random numbers
    srand(time(0));

    // populate the array
    for (int i = 0; i < ARRAY_LENGTH; ++i)
        for (int j = 0; j < ARRAY_LENGTH; ++j)
            array[i][j] = rand() % ARRAY_LIMIT + 1;

    update_max();
}

// FRONT-END FUNCTIONS
void plot_pixel(int x, int y, short int line_color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT)
        *(short int *)(pixel_buffer_start + (y << 10) + (x << 1)) = line_color;
}

void clear_screen(){
    for (int y = 0; y < SCREEN_HEIGHT; y++)
        for (int x = 0; x < SCREEN_WIDTH; x++)
            plot_pixel(x, y, 0x0);
}

void fill_rect(int x1, int y1, int x2, int y2, short int colour) {
    for (int i = x1;  i < x2; i++)
        for (int j = y1; j < y2; j++)
            plot_pixel(i, j, colour);
}

void draw_spaghetti(int x, int y, int height) {
    if (height != 0) {
        // ceiling
        for (int k = 0; k < CUBE_LENGTH; ++k)
            for (int i = x - (CUBE_LENGTH - k);  i < (x + k) + 1; i++)
                plot_pixel(i, (y - height) + k - (i - (x - CUBE_LENGTH))/2, 0x001F);

        // left side
        for (int i = x - CUBE_LENGTH;  i < x; i++)
            for (int j = y - height; j < y; j++)
                plot_pixel(i, j + (i - (x - CUBE_LENGTH))/2, 0x100F);

        // right side
        for (int i = x;  i < x + CUBE_LENGTH; i++)
            for (int j = y - height; j < y; j++)
                plot_pixel(i, j - (i - (x + CUBE_LENGTH))/2, 0x020F);
    }
}

void wait_for_vsync(){
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;  // buffer register of pixel buffer controller
    volatile int * status_ptr = (int *)0xFF20302C;      // status register of pixel buffer controller

    // write a 1 to the buffer register
    *pixel_ctrl_ptr = 0x01;

    int status = *status_ptr & 0x01;                    // get the S bit from status register
    // wait for the S bit to become 0
    while (status != 0)
        status = *status_ptr & 0x1;
}

int main() {
    volatile int * pixel_ctrl_ptr = (int *)0xFF203020;
    /* Read location of the pixel buffer from the pixel buffer controller */
    pixel_buffer_start = *pixel_ctrl_ptr;

    //set front pixel buffer to start of FPGA On-chip memory
    *(pixel_ctrl_ptr + 1) = 0xC8000000; // first store the address in the back buffer
    //now, swap the front/back buffers, to set the front buffer location
    wait_for_vsync();
    /* initialize a pointer to the pixel buffer, used by drawing functions */
    pixel_buffer_start = *pixel_ctrl_ptr;

    clear_screen(); // pixel_buffer_start points to the pixel buffer

    // set back pixel buffer to start of SDRAM memory
    *(pixel_ctrl_ptr + 1) = 0xC0000000;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1); // we draw on the back buffer

    // position of the scanning bar
    float scan_height = -50.0;
    float d_scan_height = 0.0;
    float dd_scan_height = 0.15;

    // initialize array
    populate_array();

    while (true) {
        clear_screen();

        // move the scanning bar
        if (d_scan_height < 5)
            d_scan_height += dd_scan_height;
        if (max_height != 0)
            scan_height += d_scan_height;

        // check for max spaghetti
        if ((scan_height + 35) >= SCREEN_HEIGHT/2 + 50 - max_height) {
            d_scan_height = 0;
            pop_max();
        }

        // draw scanning bar
        for (int i = 35;  i < 35 + (CUBE_LENGTH*1.25*ARRAY_LENGTH); i++) {
            plot_pixel(i + (CUBE_LENGTH*1.25*ARRAY_LENGTH), scan_height + i/2 - (CUBE_LENGTH*1.25*ARRAY_LENGTH)*(2/4.), 0xFFFF);
            plot_pixel(i, scan_height - i/2 + (CUBE_LENGTH*1.25*ARRAY_LENGTH)*(1/4.) + 3, 0xFFFF);
        }

        //draw data bars
        for (int i = 0; i < ARRAY_LENGTH; i++)
            for (int j = ARRAY_LENGTH - 1; j > 0; j--)
                draw_spaghetti(35 + (i + j)*(CUBE_LENGTH)*1.25, SCREEN_HEIGHT/2 + 50 + (i - j)*((CUBE_LENGTH*1.25)/2), array[i][j]);

        // draw scanning bar
        for (int i = 35;  i < 35 + (CUBE_LENGTH*1.25*ARRAY_LENGTH); i++) {
            plot_pixel(i, scan_height + i/2, 0xFFFF);
            plot_pixel(i + (CUBE_LENGTH*1.25*ARRAY_LENGTH), scan_height - i/2 + (CUBE_LENGTH*1.25*ARRAY_LENGTH)*(3/4.) + 3, 0xFFFF);
        }

        wait_for_vsync(); // swap front and back buffers on VGA vertical sync
        pixel_buffer_start = *(pixel_ctrl_ptr + 1); // new back buffer
    }
}
