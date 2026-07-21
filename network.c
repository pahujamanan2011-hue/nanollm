/* network.c - libcurl HTTP handling */
#include "nanollm.h"
#include <stdlib.h>
#include <string.h>

size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    MemoryStruct *mem = (MemoryStruct*)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = '\0';
    return realsize;
}

char* http_post(const char *url, const char *post_data, struct curl_slist *headers, long *http_code) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    MemoryStruct chunk = {0};
    chunk.memory = malloc(1);
    chunk.memory[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(chunk.memory);
        return NULL;
    }
    return chunk.memory;
}

char* http_get(const char *url, struct curl_slist *headers, long *http_code) {
    CURL *curl = curl_easy_init();
    if (!curl) return NULL;

    MemoryStruct chunk = {0};
    chunk.memory = malloc(1);
    chunk.memory[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(chunk.memory);
        return NULL;
    }
    return chunk.memory;
}
