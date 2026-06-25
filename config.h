#define BORDER_THICKNESS 1
#define SHADOW_COLOR (float[4]){ 0, 0, 0, .25 }
#define SHADOW_BLUR 42.0f

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 0;
static const int natural_scrolling = 1;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 1;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;

/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;


/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",
	*/
	.options = NULL,
  .layout = "us",
  .variant = "altgr-intl"
};


/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_LOGO

#define TAGKEYS(KEY,SKEY,TAG) \
	{ MODKEY,                    KEY,            view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY,            toggleview,      {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, SKEY,           tag,             {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT,SKEY,toggletag, {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static const char *terminal[] = { "kitty", NULL };
static const char *menu[] = { "rofi-app-menu.sh", NULL };
static const char *browser[] = { "firefox", NULL };
static const char *fileManager[] = { "kitty", "yazi", NULL };
static const char *codeEditor[] = { "code", NULL };
static const char *lockScreen[] = { "hyprlock", NULL };
static const char *btop[] = { "kitty", "btop", NULL };
static const char *wifiMenu[] = { "kitty", "nmtui", NULL };
static const char *bluetoothMenu[] = { "kitty", "bluetui", NULL };
static const char *powerMenu[] = { "rofi-power-menu", NULL };
static const char *clipboardHistory[] = { "Clipboard-History", NULL };
static const char *wallpaperSwitcher[] = { "rofi-wallpaper-switcher", NULL };

static const char *playPause[] = { "playerctl", "play-pause", NULL };
static const char *mediaNext[] = { "playerctl", "next", NULL };

// Basic volume controls.
static const char *up_vol[]   = { "pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%", NULL };
static const char *down_vol[] = { "pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%", NULL };
static const char *mute_vol[] = { "pactl", "set-sink-mute",   "@DEFAULT_SINK@", "toggle", NULL };

static const char *up_brightness[]   = { "brightnessctl", "-e4", "-n2", "set", "5%+", NULL };
static const char *down_brightness[] =  { "brightnessctl", "-e4", "-n2", "set", "5%-", NULL };

static const char *screenToggle[] = { "wlr-randr", "--output", "eDP-1", "--toggle", NULL };

static const char *screenshot[] = { "Screenshot", NULL };

static const Key keys[] = {}
	/* Note that Shift changes certain key codes: 2 -> at, etc. */
	/* modifier                  key                  function          argument */
// 	{ WLR_MODIFIER_ALT,          XKB_KEY_r,           spawn,            {.v = menu} },
// 	{ MODKEY,                    XKB_KEY_t,           spawn,            {.v = terminal} },
//   	{ MODKEY,                    XKB_KEY_f,           spawn,            {.v = browser} },
// 	{ MODKEY, 					 XKB_KEY_e, 		  spawn, 			{.v = fileManager} },
// 	{ MODKEY, 					 XKB_KEY_v, 		  spawn, 			{.v = codeEditor} },
// 	{ MODKEY, 					 XKB_KEY_l, 		  spawn, 			{.v = lockScreen} },
// 	{ MODKEY, 					 XKB_KEY_b, 		  spawn, 			{.v = btop} },
// 	{ WLR_MODIFIER_ALT,			 XKB_KEY_w,			  spawn, 			{.v = wifiMenu} },
// 	{ WLR_MODIFIER_ALT,			 XKB_KEY_b,			  spawn, 			{.v = bluetoothMenu} },
// 	{ WLR_MODIFIER_ALT,			 XKB_KEY_p,			  spawn, 			{.v = powerMenu} },
// 	{ WLR_MODIFIER_ALT,			 XKB_KEY_q,			  spawn, 			{.v = clipboardHistory} },
// 	{ WLR_MODIFIER_ALT,			 XKB_KEY_t,			  spawn, 			{.v = wallpaperSwitcher} },
// 	{ WLR_MODIFIER_ALT, 		 XKB_KEY_x,     	  spawn, 			{.v = playPause} },
// 	{ WLR_MODIFIER_ALT, 		 XKB_KEY_z,     	  spawn, 			{.v = mediaNext} },
// 	{ 0, 						 XKB_KEY_F11, 		  spawn,	 		{.v = screenshot} },
// 	{ WLR_MODIFIER_ALT, 		 XKB_KEY_Tab,		  swapfocus,        {0} },
// 	{ MODKEY,                    XKB_KEY_j,           focusstack,       {.i = +1} },
// 	{ MODKEY,                    XKB_KEY_k,           focusstack,       {.i = -1} },
// 	{ MODKEY,                    XKB_KEY_i,           incnmaster,       {.i = +1} },
// 	{ MODKEY,                    XKB_KEY_d,           incnmaster,       {.i = -1} },
// 	{ MODKEY,                    XKB_KEY_h,           setmfact,         {.f = -0.05f} },
// 	{ MODKEY,                    XKB_KEY_l,           setmfact,         {.f = +0.05f} },
// 	{ MODKEY,                    XKB_KEY_Return,      zoom,             {0} },
// 	{ MODKEY,                    XKB_KEY_Tab,         view,             {0} },
// 	{ MODKEY,                    XKB_KEY_w,           killclient,       {0} },
// 	{ MODKEY,                    XKB_KEY_space,       setlayout,        {0} },
// 	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_space,       togglefloating,   {0} },
// 	{ MODKEY,                    XKB_KEY_a,           togglefullscreen, {0} },
// 	{ MODKEY, 					 XKB_KEY_q, 		  minimizeKeybind,	{0} },
// 	{ MODKEY, 					 XKB_KEY_m, 		  maximizeKeybind,	{0} },
// 	{ MODKEY,                    XKB_KEY_0,           view,             {.ui = ~0} },
// 	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenright,  tag,              {.ui = ~0} },
// 	{ MODKEY,                    XKB_KEY_comma,       focusmon,         {.i = WLR_DIRECTION_LEFT} },
// 	{ MODKEY,                    XKB_KEY_period,      focusmon,         {.i = WLR_DIRECTION_RIGHT} },
// 	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_less,        tagmon,           {.i = WLR_DIRECTION_LEFT} },
// 	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_greater,     tagmon,           {.i = WLR_DIRECTION_RIGHT} },
//   	{ 0, XKB_KEY_XF86AudioRaiseVolume, spawn, {.v = up_vol } },
//   	{ 0, XKB_KEY_XF86AudioLowerVolume, spawn, {.v = down_vol } },
//   	{ 0, XKB_KEY_XF86AudioMute, spawn, {.v = mute_vol } },
// 	{ 0, XKB_KEY_XF86MonBrightnessUp, spawn, {.v = up_brightness } },
//   	{ 0, XKB_KEY_XF86MonBrightnessDown, spawn, {.v = down_brightness } },
// 	{ 0, XKB_KEY_XF86Launch1, spawn, {.v = screenToggle} },
// 	{ 0, XKB_KEY_F12, spawn, {.v = screenToggle} },
// 	TAGKEYS(          XKB_KEY_1, XKB_KEY_exclam,                        0),
// 	TAGKEYS(          XKB_KEY_2, XKB_KEY_at,                            1),
// 	TAGKEYS(          XKB_KEY_3, XKB_KEY_numbersign,                    2),
// 	TAGKEYS(          XKB_KEY_4, XKB_KEY_dollar,                        3),
// 	TAGKEYS(          XKB_KEY_5, XKB_KEY_percent,                       4),
// 	TAGKEYS(          XKB_KEY_6, XKB_KEY_asciicircum,                   5),
// 	TAGKEYS(          XKB_KEY_7, XKB_KEY_ampersand,                     6),
// 	TAGKEYS(          XKB_KEY_8, XKB_KEY_asterisk,                      7),
// 	TAGKEYS(          XKB_KEY_9, XKB_KEY_parenleft,                     8),
// 	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_q,           quit,             {0} }, //TODO Probably should make this harder to press when minimise.

// 	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server */
// 	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_Terminate_Server, quit, {0} },
// 	/* Ctrl-Alt-Fx is used to switch to another VT, if you don't know what a VT is
// 	 * do not remove them.
// 	 */
// #define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
// 	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
// 	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
// };

// static const Button buttons[] = {
// 	{ MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
// 	{ MODKEY, BTN_MIDDLE, togglefloating, {0} },
// 	{ WLR_MODIFIER_ALT, BTN_LEFT,  moveresize,     {.ui = CurResize} },
// };
