/* config.c - Configuration management */
#include "nanollm.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char* get_config_path(const char *filename) {
    static char path[1024];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s/%s", home, CONFIG_DIR, filename);
    return path;
}

void config_ensure_dir(void) {
    char path[1024];
    const char *home = getenv("HOME");
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/%s", home, CONFIG_DIR);
    mkdir(path, 0755);
    snprintf(path, sizeof(path), "%s/%s/%s", home, CONFIG_DIR, HISTORY_DIR);
    mkdir(path, 0755);
}

AppConfig* config_load(void) {
    AppConfig *cfg = (AppConfig*)calloc(1, sizeof(AppConfig));
    cfg->default_provider = PROVIDER_GROQ;

    char *path = get_config_path(CONFIG_FILE);
    FILE *f = fopen(path, "r");
    if (!f) return cfg;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *data = (char*)malloc(len + 1);
    if (!data) { fclose(f); return cfg; }
    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(data);
    free(data);
    if (!root) return cfg;

    cJSON *item = cJSON_GetObjectItem(root, "api_key_groq");
    if (cJSON_IsString(item)) cfg->api_key_groq = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "api_key_gemini");
    if (cJSON_IsString(item)) cfg->api_key_gemini = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "api_key_hf");
    if (cJSON_IsString(item)) cfg->api_key_hf = strdup(item->valuestring);

    item = cJSON_GetObjectItem(root, "default_provider");
    if (cJSON_IsNumber(item)) {
        int v = item->valueint;
        if (v >= 0 && v < PROVIDER_COUNT) cfg->default_provider = (Provider)v;
    }

    cJSON_Delete(root);
    return cfg;
}

void config_save(AppConfig *cfg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "api_key_groq", cfg->api_key_groq ? cfg->api_key_groq : "");
    cJSON_AddStringToObject(root, "api_key_gemini", cfg->api_key_gemini ? cfg->api_key_gemini : "");
    cJSON_AddStringToObject(root, "api_key_hf", cfg->api_key_hf ? cfg->api_key_hf : "");
    cJSON_AddNumberToObject(root, "default_provider", cfg->default_provider);

    char *json = cJSON_Print(root);
    cJSON_Delete(root);

    char *path = get_config_path(CONFIG_FILE);
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(json, f);
        fclose(f);
    }
    free(json);
}

void config_free(AppConfig *cfg) {
    if (!cfg) return;
    free(cfg->api_key_groq);
    free(cfg->api_key_gemini);
    free(cfg->api_key_hf);
    free(cfg);
}
