/* gui.c - GTK3 GUI implementation */
#include "nanollm.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    AppState *app;
    char *prompt;
} ThreadArgs;

typedef struct {
    AppState *app;
    char *response;
    gboolean success;
} ThreadResult;

static void on_send_clicked(GtkButton *btn, gpointer user_data);
static void on_new_chat_clicked(GtkButton *btn, gpointer user_data);
static void on_chat_selected(GtkTreeView *tv, gpointer user_data);
static void on_api_keys_clicked(GtkButton *btn, gpointer user_data);
static void on_provider_changed(GtkComboBox *combo, gpointer user_data);
static void on_window_destroy(GtkWidget *w, gpointer user_data);
static gboolean on_input_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);

static gboolean on_api_complete(gpointer user_data) {
    ThreadResult *res = (ThreadResult*)user_data;
    AppState *app = res->app;

    pthread_mutex_lock(&app->lock);
    app->is_generating = FALSE;
    pthread_mutex_unlock(&app->lock);

    if (res->success && res->response) {
        session_add_message(app->current_session, "assistant", res->response);
        gui_append_message(app, "assistant", res->response);
        session_save(app->current_session);
    } else {
        gui_append_message(app, "assistant", "Error: Failed to get response from API. Check your API key and network connection.");
    }

    gtk_widget_set_sensitive(app->input_view, TRUE);
    gui_set_status(app, "Ready");

    free(res->response);
    free(res);
    return G_SOURCE_REMOVE;
}

static void* api_thread_func(void *arg) {
    ThreadArgs *args = (ThreadArgs*)arg;
    AppState *app = args->app;

    const char *api_key = NULL;
    switch (app->current_session->provider) {
        case PROVIDER_GROQ: api_key = app->config->api_key_groq; break;
        case PROVIDER_GEMINI: api_key = app->config->api_key_gemini; break;
        case PROVIDER_HUGGINGFACE: api_key = app->config->api_key_hf; break;
        default: break;
    }

    ThreadResult *res = (ThreadResult*)calloc(1, sizeof(ThreadResult));
    res->app = app;

    if (api_key && strlen(api_key) > 0) {
        res->response = api_send_message(app->current_session->provider, api_key,
                                         app->current_session, args->prompt);
        res->success = (res->response != NULL);
    } else {
        res->response = strdup("Error: No API key configured for this provider.");
        res->success = FALSE;
    }

    g_idle_add(on_api_complete, res);

    free(args->prompt);
    free(args);
    return NULL;
}

static void on_send_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState*)user_data;

    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(app->input_buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(app->input_buffer, &start, &end, FALSE);

    if (!text || strlen(text) == 0) {
        g_free(text);
        return;
    }

    pthread_mutex_lock(&app->lock);
    if (app->is_generating) {
        pthread_mutex_unlock(&app->lock);
        g_free(text);
        return;
    }
    app->is_generating = TRUE;
    pthread_mutex_unlock(&app->lock);

    session_add_message(app->current_session, "user", text);
    gui_append_message(app, "user", text);
    session_save(app->current_session);
    gui_refresh_chat_list(app);

    gtk_text_buffer_set_text(app->input_buffer, "", -1);
    gtk_widget_set_sensitive(app->input_view, FALSE);
    gui_set_status(app, "Generating...");

    ThreadArgs *args = (ThreadArgs*)calloc(1, sizeof(ThreadArgs));
    args->app = app;
    args->prompt = strdup(text);

    pthread_t thread;
    pthread_create(&thread, NULL, api_thread_func, args);
    pthread_detach(thread);

    g_free(text);
}

static void on_new_chat_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState*)user_data;

    pthread_mutex_lock(&app->lock);
    if (app->is_generating) {
        pthread_mutex_unlock(&app->lock);
        return;
    }
    pthread_mutex_unlock(&app->lock);

    if (app->current_session) {
        session_save(app->current_session);
    }

    ChatSession *s = session_new(app->config->default_provider);
    app->all_sessions = g_list_append(app->all_sessions, s);
    app->current_session = s;

    gtk_text_buffer_set_text(app->conversation_buffer, "", -1);
    gui_refresh_chat_list(app);
    gui_set_status(app, "New chat started");
}

static void on_chat_selected(GtkTreeView *tv, gpointer user_data) {
    (void)tv;
    AppState *app = (AppState*)user_data;

    pthread_mutex_lock(&app->lock);
    if (app->is_generating) {
        pthread_mutex_unlock(&app->lock);
        return;
    }
    pthread_mutex_unlock(&app->lock);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(app->chat_tree));
    GtkTreeModel *model;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(sel, &model, &iter)) return;

    gpointer ptr;
    gtk_tree_model_get(model, &iter, 1, &ptr, -1);
    ChatSession *s = (ChatSession*)ptr;
    if (!s) return;

    if (app->current_session) {
        session_save(app->current_session);
    }

    app->current_session = s;
    gui_load_session(app, s);
}

static void on_api_keys_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    AppState *app = (AppState*)user_data;

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        "API Keys Configuration",
        GTK_WINDOW(app->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Save", GTK_RESPONSE_ACCEPT,
        "Cancel", GTK_RESPONSE_REJECT,
        NULL
    );

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);

    GtkWidget *l1 = gtk_label_new("Groq API Key:");
    gtk_widget_set_halign(l1, GTK_ALIGN_END);
    GtkWidget *e1 = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e1), 50);
    if (app->config->api_key_groq) gtk_entry_set_text(GTK_ENTRY(e1), app->config->api_key_groq);
    gtk_entry_set_visibility(GTK_ENTRY(e1), FALSE);

    GtkWidget *l2 = gtk_label_new("Gemini API Key:");
    gtk_widget_set_halign(l2, GTK_ALIGN_END);
    GtkWidget *e2 = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e2), 50);
    if (app->config->api_key_gemini) gtk_entry_set_text(GTK_ENTRY(e2), app->config->api_key_gemini);
    gtk_entry_set_visibility(GTK_ENTRY(e2), FALSE);

    GtkWidget *l3 = gtk_label_new("Hugging Face API Key:");
    gtk_widget_set_halign(l3, GTK_ALIGN_END);
    GtkWidget *e3 = gtk_entry_new();
    gtk_entry_set_width_chars(GTK_ENTRY(e3), 50);
    if (app->config->api_key_hf) gtk_entry_set_text(GTK_ENTRY(e3), app->config->api_key_hf);
    gtk_entry_set_visibility(GTK_ENTRY(e3), FALSE);

    gtk_grid_attach(GTK_GRID(grid), l1, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e1, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), l2, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e2, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), l3, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), e3, 1, 2, 1, 1);

    gtk_box_pack_start(GTK_BOX(content), grid, TRUE, TRUE, 0);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        free(app->config->api_key_groq);
        free(app->config->api_key_gemini);
        free(app->config->api_key_hf);

        app->config->api_key_groq = strdup(gtk_entry_get_text(GTK_ENTRY(e1)));
        app->config->api_key_gemini = strdup(gtk_entry_get_text(GTK_ENTRY(e2)));
        app->config->api_key_hf = strdup(gtk_entry_get_text(GTK_ENTRY(e3)));

        config_save(app->config);
    }

    gtk_widget_destroy(dialog);
}

static void on_provider_changed(GtkComboBox *combo, gpointer user_data) {
    AppState *app = (AppState*)user_data;
    int active = gtk_combo_box_get_active(combo);
    if (active >= 0 && active < PROVIDER_COUNT) {
        app->config->default_provider = (Provider)active;
        if (app->current_session) {
            app->current_session->provider = (Provider)active;
        }
        config_save(app->config);
    }
}

static void on_window_destroy(GtkWidget *w, gpointer user_data) {
    (void)w;
    AppState *app = (AppState*)user_data;
    if (app->current_session) {
        session_save(app->current_session);
    }
    config_save(app->config);
    gtk_main_quit();
}

static gboolean on_input_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    (void)widget;
    if ((event->state & GDK_CONTROL_MASK) &&
        (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) {
        on_send_clicked(NULL, user_data);
        return TRUE;
    }
    return FALSE;
}

void gui_append_message(AppState *app, const char *role, const char *content) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(app->conversation_buffer, &end);

    GtkTextIter start;
    gtk_text_buffer_get_start_iter(app->conversation_buffer, &start);
    if (gtk_text_iter_compare(&start, &end) != 0) {
        gtk_text_buffer_insert(app->conversation_buffer, &end, "\n\n", -1);
    }

    char label[64];
    snprintf(label, sizeof(label), "[%s]: ", strcmp(role, "user") == 0 ? "You" : "AI");
    gtk_text_buffer_get_end_iter(app->conversation_buffer, &end);
    gtk_text_buffer_insert_with_tags(app->conversation_buffer, &end, label, -1, app->bold_tag, NULL);

    gtk_text_buffer_get_end_iter(app->conversation_buffer, &end);
    gtk_text_buffer_insert(app->conversation_buffer, &end, content, -1);

    gtk_text_buffer_get_end_iter(app->conversation_buffer, &end);
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(app->conversation_view), &end, 0.0, FALSE, 0.0, 0.0);
}

void gui_set_status(AppState *app, const char *msg) {
    gtk_statusbar_push(GTK_STATUSBAR(app->statusbar), app->status_ctx, msg);
}

void gui_refresh_chat_list(AppState *app) {
    gtk_list_store_clear(app->chat_store);
    GList *l = app->all_sessions;
    while (l) {
        ChatSession *s = (ChatSession*)l->data;
        GtkTreeIter iter;
        gtk_list_store_append(app->chat_store, &iter);
        gtk_list_store_set(app->chat_store, &iter, 0, s->title, 1, s, -1);
        l = l->next;
    }
}

void gui_load_session(AppState *app, ChatSession *session) {
    gtk_text_buffer_set_text(app->conversation_buffer, "", -1);

    GList *l = session->messages;
    while (l) {
        Message *m = (Message*)l->data;
        gui_append_message(app, m->role, m->content);
        l = l->next;
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(app->provider_combo), session->provider);
    gui_set_status(app, "Session loaded");
}

void gui_init(int *argc, char ***argv, AppState *state) {
    gtk_init(argc, argv);

    state->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(state->window), APP_NAME);
    gtk_window_set_default_size(GTK_WINDOW(state->window), 1000, 700);
    g_signal_connect(state->window, "destroy", G_CALLBACK(on_window_destroy), state);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(state->window), vbox);

    /* Menu Bar */
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);

    GtkWidget *new_chat_item = gtk_menu_item_new_with_label("New Chat");
    g_signal_connect(new_chat_item, "activate", G_CALLBACK(on_new_chat_clicked), state);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), new_chat_item);

    GtkWidget *sep1 = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), sep1);

    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_item);

    GtkWidget *edit_menu = gtk_menu_new();
    GtkWidget *edit_item = gtk_menu_item_new_with_label("Edit");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(edit_item), edit_menu);

    GtkWidget *keys_item = gtk_menu_item_new_with_label("API Keys...");
    g_signal_connect(keys_item, "activate", G_CALLBACK(on_api_keys_clicked), state);
    gtk_menu_shell_append(GTK_MENU_SHELL(edit_menu), keys_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), edit_item);
    gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);

    /* Horizontal Paned: Sidebar | Main */
    GtkWidget *hpaned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(hpaned), 250);
    gtk_box_pack_start(GTK_BOX(vbox), hpaned, TRUE, TRUE, 0);

    /* Left Sidebar */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 5);

    GtkWidget *prov_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(prov_label), "<b>Provider</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), prov_label, FALSE, FALSE, 0);

    state->provider_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->provider_combo), "Groq (Llama 3)");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->provider_combo), "Google Gemini");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(state->provider_combo), "Hugging Face");
    gtk_combo_box_set_active(GTK_COMBO_BOX(state->provider_combo), state->config->default_provider);
    g_signal_connect(state->provider_combo, "changed", G_CALLBACK(on_provider_changed), state);
    gtk_box_pack_start(GTK_BOX(sidebar), state->provider_combo, FALSE, FALSE, 0);

    GtkWidget *api_keys_btn = gtk_button_new_with_label("Configure API Keys");
    g_signal_connect(api_keys_btn, "clicked", G_CALLBACK(on_api_keys_clicked), state);
    gtk_box_pack_start(GTK_BOX(sidebar), api_keys_btn, FALSE, FALSE, 0);

    GtkWidget *hist_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hist_label), "<b>Chat History</b>");
    gtk_box_pack_start(GTK_BOX(sidebar), hist_label, FALSE, FALSE, 5);

    state->chat_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
    state->chat_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(state->chat_store));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Title", renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->chat_tree), column);

    GtkWidget *scroll_list = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_list),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll_list), state->chat_tree);
    gtk_box_pack_start(GTK_BOX(sidebar), scroll_list, TRUE, TRUE, 0);

    g_signal_connect(state->chat_tree, "cursor-changed", G_CALLBACK(on_chat_selected), state);

    GtkWidget *new_chat_btn = gtk_button_new_with_label("New Chat");
    g_signal_connect(new_chat_btn, "clicked", G_CALLBACK(on_new_chat_clicked), state);
    gtk_box_pack_start(GTK_BOX(sidebar), new_chat_btn, FALSE, FALSE, 0);

    gtk_paned_pack1(GTK_PANED(hpaned), sidebar, FALSE, FALSE);

    /* Right Side - Vertical Paned: Conversation | Input */
    GtkWidget *vpaned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(vpaned), 500);

    /* Conversation View */
    GtkWidget *scroll_conv = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_conv),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    state->conversation_view = gtk_text_view_new();
    state->conversation_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->conversation_view));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(state->conversation_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->conversation_view), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(scroll_conv), state->conversation_view);
    gtk_paned_pack1(GTK_PANED(vpaned), scroll_conv, TRUE, FALSE);

    /* Input Area */
    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(input_box), 5);

    GtkWidget *scroll_input = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_input),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    state->input_view = gtk_text_view_new();
    state->input_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(state->input_view));
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(state->input_view), GTK_WRAP_WORD_CHAR);
    gtk_widget_set_size_request(state->input_view, -1, 100);
    gtk_container_add(GTK_CONTAINER(scroll_input), state->input_view);
    gtk_box_pack_start(GTK_BOX(input_box), scroll_input, TRUE, TRUE, 0);

    GtkWidget *send_btn = gtk_button_new_with_label("Send");
    gtk_widget_set_size_request(send_btn, 80, -1);
    g_signal_connect(send_btn, "clicked", G_CALLBACK(on_send_clicked), state);
    gtk_box_pack_start(GTK_BOX(input_box), send_btn, FALSE, FALSE, 0);

    gtk_paned_pack2(GTK_PANED(vpaned), input_box, FALSE, FALSE);
    gtk_paned_pack2(GTK_PANED(hpaned), vpaned, TRUE, FALSE);

    /* Status Bar */
    state->statusbar = gtk_statusbar_new();
    state->status_ctx = gtk_statusbar_get_context_id(GTK_STATUSBAR(state->statusbar), "main");
    gtk_box_pack_start(GTK_BOX(vbox), state->statusbar, FALSE, FALSE, 0);

    /* Bold tag for labels */
    state->bold_tag = gtk_text_buffer_create_tag(state->conversation_buffer, "bold",
        "weight", PANGO_WEIGHT_BOLD, NULL);

    /* Keyboard shortcut */
    g_signal_connect(state->input_view, "key-press-event", G_CALLBACK(on_input_key_press), state);

    gtk_widget_show_all(state->window);
    gui_set_status(state, "Ready");
}

void gui_run(AppState *state) {
    (void)state;
    gtk_main();
}
