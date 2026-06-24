static void xwayland_toplevel_map(struct wl_listener *listener, void *data) {
  /* Called when the surface is mapped, or ready to display on-screen. */
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, xwayland_map);
  struct quackwm_toplevel *_toplevel = data;

  wlr_scene_node_set_enabled(&toplevel->scene_tree->node, true);
  wlr_scene_node_for_each_buffer(&toplevel->scene_tree->node,
      iter_xdg_scene_buffers, toplevel);

  wl_list_insert(&toplevel->server->toplevels, &toplevel->link);

  add_toplevel_tag(toplevel, toplevel->server->current_tag);
  focus_toplevel(toplevel, toplevel->xwayland_surface->surface);
}

static void xwayland_toplevel_unmap(struct wl_listener *listener, void *data) {
  /* Called when the surface is unmapped, and should no longer be shown. */
  struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, xwayland_unmap);

  wlr_scene_node_set_enabled(&toplevel->scene_tree->node, false);

  /* Reset the cursor mode if the grabbed toplevel was unmapped. */
  if (toplevel == toplevel->server->grabbed_toplevel) {
    reset_cursor_mode(toplevel->server);
  }

  wl_list_remove(&toplevel->link);
}

static void xwayland_toplevel_configure(struct wl_listener *listener, void *data) {
    struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, request_configure);
    struct wlr_xwayland_surface_configure_event *event = data;

    wlr_log(WLR_INFO, "xwayland_toplevel_configure");
    wlr_log(WLR_INFO, "xwayland surface %d ", toplevel->xwayland_surface);

    if (!toplevel || !toplevel->xwayland_surface ) {
        wlr_log(WLR_ERROR, "Invalid toplevel or xwayland_surface in configure");
        return;
    }

    wlr_log(WLR_INFO, "Configuring XWayland surface, x=%d, y=%d, w=%d, h=%d",
        event->x, event->y, event->width, event->height);

    wlr_xwayland_surface_configure(toplevel->xwayland_surface,
        event->x, event->y, event->width, event->height);

    // Update scene tree and shadow
    struct wlr_box geometry = {
        .x = event->x,
        .y = event->y,
        .width = event->width,
        .height = event->height
    };
    wlr_scene_node_set_position(&toplevel->scene_tree->node, geometry.x, geometry.y);
    wlr_scene_shadow_set_size(toplevel->shadow,
        geometry.width + (SHADOW_BLUR * 2),
        geometry.height + (SHADOW_BLUR * 2));
}

static void xwayland_toplevel_destroy(struct wl_listener *listener, void *data) {
    struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, destroy);

    wlr_log(WLR_INFO, "xwayland_toplevel_destroy map link: %p", &toplevel->xwayland_map.link);

    if (toplevel->xwayland_map.link.prev != NULL || toplevel->xwayland_map.link.next != NULL) {
      wl_list_remove(&toplevel->xwayland_map.link);
      wl_list_remove(&toplevel->xwayland_unmap.link);
    } else {
      wlr_log(WLR_INFO, "map.link already removed, skipping");
    }

    wl_list_remove(&toplevel->associate.link);
    wl_list_remove(&toplevel->request_activate.link);
    wl_list_remove(&toplevel->request_configure.link);
    wl_list_remove(&toplevel->destroy.link);
    wlr_scene_node_destroy(&toplevel->scene_tree->node);
    free(toplevel);
}

static void xwayland_toplevel_activate(struct wl_listener *listener, void *data) {
    struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, request_activate);
    wlr_xwayland_surface_activate(toplevel->xwayland_surface, true);
    wlr_log(WLR_INFO, "XWayland surface activated, surface=%p", toplevel->xwayland_surface->surface);
}

static void xwayland_toplevel_associate(struct wl_listener *listener, void *data) {
    struct quackwm_toplevel *toplevel = wl_container_of(listener, toplevel, associate);

    if (!toplevel->xwayland_surface->surface) {
        wlr_log(WLR_ERROR, "No surface associated with xwayland_surface");
        return;
    }

    wlr_log(WLR_INFO, "XWayland surface associated, surface=%p", toplevel->xwayland_surface->surface);

    // Create scene surface
    struct wlr_scene_surface *scene_surface = wlr_scene_surface_create(toplevel->scene_tree, toplevel->xwayland_surface->surface);
    if (!scene_surface) {
        wlr_log(WLR_ERROR, "Failed to create scene surface for XWayland");
        return;
    }

    toplevel->xwayland_map.notify = xwayland_toplevel_map;
    wl_list_init(&toplevel->xwayland_map.link);
    wl_signal_add(&toplevel->xwayland_surface->surface->events.map, &toplevel->xwayland_map);

    toplevel->xwayland_unmap.notify = xwayland_toplevel_unmap;
    wl_list_init(&toplevel->xwayland_unmap.link);
    wl_signal_add(&toplevel->xwayland_surface->surface->events.unmap, &toplevel->xwayland_unmap);
}

static void xwayland_new_surface(struct wl_listener *listener, void *data) {
    struct wlr_xwayland_surface *xwayland_surface = data;
    struct quackwm_server *server = wl_container_of(listener, server, xwayland_new_surface);

    if (!xwayland_surface) {
        wlr_log(WLR_ERROR, "Null xwayland_surface in xwayland_new_surface");
        return;
    }

    wlr_log(WLR_INFO, "QUACKWM: creating new XWayland surface, title %s", xwayland_surface->title ?: "unknown");

    struct quackwm_toplevel *toplevel = calloc(1, sizeof(*toplevel));
    if (!toplevel) {
        wlr_log(WLR_ERROR, "Failed to allocate toplevel");
        return;
    }
    wl_list_init(&toplevel->link);

    toplevel->server = server;
    toplevel->is_xwayland = true;
    toplevel->xwayland_surface = xwayland_surface;

    if (!server->layers.toplevel_layer) {
        wlr_log(WLR_ERROR, "Toplevel layer not initialized");
        free(toplevel);
        return;
    }
    toplevel->scene_tree = wlr_scene_tree_create(server->layers.toplevel_layer);
    if (!toplevel->scene_tree) {
        wlr_log(WLR_ERROR, "Failed to create scene tree");
        free(toplevel);
        return;
    }
    toplevel->scene_tree->node.data = toplevel;
    xwayland_surface->data = toplevel->scene_tree;

    toplevel->shadow = wlr_scene_shadow_create(toplevel->scene_tree, 0, 0,
        toplevel->corner_radius, SHADOW_BLUR, SHADOW_COLOR);
    if (!toplevel->shadow) {
        wlr_log(WLR_ERROR, "Failed to create shadow");
        wlr_scene_node_destroy(&toplevel->scene_tree->node);
        free(toplevel);
        return;
    }
    wlr_scene_node_set_position(&toplevel->shadow->node, -SHADOW_BLUR, -SHADOW_BLUR + 12);
    wlr_scene_node_lower_to_bottom(&toplevel->shadow->node);

    toplevel->associate.notify = xwayland_toplevel_associate;
    wl_list_init(&toplevel->associate.link);
    wl_signal_add(&xwayland_surface->events.associate, &toplevel->associate);

    toplevel->request_activate.notify = xwayland_toplevel_activate;
    wl_list_init(&toplevel->request_activate.link);
    wl_signal_add(&toplevel->xwayland_surface->events.request_activate, &toplevel->request_activate);

    toplevel->request_configure.notify = xwayland_toplevel_configure;
    wl_list_init(&toplevel->request_configure.link);
    wl_signal_add(&toplevel->xwayland_surface->events.request_configure, &toplevel->request_configure);

    toplevel->destroy.notify = xwayland_toplevel_destroy;
    wl_list_init(&toplevel->destroy.link);
    wl_signal_add(&toplevel->xwayland_surface->events.destroy, &toplevel->destroy);

    if (xwayland_surface->parent) {
      struct wlr_xwayland_surface *parent = xwayland_surface->parent;
      struct wlr_scene_tree *parent_tree = parent->data;
      toplevel->scene_tree = wlr_scene_tree_create(parent_tree);
      wlr_xwayland_surface_configure(xwayland_surface, 0, 0, 
          xwayland_surface->width ? xwayland_surface->width : 200,
          xwayland_surface->height ? xwayland_surface->height : 200);
    }

    toplevel->corner_radius = 6;
    toplevel->opacity = 1.0f;
    wlr_scene_node_raise_to_top(&toplevel->scene_tree->node);
}
