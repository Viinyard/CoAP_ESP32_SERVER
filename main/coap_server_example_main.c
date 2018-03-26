/* CoAP server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <sys/socket.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "nvs_flash.h"

#include "coap.h"

#include "lwip/err.h"
#include "lwip/sys.h"

/* The examples use simple WiFi configuration that you can set via
   'make menuconfig'.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_ESP_WIFI_MODE_AP   CONFIG_ESP_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN       CONFIG_MAX_STA_CONN

#define COAP_DEFAULT_TIME_SEC 5
#define COAP_DEFAULT_TIME_USEC 0

#ifdef __GNUC__
#define UNUSED_PARAM __attribute__ ((unused))
#else /* not a GCC */
#define UNUSED_PARAM
#endif /* GCC */

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const static int WIFI_CONNECTED_BIT = BIT0;

const static char *TAG = "CoAP_server";

#define INDEX "Ceci est un test du protocole CoAP avec libcoap (voir https://libcoap.net)\n\n"

static coap_async_state_t *async = NULL;

static time_t my_clock_base = 0;
static time_t clock_offset;
struct coap_resource_t *time_resource = NULL;




static void
hnd_put_time(coap_context_t *ctx UNUSED_PARAM,
             struct coap_resource_t *resource UNUSED_PARAM,
             const coap_endpoint_t *local_interface UNUSED_PARAM,
             coap_address_t *peer UNUSED_PARAM,
             coap_pdu_t *request,
             str *token UNUSED_PARAM,
             coap_pdu_t *response) {
  coap_tick_t t;
  size_t size;
  unsigned char *data;

  /* FIXME: re-set my_clock_base to clock_offset if my_clock_base == 0
   * and request is empty. When not empty, set to value in request payload
   * (insist on query ?ticks). Return Created or Ok.
   */

  /* if my_clock_base was deleted, we pretend to have no such resource */
  response->hdr->code =
    my_clock_base ? COAP_RESPONSE_CODE(204) : COAP_RESPONSE_CODE(201);

  resource->dirty = 1;

  /* coap_get_data() sets size to 0 on error */
  (void)coap_get_data(request, &size, &data);

  if (size == 0)        /* re-init */
    my_clock_base = clock_offset;
  else {
    my_clock_base = 0;
    coap_ticks(&t);
    while(size--)
      my_clock_base = my_clock_base * 10 + *data++;
    my_clock_base -= t / COAP_TICKS_PER_SECOND;
  }
}

static void
hnd_delete_time(coap_context_t *ctx UNUSED_PARAM,
                struct coap_resource_t *resource UNUSED_PARAM,
                const coap_endpoint_t *local_interface UNUSED_PARAM,
                coap_address_t *peer UNUSED_PARAM,
                coap_pdu_t *request UNUSED_PARAM,
                str *token UNUSED_PARAM,
                coap_pdu_t *response UNUSED_PARAM) {
  my_clock_base = 0;    /* mark clock as "deleted" */

   response->hdr->code = COAP_RESPONSE_CODE(203);
}

static void
hnd_get_index(coap_context_t *ctx UNUSED_PARAM,
              struct coap_resource_t *resource UNUSED_PARAM,
              const coap_endpoint_t *local_interface UNUSED_PARAM,
              coap_address_t *peer UNUSED_PARAM,
              coap_pdu_t *request UNUSED_PARAM,
              str *token UNUSED_PARAM,
              coap_pdu_t *response) {
  unsigned char buf[3];

  response->hdr->code = COAP_RESPONSE_CODE(205);

  coap_add_option(response,
                  COAP_OPTION_CONTENT_TYPE,
                  coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);

  coap_add_option(response,
                  COAP_OPTION_MAXAGE,
                  coap_encode_var_bytes(buf, 0x2ffff), buf);

  coap_add_data(response, strlen(INDEX), (unsigned char *)INDEX);
}

static void
hnd_get_time(coap_context_t  *ctx,
             struct coap_resource_t *resource,
             const coap_endpoint_t *local_interface UNUSED_PARAM,
             coap_address_t *peer,
             coap_pdu_t *request,
             str *token,
             coap_pdu_t *response) {
  coap_opt_iterator_t opt_iter;
  coap_opt_t *option;
  unsigned char buf[40];
  size_t len;
  time_t now;
  coap_tick_t t;

  /* FIXME: return time, e.g. in human-readable by default and ticks
   * when query ?ticks is given. */

  /* if my_clock_base was deleted, we pretend to have no such resource */
  response->hdr->code =
    my_clock_base ? COAP_RESPONSE_CODE(205) : COAP_RESPONSE_CODE(404);

  if (coap_find_observer(resource, peer, token)) {
    /* FIXME: need to check for resource->dirty? */
    coap_add_option(response,
                    COAP_OPTION_OBSERVE,
                    coap_encode_var_bytes(buf, ctx->observe), buf);
  }

  if (my_clock_base)
    coap_add_option(response,
                    COAP_OPTION_CONTENT_FORMAT,
                    coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);

  coap_add_option(response,
                  COAP_OPTION_MAXAGE,
                  coap_encode_var_bytes(buf, 0x01), buf);

  if (my_clock_base) {

    /* calculate current time */
    coap_ticks(&t);
    now = my_clock_base + (t / COAP_TICKS_PER_SECOND);

    if (request != NULL
        && (option = coap_check_option(request, COAP_OPTION_URI_QUERY, &opt_iter))
        && memcmp(COAP_OPT_VALUE(option), "ticks",
        min(5, COAP_OPT_LENGTH(option))) == 0) {
          /* output ticks */
          len = snprintf((char *)buf,
                         min(sizeof(buf),
                             response->max_size - response->length),
                             "%u", (unsigned int)now);
          coap_add_data(response, len, buf);

    } else {      /* output human-readable time */
      struct tm *tmp;
      tmp = gmtime(&now);
      len = strftime((char *)buf,
                     min(sizeof(buf),
                     response->max_size - response->length),
                     "%b %d %H:%M:%S", tmp);
      coap_add_data(response, len, buf);
    }
  }
}


static void
init_resources(coap_context_t *ctx) {
  coap_resource_t *r;

  r = coap_resource_init(NULL, 0, 0);
  coap_register_handler(r, COAP_REQUEST_GET, hnd_get_index);

  coap_add_attr(r, (unsigned char *)"ct", 2, (unsigned char *)"0", 1, 0);
  coap_add_attr(r, (unsigned char *)"title", 5, (unsigned char *)"\"General Info\"", 14, 0);
  coap_add_resource(ctx, r);

  /* store clock base to use in /time */
  my_clock_base = clock_offset;

  r = coap_resource_init((unsigned char *)"time", 4, COAP_RESOURCE_FLAGS_NOTIFY_CON);
  coap_register_handler(r, COAP_REQUEST_GET, hnd_get_time);
  coap_register_handler(r, COAP_REQUEST_PUT, hnd_put_time);
  coap_register_handler(r, COAP_REQUEST_DELETE, hnd_delete_time);

  coap_add_attr(r, (unsigned char *)"ct", 2, (unsigned char *)"0", 1, 0);
  coap_add_attr(r, (unsigned char *)"title", 5, (unsigned char *)"\"Internal Clock\"", 16, 0);
  coap_add_attr(r, (unsigned char *)"rt", 2, (unsigned char *)"\"Ticks\"", 7, 0);
  r->observable = 1;
  coap_add_attr(r, (unsigned char *)"if", 2, (unsigned char *)"\"clock\"", 7, 0);

  coap_add_resource(ctx, r);
  time_resource = r;


}




static void send_async_response(coap_context_t *ctx, const coap_endpoint_t *local_if)
{
    coap_pdu_t *response;
    unsigned char buf[3];
    const char* response_data     = "Hello World!";
    size_t size = sizeof(coap_hdr_t) + 20;
    response = coap_pdu_init(async->flags & COAP_MESSAGE_CON, COAP_RESPONSE_CODE(205), 0, size);
    response->hdr->id = coap_new_message_id(ctx);
    if (async->tokenlen)
        coap_add_token(response, async->tokenlen, async->token);
    coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);
    coap_add_data  (response, strlen(response_data), (unsigned char *)response_data);

    if (coap_send(ctx, local_if, &async->peer, response) == COAP_INVALID_TID) {

    }
    coap_delete_pdu(response);
    coap_async_state_t *tmp;
    coap_remove_async(ctx, async->id, &tmp);
    coap_free_async(async);
    async = NULL;
}

/*
 * The resource handler
 */
static void
async_handler(coap_context_t *ctx, struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface, coap_address_t *peer,
              coap_pdu_t *request, str *token, coap_pdu_t *response)
{
    async = coap_register_async(ctx, peer, request, COAP_ASYNC_SEPARATE | COAP_ASYNC_CONFIRM, (void*)"no data");
}

static void coap_example_thread(void *p)
{
  ESP_LOGI(TAG, "coap_example_thread begin");
    coap_context_t*  ctx = NULL;
    coap_address_t   serv_addr;
    coap_resource_t* resource = NULL;
    fd_set           readfds;
    struct timeval tv;
    int flags = 0;

ESP_LOGI(TAG, "coap_example_thread ok");
    while (1) {
        /* Wait for the callback to set the WIFI_CONNECTED_BIT in the
           event group.
        */
        
        ESP_LOGI(TAG, "Connected to AP");

        /* Prepare the CoAP server socket */
        coap_address_init(&serv_addr);
        serv_addr.addr.sin.sin_family      = AF_INET;
        serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
        serv_addr.addr.sin.sin_port        = htons(COAP_DEFAULT_PORT);
        ctx                                = coap_new_context(&serv_addr);
        if (ctx) {
            flags = fcntl(ctx->sockfd, F_GETFL, 0);
            fcntl(ctx->sockfd, F_SETFL, flags|O_NONBLOCK);

            tv.tv_usec = COAP_DEFAULT_TIME_USEC;
            tv.tv_sec = COAP_DEFAULT_TIME_SEC;
            /* Initialize the resource */

            init_resources(ctx);

            resource = coap_resource_init((unsigned char *)"Espressif", 9, 0);

            if (resource){
                coap_register_handler(resource, COAP_REQUEST_GET, async_handler);
                coap_add_resource(ctx, resource);
                /*For incoming connections*/
                for (;;) {
                    FD_ZERO(&readfds);
                    FD_CLR( ctx->sockfd, &readfds);
                    FD_SET( ctx->sockfd, &readfds);

                    int result = select( ctx->sockfd+1, &readfds, 0, 0, &tv );
                    if (result > 0){
                        if (FD_ISSET( ctx->sockfd, &readfds ))
                            coap_read(ctx);
                    } else if (result < 0){
                        break;
                    } else {
                        ESP_LOGE(TAG, "select timeout");
                    }

                    if (async) {
                        send_async_response(ctx, ctx->endpoint);
                    }
                }
            }

            coap_free_context(ctx);
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s",
                 ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                 MAC2STR(event->event_info.sta_connected.mac),
                 event->event_info.sta_connected.aid);
        break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                 MAC2STR(event->event_info.sta_disconnected.mac),
                 event->event_info.sta_disconnected.aid);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_softap()
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished.SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void wifi_init_sta()
{
    wifi_event_group = xEventGroupCreate();

    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void app_main(void)
{
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

#if EXAMPLE_ESP_WIFI_MODE_AP
  ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
  wifi_init_softap();
#else
  ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
  wifi_init_sta();
#endif /*EXAMPLE_ESP_WIFI_MODE_AP*/



ESP_LOGI(TAG, "ESP task create");
    xTaskCreate(coap_example_thread, "coap", 2048, NULL, 5, NULL);
}
