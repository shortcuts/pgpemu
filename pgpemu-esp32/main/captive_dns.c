/*
 * Tiny captive-portal DNS for ESP-IDF (IPv4 only)
 * Answers every query with a fixed A record.
 *
 * 2025 – Public domain / CC0
 */
#include "captive_dns.h"

#include "esp_err.h"
#include "esp_log.h"

#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/pbuf.h"
#include <string.h>

static const char *TAG = "captive_dns";

static struct udp_pcb *s_pcb;          // UDP handle
static ip_addr_t       s_target_ip;    // IP we point every host to

/* ---------- very small DNS helper ---------- */

static void dns_recv_cb(void *arg, struct udp_pcb *pcb,
                        struct pbuf *p, const ip_addr_t *addr, u16_t port)
{
    if (!p) return;

    /* copy the whole packet into RAM (max 512 B for DNS over UDP) */
    uint8_t buf[512];
    if (p->tot_len > sizeof(buf)) {      // ignore oversize queries
        pbuf_free(p);
        return;
    }
    pbuf_copy_partial(p, buf, p->tot_len, 0);
    uint16_t qdcount = (buf[4] << 8) | buf[5];
    if (qdcount == 0) {                  // malformed – drop
        pbuf_free(p);
        return;
    }

    /* build DNS response in-place --------------------------------- */
    buf[2] = 0x81;       // QR=1, Opcode=0, AA=1
    buf[3] = 0x80;       // RA=0, RCODE=0
    buf[6] = buf[4];     // ANCOUNT = QDCOUNT
    buf[7] = buf[5];
    buf[8] = buf[9] = buf[10] = buf[11] = 0;   // NSCOUNT, ARCOUNT = 0

    size_t idx = 12;           // skip header
    /* skip over all questions */
    for (uint16_t q = 0; q < qdcount; ++q) {
        while (idx < p->tot_len && buf[idx] != 0)      // labels
            idx += buf[idx] + 1;
        idx += 5;   // zero + type + class
    }
    /* append one answer per question */
    for (uint16_t q = 0; q < qdcount; ++q) {
        buf[idx++] = 0xC0; buf[idx++] = 0x0C;          // name pointer to 0x0c
        buf[idx++] = 0x00; buf[idx++] = 0x01;          // TYPE  = A
        buf[idx++] = 0x00; buf[idx++] = 0x01;          // CLASS = IN
        buf[idx++] = 0x00; buf[idx++] = 0x00; buf[idx++] = 0x00; buf[idx++] = 0x1E; // TTL 30 s
        buf[idx++] = 0x00; buf[idx++] = 0x04;          // RDLEN = 4
        uint32_t ip_be = lwip_htonl(                 /* RDATA must be big-endian */
            ip4_addr_get_u32(        /* get uint32_t */
                ip_2_ip4(&s_target_ip)));
                memcpy(&buf[idx], &ip_be, sizeof(ip_be));       // RDATA = target IPv4
        idx += 4;
    }

    struct pbuf *resp = pbuf_alloc(PBUF_TRANSPORT, idx, PBUF_RAM);
    if (resp) {
        pbuf_take(resp, buf, idx);
        udp_sendto(pcb, resp, addr, port);
        pbuf_free(resp);
    }
    pbuf_free(p);
}

/* ---------- public API ------------------------------------------ */

esp_err_t captive_dns_start(const char *ip_str)
{
    if (s_pcb) {
        ESP_LOGW(TAG, "already running");
        return ESP_OK;
    }
    if (!ipaddr_aton(ip_str, &s_target_ip)) {
        return ESP_ERR_INVALID_ARG;
    }

    s_pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (!s_pcb) return ESP_ERR_NO_MEM;

    err_t err = udp_bind(s_pcb, IP_ADDR_ANY, 53);
    if (err != ERR_OK) {
        ESP_LOGE(TAG, "udp_bind() failed: %d", err);
        udp_remove(s_pcb);
        s_pcb = NULL;
        return ESP_FAIL;
    }
    udp_recv(s_pcb, dns_recv_cb, NULL);

    ESP_LOGI(TAG, "Captive DNS started, hijacking all hosts to %s", ip_str);
    return ESP_OK;
}

void captive_dns_stop(void)
{
    if (s_pcb) {
        udp_remove(s_pcb);
        s_pcb = NULL;
        ESP_LOGI(TAG, "Captive DNS stopped");
    }
}
