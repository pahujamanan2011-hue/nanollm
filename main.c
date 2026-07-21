/* main.c - Entry point for NanoLLM Editor */
#include "nanollm.h"
#include <stdlib.h>

int main(int argc, char *argv[]) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    AppState state = {0};
    pthread_mutex_init(&state.lock, NULL);

    config_ensure_dir();
    state.config = config_load();

    gui_init(&argc, &argv, &state);

    state.all_sessions = history_load_all();
    gui_refresh_chat_list(&state);

    if (state.all_sessions != NULL) {
        state.current_session = (ChatSession*)state.all_sessions->data;
        gui_load_session(&state, state.current_session);
    } else {
        state.current_session = session_new(state.config->default_provider);
        state.all_sessions = g_list_append(state.all_sessions, state.current_session);
        gui_load_session(&state, state.current_session);
    }

    gui_run(&state);

    /* Cleanup */
    config_save(state.config);
    config_free(state.config);

    g_list_free_full(state.all_sessions, (GDestroyNotify)session_free);

    pthread_mutex_destroy(&state.lock);
    curl_global_cleanup();

    return 0;
}
