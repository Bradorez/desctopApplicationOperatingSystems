/*  main.c  –  GTK-4 CPU-Scheduling Simulator
 */
#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct {
    int  pid;
    int  arrival;
    int  burst;
    int  remaining;
    int  start_time;
    int  finish_time;
} Process;

typedef struct _App App;
typedef void (*ScheduleStepFn)(App *app, int tick);

struct _App {

    GtkApplication *gtk_app;
    GtkWidget *win;
    GtkWidget *entry_pid;
    GtkWidget *entry_arrival;
    GtkWidget *entry_burst;
    GtkWidget *entry_quantum;
    GtkWidget *alg_radio_rr;
    GtkListStore *store;
    GtkDrawingArea *canvas;
    GtkWidget *btn_start;
    GtkWidget *btn_reset;

    GtkWidget *alg_radio_fcfs;
    GtkWidget *alg_radio_sjn;
    GtkWidget *alg_radio_srt;

    GPtrArray *processes;
    ScheduleStepFn step_fn;
    guint sim_tick_ms;
    guint timer_id;
    int clock;
    int quantum;
    int quantum_left;
    Process *running;
};

static void gui_build(App *app);

static gboolean simulate_timeout_cb(gpointer user_data);

static void add_process_to_view(App *app, Process *p)
{
    GtkTreeIter it;
    gtk_list_store_append(app->store, &it);
    gtk_list_store_set(app->store, &it,0, p->pid, 1, p->arrival, 2, p->burst, -1);
}

static void on_add_clicked(GtkButton *btn, gpointer user_data)
{
    App *app = user_data;
    const gchar *pid_txt = gtk_editable_get_text(GTK_EDITABLE(app->entry_pid));
    const gchar *arr_txt = gtk_editable_get_text(GTK_EDITABLE(app->entry_arrival));
    const gchar *burst_txt = gtk_editable_get_text(GTK_EDITABLE(app->entry_burst));

    if (*pid_txt == '\0' || *arr_txt == '\0' || *burst_txt == '\0')
        return;

    Process *p = g_new0(Process, 1);
    p->pid = atoi(pid_txt);
    p->arrival = atoi(arr_txt);
    p->burst = atoi(burst_txt);
    p->remaining = p->burst;
    g_ptr_array_add(app->processes, p);
    int rows = app->processes->len;
    gtk_widget_set_size_request(GTK_WIDGET(app->canvas), -1, rows * 24 + 10);

    add_process_to_view(app, p);

    gtk_editable_set_text(GTK_EDITABLE(app->entry_pid), "");
    gtk_editable_set_text(GTK_EDITABLE(app->entry_arrival), "");
    gtk_editable_set_text(GTK_EDITABLE(app->entry_burst), "");
}

static void on_clear_clicked(GtkButton *btn, gpointer user_data)
{
    App *app = user_data;
    gtk_list_store_clear(app->store);
    for (guint i=0;i<app->processes->len;i++)
    g_free(g_ptr_array_index(app->processes,i));
    g_ptr_array_set_size(app->processes,0);
}

static void set_color_for_pid(cairo_t *cr, int pid)
{
    const double base[3] = {0.3, 0.6, 0.9};
    cairo_set_source_rgb(cr,
        fmod(base[0] + (pid*0.17), 1.0),
        fmod(base[1] + (pid*0.29), 1.0),
        fmod(base[2] + (pid*0.41), 1.0));
}

static void draw_cb(GtkDrawingArea *area,
    cairo_t *cr,
    int width,
    int height,
    gpointer data)
{
App *app = data;
const int row_h = 24;
const int label_w = 50;
const int text_off= 4;

cairo_set_source_rgb(cr, 1,1,1);
cairo_paint(cr);

int visible_i = 0;
for (guint i = 0; i < app->processes->len; ++i)
{
Process *p = g_ptr_array_index(app->processes, i);
if (p->arrival > app->clock)
continue;

int y = visible_i * row_h;
int bar_w = width - label_w - 80;

cairo_set_source_rgb(cr, 0,0,0);
cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
cairo_set_font_size(cr, 12);
char pid_txt[16]; snprintf(pid_txt,sizeof(pid_txt),"P%d",p->pid);
cairo_move_to(cr, text_off, y + 16);
cairo_show_text(cr, pid_txt);

cairo_set_source_rgb(cr, .9,.9,.9);
cairo_rectangle(cr, label_w, y + 2, bar_w, 20);
cairo_fill(cr);

double frac = 1.0 - (double)p->remaining / p->burst;
set_color_for_pid(cr, p->pid);
cairo_rectangle(cr, label_w, y + 2, frac * bar_w, 20);
cairo_fill(cr);

const char *state = NULL;
if (p->remaining == 0)
    state = "done";
else if (p == app->running)
    state = "running";
else
    state = "ready";

if (state)
{
cairo_set_source_rgb(cr, 0,0,0);
cairo_move_to(cr, label_w + bar_w + text_off, y + 16);
cairo_show_text(cr, state);
}

++visible_i;
}
}

static void scheduler_fcfs(App *app, int tick)
{
    if (!app->running) {
        Process *earliest = NULL;
        int min_arrival = INT_MAX;

        for (guint i = 0; i < app->processes->len; ++i) {
            Process *p = g_ptr_array_index(app->processes, i);
            if (p->remaining > 0 && p->arrival <= app->clock) {
                if (p->arrival < min_arrival) {
                    min_arrival = p->arrival;
                    earliest = p;
                } else if (p->arrival == min_arrival && !earliest) {
                    earliest = p;
                }
            }
        }

        if (earliest) {
            app->running = earliest;
        }
    }

    if (app->running) {
        app->running->remaining--;

        if (app->running->remaining == 0) {
            app->running = NULL;
        }
    }
}
static void scheduler_sjn(App *app, int tick)
{
    if (!app->running) {
        Process *shortest = NULL;
        int min_burst = INT_MAX;
        int earliest_arrival = INT_MAX;

        for (guint i = 0; i < app->processes->len; ++i) {
            Process *p = g_ptr_array_index(app->processes, i);
            if (p->remaining > 0 && p->arrival <= app->clock) {
                if (p->burst < min_burst || (p->burst == min_burst && p->arrival < earliest_arrival)) {
                    min_burst = p->burst;
                    earliest_arrival = p->arrival;
                    shortest = p;
                }
            }
        }

        if (shortest)
            app->running = shortest;
    }

    if (app->running) {
        app->running->remaining--;
        if (app->running->remaining == 0)
            app->running = NULL;
    }
}


static void scheduler_srt(App *app, int tick)
{
    Process *shortest = NULL;
    int min_remaining = INT_MAX;

    for (guint i = 0; i < app->processes->len; ++i) {
        Process *p = g_ptr_array_index(app->processes, i);
        if (p->remaining > 0 && p->arrival <= app->clock) {
            if (p->remaining < min_remaining ||
                (p->remaining == min_remaining && p->arrival < (shortest ? shortest->arrival : INT_MAX))) {
                min_remaining = p->remaining;
                shortest = p;
            }
        }
    }

    app->running = shortest;

    if (app->running) {
        app->running->remaining--;
        if (app->running->remaining == 0)
            app->running = NULL;
    }
}

static void scheduler_rr(App *app, int tick)
{
    static int last_index = -1;

    if (!app->running || app->running->remaining == 0 || app->quantum_left == 0)
    {
        if (app->running && app->running->remaining == 0)
            app->running = NULL;

        guint total = app->processes->len;
        for (guint offset = 1; offset <= total; offset++) {
            guint i = (last_index + offset) % total;
            Process *p = g_ptr_array_index(app->processes, i);

            if (p->arrival <= app->clock && p->remaining > 0) {
                app->running = p;
                last_index = i;
                app->quantum_left = app->quantum;
                break;
            }
        }
    }

    if (app->running) {
        app->running->remaining--;
        app->quantum_left--;

        if (app->running->remaining == 0)
            app->running = NULL;
    }
}

static void on_start_clicked(GtkButton *btn, gpointer user_data)
{
    App *app = user_data;
    if (app->processes->len == 0) return;

    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(app->alg_radio_rr))) {
        app->step_fn  = scheduler_rr;
        app->quantum  = atoi(gtk_editable_get_text(GTK_EDITABLE(app->entry_quantum)));
    }
    else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(app->alg_radio_sjn))) {
        app->step_fn  = scheduler_sjn;
    }
    else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(app->alg_radio_srt))) {
        app->step_fn  = scheduler_srt;
    }
    else {
        app->step_fn  = scheduler_fcfs;
    }

    gtk_widget_set_sensitive(GTK_WIDGET(app->btn_start), FALSE);
    app->clock     = 0;
    app->running   = NULL;
    app->timer_id  = g_timeout_add(app->sim_tick_ms, simulate_timeout_cb, app);
}

static gboolean simulate_timeout_cb(gpointer user_data)
{
    App *app = user_data;

    if (app->step_fn)
        app->step_fn(app, app->clock);

    gtk_widget_queue_draw(GTK_WIDGET(app->canvas));
    app->clock++;

    gboolean done = TRUE;
    for (guint i=0;i<app->processes->len;i++) {
        Process *p=g_ptr_array_index(app->processes,i);
        if (p->remaining>0) { done=FALSE; break; }
    }
    if (done) {
        gtk_widget_set_sensitive(GTK_WIDGET(app->btn_start), TRUE);
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void on_reset_clicked(GtkButton *btn, gpointer user_data)
{
    App *app = user_data;
    if (app->timer_id) g_source_remove(app->timer_id);
    gtk_widget_set_sensitive(GTK_WIDGET(app->btn_start), TRUE);
    app->clock=0; app->running=NULL;
    for (guint i=0;i<app->processes->len;i++) {
        Process *p=g_ptr_array_index(app->processes,i);
        p->remaining=p->burst;
    }
    gtk_widget_queue_draw(GTK_WIDGET(app->canvas));
}

static void on_algo_toggled(GtkCheckButton *tb, gpointer data)
{
    App *app = data;
    gboolean rr = gtk_check_button_get_active(tb);
    gtk_widget_set_visible(app->entry_quantum, rr);
}

static void gui_build(App *app)
{
    app->win = gtk_application_window_new(app->gtk_app);
    gtk_window_set_title(GTK_WINDOW(app->win),"CPU Scheduling Simulator");
    gtk_window_set_default_size(GTK_WINDOW(app->win),800,600);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid),8);
    gtk_grid_set_column_spacing(GTK_GRID(grid),12);
    gtk_widget_set_margin_top(grid,10);
    gtk_widget_set_margin_start(grid,10);
    gtk_widget_set_margin_end(grid,10);
    gtk_widget_set_margin_bottom(grid,10);
    gtk_window_set_child(GTK_WINDOW(app->win),grid);

    GtkWidget *lbl_pid = gtk_label_new("PID");
    GtkWidget *lbl_arrival = gtk_label_new("Arrival");
    GtkWidget *lbl_burst = gtk_label_new("Burst");
    app->entry_pid = gtk_entry_new();
    app->entry_arrival = gtk_entry_new();
    app->entry_burst = gtk_entry_new();
    GtkWidget *btn_add = gtk_button_new_with_label("Add");
    GtkWidget *btn_clear = gtk_button_new_with_label("Clear");

    g_signal_connect(btn_add, "clicked", G_CALLBACK(on_add_clicked), app);
    g_signal_connect(btn_clear, "clicked", G_CALLBACK(on_clear_clicked), app);

    gtk_grid_attach(GTK_GRID(grid), lbl_pid, 0,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), app->entry_pid, 1,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), lbl_arrival, 2,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), app->entry_arrival, 3,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), lbl_burst, 4,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), app->entry_burst, 5,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_add, 6,0,1,1);
    gtk_grid_attach(GTK_GRID(grid), btn_clear, 7,0,1,1);

    GtkWidget *radio_fcfs = gtk_check_button_new_with_label("FCFS");
    GtkWidget *radio_sjn = gtk_check_button_new_with_label("SJN");
    GtkWidget *radio_srt = gtk_check_button_new_with_label("SRT");
    GtkWidget *radio_rr  = gtk_check_button_new_with_label("RR");

    gtk_check_button_set_group(GTK_CHECK_BUTTON(radio_sjn), GTK_CHECK_BUTTON(radio_fcfs));
    gtk_check_button_set_group(GTK_CHECK_BUTTON(radio_srt), GTK_CHECK_BUTTON(radio_fcfs));
    gtk_check_button_set_group(GTK_CHECK_BUTTON(radio_rr), GTK_CHECK_BUTTON(radio_fcfs));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(radio_fcfs), TRUE);

    app->alg_radio_rr = radio_rr;
    app->alg_radio_fcfs = radio_fcfs;
    app->alg_radio_sjn = radio_sjn;
    app->alg_radio_srt = radio_srt;

    gtk_grid_attach(GTK_GRID(grid), radio_fcfs, 0,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), radio_sjn, 1,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), radio_srt, 2,1,1,1);
    gtk_grid_attach(GTK_GRID(grid), radio_rr, 3,1,1,1);

    app->entry_quantum = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry_quantum),"Quantum");
    gtk_grid_attach(GTK_GRID(grid), app->entry_quantum,4,1,1,1);
    gtk_widget_set_visible(app->entry_quantum,FALSE);
    g_signal_connect(radio_rr,"toggled", G_CALLBACK(on_algo_toggled), app);

    app->store = gtk_list_store_new(3,G_TYPE_INT,G_TYPE_INT,G_TYPE_INT);
    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(app->store));
    gtk_widget_set_size_request(tree, -1,150);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *col;
    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("PID", renderer, "text",0,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),col);
    col = gtk_tree_view_column_new_with_attributes("Arr", renderer, "text",1,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),col);
    col = gtk_tree_view_column_new_with_attributes("Burst", renderer, "text",2,NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),col);

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll),tree);
    gtk_grid_attach(GTK_GRID(grid), scroll,0,2,8,1);

    app->canvas = GTK_DRAWING_AREA(gtk_drawing_area_new());
    gtk_widget_set_size_request(GTK_WIDGET(app->canvas), -1, 200);
    gtk_drawing_area_set_draw_func(app->canvas, draw_cb, app, NULL);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(app->canvas),0,3,8,1);

    app->btn_start = gtk_button_new_with_label("Start Simulation");
    app->btn_reset = gtk_button_new_with_label("Reset");
    gtk_grid_attach(GTK_GRID(grid), app->btn_start, 0,4,2,1);
    gtk_grid_attach(GTK_GRID(grid), app->btn_reset, 2,4,2,1);

    g_signal_connect(app->btn_start,"clicked",G_CALLBACK(on_start_clicked),app);
    g_signal_connect(app->btn_reset,"clicked",G_CALLBACK(on_reset_clicked),app);
}

static void on_activate(GtkApplication *gtk_app, gpointer user_data)
{
    App *app = g_new0(App,1);
    app->gtk_app = gtk_app;
    app->processes = g_ptr_array_new();
    app->sim_tick_ms= 500;

    gui_build(app);
    gtk_window_present(GTK_WINDOW(app->win));
}

int main(int argc,char **argv)
{
    GtkApplication *gtk_app = gtk_application_new("org.example.GtkScheduler", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app,"activate",G_CALLBACK(on_activate),NULL);
    int status = g_application_run(G_APPLICATION(gtk_app),argc,argv);
    g_object_unref(gtk_app);
    return status;
}