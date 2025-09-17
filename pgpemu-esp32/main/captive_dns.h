#pragma once
#include "esp_err.h"

/* Start a wildcard DNS server that redirects every domain to ip_str
 * (must be dotted-decimal, e.g. "192.168.4.1").
 */
esp_err_t captive_dns_start(const char* ip_str);

void captive_dns_stop(void);
