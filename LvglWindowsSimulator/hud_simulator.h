/**
 * @file hud_simulator.h
 * @brief Simulator-only controls for driving the portable HUD with fake data.
 *
 * This header and its implementation belong only to the Windows simulator.
 * The portable HUD interface remains in hud.h and hud_minimal.c.
 */

#ifndef HUD_SIMULATOR_H
#define HUD_SIMULATOR_H

#include "hud.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the simulator controls inside a sized parent object.
 * @param parent Container reserved for the lower simulator panel.
 * @param initial Initial data copied into the simulator state.
 */
void hud_simulator_create(lv_obj_t * parent, const hud_data_t * initial);

#ifdef __cplusplus
}
#endif

#endif /* HUD_SIMULATOR_H */
