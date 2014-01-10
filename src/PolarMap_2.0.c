/** \file
 * World map clock.
 *
 * Image from: http://commons.wikimedia.org/wiki/File:Northern_Hemisphere_Azimuthal_projections.svg
 *
 * Feature ideas:
 * - Sunset/sunrise terminator markers (seasonally adjusted)
 * - Moon phase?
 */
#include <pebble.h>

// Update once per second to test things
#undef CONFIG_DEBUG

static const int gmt_offset = +1; // hours from GMT

static Window *window;
static RotBitmapLayer *mapLayer;
static GBitmap *mapBitmap;
static Layer *marker_layer;

static char time_text[8];
static TextLayer *time_layer;

static GFont font;

/** Update the line markers. */
static void marker_layer_update(Layer *me, GContext * ctx) {
	for (int i = 84 ; i < 168 ; i += 8) {
		graphics_draw_line(ctx, GPoint(72, i), GPoint(72, i+1));
	}
}



/** Called once per minute.
 * Update the map rotation angle and flag it as dirty to force
 * a redraw..
 */
static void handle_tick(struct tm *tick_time, TimeUnits units_changed) {
#ifdef CONFIG_DEBUG
	// Fake the time
	tick_time->tm_hour = 0;
	tick_time->tm_min = 0;
#endif

	// Adjust hour by GMT offset and reverse angle
	// Note that this image is has GMT "noon" at the bottom.
	// (world rotates "backwards" relative to the sun)
	const int32_t angle = 0
		+ (TRIG_MAX_ANGLE * gmt_offset) / 24
		+ (TRIG_MAX_ANGLE * (tick_time->tm_hour + gmt_offset)) / 24
		+ (TRIG_MAX_ANGLE * tick_time->tm_min) / (60 * 24);

	const int q = (((angle + TRIG_MAX_ANGLE) & TRIG_MAX_RATIO) * 32) / TRIG_MAX_ANGLE;

	// Check for quadrants to determine where to move the time
	Layer *layer = text_layer_get_layer(time_layer);
	GRect frame = layer_get_frame(layer);
	if (q <= 7) {
		// Upper left
		frame.origin = GPoint(0,-6);
	} else if (q <= 15) {
		// Lower left
		frame.origin = GPoint(0,150);
	} else if (q <= 23) {
		// Lower right
		frame.origin = GPoint(90, 150);
	} else if (q <= 31) {
		// Upper right
		frame.origin = GPoint(90, -6);
	}
	layer_set_frame(layer, frame);

	snprintf(time_text, sizeof(time_text), "%.2d:%.2d", tick_time->tm_hour, tick_time->tm_min);
	text_layer_set_text(time_layer, time_text);

	rot_bitmap_layer_set_angle(mapLayer, -angle);
}


static void text_layer(TextLayer **layer, GRect frame, GFont font) {
	*layer = text_layer_create(frame);
	text_layer_set_text(*layer, "");
#if 0
	// white bg to help with layout
	text_layer_set_text_color(*layer, GColorBlack);
	text_layer_set_background_color(*layer, GColorWhite);
#else
	// black bg for real
	text_layer_set_text_color(*layer, GColorWhite);
	text_layer_set_background_color(*layer, GColorBlack);
#endif

	text_layer_set_font(*layer, font);

	layer_add_child(window_get_root_layer(window), text_layer_get_layer(*layer));
}


static void init() {
	window = window_create();
	window_set_background_color(window, GColorBlack);
	window_stack_push(window, false);

	// This doesn't need the transparency, but there is no
	// rotbmp that isn't a pair?
	
	mapBitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_MAP);
	mapLayer = rot_bitmap_layer_create(mapBitmap);
	rot_bitmap_set_src_ic(mapLayer, GPoint(100,100));

	// No dest_ic routines?  Relocate the point of rotation to
	// the center of the map for both layers.
	//map_container.layer.white_layer.dest_ic = GPoint(144/2, 168/2);
	//map_container.layer.black_layer.dest_ic = GPoint(144/2, 168/2);
	GRect frame = layer_get_frame((Layer *)mapLayer);
	frame.origin = GPoint(72-141, 84-141);
	layer_set_frame((Layer *)mapLayer, frame);
	layer_add_child(window_get_root_layer(window), (Layer *)mapLayer);

	font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_SOURCECODEPRO_LIGHT_18));
	text_layer(&time_layer, GRect(90, -6, 60, 20), font);

	marker_layer = layer_create(GRect(0, 0, 144, 168));
	layer_set_update_proc(marker_layer, marker_layer_update);
	layer_add_child(window_get_root_layer(window), marker_layer);
	layer_mark_dirty(marker_layer);
	
#ifdef CONFIG_DEBUG
	tick_timer_service_subscribe(SECOND_UNIT, handle_tick);
#else
	tick_timer_service_subscribe(MINUTE_UNIT, handle_tick);
#endif
}


static void deinit() {
	tick_timer_service_unsubscribe();
	
	layer_destroy(marker_layer);
	text_layer_destroy(time_layer);
	
	fonts_unload_custom_font(font);
	rot_bitmap_layer_destroy(mapLayer);
	gbitmap_destroy(mapBitmap);
	
	window_destroy(window);
}


int main(void) {
	init();
	app_event_loop();
	deinit();
}
