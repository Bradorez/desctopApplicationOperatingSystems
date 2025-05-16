#include <gtk/gtk.h>

typedef struct {
    GtkWidget *entry_pid;
    GtkWidget *entry_arrival;
    GtkWidget *entry_burst;
    GtkWidget *process_list;
    GtkWidget *list_store;
} AppWidgets;

static void on_add_process(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;

    const char *pid_text = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_pid));
    const char *arrival_text = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_arrival));
    const char *burst_text = gtk_editable_get_text(GTK_EDITABLE(widgets->entry_burst));

    if (g_strcmp0(pid_text, "") == 0 || g_strcmp0(arrival_text, "") == 0 || g_strcmp0(burst_text, "") == 0)
        return;

    GtkListStore *store = GTK_LIST_STORE(widgets->list_store);
    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       0, pid_text,
                       1, arrival_text,
                       2, burst_text,
                       -1);

    gtk_editable_set_text(GTK_EDITABLE(widgets->entry_pid), "");
    gtk_editable_set_text(GTK_EDITABLE(widgets->entry_arrival), "");
    gtk_editable_set_text(GTK_EDITABLE(widgets->entry_burst), "");
}

static void on_activate(GtkApplication *app, gpointer user_data) {
    AppWidgets *widgets = g_malloc(sizeof(AppWidgets));

    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "GTK Scheduler Simulator");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    gtk_widget_set_margin_top(grid, 10);
    gtk_widget_set_margin_start(grid, 10);

    // Labels and Entry Fields
    GtkWidget *label_pid = gtk_label_new("PID:");
    GtkWidget *label_arrival = gtk_label_new("Arrival:");
    GtkWidget *label_burst = gtk_label_new("Burst:");

    widgets->entry_pid = gtk_entry_new();
    widgets->entry_arrival = gtk_entry_new();
    widgets->entry_burst = gtk_entry_new();

    GtkWidget *button_add = gtk_button_new_with_label("Add Process");
    g_signal_connect(button_add, "clicked", G_CALLBACK(on_add_process), widgets);

    gtk_grid_attach(GTK_GRID(grid), label_pid,     0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_pid,     1, 0, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), label_arrival, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_arrival, 1, 1, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), label_burst,   0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->entry_burst,   1, 2, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), button_add,    0, 3, 3, 1);

    // Process List
    GtkListStore *store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    widgets->list_store = GTK_WIDGET(store);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    widgets->process_list = tree;

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("PID", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    column = gtk_tree_view_column_new_with_attributes("Arrival", renderer, "text", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    column = gtk_tree_view_column_new_with_attributes("Burst", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), column);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tree);
    gtk_widget_set_size_request(scroll, 600, 200);

    gtk_grid_attach(GTK_GRID(grid), scroll, 0, 4, 3, 1);

    gtk_window_set_child(GTK_WINDOW(window), grid);
    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.Scheduler", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}