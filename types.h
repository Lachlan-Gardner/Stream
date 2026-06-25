

/* For brevity's sake, struct members are annotated where they are used. */
enum quackwm_cursor_mode {
  QUACKWM_CURSOR_PASSTHROUGH,
  QUACKWM_CURSOR_MOVE,
  QUACKWM_CURSOR_RESIZE,
};

enum { bg_layer, bottom_layer, toplevel_layer, overlay_layer, NUM_LAYERS }; /* scene layers */

struct layer_shell_surface {
  struct wlr_box geom;
  struct wlr_scene *scene;
  struct wlr_scene_tree *tree;
  struct wlr_scene_tree *popups;
  struct wlr_scene_tree *scene_layer;
  struct wlr_scene_layer_surface_v1 *scene_layer_surface;
  struct wl_list link;
  int mapped;
  struct wlr_layer_surface_v1 *layer_surface;

  struct wl_listener destroy;
  struct wl_listener unmap;
  struct wl_listener surface_commit;

  // from dwl: Monitor struct
  struct wlr_output *wlr_output;
};

static struct wlr_xdg_decoration_manager_v1 *xdg_decoration_mgr;
static struct wlr_output_manager_v1 *output_mgr;
static struct wlr_layer_shell_v1 *layer_shell;

static struct wlr_output_layout *output_layout;
static struct wlr_box sgeom;

static struct wlr_scene_tree *server_layers[NUM_LAYERS];

/* Map from ZWLR_LAYER_SHELL_* constants to Lyr* enum */
static const int layermap[] = { bg_layer, bottom_layer, toplevel_layer, overlay_layer };

struct quackwm_server {
  struct wl_display *wl_display;
  struct wlr_backend *backend;
  struct wlr_renderer *renderer;
  struct wlr_allocator *allocator;
  struct wlr_scene *scene;
  struct wlr_scene_output_layout *scene_layout;
  struct {
    struct wlr_scene_tree *bg_layer;
    struct wlr_scene_tree *bottom_layer;
    /** Used for optimized blur. Everything exclusively below gets blurred */
    struct wlr_scene_optimized_blur *blur_layer;
    struct wlr_scene_tree *toplevel_layer;
    struct wlr_scene_tree *overlay_layer;
  } layers;

  struct wlr_compositor *compositor;

  struct wlr_xdg_shell *xdg_shell;
  struct wl_listener new_xdg_toplevel;
  struct wl_listener new_xdg_popup;
  struct wl_list toplevels;

  struct wlr_cursor *cursor;
  struct wlr_xcursor_manager *cursor_mgr;
  struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
  struct wl_listener cursor_motion;
  struct wl_listener cursor_motion_absolute;
  struct wl_listener cursor_button;
  struct wl_listener cursor_axis;
  struct wl_listener cursor_frame;

  struct wlr_seat *seat;
  struct wl_listener new_input;
  struct wl_listener request_cursor;
  struct wl_listener request_set_selection;
  struct wl_listener request_set_primary_selection;
  struct wl_listener request_start_drag;

  struct wlr_scene_tree *drag_icon;

  struct wl_list keyboards;
  enum quackwm_cursor_mode cursor_mode;
  struct quackwm_toplevel *grabbed_toplevel;
  double grab_x, grab_y;
  int start_x, start_y;
  int cursor_x, cursor_y;
  struct wlr_box grab_geobox;

  struct wlr_output_layout *output_layout;
  struct wl_list outputs;
  struct wl_listener new_output;

  uint32_t resize_edges;
  uint32_t resizing;
  uint32_t current_tag;

  int start_w;
  int start_h;

  struct wlr_box new_geom;
  struct wlr_xwayland *xwayland; // Added for XWayland
  struct wl_listener xwayland_new_surface; // Added for XWayland
};

struct quackwm_output {
  struct wl_list link;
  struct quackwm_server *server;
  struct wlr_output *wlr_output;
  struct wl_listener frame;
  struct wl_listener request_state;
  struct wl_listener destroy;
};

struct quackwm_toplevel {
  struct wl_list link;
  struct quackwm_server *server;
  // struct wlr_xdg_toplevel *xdg_toplevel;
  struct wlr_scene_tree *xdg_scene_tree;
  struct wlr_scene_tree *scene_tree;

  bool is_xwayland; // Flag to distinguish XWayland surfaces
  union {
    struct wlr_xdg_toplevel *xdg_toplevel;
    struct wlr_xwayland_surface *xwayland_surface;
  };

  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener commit;
  struct wl_listener destroy;
  struct wl_listener request_move;
  struct wl_listener request_resize;
  struct wl_listener request_maximize;
  struct wl_listener request_fullscreen;

  struct wl_listener xwayland_map;
  struct wl_listener xwayland_unmap;
	struct wl_listener associate;
	struct wl_listener activate;
	struct wl_listener configure;
	struct wl_listener request_activate;
	struct wl_listener request_configure;

  // from dwl
  struct wlr_xdg_toplevel_decoration_v1 *decoration;
  struct wl_listener set_decoration_mode;
  struct wl_listener destroy_decoration;
  // end dwl

  struct wlr_scene_shadow *shadow;
  struct wlr_scene_rect *border;
  float opacity;
  int corner_radius;
  bool enabled;

  uint32_t tags;
};

struct quackwm_popup {
  struct wlr_xdg_popup *xdg_popup;
  struct wl_listener commit;
  struct wl_listener destroy;
};

struct quackwm_keyboard {
  struct wl_list link;
  struct quackwm_server *server;
  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener modifiers;
  struct wl_listener key;
  struct wl_listener destroy;
};

typedef union {
	int i;
	uint32_t ui;
	float f;
	const void *v;
} Arg;

typedef struct {
	unsigned int mod;
	unsigned int button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;

typedef struct {
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
} Key;

typedef struct {
	struct wlr_keyboard_group *wlr_group;

	int nsyms;
	const xkb_keysym_t *keysyms; /* invalid if nsyms == 0 */
	uint32_t mods; /* invalid if nsyms == 0 */
	struct wl_event_source *key_repeat_source;

	struct wl_listener modifiers;
	struct wl_listener key;
	struct wl_listener destroy;
} KeyboardGroup;

static struct wlr_idle_notifier_v1 *idle_notifier;

static int locked;

/*
 * Macros from dwl/dwl.c
 */
#define LISTEN(E, L, H)         wl_signal_add((E), ((L)->notify = (H), (L)))
#define LISTEN_STATIC(E, H)     do { static struct wl_listener _l = {.notify = (H)}; wl_signal_add((E), &_l); } while (0)

#define TAG_MASK  ((1u << TAG_COUNT) - 1)
#define TAG_COUNT (uint32_t) 6

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#define CLEANMASK(mask)         (mask & ~WLR_MODIFIER_CAPS)
#define VISIBLEON(C, M)         ((M) && (C)->mon == (M) && ((C)->tags & (M)->tagset[(M)->seltags]))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
