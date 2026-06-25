#include <assert.h>
#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/xwayland.h>
#include <wlr/util/log.h>

#include <scenefx/render/fx_renderer/fx_renderer.h>
#include <scenefx/types/fx/blur_data.h>
#include <scenefx/types/fx/corner_location.h>
#include <scenefx/types/wlr_scene.h>

#include <xkbcommon/xkbcommon.h>

#include "types.h"
#include "config.h"
// #include <linux/time.h>


;enum { CurNormal, CurPressed, CurMove, CurResize }; /* cursor */

// Defining which keys count as modifier keybinds.
// If they're detected them they are checked before being passed to client.
int MOD_KEY = WLR_MODIFIER_LOGO;
int OTHER_MOD_KEY = WLR_MODIFIER_ALT;

static void create_decoration(struct wl_listener *listener, void *data);
static void commit_layer_surface_notify(struct wl_listener *listener, void *data);

static void output_mgr_apply(struct wl_listener *listener, void *data);
static void output_mgr_apply_or_test(struct wlr_output_configuration_v1 *config, int test);

static void output_configure_scene(struct wlr_scene_node *node,
    struct quackwm_toplevel *toplevel);

static struct quackwm_toplevel *desktop_toplevel_at(
    struct quackwm_server *server, double lx, double ly,
    struct wlr_surface **surface, double *sx, double *sy);

static void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int sx,
    int sy, void *user_data);

static void add_toplevel_tag(struct quackwm_toplevel *toplevel, int tag);
static void focus_toplevel(struct quackwm_toplevel *toplevel, struct wlr_surface *surface);
static void reset_cursor_mode(struct quackwm_server *server);

/* Global server */
static struct quackwm_server server = {0};

#include <xwayland.c>

/*
 * QuackWM methods
 * Here's the core of everything.
 */

static bool is_visible_on_tag(struct quackwm_toplevel *toplevel, int tag) {
  return (bool)((toplevel->tags & TAG_MASK) & (1u << server.current_tag));
}

static void switch_to_tag(int tag) {
  server.current_tag = tag;

  struct quackwm_toplevel *toplevel;
  wl_list_for_each(toplevel, &server.toplevels, link) {
    // iterate over all toplevels and set visibility according to its tags
    wlr_scene_node_set_enabled(&toplevel->scene_tree->node, is_visible_on_tag(toplevel, (1u << tag)));
  }
}

static void add_toplevel_tag(struct quackwm_toplevel *toplevel, int tag) {
  wlr_log(WLR_INFO, "Adding toplevel to tag %d", tag);
  toplevel->tags |= (1u << tag);
}

static void reset_toplevel_tag(int tag) {
  // sets only one tag the toplevel, removing the previous ones
  wlr_log(WLR_INFO, "Setting toplevel tag %d", tag);

  struct quackwm_toplevel *toplevel = 
        wl_container_of(server.toplevels.next, toplevel, link);

  toplevel->tags = (1u << tag);

  wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);
}

static void remove_toplevel_tag(struct quackwm_toplevel *toplevel, int tag) {
  // creates an inverted binary to perform a bitwise-and with all
  // the tags, excluding the one selected
  long remove_mask = ~(1 << tag);
  toplevel->tags = toplevel->tags & remove_mask;

  if (toplevel->is_xwayland) {
    wlr_xwayland_surface_activate(toplevel->xwayland_surface, is_visible_on_tag(toplevel, tag));
  } else {
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, is_visible_on_tag(toplevel, tag));
  }
}

static void set_keyboard_focus(struct wlr_seat *seat, struct wlr_surface *surface) {
  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if (keyboard != NULL) {
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes,
        keyboard->num_keycodes, &keyboard->modifiers);
  }
}

static void focus_toplevel(struct quackwm_toplevel *toplevel, struct wlr_surface *surface) {
  /* Note: this function only deals with keyboard focus. */
  if (toplevel == NULL) {
    return;
  }
  struct quackwm_server *server = toplevel->server;
  struct wlr_seat *seat = server->seat;

  struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
  struct wlr_surface *prev_surface_pointer = seat->pointer_state.focused_surface;

  if (prev_surface == surface) {
    /* Don't re-focus an already focused surface. */
    return;
  }
  /*if (prev_surface) {
    /*
     * Deactivate the previously focused surface. This lets the client know
     * it no longer has focus and the client will repaint accordingly, e.g.
     * stop displaying a caret.
     
    struct wlr_xdg_toplevel *prev_toplevel =
      wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
    if (prev_toplevel != NULL) {
      wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }
  } */
  /* Move the toplevel to the front */
  wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
  wl_list_remove(&toplevel->link);
  wl_list_insert(&server->toplevels, &toplevel->link);

  wlr_log(WLR_INFO, "FOCUS_TOPLEVEL :: activate and set keyboard focus");
  /* Activate the new surface */
  if (toplevel->is_xwayland) {
    wlr_log(WLR_INFO, "Focus toplevel: Xwayland");
    wlr_xwayland_surface_activate(toplevel->xwayland_surface, true);
    set_keyboard_focus(seat, toplevel->xwayland_surface->surface);
  } else {
    wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, true);
    set_keyboard_focus(seat, toplevel->xdg_toplevel->base->surface);
  }
}

// TODO is this method necessary?
static void unfocus_toplevels(void) {
  // wlr_log(WLR_INFO, "clearing toplevels focus...");
  wlr_seat_keyboard_notify_clear_focus(server.seat);

  struct quackwm_toplevel *toplevel;
	wl_list_for_each_reverse(toplevel, &server.toplevels, link) {
	  // wlr_xdg_toplevel_set_activated(toplevel->xdg_toplevel, false);
    wlr_seat_keyboard_clear_focus(server.seat);
    wlr_scene_node_lower_to_bottom(&toplevel->scene_tree->node);
  }
}

static void close_toplevel(struct quackwm_toplevel *toplevel) {
  if (toplevel->is_xwayland) {
    wlr_xwayland_surface_close(toplevel->xwayland_surface);
  } else {
    wlr_xdg_toplevel_send_close(toplevel->xdg_toplevel);
  }
}

static void exec_shell_command(const char **args) {
  char *prefix = "which ";
  char *suffix = " > /dev/null 2>&1";
  const char *command = args[0];
  char *command_test = malloc(strlen(prefix) + strlen(command) + strlen(suffix) + 1); // +1 for the null-terminator
  // TODO check for errors in malloc here
  strcpy(command_test, prefix);
  strcat(command_test, command);
  strcat(command_test, suffix);

  if (system(command_test)) {
    wlr_log(WLR_INFO, "Command %s not found... skipping", command);
    return;
  }

  if (fork() == 0) {
    dup2(STDERR_FILENO, STDOUT_FILENO);
    setsid();
    execvp(command, (char **)args);
  }
}

//TODO Fix all these things to add keybinding to this one.
int
keybinding(uint32_t mods, xkb_keysym_t sym)
{
	/*
	 * Here we handle compositor keybindings. This is when the compositor is
	 * processing keys, rather than passing them on to the client for its own
	 * processing.
	 */
	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod)
				&& xkb_keysym_to_lower(sym) == xkb_keysym_to_lower(k->keysym)
				&& k->func) {
			k->func(&k->arg);
			return 1;
		}
	}
	return 0;
}

void keyboard_handle_key(struct wl_listener *listener, void *data)
{
	int i;
	/* This event is raised when a key is pressed or released. */
	KeyboardGroup *group = wl_container_of(listener, group, key);
	struct wlr_keyboard_key_event *event = data;

	/* Translate libinput keycode -> xkbcommon */
	uint32_t keycode = event->keycode + 8;
	/* Get a list of keysyms based on the keymap for this keyboard */
	const xkb_keysym_t *syms;
	int nsyms = xkb_state_key_get_syms(group->wlr_group->keyboard.xkb_state, keycode, &syms);

	int handled = 0;
	uint32_t mods = wlr_keyboard_get_modifiers(&group->wlr_group->keyboard);

	wlr_idle_notifier_v1_notify_activity(idle_notifier, server.seat);

	/* On _press_ if there is no active screen locker,
	 * attempt to process a compositor keybinding. */
	if (!locked && event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		for (i = 0; i < nsyms; i++)
			handled = keybinding(mods, syms[i]) || handled;
	}

	if (handled && group->wlr_group->keyboard.repeat_info.delay > 0) {
		group->mods = mods;
		group->keysyms = syms;
		group->nsyms = nsyms;
		wl_event_source_timer_update(group->key_repeat_source,
				group->wlr_group->keyboard.repeat_info.delay);
	} else {
		group->nsyms = 0;
		wl_event_source_timer_update(group->key_repeat_source, 0);
	}

	if (handled)
		return;

	wlr_seat_set_keyboard(server.seat, &group->wlr_group->keyboard);
	/* Pass unhandled keycodes along to the client. */
	wlr_seat_keyboard_notify_key(server.seat, event->time_msec,
			event->keycode, event->state);
}

void
keyboard_handle_modifiers(struct wl_listener *listener, void *data)
{
	/* This event is raised when a modifier key, such as shift or alt, is
	 * pressed. We simply communicate this to the client. */
	KeyboardGroup *group = wl_container_of(listener, group, modifiers);

	wlr_seat_set_keyboard(server.seat, &group->wlr_group->keyboard);
	/* Send modifiers to the client. */
	wlr_seat_keyboard_notify_modifiers(server.seat, &group->wlr_group->keyboard.modifiers);
}

int
keyrepeat(void *data)
{
	KeyboardGroup *group = data;
	int i;
	if (!group->nsyms || group->wlr_group->keyboard.repeat_info.rate <= 0)
		return 0;

	wl_event_source_timer_update(group->key_repeat_source,
			1000 / group->wlr_group->keyboard.repeat_info.rate);

	for (i = 0; i < group->nsyms; i++)
		keybinding(group->mods, group->keysyms[i]);

	return 0;
}

static void keyboard_handle_destroy(struct wl_listener *listener, void *data) {
  /* This event is raised by the keyboard base wlr_input_device to signal
   * the destruction of the wlr_keyboard. It will no longer receive events
   * and should be destroyed.
   */
  struct quackwm_keyboard *keyboard =
    wl_container_of(listener, keyboard, destroy);
  wl_list_remove(&keyboard->modifiers.link);
  wl_list_remove(&keyboard->key.link);
  wl_list_remove(&keyboard->destroy.link);
  wl_list_remove(&keyboard->link);
  free(keyboard);
}

static void server_new_keyboard(struct quackwm_server *server,
    struct wlr_input_device *device) {
  struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);

  struct quackwm_keyboard *keyboard = calloc(1, sizeof(*keyboard));
  keyboard->server = server;
  keyboard->wlr_keyboard = wlr_keyboard;

  /* We need to prepare an XKB keymap and assign it to the keyboard. This
   * assumes the defaults (e.g. layout = "us"). */
  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, &xkb_rules,
      XKB_KEYMAP_COMPILE_NO_FLAGS);

  wlr_keyboard_set_keymap(wlr_keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);
  wlr_keyboard_set_repeat_info(wlr_keyboard, 25, 600);

  /* Here we set up listeners for keyboard events. */
  keyboard->modifiers.notify = keyboard_handle_modifiers;
  wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifiers);

  keyboard->key.notify = keyboard_handle_key;
  wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);

  keyboard->destroy.notify = keyboard_handle_destroy;
  wl_signal_add(&device->events.destroy, &keyboard->destroy);

  wlr_seat_set_keyboard(server->seat, keyboard->wlr_keyboard);

  /* And add the keyboard to our list of keyboards */
  wl_list_insert(&server->keyboards, &keyboard->link);
}

static void server_new_pointer(struct quackwm_server *server,
    struct wlr_input_device *inp_device) {
  /* using code from DWL -- setup libinput configuration */

  struct libinput_device *device;
  if (wlr_input_device_is_libinput(inp_device)
      && (device = wlr_libinput_get_device_handle(inp_device))) {

    if (libinput_device_config_tap_get_finger_count(device)) {
      libinput_device_config_tap_set_enabled(device, tap_to_click);
      libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
      libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
      libinput_device_config_tap_set_button_map(device, button_map);
    }

    if (libinput_device_config_scroll_has_natural_scroll(device))
      libinput_device_config_scroll_set_natural_scroll_enabled(device, natural_scrolling);

    if (libinput_device_config_dwt_is_available(device))
      libinput_device_config_dwt_set_enabled(device, disable_while_typing);

    if (libinput_device_config_middle_emulation_is_available(device))
      libinput_device_config_middle_emulation_set_enabled(device, middle_button_emulation);

    if (libinput_device_config_click_get_methods(device) != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
      libinput_device_config_click_set_method (device, click_method);

    if (libinput_device_config_send_events_get_modes(device))
      libinput_device_config_send_events_set_mode(device, send_events_mode);

    if (libinput_device_config_accel_is_available(device)) {
      libinput_device_config_accel_set_profile(device, accel_profile);
      libinput_device_config_accel_set_speed(device, accel_speed);
    }
  }

  wlr_cursor_attach_input_device(server->cursor, inp_device);
}

static void server_new_input(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new input device becomes
   * available. */
  struct quackwm_server *server =
    wl_container_of(listener, server, new_input);
  struct wlr_input_device *device = data;

  switch (device->type) {
    case WLR_INPUT_DEVICE_KEYBOARD:
      server_new_keyboard(server, device);
      break;
    case WLR_INPUT_DEVICE_POINTER:
      server_new_pointer(server, device);
      break;
    default:
      break;
  }
  /* We need to let the wlr_seat know what our capabilities are, which is
   * communiciated to the client. In TinyWL we always have a cursor, even if
   * there are no pointer devices, so we always include that capability. */
  uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
  if (!wl_list_empty(&server->keyboards)) {
    caps |= WL_SEAT_CAPABILITY_KEYBOARD;
  }
  wlr_seat_set_capabilities(server->seat, caps);
}

static void seat_request_cursor(struct wl_listener *listener, void *data) {
  struct quackwm_server *server = wl_container_of(
      listener, server, request_cursor);
  /* This event is raised by the seat when a client provides a cursor image */
  struct wlr_seat_pointer_request_set_cursor_event *event = data;
  struct wlr_seat_client *focused_client =
    server->seat->pointer_state.focused_client;
  /* This can be sent by any client, so we check to make sure this one is
   * actually has pointer focus first. */
  if (focused_client == event->seat_client) {
    /* Once we've vetted the client, we can tell the cursor to use the
     * provided surface as the cursor image. It will set the hardware cursor
     * on the output that it's currently on and continue to do so as the
     * cursor moves between outputs. */
    wlr_cursor_set_surface(server->cursor, event->surface,
        event->hotspot_x, event->hotspot_y);
  }
}

static void seat_request_set_selection(struct wl_listener *listener, void *data) {
  /* Event raised by the seat when a client wants to set the selection,
   * usually when the user copies something. wlroots allows compositors to
   * ignore such requests if they so choose, but in quackwm we always honor
   */
  struct quackwm_server *server = wl_container_of(
      listener, server, request_set_selection);
  struct wlr_seat_request_set_selection_event *event = data;
  wlr_seat_set_selection(server->seat, event->source, event->serial);
}

static void seat_request_set_primary_selection(struct wl_listener *listener, void *data)
{
  struct quackwm_server *server = wl_container_of(
      listener, server, request_set_primary_selection);
	struct wlr_seat_request_set_primary_selection_event *event = data;
	wlr_seat_set_primary_selection(server->seat, event->source, event->serial);
}

static void seat_request_start_drag(struct wl_listener *listener, void *data) {
  /* Event is raised by the seat when a client starts dragging */
  struct quackwm_server *server = wl_container_of(
      listener, server, request_start_drag);
  /*
  struct wlr_seat_request_start_drag_event *event = data;
  wlr_seat_set_selection(server->seat, event->drag->source, event->serial);*/

  struct wlr_seat_request_start_drag_event *event = data;
  // wlr_log(WLR_INFO, "########### Drag REQUEST");

	if (wlr_seat_validate_pointer_grab_serial(server->seat, event->origin,
			event->serial))
		wlr_seat_start_pointer_drag(server->seat, event->drag, event->serial);
	else
		wlr_data_source_destroy(event->drag->source);
}

static struct quackwm_toplevel *desktop_toplevel_at(
    struct quackwm_server *server, double lx, double ly,
    struct wlr_surface **surface, double *sx, double *sy) {
  /* This returns the topmost node in the scene at the given layout coords.
   * We only care about surface nodes as we are specifically looking for a
   * surface in the surface tree of a quackwm_toplevel. */
  struct wlr_scene_node *node = wlr_scene_node_at(
      &server->scene->tree.node, (int)lx, (int)ly, sx, sy);

  if (node == NULL || node->type != WLR_SCENE_NODE_BUFFER) {
    return NULL;
  }

  struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
  struct wlr_scene_surface *scene_surface =
    wlr_scene_surface_try_from_buffer(scene_buffer);
  if (!scene_surface) {
    return NULL;
  }

  *surface = scene_surface->surface;

  /* Find the node corresponding to the quackwm_toplevel at the root of this
   * surface tree, it is the only one for which we set the data field. */
  struct wlr_scene_tree *tree = node->parent;

  while (tree != NULL && tree->node.data == NULL && tree->node.parent != NULL) {
    tree = tree->node.parent;
  }

  return tree->node.data;
}

static void reset_cursor_mode(struct quackwm_server *server) {
  // Reset the cursor mode to passthrough.
  wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
  server->cursor_mode = QUACKWM_CURSOR_PASSTHROUGH;

  server->resizing = 0;
  server->grabbed_toplevel = NULL;
}

static uint32_t resize_toplevel(struct quackwm_toplevel *toplevel) {
  int32_t width = toplevel->server->new_geom.width;
  int32_t height = toplevel->server->new_geom.height;

  if (toplevel->is_xwayland) {
    if (width == toplevel->xwayland_surface->width && height == toplevel->xwayland_surface->height) {
      return 0;
    }
    wlr_xwayland_surface_configure(toplevel->xwayland_surface,
        toplevel->xwayland_surface->x, toplevel->xwayland_surface->y, width, height);
    return 0;
  }
  else {
    if (width == (int32_t)toplevel->xdg_toplevel->base->surface->current.width
        && height == (int32_t)toplevel->xdg_toplevel->base->surface->current.height)
      return 0;
  }

  wlr_xdg_toplevel_set_bounds(toplevel->xdg_toplevel, width, height);
  toplevel->server->resizing = wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, width, height);

  // as seen on dwl/dwl.c:2213, but here something odd happens
  // maybe a bug because of shadows rendering?
  // wlr_scene_subsurface_tree_set_clip(&toplevel->scene_tree->node, &toplevel->server->new_geom);

  return toplevel->server->resizing;
}

static void process_cursor_move(struct quackwm_server *server, uint32_t time, int dx, int dy) {
  /* Move the grabbed toplevel to the new position. */
  struct quackwm_toplevel *toplevel = server->grabbed_toplevel;

  wlr_scene_node_set_position(&toplevel->scene_tree->node,
      (server->cursor_x) - server->grab_x,
      (server->cursor_y) - server->grab_y);
}

static void process_cursor_resize(struct quackwm_server *server, uint32_t time) {
  struct quackwm_toplevel *toplevel = server->grabbed_toplevel;
  struct wlr_box geo_box;

  // initial diff between cursor coordinates with the window coord
  int32_t dx = (int)server->start_x - (int)round(server->cursor_x);
  int32_t dy = (int)server->start_y - (int)round(server->cursor_y);

  int32_t new_w, new_h;
  int32_t gw = server->grab_geobox.width;
  int32_t gh = server->grab_geobox.height;

  new_w = gw - dx;
  new_h = gh - dy;

  if (new_w == gw && new_h == gh)
    return;

  // wlr_log(WLR_INFO, "new w h %d %d", new_w, new_h);

  server->new_geom = (struct wlr_box){
    .width = new_w,
      .height = new_h,
      .x = toplevel->scene_tree->node.x,
      .y = toplevel->scene_tree->node.y
  };

  resize_toplevel(toplevel);
}

static void process_cursor_motion(struct quackwm_server *server, uint32_t time, int dx, int dy) {
  // If the mode is non-passthrough, delegate to those functions
  if (server->cursor_mode == QUACKWM_CURSOR_MOVE) {
    process_cursor_move(server, time, dx, dy);
    return;
  } else if (server->cursor_mode == QUACKWM_CURSOR_RESIZE) {
    process_cursor_resize(server, time);
    return;
  }

  // Otherwise, find the toplevel under the pointer and send the event along.
  double sx, sy;
  struct wlr_seat *seat = server->seat;
  struct wlr_surface *surface = NULL;

  struct quackwm_toplevel *toplevel = desktop_toplevel_at(server,
      server->cursor_x, server->cursor_y, &surface, &sx, &sy);

	// Update drag icon's position
	wlr_scene_node_set_position(&server->drag_icon->node,
      server->cursor_x, server->cursor_y);

  if (!toplevel && server->cursor_mode == QUACKWM_CURSOR_PASSTHROUGH) {
    /* If there's no toplevel under the cursor, set the cursor image to a
     * default. This is what makes the cursor image appear when you move it
     * around the screen, not over any toplevels. */
    wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "default");
  }
  if (surface) {
    // wlr_log(WLR_INFO, "surface title %s", toplevel->xdg_toplevel->title);
    /*
     * Send pointer enter and motion events.
     *
     * The enter event gives the surface "pointer focus", which is distinct
     * from keyboard focus. You get pointer focus by moving the pointer over
     * a window.
     *
     * Note that wlroots will avoid sending duplicate enter/motion events if
     * the surface has already has pointer focus or if the client is already
     * aware of the coordinates passed.
     */
    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
  } else {
    /* Clear pointer focus so future button events and such are not sent to
     * the last client to have the cursor over it. */
    wlr_seat_pointer_clear_focus(seat);
  }
}

static void server_cursor_motion(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits a _relative_
   * pointer motion event (i.e. a delta) */
  struct quackwm_server *server =
    wl_container_of(listener, server, cursor_motion);

  struct wlr_pointer_motion_event *event = data;
  /* The cursor doesn't move unless we tell it to. The cursor automatically
   * handles constraining the motion to the output layout, as well as any
   * special configuration applied for the specific input device which
   * generated the event. You can pass NULL for the device if you want to move
   * the cursor around without any input. */
  wlr_cursor_move(server->cursor, &event->pointer->base,
      event->delta_x, event->delta_y);

  // make it compatible with absolute coordinates: this will be used
  // on button press event and move/resize actions
  server->cursor_x = server->cursor->x;
  server->cursor_y = server->cursor->y;

  // wlr_log(WLR_INFO, "Motion rel x y %d %d", server->cursor->x, server->cursor->y);
  process_cursor_motion(server, event->time_msec, event->delta_x, event->delta_y);
}

static void server_cursor_motion_absolute(
    struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an _absolute_
   * motion event, from 0..1 on each axis. This happens, for example, when
   * wlroots is running under a Wayland window rather than KMS+DRM, and you
   * move the mouse over the window. You could enter the window from any edge,
   * so we have to warp the mouse there. There is also some hardware which
   * emits these events. */
  struct quackwm_server *server =
    wl_container_of(listener, server, cursor_motion_absolute);
  struct wlr_pointer_motion_absolute_event *event = data;

  double lx, ly, dx, dy;

  if (!event->time_msec) /* this is 0 with virtual pointers */
    wlr_cursor_warp_absolute(server->cursor, &event->pointer->base, event->x, event->y);

  // wlr_log(WLR_INFO, "Motion abs x y %d %d", server->cursor->x, server->cursor->y);

  wlr_cursor_absolute_to_layout_coords(server->cursor, &event->pointer->base, event->x, event->y, &lx, &ly);

  dx = lx - server->cursor->x;
  dy = ly - server->cursor->y;

  server->cursor_x = lx;
  server->cursor_y = ly;

  process_cursor_motion(server, event->time_msec, dx, dy);
}

static void server_cursor_button(struct wl_listener *listener, void *data) {
  // Event forwarded by the cursor when a pointer emits a button event.
  struct wlr_pointer_button_event *event = data;
  struct quackwm_server *server =
    wl_container_of(listener, server, cursor_button);

  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
  uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

  if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
    bool mod_pressed = (mods == MOD_KEY);
    
    double sx, sy;
    struct wlr_surface *surface;
    struct quackwm_toplevel *toplevel = desktop_toplevel_at(server,
        server->cursor_x, server->cursor_y, &surface, &sx, &sy);

    // wlr_log(WLR_INFO, "surface, toplevel %d %d", surface, toplevel);
    // wlr_log(WLR_INFO, "x y %d %d", server->cursor_x, server->cursor_y);
    if (event->button == BTN_LEFT && toplevel && mod_pressed) {
      server->grabbed_toplevel = toplevel;

      server->grab_x = server->cursor_x - toplevel->scene_tree->node.x;
      server->grab_y = server->cursor_y - toplevel->scene_tree->node.y;

      wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "fleur");
      server->cursor_mode = QUACKWM_CURSOR_MOVE;
      return;
    }

    if (event->button == BTN_LEFT && toplevel) {
      wlr_log(WLR_INFO, "Toplevel clicked :: BTN_LEFT");
      focus_toplevel(toplevel, surface);
    }

    if (event->button == BTN_RIGHT && toplevel && mod_pressed) {
      struct wlr_box geo_box;
      if (toplevel->is_xwayland) {
        geo_box.x = toplevel->xwayland_surface->x;
        geo_box.y = toplevel->xwayland_surface->y;
        geo_box.width = toplevel->xwayland_surface->width;
        geo_box.height = toplevel->xwayland_surface->height;
      } else {
        geo_box = toplevel->xdg_toplevel->base->current.geometry;
      }

      server->grabbed_toplevel = toplevel;
      server->grab_geobox = geo_box;

      int right = (int)toplevel->scene_tree->node.x + (int)geo_box.width;
      int bottom = (int)toplevel->scene_tree->node.y + (int)geo_box.height;

      // move cursor to the bottom-right window corner
      wlr_cursor_warp_closest(server->cursor, NULL, right, bottom);

      server->start_x = (int)right;
      server->start_y = bottom;

      if (!toplevel->is_xwayland) {
        server->resizing = wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, geo_box.width, geo_box.height);
      }
      //server->resizing = wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, geo_box.width, geo_box.height);

      // set the cursor shape to corner resize
      wlr_cursor_set_xcursor(server->cursor, server->cursor_mgr, "se-resize");
      server->cursor_mode = QUACKWM_CURSOR_RESIZE;
      return;
    }
  }

  if (event->state == WL_POINTER_BUTTON_STATE_RELEASED) {
    reset_cursor_mode(server);
  }

  // notify the client with pointer focus that a button press has occurred
  wlr_seat_pointer_notify_button(server->seat,
      event->time_msec, event->button, event->state);
}

static void server_cursor_axis(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits an axis event,
   * for example when you move the scroll wheel. */
  struct quackwm_server *server =
    wl_container_of(listener, server, cursor_axis);

  struct wlr_pointer_axis_event *event = data;
	wlr_idle_notifier_v1_notify_activity(idle_notifier, server->seat);

  /* Notify the client with pointer focus of the axis event. */

  wlr_seat_pointer_notify_axis(server->seat, event->time_msec, event->orientation, event->delta, event->delta_discrete, event->source, event->relative_direction);
}

static void server_cursor_frame(struct wl_listener *listener, void *data) {
  /* This event is forwarded by the cursor when a pointer emits a frame
   * event. Frame events are sent after regular pointer events to group
   * multiple events together. For instance, two axis events may happen at the
   * same time, in which case a frame event won't be sent in between. */
  struct quackwm_server *server =
    wl_container_of(listener, server, cursor_frame);
  /* Notify the client with pointer focus of the frame event. */
  wlr_seat_pointer_notify_frame(server->seat);
}

static void request_decoration_mode(struct wl_listener *listener, void *data) {
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, set_decoration_mode);

  if (toplevel->xdg_toplevel->base->initialized) {
    // wlr_log(WLR_INFO, "Setting decoration mode");
    wlr_xdg_toplevel_decoration_v1_set_mode(toplevel->decoration,
        WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }
}

static void destroy_decoration(struct wl_listener *listener, void *data) {
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, destroy_decoration);

  wlr_log(WLR_INFO, "calling destroy decoration");

  wl_list_remove(&toplevel->destroy_decoration.link);
  wl_list_remove(&toplevel->set_decoration_mode.link);

  if (toplevel && toplevel->decoration)
    toplevel->decoration = NULL;
}

/* implementation from dwl/dwl.c */
static void create_decoration(struct wl_listener *listener, void *data)
{
  struct wlr_xdg_toplevel_decoration_v1 *deco = data;
  struct quackwm_toplevel *toplevel = calloc(1, sizeof(*toplevel));// deco->toplevel->base->data;
  toplevel->xdg_toplevel = deco->toplevel->base->data;
  toplevel->decoration = deco;

  LISTEN(&deco->events.request_mode, &toplevel->set_decoration_mode, request_decoration_mode);
  LISTEN(&deco->events.destroy, &toplevel->destroy_decoration, destroy_decoration);
}

/* 
 * Creates a new layer_shell surface.
 * Implementation from sway.
 *
 * https://github.com/swaywm/sway/blob/a6c0441ee060c7045b24aff943c7d0bf0f47412b/sway/desktop/layer_shell.c#L413
 */
static void create_layer_surface(struct wl_listener *listener, void *data)
{
  struct wlr_layer_surface_v1 *layer_surface = data;
  struct wlr_surface *surface = layer_surface->surface;

  wlr_log(WLR_INFO, "QUACKWM :: create_layer_surface");

  if (!layer_surface->output) {
    struct wlr_output *output = wlr_output_layout_output_at(server.output_layout, 0, 0);

    if (!output)
      return;

    layer_surface->output = output;
  }

  enum zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;
  struct wlr_scene_tree *output_layer = server_layers[layer_type];

  struct wlr_scene_layer_surface_v1 *scene_surface =
    wlr_scene_layer_surface_v1_create(output_layer, layer_surface);

  // wlr_log(WLR_INFO, "Layer type %d", layer_type);

  // start creating the layer shell surface
  struct layer_shell_surface *ls_surface = calloc(1, sizeof(*surface));

  struct wlr_scene_tree *popups = wlr_scene_tree_create(output_layer);
  // this is done in original sway code; maybe it's not required here
  // ls_surface->desc.relative = &scene_surface->tree->node;
  ls_surface->tree = scene_surface->tree;
  ls_surface->scene_layer_surface = scene_surface;
  ls_surface->layer_surface = scene_surface->layer_surface;
  ls_surface->popups = popups;
  ls_surface->layer_surface->data = surface;
  // end creating the ls surface

  ls_surface->wlr_output = layer_surface->output->data;

  // now that the surface's output is known, we can advertise its scale
  wlr_fractional_scale_v1_notify_scale(ls_surface->layer_surface->surface,
      layer_surface->output->scale);
  wlr_surface_set_preferred_buffer_scale(ls_surface->layer_surface->surface,
      ceil(layer_surface->output->scale));

  ls_surface->surface_commit.notify = commit_layer_surface_notify;
  wl_signal_add(&layer_surface->surface->events.commit,
      &ls_surface->surface_commit);
}

static void commit_layer_surface_notify(struct wl_listener *listener, void *data)
{
  struct layer_shell_surface *surface =
    wl_container_of(listener, surface, surface_commit);

  // wlr_log(WLR_INFO, "commit_layer_surface_notify");

  struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface;
  if (!layer_surface->initialized) {
    return;
  }

  uint32_t committed = layer_surface->current.committed;
  if (committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
    enum zwlr_layer_shell_v1_layer layer_type = layer_surface->current.layer;

    struct wlr_scene_tree *output_layer = server_layers[layer_type];
    // wlr_log(WLR_INFO, "committed is layer -->, layer type: %d", output_layer);

    wlr_scene_node_reparent(&surface->scene->tree.node, output_layer);
    wlr_scene_node_raise_to_top(&surface->scene->tree.node);
  }

  if (layer_surface->initial_commit || committed || layer_surface->surface->mapped != surface->mapped) {
    surface->mapped = layer_surface->surface->mapped;
    // arrange_layers(surface->output); // from sway code
    // transaction_commit_dirty();

    wlr_fractional_scale_v1_notify_scale(layer_surface->surface, layer_surface->output->scale);

    wlr_surface_set_preferred_buffer_scale(
        layer_surface->surface, (int32_t)ceilf(layer_surface->output->scale));

    struct wlr_box usable_area = { 0 };
    wlr_output_effective_resolution(layer_surface->output,
        &usable_area.width, &usable_area.height);
    const struct wlr_box full_area = usable_area;

    wlr_scene_layer_surface_v1_configure(surface->scene_layer_surface, &full_area, &usable_area);

    set_keyboard_focus(server.seat, layer_surface->surface);
  }
}

static void output_configure_scene(struct wlr_scene_node *node,
    struct quackwm_toplevel *toplevel) {

  if (!node->enabled) {
    return;
  }

  struct quackwm_toplevel *_toplevel = node->data;
  if (_toplevel) {
    toplevel = _toplevel;
  }

  if (node->type == WLR_SCENE_NODE_BUFFER) {
    struct wlr_scene_buffer *buffer = wlr_scene_buffer_from_node(node);

    struct wlr_scene_surface * scene_surface =
      wlr_scene_surface_try_from_buffer(buffer);

    if (!scene_surface) {
      return;
    }

    struct wlr_xdg_surface *xdg_surface =
      wlr_xdg_surface_try_from_wlr_surface(scene_surface->surface);

    struct wlr_xwayland_surface *xwayland_surface = wlr_xwayland_surface_try_from_wlr_surface(scene_surface->surface);

    if ((toplevel && xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) ||
        (toplevel && xwayland_surface)) {

      const struct wlr_alpha_modifier_surface_v1_state *alpha_modifier_state =
        wlr_alpha_modifier_v1_get_surface_state(scene_surface->surface);

      if (alpha_modifier_state != NULL) {
        wlr_log(WLR_INFO, "####### Output configure scene :: alpha_modifier_state: %f", alpha_modifier_state->multiplier);
        toplevel->opacity *= (float)alpha_modifier_state->multiplier;
      }

      if (!wlr_subsurface_try_from_wlr_surface(scene_surface->surface)) {
        wlr_scene_buffer_set_corner_radius(
            buffer, toplevel->corner_radius, CORNER_LOCATION_ALL);
      }
    }
  } else if (node->type == WLR_SCENE_NODE_TREE) {
    struct wlr_scene_tree *tree = wl_container_of(node, tree, node);
    struct wlr_scene_node *node;

    wl_list_for_each(node, &tree->children, link) {
      output_configure_scene(node, toplevel);
    }
  }
}

static void output_frame(struct wl_listener *listener, void *data) {
  /* This function is called every time an output is ready to display a frame,
   * generally at the output's refresh rate (e.g. 60Hz). */
  struct quackwm_output *output = wl_container_of(listener, output, frame);
  struct wlr_scene *scene = output->server->scene;

  struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(
      scene, output->wlr_output);

  output_configure_scene(&scene_output->scene->tree.node, NULL);

  /* Render the scene if needed and commit the output */
  wlr_scene_output_commit(scene_output, NULL);

  struct timespec now;
  clock_gettime( CLOCK_MONOTONIC, &now );
  wlr_scene_output_send_frame_done(scene_output, &now);
}

static void output_request_state(struct wl_listener *listener, void *data) {
  /* This function is called when the backend requests a new state for
   * the output. For example, Wayland and X11 backends request a new mode
   * when the output window is resized. */
  struct quackwm_output *output = wl_container_of(listener, output, request_state);
  const struct wlr_output_event_request_state *event = data;

  // wlr_log(WLR_INFO, "output_request_state");

  wlr_output_commit_state(output->wlr_output, event->state);

  /*
   * Sets the blur node size to the outputs size. Assumes there's only one
   * output. More advanced compositors should either implement per output blur
   * nodes or set it to the size of all outputs.
   */
  wlr_scene_optimized_blur_set_size(output->server->layers.blur_layer,
      output->wlr_output->width, output->wlr_output->height);
}

static void output_destroy(struct wl_listener *listener, void *data) {
  struct quackwm_output *output = wl_container_of(listener, output, destroy);

  wl_list_remove(&output->frame.link);
  wl_list_remove(&output->request_state.link);
  wl_list_remove(&output->destroy.link);
  wl_list_remove(&output->link);
  free(output);
}

static void server_new_output(struct wl_listener *listener, void *data) {
  /* This event is raised by the backend when a new output (aka a display or
   * monitor) becomes available. */
  struct quackwm_server *server =
    wl_container_of(listener, server, new_output);
  struct wlr_output *wlr_output = data;

  wlr_log(WLR_INFO, "Adding new output :: server_new_output");

  /* Configures the output created by the backend to use our allocator
   * and our renderer. Must be done once, before commiting the output */
  wlr_output_init_render(wlr_output, server->allocator, server->renderer);

  /* The output may be disabled, switch it on. */
  struct wlr_output_state state;
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);

  /* Some backends don't have modes. DRM+KMS does, and we need to set a mode
   * before we can use the output. The mode is a tuple of (width, height,
   * refresh rate), and each monitor supports only a specific set of modes. We
   * just pick the monitor's preferred mode, a more sophisticated compositor
   * would let the user configure it. */
  struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
  if (mode != NULL) {
    wlr_log(WLR_INFO, "MODE IS NULL");
    wlr_output_state_set_mode(&state, mode);
  }

  wlr_output_state_set_transform(&state, WL_OUTPUT_TRANSFORM_NORMAL);
  wlr_output_state_set_scale(&state, 1);
  wlr_output_state_set_adaptive_sync_enabled(&state, true);

  /* Atomically applies the new output state. */
  wlr_output_commit_state(wlr_output, &state);
  wlr_output_state_finish(&state);

  /* Allocates and configures our state for this output */
  struct quackwm_output *output = calloc(1, sizeof(*output));
  output->wlr_output = wlr_output;
  output->server = server;

  /* Sets up a listener for the frame event. */
  output->frame.notify = output_frame;
  wl_signal_add(&wlr_output->events.frame, &output->frame);

  /* Sets up a listener for the state request event. */
  output->request_state.notify = output_request_state;
  wl_signal_add(&wlr_output->events.request_state, &output->request_state);

  /* Sets up a listener for the destroy event. */
  output->destroy.notify = output_destroy;
  wl_signal_add(&wlr_output->events.destroy, &output->destroy);

  wl_list_insert(&server->outputs, &output->link);

  wlr_log(WLR_INFO, "output scale :: %d", output->wlr_output->scale);

  /* Adds this to the output layout. The add_auto function arranges outputs
   * from left-to-right in the order they appear. A more sophisticated
   * compositor would let the user configure the arrangement of outputs in the
   * layout.
   *
   * The output layout utility automatically adds a wl_output global to the
   * display, which Wayland clients can see to find out information about the
   * output (such as DPI, scale factor, manufacturer, etc).
   */
  struct wlr_output_layout_output *l_output = wlr_output_layout_add_auto(server->output_layout,
      wlr_output);
  struct wlr_scene_output *scene_output = wlr_scene_output_create(server->scene, wlr_output);
  wlr_scene_output_layout_add_output(server->scene_layout, l_output, scene_output);

  /*
   * Sets the blur node size to the outputs size. Assumes there's only one
   * output. More advanced compositors should either implement per output blur
   * nodes or set it to the size of all outputs.
   */
  wlr_scene_optimized_blur_set_size(output->server->layers.blur_layer,
      output->wlr_output->width, output->wlr_output->height);
}

static void iter_xdg_scene_buffers(struct wlr_scene_buffer *buffer, int sx,
    int sy, void *user_data) {
  struct quackwm_toplevel *toplevel = user_data;

  struct wlr_scene_surface * scene_surface = wlr_scene_surface_try_from_buffer(buffer);
  if (!scene_surface) {
    return;
  }

  struct wlr_xdg_surface *xdg_surface =
    wlr_xdg_surface_try_from_wlr_surface(scene_surface->surface);

  struct wlr_xwayland_surface *xwayland_surface = wlr_xwayland_surface_try_from_wlr_surface(scene_surface->surface);

  if ((toplevel && xdg_surface && xdg_surface->role == WLR_XDG_SURFACE_ROLE_TOPLEVEL) ||
      (toplevel && xwayland_surface)) {

    // TODO: Be able to set whole decoration_data instead of calling
    // each individually?
    // wlr_scene_buffer_set_opacity(buffer, toplevel->opacity);

    if (!wlr_subsurface_try_from_wlr_surface(scene_surface->surface)) {
      wlr_scene_buffer_set_corner_radius(buffer, toplevel->corner_radius,
          CORNER_LOCATION_ALL);

      wlr_scene_buffer_set_backdrop_blur(buffer, true);
      wlr_scene_buffer_set_backdrop_blur_optimized(buffer, true);
      wlr_scene_buffer_set_backdrop_blur_ignore_transparent(buffer, true);
    }
  }
}

static void xdg_toplevel_commit(struct wl_listener *listener, void *data) {
  /* Called when a new surface state is committed. */
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, commit);

  if (toplevel->xdg_toplevel->base->initial_commit) {
    wlr_log(WLR_INFO, "xdg_toplevel_commit :: initial commit");
    /* When an xdg_surface performs an initial commit, the compositor must
     * reply with a configure so the client can map the surface. quackwm
     * configures the xdg_toplevel with 0,0 size to let the client pick the
     * dimensions itself. */
    struct wlr_surface *surface = toplevel->xdg_toplevel->base->surface;
    struct quackwm_output *output = wl_container_of(server.outputs.next, output, link);
    int32_t scale = output->wlr_output->scale;

    wlr_fractional_scale_v1_notify_scale(surface, scale);
    wlr_surface_set_preferred_buffer_scale(surface, (int32_t)ceilf(scale));

    wlr_xdg_toplevel_set_wm_capabilities(toplevel->xdg_toplevel->base->toplevel, WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);

    wlr_xdg_toplevel_set_size(toplevel->xdg_toplevel, 0, 0);

    return;
  }

  struct wlr_box geometry;
  
  geometry = toplevel->xdg_toplevel->base->current.geometry;

  int blur_sigma = toplevel->shadow->blur_sigma;
  wlr_scene_shadow_set_size(toplevel->shadow,
      geometry.width + (blur_sigma) * 2,
      geometry.height + (blur_sigma) * 2);
}

static void xdg_toplevel_map(struct wl_listener *listener, void *data) {
  /* Called when the surface is mapped, or ready to display on-screen. */
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, map);
  struct quackwm_toplevel *_toplevel = data;

  wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);

  wlr_scene_node_for_each_buffer(&toplevel->scene_tree->node,
      iter_xdg_scene_buffers, toplevel);

  wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

  // adds the current tag
  add_toplevel_tag(toplevel, toplevel->server->current_tag);

  focus_toplevel(toplevel, toplevel->xdg_toplevel->base->surface);
}

static void xdg_toplevel_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, unmap);

  wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);

  /* Reset the cursor mode if the grabbed toplevel was unmapped. */
  if (toplevel == toplevel->server->grabbed_toplevel) {
    reset_cursor_mode(toplevel->server);
  }

  wl_list_remove(&toplevel->link);
}

static void xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
  /* Called when the xdg_toplevel is destroyed. */
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

  wl_list_remove(&toplevel->map.link);
  wl_list_remove(&toplevel->unmap.link);
  wl_list_remove(&toplevel->commit.link);
  wl_list_remove(&toplevel->destroy.link);
  /* not using the default XDG events anymore: these are
   * attached to regular decorations, I guess
   wl_list_remove(&toplevel->request_fullscreen.link); */

  wlr_scene_node_destroy(&toplevel->scene_tree->node);

  free(toplevel);
}


static void xdg_toplevel_request_fullscreen(
    struct wl_listener *listener, void *data) {
  /* Just as with request_maximize, we must send a configure here. */
  struct quackwm_toplevel *toplevel =
    wl_container_of(listener, toplevel, request_fullscreen);
  if (toplevel->xdg_toplevel->base->initialized) {
    wlr_xdg_surface_schedule_configure(toplevel->xdg_toplevel->base);
  }
}

static void xdg_toplevel_create(struct wl_listener *listener, void *data) {
  /* This event is raised when a client creates a new toplevel (application window). */
  struct wlr_xdg_toplevel *xdg_toplevel = data;

  wlr_log(WLR_INFO,
      "QUACKWM: creating new toplevel xdg, surface %d", xdg_toplevel->base->surface);
  wlr_xdg_surface_ping(xdg_toplevel->base);
  
  // Allocate a quackwm_toplevel for this surface
  struct quackwm_toplevel *toplevel = calloc(1, sizeof(struct quackwm_toplevel));

  struct quackwm_server *server = wl_container_of(listener, server, new_xdg_toplevel);

  /* Listen to the various events it can emit */
  toplevel->map.notify = xdg_toplevel_map;
  wl_signal_add(&xdg_toplevel->base->surface->events.map, &toplevel->map);

  toplevel->unmap.notify = xdg_toplevel_unmap;
  wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &toplevel->unmap);

  toplevel->commit.notify = xdg_toplevel_commit;
  wl_signal_add(&xdg_toplevel->base->surface->events.commit, &toplevel->commit);

  toplevel->destroy.notify = xdg_toplevel_destroy;
  wl_signal_add(&xdg_toplevel->events.destroy, &toplevel->destroy);

  toplevel->server = server;
  toplevel->scene_tree = wlr_scene_tree_create(toplevel->server->layers.toplevel_layer);
  toplevel->xdg_scene_tree = wlr_scene_xdg_surface_create(
      toplevel->scene_tree, xdg_toplevel->base);

  toplevel->xdg_toplevel = xdg_toplevel;

  toplevel->scene_tree->node.data = toplevel;
  xdg_toplevel->base->data = toplevel->scene_tree;

  wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);

  /* Set the scene_nodes decoration data */
  // toplevel->opacity = 1.0f;
  toplevel->corner_radius = 6;

  /* Window shadows */
  float blur_sigma = SHADOW_BLUR;
  toplevel->shadow = wlr_scene_shadow_create(toplevel->scene_tree,
      0, 0, toplevel->corner_radius, blur_sigma, SHADOW_COLOR);

  wlr_scene_node_set_position(&toplevel->shadow->node, -blur_sigma, -blur_sigma + 12);

  // Lower the shadow below the border
  wlr_scene_node_lower_to_bottom(&toplevel->shadow->node);
}

static void xdg_popup_commit(struct wl_listener *listener, void *data) {
  /* Called when a new surface state is committed. */
  struct quackwm_popup *popup = wl_container_of(listener, popup, commit);

  if (popup->xdg_popup->base->initial_commit) {
    /* When an xdg_surface performs an initial commit, the compositor must
     * reply with a configure so the client can map the surface.
     * quackwm sends an empty configure. A more sophisticated compositor
     * might change an xdg_popup's geometry to ensure it's not positioned
     * off-screen, for example. */
    // FIXME add mouse constraints to avoid popups out of the screen layout
    wlr_xdg_surface_schedule_configure(popup->xdg_popup->base);
  }
}

static void xdg_popup_destroy(struct wl_listener *listener, void *data) {
  /* Called when the xdg_popup is destroyed. */
  /* Allocate a quackwm_toplevel for this surface */
  struct quackwm_popup *popup = wl_container_of(listener, popup, destroy);

  wl_list_remove(&popup->commit.link);
  wl_list_remove(&popup->destroy.link);

  free(popup);
}

static void server_new_xdg_popup(struct wl_listener *listener, void *data) {
  /* This event is raised when a client creates a new popup. */
  struct wlr_xdg_popup *xdg_popup = data;

  struct quackwm_popup *popup = calloc(1, sizeof(*popup));
  popup->xdg_popup = xdg_popup;

  /* We must add xdg popups to the scene graph so they get rendered. The
   * wlroots scene graph provides a helper for this, but to use it we must
   * provide the proper parent scene node of the xdg popup. To enable this,
   * we always set the user data field of xdg_surfaces to the corresponding
   * scene node. */
  struct wlr_xdg_surface *parent = wlr_xdg_surface_try_from_wlr_surface(xdg_popup->parent);
  assert(parent != NULL);

  struct wlr_scene_tree *parent_tree = parent->data;
  xdg_popup->base->data = wlr_scene_xdg_surface_create(parent_tree, xdg_popup->base);

  popup->commit.notify = xdg_popup_commit;
  wl_signal_add(&xdg_popup->base->surface->events.commit, &popup->commit);

  popup->destroy.notify = xdg_popup_destroy;
  wl_signal_add(&xdg_popup->events.destroy, &popup->destroy);
}


  void
output_mgr_apply(struct wl_listener *listener, void *data)
{
  struct wlr_output_configuration_v1 *config = data;
  output_mgr_apply_or_test(config, 0);
}

  void
output_mgr_apply_or_test(struct wlr_output_configuration_v1 *config, int test)
{
  /*
   * Called when a client such as wlr-randr requests a change in output
   * configuration. This is only one way that the layout can be changed,
   * so any Monitor information should be updated by updatemons() after an
   * output_layout.change event, not here.
   */
  struct wlr_output_configuration_head_v1 *config_head;
  int ok = 1;

  wl_list_for_each(config_head, &config->heads, link) {
    struct wlr_output *wlr_output = config_head->state.output;
    // Monitor *m = wlr_output->data;
    struct wlr_output_state state;

    /* Ensure displays previously disabled by wlr-output-power-management-v1
     * are properly handled*/
    // m->asleep = 0;

    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, config_head->state.enabled);
    if (!config_head->state.enabled)
      goto apply_or_test;

    if (config_head->state.mode)
      wlr_output_state_set_mode(&state, config_head->state.mode);
    else
      wlr_output_state_set_custom_mode(&state,
          config_head->state.custom_mode.width,
          config_head->state.custom_mode.height,
          config_head->state.custom_mode.refresh);

    wlr_output_state_set_transform(&state, config_head->state.transform);
    wlr_output_state_set_scale(&state, config_head->state.scale);
    wlr_output_state_set_adaptive_sync_enabled(&state,
        config_head->state.adaptive_sync_enabled);

apply_or_test:
    ok &= test ? wlr_output_test_state(wlr_output, &state)
      : wlr_output_commit_state(wlr_output, &state);

    /* Don't move monitors if position wouldn't change, this to avoid
     * wlroots marking the output as manually configured.
     * wlr_output_layout_add does not like disabled outputs */
    // if (!test && wlr_output->enabled && (m->m.x != config_head->state.x || m->m.y != config_head->state.y))
    if (!test && wlr_output->enabled)
      wlr_output_layout_add(output_layout, wlr_output,
          config_head->state.x, config_head->state.y);

    wlr_output_state_finish(&state);
  }

  if (ok)
    wlr_output_configuration_v1_send_succeeded(config);
  else
    wlr_output_configuration_v1_send_failed(config);

  wlr_output_configuration_v1_destroy(config);
}



int main(int argc, char *argv[]) {
  wlr_log_init(WLR_DEBUG, NULL);
  char *startup_cmd = NULL;

  int c;
  while ((c = getopt(argc, argv, "s:h")) != -1) {
    switch (c) {
      case 's':
        startup_cmd = optarg;
        break;
      default:
        printf("Usage: %s [-s startup command]\n", argv[0]);
        return 0;
    }
  }
  if (optind < argc) {
    printf("Usage: %s [-s startup command]\n", argv[0]);
    return 0;
  }

  /* The Wayland display is managed by libwayland. It handles accepting
   * clients from the Unix socket, manging Wayland globals, and so on. */
  server.wl_display = wl_display_create();

  /* The backend is a wlroots feature which abstracts the underlying input and
   * output hardware. The autocreate option will choose the most suitable
   * backend based on the current environment, such as opening an X11 window
   * if an X11 server is running. */
  server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display), NULL);

  if (server.backend == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_backend");
    return 1;
  }

  /* Creates the fx_renderer from scenefx project, allowing
   * blur, shadows and rounded corners in toplevel clients */
  server.renderer = fx_renderer_create(server.backend);

  if (server.renderer == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_renderer");
    return 1;
  }

  /* Autocreates an allocator for us.
   * The allocator is the bridge between the renderer and the backend. It
   * handles the buffer creation, allowing wlroots to render onto the
   * screen */
  server.allocator = wlr_allocator_autocreate(server.backend,
      server.renderer);

  if (server.allocator == NULL) {
    wlr_log(WLR_ERROR, "failed to create wlr_allocator");
    return 1;
  }

  /* This creates some hands-off wlroots interfaces. The compositor is
   * necessary for clients to allocate surfaces, the subcompositor allows to
   * assign the role of subsurfaces to surfaces and the data device manager
   * handles the clipboard. Each of these wlroots interfaces has room for you
   * to dig your fingers in and play with their behavior if you want. Note that
   * the clients cannot set the selection directly without compositor approval,
   * see the handling of the request_set_selection event below.*/
  server.compositor = wlr_compositor_create(server.wl_display, 5, server.renderer);
  wlr_subcompositor_create(server.wl_display);
  wlr_data_device_manager_create(server.wl_display);
  wlr_data_control_manager_v1_create(server.wl_display);
  wlr_primary_selection_v1_device_manager_create(server.wl_display);
  wlr_viewporter_create(server.wl_display);
	wlr_screencopy_manager_v1_create(server.wl_display);
  wlr_export_dmabuf_manager_v1_create(server.wl_display);
  wlr_single_pixel_buffer_manager_v1_create(server.wl_display);
  wlr_fractional_scale_manager_v1_create(server.wl_display, 1);
  wlr_alpha_modifier_v1_create(server.wl_display);

  server.xwayland = wlr_xwayland_create(server.wl_display, server.compositor, true);
  if (!server.xwayland) {
    wlr_log(WLR_ERROR, "Failed to create XWayland");
    return 1;
  }
  server.xwayland_new_surface.notify = xwayland_new_surface;
  wl_signal_add(&server.xwayland->events.new_surface, &server.xwayland_new_surface);
  setenv("DISPLAY", server.xwayland->display_name, 1);

  /* Creates an output layout, which a wlroots utility for working with an
   * arrangement of screens in a physical layout. */
  server.output_layout = wlr_output_layout_create(server.wl_display);
  output_layout = server.output_layout;

  // -- from dwl l:2522
  wlr_xdg_output_manager_v1_create(server.wl_display, server.output_layout);

  /* Configure a listener to be notified when new outputs are available on the
   * backend. */
  wl_list_init(&server.outputs);
  server.new_output.notify = server_new_output;
  wl_signal_add(&server.backend->events.new_output, &server.new_output);

  /* Create a scene graph. This is a wlroots abstraction that handles all
   * rendering and damage tracking. All the compositor author needs to do
   * is add things that should be rendered to the scene graph at the proper
   * positions and then call wlr_scene_output_commit() to render a frame if
   * necessary.
   */
  server.scene = wlr_scene_create();
  server.scene_layout = wlr_scene_attach_output_layout(server.scene, server.output_layout);

  /* background layer */
  server.layers.bg_layer = wlr_scene_tree_create(&server.scene->tree);

  /* initialize blur layer */
  server.layers.blur_layer = wlr_scene_optimized_blur_create(&server.scene->tree, 0, 0);

  /* bottom layer */
  server.layers.bottom_layer = wlr_scene_tree_create(&server.scene->tree);

  /* toplevel layer */
  server.layers.toplevel_layer = wlr_scene_tree_create(&server.scene->tree);

  /* overlay layer */
  server.layers.overlay_layer = wlr_scene_tree_create(&server.scene->tree);

  // exclude blur layer-it's a different type
  server_layers[0] = server.layers.bg_layer;
  server_layers[1] = server.layers.bottom_layer;
  server_layers[2] = server.layers.toplevel_layer;
  server_layers[3] = server.layers.overlay_layer;

  /* initialize blur data */
  struct blur_data blur_data = blur_data_get_default();
  wlr_scene_set_blur_data(server.scene, blur_data);

	server.drag_icon = wlr_scene_tree_create(&server.scene->tree);
	  wlr_scene_node_place_below(&server.drag_icon->node, &server.layers.overlay_layer->node);

  /* Create shm, drm and linux_dmabuf interfaces by ourselves.
   * The simplest way is call:
   *      wlr_renderer_init_wl_display(drw);
   * but we need to create manually the linux_dmabuf interface to integrate it
   * with wlr_scene. */
  wlr_renderer_init_wl_shm(server.renderer, server.wl_display);

  if (wlr_renderer_get_texture_formats(server.renderer, WLR_BUFFER_CAP_DMABUF)) {
    wlr_drm_create(server.wl_display, server.renderer);
    wlr_scene_set_linux_dmabuf_v1(server.scene,
        wlr_linux_dmabuf_v1_create_with_renderer(server.wl_display, 5, server.renderer));
  }

  /* Add background rectangle */
  float root_rect_color[4] = { 0.7, 0.7, 0.7, 1 };
  static struct wlr_scene_rect *root_bg;
  root_bg = wlr_scene_rect_create(server.layers.bg_layer, 0, 0, root_rect_color);

  /* Set up xdg-shell version 6. The xdg-shell is a Wayland protocol which is
   * used for application windows. For more detail on shells, refer to
   * https://drewdevault.com/2018/07/29/Wayland-shells.html.
   */
  wl_list_init(&server.toplevels);
  server.xdg_shell = wlr_xdg_shell_create(server.wl_display, 6);
  server.new_xdg_toplevel.notify = xdg_toplevel_create;

  wl_signal_add(&server.xdg_shell->events.new_toplevel, &server.new_xdg_toplevel);
  server.new_xdg_popup.notify = server_new_xdg_popup;
  wl_signal_add(&server.xdg_shell->events.new_popup, &server.new_xdg_popup);

  layer_shell = wlr_layer_shell_v1_create(server.wl_display, 3);
  // create layer_shell surface...
  LISTEN_STATIC(&layer_shell->events.new_surface, create_layer_surface);

  /*
   * Creates a cursor, which is a wlroots utility for tracking the cursor
   * image shown on screen.
   */
  server.cursor = wlr_cursor_create();
  wlr_cursor_attach_output_layout(server.cursor, server.output_layout);

  /* Creates an xcursor manager, another wlroots utility which loads up
   * Xcursor themes to source cursor images from and makes sure that cursor
   * images are available at all scale factors on the screen (necessary for
   * HiDPI support). */
  server.cursor_mgr = wlr_xcursor_manager_create(NULL, 24);
  server.relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(server.wl_display);

  /*
   * wlr_cursor *only* displays an image on screen. It does not move around
   * when the pointer moves. However, we can attach input devices to it, and
   * it will generate aggregate events for all of them. In these events, we
   * can choose how we want to process them, forwarding them to clients and
   * moving the cursor around. More detail on this process is described in
   * https://drewdevault.com/2018/07/17/Input-handling-in-wlroots.html.
   *
   * And more comments are sprinkled throughout the notify functions above.
   */
  server.cursor_mode = QUACKWM_CURSOR_PASSTHROUGH;

  server.cursor_motion.notify = server_cursor_motion;
  wl_signal_add(&server.cursor->events.motion, &server.cursor_motion);

  server.cursor_motion_absolute.notify = server_cursor_motion_absolute;
  wl_signal_add(&server.cursor->events.motion_absolute,
      &server.cursor_motion_absolute);

  server.cursor_button.notify = server_cursor_button;
  wl_signal_add(&server.cursor->events.button, &server.cursor_button);

  server.cursor_axis.notify = server_cursor_axis;
  wl_signal_add(&server.cursor->events.axis, &server.cursor_axis);

  server.cursor_frame.notify = server_cursor_frame;
  wl_signal_add(&server.cursor->events.frame, &server.cursor_frame);

  // sets the default tag 0-5
  switch_to_tag(0);

  /*
   * Configures a seat, which is a single "seat" at which a user sits and
   * operates the computer. This conceptually includes up to one keyboard,
   * pointer, touch, and drawing tablet device. We also rig up a listener to
   * let us know when new input devices are available on the backend.
   */
  wl_list_init(&server.keyboards);
  server.new_input.notify = server_new_input;

  wl_signal_add(&server.backend->events.new_input, &server.new_input);
  server.seat = wlr_seat_create(server.wl_display, "seat0");
  server.request_cursor.notify = seat_request_cursor;

	wlr_xwayland_set_seat(server.xwayland, server.seat);

  wl_signal_add(&server.seat->events.request_set_cursor,
      &server.request_cursor);

  server.request_set_selection.notify = seat_request_set_selection;
  wl_signal_add(&server.seat->events.request_set_selection,
      &server.request_set_selection);
  server.request_set_primary_selection.notify = seat_request_set_primary_selection;
  wl_signal_add(&server.seat->events.request_set_primary_selection,
      &server.request_set_primary_selection);

  server.request_start_drag.notify = seat_request_start_drag;
  wl_signal_add(&server.seat->events.request_start_drag,
      &server.request_start_drag);

  /* Add a Unix socket to the Wayland display. */
  const char *socket = wl_display_add_socket_auto(server.wl_display);
  if (!socket) {
    wlr_backend_destroy(server.backend);
    return 1;
  }

  /* Start the backend. This will enumerate outputs and inputs, become the DRM
   * master, etc */
  if (!wlr_backend_start(server.backend)) {
    wlr_backend_destroy(server.backend);
    wl_display_destroy(server.wl_display);
    return 1;
  }

  /* *************************************************************
   * Use decoration protocols to negotiate server-side decorations
   *
   * FIXME
   * [types/xdg_shell/wlr_xdg_surface.c:169]
   *    A configure is scheduled for an uninitialized xdg_surface
   * */
  wlr_server_decoration_manager_set_default_mode(
      wlr_server_decoration_manager_create(server.wl_display),
      WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);
  xdg_decoration_mgr = wlr_xdg_decoration_manager_v1_create(server.wl_display);

  LISTEN_STATIC(&xdg_decoration_mgr->events.new_toplevel_decoration, create_decoration);

  // probably used to switch monitors
  output_mgr = wlr_output_manager_v1_create(server.wl_display);
  LISTEN_STATIC(&output_mgr->events.apply, output_mgr_apply);
  // LISTEN_STATIC(&output_mgr->events.test, outputmgrtest);

  /* Now that we update the output layout we can get its box */
  // LINES BELOW MUST BE AFTER the outut_mgr creation
  wlr_output_layout_get_box(server.output_layout, NULL, &sgeom);

  // set the root background rectangle position
  wlr_scene_node_set_position(&root_bg->node, sgeom.x, sgeom.y);
  wlr_scene_rect_set_size(root_bg, sgeom.width, sgeom.height);

  wlr_scene_node_lower_to_bottom(&root_bg->node);
  // wlr_scene_node_set_enabled(&root_bg->node, 0);

  /* Set the WAYLAND_DISPLAY environment variable to our socket and run the
   * startup command if requested. */
  setenv("WAYLAND_DISPLAY", socket, true);
  if (startup_cmd) {
    if (fork() == 0) {
      execl("/bin/sh", "/bin/sh", "-c", startup_cmd, (void *)NULL);
    }
  }

  char *modkey;
  if (modkey = getenv("MOD_KEY")) {
    if (strcmp(modkey, "logo") == 0)
      MOD_KEY = WLR_MODIFIER_LOGO;
    //else
    wlr_log(WLR_INFO, "---- MOD_KEY %d", MOD_KEY);
  }

  /* Run the Wayland event loop. This does not return until you exit the
   * compositor. Starting the backend rigged up all of the necessary event
   * loop configuration to listen to libinput events, DRM events, generate
   * frame events at the refresh rate, and so on. */
  wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
      socket);
  wl_display_run(server.wl_display);

  /* Once wl_display_run returns, we destroy all clients then shut down the
   * server. */
  wl_display_destroy_clients(server.wl_display);
  wlr_xwayland_destroy(server.xwayland);
  wlr_scene_node_destroy(&server.scene->tree.node);
  wlr_xcursor_manager_destroy(server.cursor_mgr);
  wlr_cursor_destroy(server.cursor);
  wlr_allocator_destroy(server.allocator);
  wlr_renderer_destroy(server.renderer);
  wlr_backend_destroy(server.backend);
  wl_display_destroy(server.wl_display);

  return 0;
}
