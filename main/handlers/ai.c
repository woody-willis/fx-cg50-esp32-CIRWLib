#include "ai.h"
#include "http.h"
#include "config_handler.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cJSON.h"

static response_t query_openai(const char *query) {
    char *api_key = get_config_string(CONFIG_KEY_OPENAI_API_KEY);
    char *api_url = get_config_string(CONFIG_KEY_OPENAI_API_URL);
    if (!api_key || !api_url) {
        free(api_key);
        free(api_url);
        return response_status(RESP_ERROR_AI_REQUEST_FAILED);
    }

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);

    const char *headers[][2] = {
        {"Authorization", auth_header},
        {"Content-Type", "application/json"}
    };

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "model", "gemini-3-flash-preview");
    
    cJSON *messages = cJSON_AddArrayToObject(root, "messages");
    
    cJSON *sys_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(sys_msg, "role", "system");
    cJSON_AddStringToObject(sys_msg, "content", "You are a helpful assistant that answers questions concisely. If you don't know the answer, say you don't know. Never try to make up an answer. Do not use any markdown or latex formatting. Just provide a plain text answer.");
    cJSON_AddItemToArray(messages, sys_msg);

    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(user_msg, "role", "user");
    cJSON_AddStringToObject(user_msg, "content", query);
    cJSON_AddItemToArray(messages, user_msg);

    char *post_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char url[256];
    snprintf(url, sizeof(url), "%s/chat/completions", api_url);

    uint16_t raw_len = 0;
    uint8_t *raw_response = perform_http_request(url, "POST", post_data, headers, 2, &raw_len);
    free(post_data);
    free(api_key);
    free(api_url);

    if (!raw_response) return response_status(RESP_ERROR_AI_REQUEST_FAILED);

    // Parse JSON response (raw body is null-terminated by the HTTP buffer)
    cJSON *resp_json = cJSON_Parse((const char*)raw_response);
    free(raw_response);
    
    if (!resp_json) return response_status(RESP_ERROR_AI_PARSE_FAILED);

    cJSON *choices = cJSON_GetObjectItem(resp_json, "choices");
    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(choice, "message");
    cJSON *content = cJSON_GetObjectItem(message, "content");

    response_t r;
    if (cJSON_IsString(content) && content->valuestring != NULL) {
        size_t clen = strlen(content->valuestring);
        uint8_t *buf = malloc(clen);
        if (buf) memcpy(buf, content->valuestring, clen);
        r.data = buf;
        r.len = (uint16_t)clen;
    } else {
        r = response_status(RESP_ERROR_AI_PARSE_FAILED);
    }

    cJSON_Delete(resp_json);
    return r;
}

response_t handle_ai_command(uint8_t *payload, uint16_t len) {
    if (!payload || len == 0) {
        return response_status(RESP_ERROR_INVALID_PARAMS);
    }
    return query_openai((const char*)payload);
}