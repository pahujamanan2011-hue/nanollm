/* chat_history.c - Chat session persistence */
#include "nanollm.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

ChatSession* session_new(Provider provider) {
    ChatSession *s = (ChatSession*)calloc(1, sizeof(ChatSession));
    s->provider = provider;
    s->created_at = time(NULL);

    char id[64];
    struct tm *tm = localtime(&s->created_at);
    snprintf(id, sizeof(id), "%04d-%02d-%02d_%02d-%02d-%02d",
             tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    s->id = strdup(id);
    s->title = strdup("New Chat");
    return s;
}

void session_free(ChatSession *s) {
    if (!s) return;
    free(s->id);
    free(s->title);
    GList *l = s->messages;
    while (l) {
        Message *m = (Message*)l->data;
        free(m->role);
        free(m->content);
        free(m);
        l = l->next;
    }
    g_list_free(s->messages);
    free(s);
}

void session_add_message(ChatSession *s, const char *role, const char *content) {
    Message *m = (Message*)calloc(1, sizeof(Message));
    m->role = strdup(role);
    m->content = strdup(content);
    m->timestamp = time(NULL);
    s->messages = g_list_append(s->messages, m);

    if (strcmp(role, "user") == 0 && (strcmp(s->title, "New Chat") == 0 || strcmp(s->title, "Untitled") == 0)) {
        free(s->title);
        char title[256];
        snprintf(title, sizeof(title), "%.50s", content);
        s->title = strdup(title);
    }
}

char* session_to_json(ChatSession *s) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "id", s->id);
    cJSON_AddStringToObject(root, "title", s->title);
    cJSON_AddNumberToObject(root, "provider", s->provider);
    cJSON_AddNumberToObject(root, "created_at", (double)s->created_at);

    cJSON *msgs = cJSON_CreateArray();
    GList *l = s->messages;
    while (l) {
        Message *m = (Message*)l->data;
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", m->role);
        cJSON_AddStringToObject(msg, "content", m->content);
        cJSON_AddNumberToObject(msg, "timestamp", (double)m->timestamp);
        cJSON_AddItemToArray(msgs, msg);
        l = l->next;
    }
    cJSON_AddItemToObject(root, "messages", msgs);

    char *json = cJSON_Print(root);
    cJSON_Delete(root);
    return json;
}

ChatSession* session_from_json(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    ChatSession *s = (ChatSession*)calloc(1, sizeof(ChatSession));

    cJSON *item = cJSON_GetObjectItem(root, "id");
    s->id = cJSON_IsString(item) ? strdup(item->valuestring) : strdup("unknown");

    item = cJSON_GetObjectItem(root, "title");
    s->title = cJSON_IsString(item) ? strdup(item->valuestring) : strdup("Untitled");

    item = cJSON_GetObjectItem(root, "provider");
    s->provider = cJSON_IsNumber(item) ? (Provider)(int)item->valuedouble : PROVIDER_GROQ;

    item = cJSON_GetObjectItem(root, "created_at");
    s->created_at = cJSON_IsNumber(item) ? (time_t)item->valuedouble : time(NULL);

    cJSON *msgs = cJSON_GetObjectItem(root, "messages");
    if (cJSON_IsArray(msgs)) {
        int count = cJSON_GetArraySize(msgs);
        for (int i = 0; i < count; i++) {
            cJSON *msg = cJSON_GetArrayItem(msgs, i);
            cJSON *role = cJSON_GetObjectItem(msg, "role");
            cJSON *content = cJSON_GetObjectItem(msg, "content");
            if (cJSON_IsString(role) && cJSON_IsString(content)) {
                session_add_message(s, role->valuestring, content->valuestring);
            }
        }
    }

    cJSON_Delete(root);
    return s;
}

char* session_get_filename(ChatSession *s) {
    static char path[1024];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s/%s/%s.json", home, CONFIG_DIR, HISTORY_DIR, s->id);
    return path;
}

void session_save(ChatSession *s) {
    char *json = session_to_json(s);
    char *path = session_get_filename(s);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(json, f);
        fclose(f);
    }
    free(json);
}

GList* history_load_all(void) {
    GList *list = NULL;
    char path[1024];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s/%s", home, CONFIG_DIR, HISTORY_DIR);

    DIR *d = opendir(path);
    if (!d) return NULL;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (strstr(entry->d_name, ".json")) {
            char fpath[1024];
            snprintf(fpath, sizeof(fpath), "%s/%s", path, entry->d_name);
            FILE *f = fopen(fpath, "r");
            if (!f) continue;
            fseek(f, 0, SEEK_END);
            long len = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *data = (char*)malloc(len + 1);
            if (!data) { fclose(f); continue; }
            fread(data, 1, len, f);
            data[len] = '\0';
            fclose(f);

            ChatSession *s = session_from_json(data);
            if (s) list = g_list_append(list, s);
            free(data);
        }
    }
    closedir(d);
    return list;
}
