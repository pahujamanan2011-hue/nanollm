/* nanollm.h - Main header for NanoLLM Editor */
#ifndef NANOLLM_H
#define NANOLLM_H

#include <gtk/gtk.h>
#include <curl/curl.h>
#include <pthread.h>
#include <time.h>

#define APP_NAME "NanoLLM Editor"
#define APP_VERSION "1.0.0"
#define CONFIG_DIR ".config/nanollm_editor"
#define HISTORY_DIR "history"
#define CONFIG_FILE "config.json"

typedef enum {
    PROVIDER_GROQ = 0,
    PROVIDER_GEMINI,
    PROVIDER_HUGGINGFACE,
    PROVIDER_COUNT
} Provider;

typedef struct {
    char *role;
    char *content;
    time_t timestamp;
} Message;

typedef struct {
    char *id;
    char *title;
    Provider provider;
    time_t created_at;
    GList *messages;
} ChatSession;

typedef struct {
    char *api_key_groq;
    char *api_key_gemini;
    char *api_key_hf;
    Provider default_provider;
} AppConfig;

typedef struct {
    GtkWidget *window;
    GtkTextBuffer *conversation_buffer;
    GtkWidget *conversation_view;
    GtkTextBuffer *input_buffer;
    GtkWidget *input_view;
    GtkWidget *statusbar;
    guint status_ctx;
    GtkListStore *chat_store;
    GtkWidget *chat_tree;
    GtkWidget *provider_combo;
    GtkTextTag *bold_tag;

    AppConfig *config;
    ChatSession *current_session;
    GList *all_sessions;

    pthread_mutex_t lock;
    volatile gboolean is_generating;
} AppState;

/* config.c */
char* get_config_path(const char *filename);
void config_ensure_dir(void);
AppConfig* config_load(void);
void config_save(AppConfig *cfg);
void config_free(AppConfig *cfg);

/* chat_history.c */
ChatSession* session_new(Provider provider);
void session_free(ChatSession *s);
void session_add_message(ChatSession *s, const char *role, const char *content);
char* session_to_json(ChatSession *s);
ChatSession* session_from_json(const char *json);
void session_save(ChatSession *s);
GList* history_load_all(void);
char* session_get_filename(ChatSession *s);

/* network.c */
typedef struct {
    char *memory;
    size_t size;
} MemoryStruct;

size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp);
char* http_post(const char *url, const char *post_data, struct curl_slist *headers, long *http_code);
char* http_get(const char *url, struct curl_slist *headers, long *http_code);

/* api_providers.c */
char* build_request_groq(ChatSession *session, const char *new_prompt);
char* build_request_gemini(ChatSession *session, const char *new_prompt);
char* build_request_hf(ChatSession *session, const char *new_prompt);

char* parse_response_groq(const char *json);
char* parse_response_gemini(const char *json);
char* parse_response_hf(const char *json);

char* api_send_message(Provider p, const char *api_key, ChatSession *session, const char *new_prompt);

/* gui.c */
void gui_init(int *argc, char ***argv, AppState *state);
void gui_run(AppState *state);
void gui_append_message(AppState *state, const char *role, const char *content);
void gui_set_status(AppState *state, const char *msg);
void gui_refresh_chat_list(AppState *state);
void gui_load_session(AppState *state, ChatSession *session);

#endif
