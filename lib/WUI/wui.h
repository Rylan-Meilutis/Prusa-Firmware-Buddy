/*
 * wui.h
 * \brief main interface functions for Web User Interface (WUI) thread
 *
 *  Created on: Dec 12, 2019
 *      Author: joshy
 */

#ifndef SRC_WUI_WUI_H_
#define SRC_WUI_WUI_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Entry point for the networking task with full functionality (marlin client, link server).
void network_run();

// Entry point for minimal networking (error screen only, no marlin client or link server).
void network_run_minimal();

// TODO: Less colliding names? Or C++ namespace and such?
void notify_ethernet_data();
void notify_esp_data();
void notify_reconfigure();

#ifdef __cplusplus
}
#endif // __cplusplus

#endif /* SRC_WUI_WUI_H_ */
