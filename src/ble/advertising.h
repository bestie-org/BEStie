#ifndef __ADVERTISING_H__
#define __ADVERTISING_H__

#include "app_error.h"
#include <stdbool.h>

void advertising_init(void);
ret_code_t advertising_start(void);

/**
 * Rudimentary replacement of fast advertising.
 *
 * If ebike is connected while LDI peer advertising set is active this immediately switches to next FTMS / CPMS advertising set
 * otherwise it prolongs duration of already active FTMS / CPMS advertising set
 *
 * If ebike is disconnected while LDI peer advertising set is inactive this immediately selects it
 * otherwise it prolongs duration of already active LDI peer advertising set
 */
void advertising_signal_ebike_connected_state(const bool ebike_connected);

#endif
