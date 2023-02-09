#include "yaru_window.h"

static GdkPoint get_cursor_position(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkDisplay* display = gdk_window_get_display(handle);
  GdkSeat* seat = gdk_display_get_default_seat(display);
  GdkDevice* pointer = gdk_seat_get_pointer(seat);
  GdkPoint pos;
  gdk_device_get_position(pointer, nullptr, &pos.x, &pos.y);
  return pos;
}

static GdkPoint get_window_origin(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkPoint pos;
  gdk_window_get_origin(handle, &pos.x, &pos.y);
  return pos;
}

// Returns true if the widget is GtkHeaderBar or HdyHeaderBar from libhandy.
static gboolean is_header_bar(GtkWidget* widget) {
  return widget != nullptr &&
         (GTK_IS_HEADER_BAR(widget) ||
          g_str_has_suffix(G_OBJECT_TYPE_NAME(widget), "HeaderBar"));
}

// Recursively searches for a Gtk/HdyHeaderBar in the widget tree.
static GtkWidget* find_header_bar(GtkWidget* widget) {
  if (is_header_bar(widget)) {
    return widget;
  }

  if (GTK_IS_CONTAINER(widget)) {
    g_autoptr(GList) children =
        gtk_container_get_children(GTK_CONTAINER(widget));
    for (GList* l = children; l != nullptr; l = l->next) {
      GtkWidget* header_bar = find_header_bar(GTK_WIDGET(l->data));
      if (header_bar != nullptr) {
        return header_bar;
      }
    }
  }

  return nullptr;
}

FlValue* yaru_window_get_state(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkWindowState state = gdk_window_get_state(handle);
  GdkWindowTypeHint type = gdk_window_get_type_hint(handle);

  gboolean active = gtk_window_is_active(window);
  gboolean closable = gtk_window_get_deletable(window);
  gboolean fullscreen = state & GDK_WINDOW_STATE_FULLSCREEN;
  gboolean maximized = gtk_window_is_maximized(window);
  gboolean minimized = state & GDK_WINDOW_STATE_ICONIFIED;
  gboolean normal = type == GDK_WINDOW_TYPE_HINT_NORMAL;
  gboolean restorable = normal && (fullscreen || maximized || minimized);
  const gchar* title = yaru_window_get_title(window);
  gboolean visible = gtk_widget_is_visible(GTK_WIDGET(window));

  FlValue* result = fl_value_new_map();
  fl_value_set_string_take(result, "id", fl_value_new_int(0));  // TODO
  fl_value_set_string_take(result, "type", fl_value_new_string("state"));
  fl_value_set_string_take(result, "active", fl_value_new_bool(active));
  fl_value_set_string_take(result, "closable", fl_value_new_bool(closable));
  fl_value_set_string_take(result, "fullscreen", fl_value_new_bool(fullscreen));
  fl_value_set_string_take(result, "maximizable",
                           fl_value_new_bool(normal && !maximized));
  fl_value_set_string_take(result, "maximized", fl_value_new_bool(maximized));
  fl_value_set_string_take(result, "minimizable",
                           fl_value_new_bool(normal && !minimized));
  fl_value_set_string_take(result, "minimized", fl_value_new_bool(minimized));
  fl_value_set_string_take(result, "movable", fl_value_new_bool(true));
  fl_value_set_string_take(result, "restorable", fl_value_new_bool(restorable));
  if (title != nullptr) {
    fl_value_set_string_take(result, "title", fl_value_new_string(title));
  }
  fl_value_set_string_take(result, "visible", fl_value_new_bool(visible));
  return result;
}

void yaru_window_drag(GtkWindow* window) {
  GdkPoint cursor = get_cursor_position(window);
  gtk_window_begin_move_drag(window, GDK_BUTTON_PRIMARY, cursor.x, cursor.y,
                             GDK_CURRENT_TIME);
}

const gchar* yaru_window_get_title(GtkWindow* window) {
  GtkWidget* titlebar = gtk_window_get_titlebar(window);
  if (!is_header_bar(titlebar)) {
    titlebar = find_header_bar(GTK_WIDGET(window));
  }
  if (titlebar != nullptr) {
    const gchar* title;
    g_object_get(titlebar, "title", &title, nullptr);
    return title;
  }
  return gtk_window_get_title(window);
}

void yaru_window_set_title(GtkWindow* window, const gchar* title) {
  GtkWidget* titlebar = gtk_window_get_titlebar(window);
  if (!is_header_bar(titlebar)) {
    titlebar = find_header_bar(GTK_WIDGET(window));
  }
  if (titlebar != nullptr) {
    g_object_set(titlebar, "title", title, nullptr);
  } else {
    gtk_window_set_title(window, title);
  }
}

void yaru_window_hide_title(GtkWindow* window) {
  GtkWidget* titlebar = gtk_window_get_titlebar(window);
  if (!is_header_bar(titlebar)) {
    titlebar = find_header_bar(GTK_WIDGET(window));
  }
  if (titlebar != nullptr) {
    gtk_widget_hide(titlebar);
  }
}

void yaru_window_show_title(GtkWindow* window) {
  GtkWidget* titlebar = gtk_window_get_titlebar(window);
  if (!is_header_bar(titlebar)) {
    titlebar = find_header_bar(GTK_WIDGET(window));
  }
  if (titlebar != nullptr) {
    gtk_widget_show(titlebar);
  }
}

void yaru_window_restore(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkWindowState state = gdk_window_get_state(handle);
  if (state & GDK_WINDOW_STATE_FULLSCREEN) {
    gtk_window_unfullscreen(window);
  } else if (state & GDK_WINDOW_STATE_MAXIMIZED) {
    gtk_window_unmaximize(window);
  } else if (state & GDK_WINDOW_STATE_ICONIFIED) {
    gtk_window_deiconify(window);
  }
}

void yaru_window_show_menu(GtkWindow* window) {
  GdkWindow* handle = gtk_widget_get_window(GTK_WIDGET(window));
  GdkDisplay* display = gdk_window_get_display(handle);
  GdkSeat* seat = gdk_display_get_default_seat(display);
  GdkDevice* pointer = gdk_seat_get_pointer(seat);

  GdkPoint cursor = get_cursor_position(window);
  GdkPoint origin = get_window_origin(window);

  g_autoptr(GdkEvent) event = gdk_event_new(GDK_BUTTON_PRESS);
  event->button.button = GDK_BUTTON_SECONDARY;
  event->button.device = pointer;
  event->button.window = handle;
  event->button.x_root = cursor.x;
  event->button.y_root = cursor.y;
  event->button.x = cursor.x - origin.x;
  event->button.y = cursor.y - origin.y;
  gdk_window_show_window_menu(handle, event);
}

void yaru_window_set_background(GtkWindow* window, guint color) {
  GdkRGBA rgba = {
      .red = ((color >> 16) & 0xFF) / 255.0,
      .green = ((color >> 8) & 0xFF) / 255.0,
      .blue = (color & 0xFF) / 255.0,
      .alpha = (color >> 24) / 255.0,
  };

  gpointer css_provider = g_object_get_data(G_OBJECT(window), "css_provider");
  if (css_provider == nullptr) {
    css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider(
        gtk_widget_get_style_context(GTK_WIDGET(window)),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_set_data_full(G_OBJECT(window), "css_provider", css_provider,
                           g_object_unref);
  }

  g_autofree gchar* str = gdk_rgba_to_string(&rgba);
  g_autofree gchar* css = g_strdup_printf("window { background: %s; }", str);

  g_autoptr(GError) error = nullptr;
  gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(css_provider), css, -1,
                                  &error);
  if (error != nullptr) {
    g_warning("set_window_background: %s", error->message);
  }
}
