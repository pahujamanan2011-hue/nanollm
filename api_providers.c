/* api_providers.c - Multi-provider LLM API handlers */
#include "nanollm.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* build_request_groq(ChatSession *session, const char *new_prompt) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "llama3-8b-8192");
    cJSON_AddBoolToObject(root, "stream", 0);

    cJSON *messages = cJSON_CreateArray();
    GList *l = session->messages;
    while (l) {
        Message *m = (Message*)l->data;
        cJSON *msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "role", m->role);
        cJSON_AddStringToObject(msg, "content", m->content);
        cJSON_AddItemToArray(messages, msg);
        l = l->next;
    }

    cJSON *new_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(new_msg, "role", "user");
    cJSON_AddStringToObject(new_msg, "content", new_prompt);
    cJSON_AddItemToArray(messages, new_msg);

    cJSON_AddItemToObject(root, "messages", messages);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* build_request_gemini(ChatSession *session, const char *new_prompt) {
    cJSON *root = cJSON_CreateObject();
    cJSON *contents = cJSON_CreateArray();

    GList *l = session->messages;
    while (l) {
        Message *m = (Message*)l->data;
        cJSON *content = cJSON_CreateObject();
        const char *role = strcmp(m->role, "assistant") == 0 ? "model" : "user";
        cJSON_AddStringToObject(content, "role", role);

        cJSON *parts = cJSON_CreateArray();
        cJSON *part = cJSON_CreateObject();
        cJSON_AddStringToObject(part, "text", m->content);
        cJSON_AddItemToArray(parts, part);
        cJSON_AddItemToObject(content, "parts", parts);

        cJSON_AddItemToArray(contents, content);
        l = l->next;
    }

    cJSON *new_content = cJSON_CreateObject();
    cJSON_AddStringToObject(new_content, "role", "user");
    cJSON *parts = cJSON_CreateArray();
    cJSON *part = cJSON_CreateObject();
    cJSON_AddStringToObject(part, "text", new_prompt);
    cJSON_AddItemToArray(parts, part);
    cJSON_AddItemToObject(new_content, "parts", parts);
    cJSON_AddItemToArray(contents, new_content);

    cJSON_AddItemToObject(root, "contents", contents);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* build_request_hf(ChatSession *session, const char *new_prompt) {
    GString *prompt = g_string_new("");

    GList *l = session->messages;
    while (l) {
        Message *m = (Message*)l->data;
        if (strcmp(m->role, "user") == 0) {
            g_string_append_printf(prompt, "User: %s\n", m->content);
        } else {
            g_string_append_printf(prompt, "Assistant: %s\n", m->content);
        }
        l = l->next;
    }

    g_string_append_printf(prompt, "User: %s\nAssistant:", new_prompt);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "inputs", prompt->str);

    cJSON *params = cJSON_CreateObject();
    cJSON_AddNumberToObject(params, "max_new_tokens", 1024);
    cJSON_AddNumberToObject(params, "temperature", 0.7);
    cJSON_AddNumberToObject(params, "return_full_text", 0);
    cJSON_AddItemToObject(root, "parameters", params);

    g_string_free(prompt, TRUE);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

char* parse_response_groq(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *choices = cJSON_GetObjectItem(root, "choices");
    if (!cJSON_IsArray(choices) || cJSON_GetArraySize(choices) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(choice, "message");
    cJSON *content = cJSON_GetObjectItem(message, "content");

    char *result = NULL;
    if (cJSON_IsString(content)) {
        result = strdup(content->valuestring);
    }

    cJSON_Delete(root);
    return result;
}

char* parse_response_gemini(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    cJSON *candidates = cJSON_GetObjectItem(root, "candidates");
    if (!cJSON_IsArray(candidates) || cJSON_GetArraySize(candidates) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *candidate = cJSON_GetArrayItem(candidates, 0);
    cJSON *content = cJSON_GetObjectItem(candidate, "content");
    cJSON *parts = cJSON_GetObjectItem(content, "parts");

    if (!cJSON_IsArray(parts) || cJSON_GetArraySize(parts) == 0) {
        cJSON_Delete(root);
        return NULL;
    }

    cJSON *part = cJSON_GetArrayItem(parts, 0);
    cJSON *text = cJSON_GetObjectItem(part, "text");

    char *result = NULL;
    if (cJSON_IsString(text)) {
        result = strdup(text->valuestring);
    }

    cJSON_Delete(root);
    return result;
}

char* parse_response_hf(const char *json) {
    cJSON *root = cJSON_Parse(json);
    if (!root) return NULL;

    if (cJSON_IsArray(root)) {
        cJSON *item = cJSON_GetArrayItem(root, 0);
        if (cJSON_IsObject(item)) {
            cJSON *text = cJSON_GetObjectItem(item, "generated_text");
            if (cJSON_IsString(text)) {
                char *result = strdup(text->valuestring);
                cJSON_Delete(root);
                return result;
            }
        }
    }

    cJSON_Delete(root);
    return NULL;
}

char* api_send_message(Provider p, const char *api_key, ChatSession *session, const char *new_prompt) {
    char *request_body = NULL;
    char *response = NULL;
    char url[2048];
    struct curl_slist *headers = NULL;
    long http_code = 0;

    switch (p) {
        case PROVIDER_GROQ: {
            request_body = build_request_groq(session, new_prompt);
            snprintf(url, sizeof(url), "https://api.groq.com/openai/v1/chat/completions");
            char auth[512];
            snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key ? api_key : "");
            headers = curl_slist_append(headers, auth);
            headers = curl_slist_append(headers, "Content-Type: application/json");
            response = http_post(url, request_body, headers, &http_code);
            break;
        }
        case PROVIDER_GEMINI: {
            request_body = build_request_gemini(session, new_prompt);
            snprintf(url, sizeof(url),
                "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=%s",
                api_key ? api_key : "");
            headers = curl_slist_append(headers, "Content-Type: application/json");
            response = http_post(url, request_body, headers, &http_code);
            break;
        }
        case PROVIDER_HUGGINGFACE: {
            request_body = build_request_hf(session, new_prompt);
            snprintf(url, sizeof(url),
                "https://api-inference.huggingface.co/models/mistralai/Mistral-7B-Instruct-v0.2");
            char auth[512];
            snprintf(auth, sizeof(auth), "Authorization: Bearer %s", api_key ? api_key : "");
            headers = curl_slist_append(headers, auth);
            headers = curl_slist_append(headers, "Content-Type: application/json");
            response = http_post(url, request_body, headers, &http_code);
            break;
        }
        default:
            break;
    }

    if (headers) curl_slist_free_all(headers);
    free(request_body);

    if (!response || http_code != 200) {
        free(response);
        return NULL;
    }

    char *result = NULL;
    switch (p) {
        case PROVIDER_GROQ: result = parse_response_groq(response); break;
        case PROVIDER_GEMINI: result = parse_response_gemini(response); break;
        case PROVIDER_HUGGINGFACE: result = parse_response_hf(response); break;
        default: break;
    }

    free(response);
    return result;
}
