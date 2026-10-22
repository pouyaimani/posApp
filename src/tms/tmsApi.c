#include "tmsApi.h"
#include "utility/cJSON.h"
#include <stdio.h>
#include <string.h>

/* Reject duplicate object keys rather than accepting whichever cJSON finds. */
static bool unique_keys(const cJSON* object) {
    const cJSON *a, *b;
    for (a = object->child; a; a = a->next) {
        if (!a->string)
            return false;
        for (b = a->next; b; b = b->next)
            if (b->string && strcmp(a->string, b->string) == 0)
                return false;
    }
    return true;
}

static TmsApiResult text_field(const cJSON* object, const char* key, char* out,
                               size_t capacity, bool required) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    size_t       n;
    if (!item || cJSON_IsNull(item))
        return required ? TMS_API_SCHEMA_ERROR : TMS_API_OK;
    if (!cJSON_IsString(item) || !item->valuestring)
        return TMS_API_SCHEMA_ERROR;
    n = strlen(item->valuestring);
    if (n >= capacity) {
        LOG_TRACE("TMS field too long: key=%s length=%lu max=%lu", key,
                  (unsigned long)n, (unsigned long)(capacity - 1));
        return TMS_API_LIMIT;
    }
    if (required && n == 0)
        return TMS_API_SCHEMA_ERROR;
    memcpy(out, item->valuestring, n + 1);
    return TMS_API_OK;
}

static bool file_size(const cJSON* item, uint32_t* out) {
    uint32_t             n = 0;
    const unsigned char* s;
    if (cJSON_IsNumber(item)) {
        double v = item->valuedouble;
        if (!(v >= 1 && v <= UINT32_MAX))
            return false;
        n = (uint32_t)v;
        if ((double)n != v)
            return false;
    } else if (cJSON_IsString(item) && item->valuestring) {
        s = (const unsigned char*)item->valuestring;
        if (!*s)
            return false;
        for (; *s; ++s) {
            uint32_t d;
            if (*s < '0' || *s > '9')
                return false;
            d = (uint32_t)(*s - '0');
            if (n > (UINT32_MAX - d) / 10)
                return false;
            n = n * 10 + d;
        }
        if (!n)
            return false;
    } else
        return false;
    *out = n;
    return true;
}

/* cJSON represents decoded strings as C strings. Reject escaped NUL so that
 * a value such as a URL cannot be silently shortened during copying. */
static bool escaped_nul(const char* json) {
    size_t i;
    for (i = 0; json[i]; ++i) {
        if (json[i] == '\\' && json[i + 1]) {
            if (strncmp(json + i + 1, "u0000", 5) == 0)
                return true;
            ++i;
        }
    }
    return false;
}

static TmsApiResult parse_response(TmsApi* api) {
    cJSON*          root = NULL;
    const cJSON *   code, *data, *flag;
    const char*     end = NULL;
    TmsApiResult    rc  = TMS_API_SCHEMA_ERROR;
    TmsUpgradeInfo* u   = &api->result.upgrade;
    if (!api->received || escaped_nul(api->json))
        return TMS_API_JSON_ERROR;
    /* require_null_terminated rejects trailing garbage after the JSON value. */
    root = cJSON_ParseWithOpts(api->json, &end, 1);
    if (!root)
        return TMS_API_JSON_ERROR;
    if (!cJSON_IsObject(root) || !unique_keys(root))
        goto done;
    code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code) ||
        !(code->valuedouble >= INT32_MIN && code->valuedouble <= INT32_MAX))
        goto done;
    api->result.server_code = (int32_t)code->valuedouble;
    if ((double)api->result.server_code != code->valuedouble)
        goto done;
    api->result.has_server_code = true;
    rc = text_field(root, "message", u->message, sizeof(u->message), false);
    if (rc != TMS_API_OK)
        goto done;
    if (api->result.server_code != 0) {
        rc = TMS_API_SERVER_ERROR;
        goto done;
    }
    data = cJSON_GetObjectItemCaseSensitive(root, "data");
    rc   = TMS_API_SCHEMA_ERROR;
    if (!cJSON_IsObject(data) || !unique_keys(data))
        goto done;
    flag = cJSON_GetObjectItemCaseSensitive(data, "isHasNewVersion");
    if (cJSON_IsBool(flag))
        u->has_update = cJSON_IsTrue(flag) != 0;
    else if (cJSON_IsNumber(flag) &&
             (flag->valuedouble == 0 || flag->valuedouble == 1))
        u->has_update = flag->valuedouble == 1;
    else
        goto done;
    rc = text_field(data, "message", u->message, sizeof(u->message), false);
    if (rc != TMS_API_OK)
        goto done;
    rc = text_field(data, "ts", u->timestamp, sizeof(u->timestamp), false);
    if (rc != TMS_API_OK)
        goto done;
    rc = text_field(data, "planId", u->plan_id, sizeof(u->plan_id), false);
    if (rc != TMS_API_OK)
        goto done;
    /* A no-update response need not include URL/version/size/hash. */
    if (!u->has_update) {
        rc = TMS_API_OK;
        goto done;
    }
    rc = text_field(data, "version", u->version, sizeof(u->version), true);
    if (rc != TMS_API_OK)
        goto done;
    rc = text_field(data, "url", u->url, sizeof(u->url), true);
    if (rc != TMS_API_OK)
        goto done;
    rc = text_field(data, "hash", u->hash, sizeof(u->hash), false);
    if (rc != TMS_API_OK)
        goto done;
    if (!file_size(cJSON_GetObjectItemCaseSensitive(data, "fileSize"),
                   &u->file_size))
        rc = TMS_API_SCHEMA_ERROR;
done:
    cJSON_Delete(root);
    if (rc != TMS_API_OK)
        u->has_update = false;
    return rc;
}

static int receive_headers(void* user, const HttpResponse* r,
                           const struct phr_header* headers, size_t count) {
    TmsApi* api = user;
    size_t  i;
    if (r->status < 200)
        return 0;
    if (r->status != 200) {
        api->receive_error = TMS_API_HTTP_ERROR;
        return -1;
    }
    for (i = 0; i < count; ++i) {
        HttpView name  = {headers[i].name, headers[i].name_len};
        HttpView value = {headers[i].value, headers[i].value_len};
        if (http_view_equal_ci(name, "Content-Encoding") &&
            !http_view_equal_ci(value, "identity")) {
            api->receive_error = TMS_API_HTTP_ERROR;
            return -1;
        }
    }
    return 0;
}

static int receive_body(void* user, const uint8_t* bytes, size_t length) {
    TmsApi* api = user;
    if (length > sizeof(api->json) - 1 - api->received) {
        api->receive_error = TMS_API_LIMIT;
        return -1;
    }
    if (memchr(bytes, 0, length)) {
        api->receive_error = TMS_API_JSON_ERROR;
        return -1;
    }
    memcpy(api->json + api->received, bytes, length);
    api->received += length;
    api->json[api->received] = 0;
    return 0;
}

static void completed(HttpClient* client, const HttpClientStatus* status,
                      void* user) {
    TmsApi* api = user;
    (void)client;
    api->result.http = *status;
    if (api->receive_error != TMS_API_OK)
        api->result.result = api->receive_error;
    else if (status->result == HTTP_CLIENT_CANCELLED)
        api->result.result = TMS_API_CANCELLED;
    else if (status->result == HTTP_CLIENT_HTTP_ERROR)
        api->result.result = TMS_API_HTTP_ERROR;
    else if (status->result != HTTP_CLIENT_OK)
        api->result.result = TMS_API_TRANSPORT_ERROR;
    else if (status->http_status != 200)
        api->result.result = TMS_API_HTTP_ERROR;
    else
        api->result.result = parse_response(api);
    api->busy = false;
    if (api->on_done) {
        api->in_callback = true;
        api->on_done(api, &api->result, api->user);
        api->in_callback = false;
    }
}

TmsApiResult tms_api_check_start(TmsApi* api, const TmsCheckRequest* q) {
    cJSON*            root;
    HttpClientRequest request;
    HttpClientResult  result;
    char              authority[NTH_MAX_HOST_LEN + 8];
    int               n;
    HttpHeader        headers[] = {
        {HTTP_VIEW_LITERAL("Content-Type"),
         HTTP_VIEW_LITERAL("application/json")},
        {HTTP_VIEW_LITERAL("Accept"), HTTP_VIEW_LITERAL("application/json")},
        {HTTP_VIEW_LITERAL("Accept-Encoding"), HTTP_VIEW_LITERAL("identity")}};
    if (!api)
        return TMS_API_INVALID;
    if (api->busy || api->in_callback || api->client.busy ||
        api->client.in_callback)
        return TMS_API_BUSY;
    if (!q || !q->host || !*q->host || !q->port || !q->timeout_ms ||
        q->timeout_ms > INT32_MAX || !q->device_sn || !*q->device_sn ||
        !q->brand_code || !*q->brand_code || !q->current_version ||
        !*q->current_version || q->pack_resource_type < 0)
        return TMS_API_INVALID;
    n = snprintf(authority, sizeof(authority), "%s:%u", q->host,
                 (unsigned)q->port);
    if (n < 0 || (size_t)n >= sizeof(authority))
        return TMS_API_INVALID;
    memset(&api->result, 0, sizeof(api->result));
    api->received      = 0;
    api->receive_error = TMS_API_OK;
    api->on_done       = q->on_done;
    api->user          = q->user;
    root               = cJSON_CreateObject();
    if (!root)
        return api->result.result = TMS_API_JSON_ERROR;
    if (!cJSON_AddStringToObject(root, "deviceSn", q->device_sn) ||
        !cJSON_AddStringToObject(root, "brandCode", q->brand_code) ||
        !cJSON_AddStringToObject(root, "currentVersion", q->current_version) ||
        !cJSON_AddNumberToObject(root, "packResourceType",
                                 q->pack_resource_type)) {
        cJSON_Delete(root);
        return api->result.result = TMS_API_JSON_ERROR;
    }
    if (!cJSON_PrintPreallocated(root, api->json, (int)sizeof(api->json), 0)) {
        cJSON_Delete(root);
        return api->result.result = TMS_API_LIMIT;
    }
    cJSON_Delete(root);
    memset(&request, 0, sizeof(request));
    request.connect_host         = q->host;
    request.port                 = q->port;
    request.timeout_ms           = q->timeout_ms;
    request.request.method       = HTTP_POST;
    request.request.target.ptr   = "/api/device/upgrade/check";
    request.request.target.len   = sizeof("/api/device/upgrade/check") - 1;
    request.request.host.ptr     = authority;
    request.request.host.len     = (size_t)n;
    request.request.headers      = headers;
    request.request.header_count = 3;
    request.body                 = (const uint8_t*)api->json;
    request.body_length          = strlen(api->json);
    request.request.body_length  = request.body_length;
    request.parser_options       = http_parser_options();
    request.parser_options.max_body_bytes = sizeof(api->json) - 1;
    request.parser_options.max_wire_bytes = 32768;
    request.callbacks.on_headers          = receive_headers;
    request.callbacks.on_body             = receive_body;
    request.on_done                       = completed;
    request.user                          = api;
    api->busy                             = true;
    result = http_client_start(&api->client, &request, api->header_storage,
                               sizeof(api->header_storage));
    if (result != HTTP_CLIENT_OK) {
        api->busy                 = false;
        api->result.http          = api->client.status;
        api->result.http.result   = result;
        return api->result.result = result == HTTP_CLIENT_HTTP_ERROR
                                        ? TMS_API_HTTP_ERROR
                                        : TMS_API_TRANSPORT_ERROR;
    }
    /* http_client_start copied the outgoing body into NTH's txBuffer. */
    api->json[0] = 0;
    return TMS_API_OK;
}

TmsApiResult tms_api_cancel(TmsApi* api) {
    if (!api)
        return TMS_API_INVALID;
    if (api->in_callback || api->client.in_callback)
        return TMS_API_BUSY;
    return http_client_cancel(&api->client) == HTTP_CLIENT_OK ? TMS_API_OK
                                                              : TMS_API_BUSY;
}
