#define _POSIX_C_SOURCE 200809L
#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <nwl/nwl.h>
#include <nwl/surface.h>
#include <nwl/cairo.h>
#include "wlr-layer-shell-unstable-v1.h"

#define DASH_LENGTH 12.0
struct nwl_surface *main_surface;

struct wlrect_button {
	char *text;
	double width;
};

int rect_buttons_num = 0;
bool rect_clickdown = false;
bool rect_clicked_button = false;
int rect_hovered_button = -1;
struct wlrect_button *rect_buttons = NULL;

bool rect_show_timer = false;
struct timespec time_start;
int time_update_fd = -1;

static void rect_render(struct nwl_surface *surface, cairo_surface_t *cairo_surface) {
	cairo_t *cr = cairo_create(cairo_surface);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
	cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_scale(cr, surface->scale, surface->scale);
	cairo_rectangle(cr, 1, 1, surface->width-2, surface->height-2);
	cairo_set_line_width(cr, 2);
	double dashes[1];
	dashes[0] = DASH_LENGTH;
	cairo_set_dash(cr, dashes, 1, 0.0);
	cairo_set_source_rgba(cr, 0.8, 0.1, 0.0, 0.85);
	cairo_stroke(cr);
	cairo_rectangle(cr, 1, 1, surface->width-2, surface->height-2);
	cairo_set_dash(cr, dashes, 1, DASH_LENGTH);
	cairo_set_source_rgba(cr, 0.8, 0.43, 0.1, 0.85);
	cairo_stroke(cr);
	cairo_destroy(cr);
	nwl_surface_swapbuffers(surface, 0, 0);
}

static void rect_sub_render(struct nwl_surface *surface, cairo_surface_t *cairo_surface) {
	cairo_t *cr = cairo_create(cairo_surface);
	cairo_set_source_rgba(cr, 0.2, 0.025, 0.0, 0.85);
	cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
	cairo_paint(cr);
	cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
	cairo_set_font_size(cr, 14);
	cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
	double cur_x_pos = 1;
	for (int i = 0; i < rect_buttons_num; i++) {
		struct wlrect_button *cur_b = &rect_buttons[i];
		if (i == rect_hovered_button) {
			cairo_set_source_rgba(cr, 0.45, 0.45, 0.45, 0.85);
		} else {
			cairo_set_source_rgba(cr, 0.4, 0.4, 0.4, 0.6);
		}
		cairo_rectangle(cr, cur_x_pos, 1, cur_b->width, surface->height-2);
		cairo_fill(cr);
		cairo_rectangle(cr, cur_x_pos, 1, cur_b->width, surface->height-2);
		if (i == rect_hovered_button) {
			cairo_set_source_rgba(cr, 0.80, 0.80, 0.80, 0.95);
		} else {
			cairo_set_source_rgba(cr, 0.6, 0.6, 0.6, 0.9);
		}
		cairo_stroke(cr);
		cairo_set_source_rgba(cr, 0.96, 0.96, 0.96, 1.0);
		cairo_move_to(cr, cur_x_pos + 4, 16);
		cairo_show_text(cr, cur_b->text);
		cur_x_pos += 4 + cur_b->width;
	}
	if (rect_show_timer) {
		char timestr[8];
		struct timespec new_time;
		clock_gettime(CLOCK_MONOTONIC, &new_time);
		int secdiff = new_time.tv_sec-time_start.tv_sec;
		int nsecdiff = new_time.tv_nsec-time_start.tv_nsec;
		if (nsecdiff < 0) {
			nsecdiff += 1000000000;
			secdiff -= 1;
		}
		nsecdiff /= 100000000;
		snprintf(timestr, 8, "%02d:%02d.%d", secdiff / 60, secdiff % 60, nsecdiff);
		cairo_set_source_rgba(cr, 0.9, 0.9, 0.9, 1.0);
		cairo_move_to(cr, cur_x_pos, 16);
		cairo_show_text(cr, timestr);
	}
	cairo_destroy(cr);
	nwl_surface_swapbuffers(surface, 0, 0);
}

static void sub_handle_pointer(struct nwl_surface *surface, struct nwl_seat *seat, struct nwl_pointer_event *event) {
	if (event->changed & NWL_POINTER_EVENT_FOCUS && !event->focus) {
		rect_hovered_button = -1;
		nwl_surface_set_need_draw(surface, false);
		rect_clickdown = false;
		return;
	}
	bool need_redraw = false;
	bool just_released = false;
	if (event->changed & NWL_POINTER_EVENT_BUTTON) {
		if (event->buttons & NWL_MOUSE_LEFT) {
			rect_clickdown = true;
		} else if (!(event->buttons & NWL_MOUSE_LEFT) && event->buttons_prev & NWL_MOUSE_LEFT) {
			rect_clickdown = false;
			just_released = true;
		}
	}
	if (event->changed & NWL_POINTER_EVENT_MOTION || just_released) {
		int pointer_x = wl_fixed_to_int(event->surface_x);
		int accum_x = 0;
		bool found = false;
		for (int i = 0; i < rect_buttons_num; i++) {
			struct wlrect_button *cur_b = &rect_buttons[i];
			if (pointer_x > accum_x && pointer_x < (accum_x + cur_b->width+2) && !found) {
				if (i == rect_hovered_button) {
					if (just_released) {
						rect_clicked_button = true;
						surface->state->num_surfaces = 0;
						return;
					}
					found = true;
					break;
				}
				if (!rect_clickdown) {
					need_redraw = true;
					rect_hovered_button = i;
					found = true;
				}
			}
			accum_x += cur_b->width + 4;
		}
		if (!found) {
			rect_hovered_button = -1;
		}
	}
	if (need_redraw) {
		nwl_surface_set_need_draw(surface, false);
	}
}

static void handle_timer(struct nwl_state *state, void *data) {
	struct nwl_surface *sub = data;
	nwl_surface_set_need_draw(sub, false);
	char expire_count[8];
	read(time_update_fd, expire_count, 8);
}

struct nwl_output *find_output(struct nwl_state *state, int32_t x, int32_t y) {
	struct nwl_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		if (x >= output->x && x < output->x + output->width &&
				y >= output->y && y < output->y + output->height) {
			return output;
		}
	}
	return NULL;
}

static void handle_term() {
	main_surface->state->num_surfaces = 0; // This hack again :/
}

static void print_usage(char *arg) {
	printf("Usage: %s [options] rect\n", arg);
	puts("(where rect is a rectangle defined like \"25,25 100x50\")");
	puts("Options...");
	puts("-t          Show a timer.");
	puts("-b label    Add a clickable button with the label \"label\".");
	puts("            Clicking it closes wlrect and writes the button label to stdout.");
	puts("            Can be used multiple times if more buttons are desired.");
}

int main (int argc, char *argv[]) {
	struct nwl_state state = {0};
	int retval = 1;
	int opt;
	while ((opt = getopt(argc, argv, "tb:")) != -1) {
		switch (opt) {
			case 't':
			rect_show_timer = true;
			break;
			case 'b': {
				rect_buttons = realloc(rect_buttons, sizeof(struct wlrect_button)*++rect_buttons_num);
				struct wlrect_button *new_b = &rect_buttons[rect_buttons_num-1];
				new_b->text = strdup(optarg);
				new_b->width = (strlen(new_b->text) * 11) + 8;
			}
			break;
			default:
			print_usage(argv[0]);
			return 0;
		}
	}
	if (argc-optind < 1) {
		puts("I need to know where to be: \"x,y widthxheight\"");
		return 1;
	}
	int x, y, width, height;
	if (sscanf(argv[optind], "%i,%i %ix%i", &x, &y, &width, &height) != 4) {
		puts("I need to know where to be: \"x,y widthxheight\"");
		return 1;
	}
	if (nwl_wayland_init(&state)) {
		return 1;
	}
	if (state.wl.layer_shell == NULL) {
		fprintf(stderr, "No layer shell. I need it.\n");
		goto finish;
	}

	// Might also want to handle multiple outputs...
	struct nwl_output *output = find_output(&state, x, y);
	if (output == NULL) {
		fprintf(stderr, "Couldn't find output!\n");
		goto finish;
	}
	struct sigaction saction = {0};
	saction.sa_handler = handle_term;
	sigaction(SIGTERM, &saction, NULL);
	saction.sa_handler = handle_term;
	sigaction(SIGINT, &saction, NULL);
	x -= output->x + 2;
	y -= output->y + 2;
	int x2 = output->width - (x + width + 4);
	int y2 = output->height - (y + height + 4);
	main_surface = nwl_surface_create(&state, "wlrect");
	nwl_surface_renderer_cairo(main_surface, false, rect_render);
	nwl_surface_role_layershell(main_surface, output->output, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
	zwlr_layer_surface_v1_set_exclusive_zone(main_surface->role.layer.wl, -1);
	zwlr_layer_surface_v1_set_anchor(main_surface->role.layer.wl, ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM|
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP|ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT|ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
	zwlr_layer_surface_v1_set_margin(main_surface->role.layer.wl, y, x2, y2, x);
	struct wl_region *reg = wl_compositor_create_region(state.wl.compositor);
	wl_surface_set_input_region(main_surface->wl.surface, reg);
	wl_region_destroy(reg);
	if (rect_show_timer || rect_buttons_num) {
		struct wl_region *sub_reg = wl_compositor_create_region(state.wl.compositor);
		struct nwl_surface *sub = nwl_surface_create(&state, "wlrect sub");
		int sub_y_pos = y2 < 24 && y > y2 ? -23 : height + 4;
		int sub_width = 0;
		for (int i = 0; i < rect_buttons_num; i++) {
			sub_width += rect_buttons[i].width + 4;
		}
		if (sub_width > 0) {
			sub_width -= 2;
			sub->impl.input_pointer = sub_handle_pointer;
			wl_region_add(sub_reg, 0, 0, sub_width - 5, 23);
		}
		nwl_surface_renderer_cairo(sub, false, rect_sub_render);
		nwl_surface_role_subsurface(sub, main_surface);
		wl_subsurface_set_position(sub->role.subsurface.wl, 0, sub_y_pos);
		wl_surface_set_input_region(sub->wl.surface, sub_reg);
		wl_region_destroy(sub_reg);
		if (rect_show_timer) {
			clock_gettime(CLOCK_MONOTONIC, &time_start);
			time_update_fd = timerfd_create(CLOCK_MONOTONIC, 0);
			struct itimerspec ts = {
				.it_interval.tv_sec = 0,
				.it_interval.tv_nsec = 100000000,
				.it_value.tv_sec = 0,
				.it_value.tv_nsec = 100000000
			};
			timerfd_settime(time_update_fd, 0, &ts, NULL);
			nwl_poll_add_fd(&state, time_update_fd, handle_timer, sub);
			sub_width += (8*8) + 4;
		}
		nwl_surface_set_size(sub, sub_width, 23);
		nwl_surface_set_need_draw(sub, false);
		wl_surface_commit(sub->wl.surface);
		wl_subsurface_set_desync(sub->role.subsurface.wl);

	}
	wl_surface_commit(main_surface->wl.surface);
	nwl_wayland_run(&state);
	retval = 0;
finish:
	nwl_wayland_uninit(&state);
	if (time_update_fd == -1) {
		close(time_update_fd);
	}
	if (rect_clicked_button) {
		puts(rect_buttons[rect_hovered_button].text);
	}
	if (rect_buttons_num) {
		for (int i = 0; i < rect_buttons_num; i++) {
			free(rect_buttons[i].text);
		}
		free(rect_buttons);
	}
	return retval;
}
