#include "ai.h"
#include "http.h"
#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cJSON.h"

static char* query_openai(const char *query) {
    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", OPENAI_API_KEY);

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
    snprintf(url, sizeof(url), "%s/chat/completions", OPENAI_API_URL);

    char *raw_response = perform_http_request(url, "POST", post_data, headers, 2);
    free(post_data);

    if (!raw_response) return strdup("ERROR+AI_REQUEST_FAILED");

    // Parse JSON response
    cJSON *resp_json = cJSON_Parse(raw_response);
    free(raw_response);
    
    if (!resp_json) return strdup("ERROR+AI_RESPONSE_PARSE_FAILED");

    cJSON *choices = cJSON_GetObjectItem(resp_json, "choices");
    cJSON *choice = cJSON_GetArrayItem(choices, 0);
    cJSON *message = cJSON_GetObjectItem(choice, "message");
    cJSON *content = cJSON_GetObjectItem(message, "content");

    char *final_response;
    if (cJSON_IsString(content) && content->valuestring != NULL) {
        final_response = strdup(content->valuestring);
    } else {
        final_response = strdup("ERROR+AI_RESPONSE_PARSE_FAILED");
    }

    cJSON_Delete(resp_json);
    return final_response;
}

char* handle_ai_command(char *payload, uint16_t len) {
    if (!payload || len == 0) {
        return strdup("ERROR_INVALID_PARAMS");
    }
    return query_openai(payload);
}