
#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

bool setup_network();
void wifi_reset_settings();
bool network_start_mdns();
void network_stop_mdns();

#endif
