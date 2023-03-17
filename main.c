#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <time.h>
#include <stdint.h>

#define BOARD_IMPLEMENTATION
#include "board.h"

#define TRY(r,t) ({               \
		t _temp_result = (r);         \
		if (!_temp_result.valid)      \
		   _temp_result.raise_error();\
		_temp_result.value;           \
	  })

#define TRY_NONNULL(expression, msg) do{if((expression) == NULL){die(msg);}}while(0)

static void
die(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	exit(1);
}

Display *dpy;
Window  win;
Pixmap  backbuffer;
XWindowAttributes win_attrs;
Atom wm_delete_window;
XFontStruct *font;
GC board_gc;
GC temp_gc;
GC cell_font_gc;
int window_width;
int window_height;

#include <math.h>

int 
my_random(void) {
	static int last = 30123;
	srand(last);
	int value = rand();
	last = value;
	return value;
}

static void
set_color_for_value(int value) {
	int color = 0xb1a194;
	switch(value) {
		case 2:
			color = 0xd5c7bc;
			break;
		case 4:
			color = 0x008855;
			break;
		case 8:
			color = 0xec8f52;
			break;
		case 16:
			color = 0xe9733f;
			break;
		case 32:
			color = 0xec5938;
			break;
		case 64:
			color = 0xea3e1a;
			break;
		case 128:
			color = 0xdbae49;
			break;
		case 256:
			color = 0xdeac3d;
			break;
		case 512:
			color = 0xdea82e;
			break;
		case 1024:
			color = 0xdea523;
			break;
		case 2048:
			color = 0xff1122;
			break;
	}
	XSetForeground(dpy, board_gc, color);
}

static void
draw_cell(int x, int y, int w, int h, int value) {	
	static char text[64];
	size_t text_len = 0;
	int color = 0xcdc1b4;
	set_color_for_value(value);
	
	XFillRectangle(dpy, backbuffer, board_gc, x, y, w, h);
	
	if (value) {
	text_len = snprintf(text,64,"%d",value);

	int text_w = XTextWidth(font, text, text_len);
	int text_h = font->ascent + font->descent;

	XDrawString(dpy, backbuffer, cell_font_gc, 
			x + (w/2) - text_w/2,
			y + (h/2) - text_h/2 + font->ascent,
			text,text_len);
	}
}

static inline void
calculate_cell_draw_position(int row, int column, int cell_size, int spacing, int *out_x, int *out_y) {
	*out_y = (row * cell_size) + (row * spacing);
	*out_x = (column * cell_size) + (column * spacing);
}

static void
draw_board(board_t *board) {
	int color = 0xbbada0;
	XSetForeground(dpy, board_gc, color);
	XFillRectangle(dpy,backbuffer,board_gc,0,0,window_width,window_height);

	int spacing = 10;
	int start_x = spacing;
	int start_y = spacing;
	int min_window_size = window_width;

	if (window_width > window_height) {
		min_window_size = window_height;
		start_x += (window_width / 2) - (window_height/2);
	} else {
		start_y += (window_height/2) - (window_width/2);
	}

	int total_spacing = spacing * (board->size + 1);
	int space_for_cells = min_window_size - total_spacing;
	int cell_size = space_for_cells / board->size;

	for (int row = 0; row < board->size; row++) {
		for (int column = 0; column < board->size; column++) {
			int index = board_tile_index(board, column, row); 
      int cell_x, cell_y;
      calculate_cell_draw_position(row, column, cell_size, spacing, &cell_x, &cell_y);
      cell_x += spacing; cell_y += spacing;
      draw_cell(cell_x + start_x - spacing, cell_y, cell_size, cell_size, board->tiles[index]);
      printf("(%d,%d).\n", cell_x, cell_y);
		}
	}
}

static inline
struct timespec timespec_from_u64(uint64_t nanoseconds) {
	struct timespec result = {0};
	result.tv_sec  = (nanoseconds / (uint64_t)1000000000L);
	result.tv_nsec = nanoseconds - (result.tv_sec * 1000000000L);
	return (result);
}

static inline
uint64_t getWallClock(void) {
	struct timespec result_time = {0};
	uint64_t result = 0;
	clock_gettime(CLOCK_MONOTONIC, &result_time);
	result = result_time.tv_sec * (uint64_t)1000000000L + result_time.tv_nsec;
	return (result);
}

int
main(int argc, char **argsv) {

	TRY_NONNULL(dpy = XOpenDisplay(NULL), "Cannot Open Display");

	TRY_NONNULL(font = XLoadQueryFont(dpy, "fixed"), "Cannot Open Font");

	win = XCreateSimpleWindow(dpy, 
			                      DefaultRootWindow(dpy),
														0, 0, 
														400, 400, 
														0,
														0,
														0xFFFFFF);

	backbuffer = XCreatePixmap(dpy, win, 1366, 768, 24);

	XGetWindowAttributes(dpy, win, &win_attrs);
	wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wm_delete_window, 1);
	
	board_gc = XCreateGC(dpy, win, 0, NULL);
	temp_gc = XCreateGC(dpy, win, 0, NULL);
	XSetForeground(dpy, temp_gc, 0x00AAFF);

	cell_font_gc = XCreateGC(dpy, win, 0, NULL);
	XSetFont(dpy, cell_font_gc, font->fid);

	XSetForeground(dpy, cell_font_gc, 0x000000);

	XSelectInput(dpy, win, StructureNotifyMask | KeyPressMask | ExposureMask);
	XMapWindow(dpy, win);
	XStoreName(dpy, win, "2048");
	XSync(dpy, False);

	window_width = 400;
	window_height = 400;

	board_t board = board_create(my_random);

	XEvent event;
	int quit = 0;
	int redraw = 0;

	float target_fps =  60;
	float target_ms_per_frame = (1000.0f / target_fps);
	uint64_t target_nsec_per_frame = (uint64_t)target_ms_per_frame * 1000000L;

	printf("Target ms per frame: %.2f\n", target_ms_per_frame);
	printf("Target ns per frame: %ld\n", (long)target_nsec_per_frame);

	uint64_t last_counter = getWallClock();

	while(!quit) {

		while(XPending(dpy)) {
			XNextEvent(dpy, &event);
			switch(event.type){
				case ClientMessage: {
						if(event.xclient.data.l[0] == wm_delete_window) {
							quit = 1;
						}
				} break;
				case ConfigureNotify:
				{
					int new_w = event.xconfigure.width;
					int new_h = event.xconfigure.height;

					if (new_w != window_width || new_h != window_height) {
						window_height = new_h;
						window_width = new_w;
						redraw = 1;
					}

				} break;
				case Expose:
				{
					if (event.xexpose.count == 0) {
						redraw = 1;
					}
				} break;
				case KeyPress: {
						switch(XLookupKeysym(&event.xkey,0)) {
							case 'q': {
								quit = 1;
							} break;
							case XK_Left: {
								printf("Key: LEFT\n");
								board_move_left(&board);
								redraw = 1;
							} break;
							case XK_Right: {
								printf("Key: Right\n");
								board_move_right(&board);
								redraw = 1;
							} break;
							case XK_Up: {
								printf("Key: Up\n");
								board_move_up(&board);
								redraw = 1;
							} break;
							case XK_Down: {
								printf("Key: Down\n");
								board_move_down(&board);
								redraw = 1;
							} break;
							default: {
								printf("Key: %d\n", event.xkey.keycode);
							}
						}
				} break;
			}
		}
		
		if (redraw) {
			draw_board(&board);
			XSync(dpy,False);
			// Swap buffer
			XCopyArea(dpy, backbuffer, win, temp_gc, 0, 0, window_width, window_height, 0, 0);
			redraw = 0;
			static int frame = 0;
			printf("Frame: %d.\n", frame++);
		}

		uint64_t target_counter = last_counter + target_nsec_per_frame;

		uint64_t work_counter = getWallClock();

		if (work_counter < target_counter) {
			uint64_t sleep_counter = target_counter - work_counter;
			struct timespec sleep_counter_timespec = timespec_from_u64(sleep_counter);
			nanosleep(&sleep_counter_timespec, NULL); 
		}

		uint64_t  end_counter = getWallClock();

		#if GAME_DEBUG
		uint64_t frame_duration_ns = (end_counter - last_counter);

		printf("Frame duration  %ldns - %ldms - %ldFPS\n",
				(long)(frame_duration_ns),
				(long)(frame_duration_ns / 1000000L),
				(long)(1000L / (frame_duration_ns / 1000000L))
		      );
		#endif

		last_counter = end_counter;

	}

	return 0;
	
}
