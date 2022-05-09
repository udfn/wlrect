#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <cairo.h>
#include <signal.h>
#include <wayland-client.h>
#include <nwl/nwl.h>
#include <nwl/surface.h>
#include <nwl/cairo.h>
#include "wlr-layer-shell-unstable-v1.h"

#define DASH_LENGTH 12.0
struct nwl_surface *main_surface;

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

int main (int argc, char *argv[]) {
	struct nwl_state state = {0};
	int retval = 1;
	if (nwl_wayland_init(&state)) {
		return 1;
	}
	if (state.wl.layer_shell == NULL) {
		fprintf(stderr, "No layer shell. I need it.\n");
		goto finish;
	}
	if (argc < 2) {
		puts("I need to know where to be: \"x,y widthxheight\"");
		goto finish;
	}
	int x, y, width, height;
	if (sscanf(argv[1], "%i,%i %ix%i", &x, &y, &width, &height) != 4) {
		puts("I need to know where to be: \"x,y widthxheight\"");
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
	wl_surface_commit(main_surface->wl.surface);
	wl_region_destroy(reg);
	nwl_wayland_run(&state);
	retval = 0;
finish:
	nwl_wayland_uninit(&state);
	return retval;
}
