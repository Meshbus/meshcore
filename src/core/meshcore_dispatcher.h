// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#ifndef MESHCORE_CORE_DISPATCHER_H_
#define MESHCORE_CORE_DISPATCHER_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meshcore_packet.h"
#include "meshcore_packet_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Internal RX/TX dispatcher for meshcore-C runtime.
 *
 * This keeps the c_impl/source structure so future behavior migration can stay
 * close to the reference modules, while the concrete radio adapter remains
 * outside this header set.
 */

struct meshcore_dispatcher;
struct meshcore_mesh;

typedef uint32_t meshcore_dispatcher_action;

#define MESHCORE_ACTION_RELEASE 0U
#define MESHCORE_ACTION_MANUAL_HOLD 1U
#define MESHCORE_ACTION_RETRANSMIT(pri) ((((uint32_t)1U + (pri))) << 24)
#define MESHCORE_ACTION_RETRANSMIT_DELAYED(pri, delay) \
  (((((uint32_t)1U + (pri))) << 24) | (delay))

#define MESHCORE_ERR_EVENT_FULL (1U << 0)
#define MESHCORE_ERR_EVENT_CAD_TIMEOUT (1U << 1)
#define MESHCORE_ERR_EVENT_STARTRX_TIMEOUT (1U << 2)

struct meshcore_dispatcher {
  struct meshcore_packet *outbound;
  unsigned long outbound_expiry;
  unsigned long outbound_start;
  unsigned long total_air_time;
  unsigned long rx_air_time;
  unsigned long next_tx_time;
  unsigned long cad_busy_start;
  unsigned long radio_nonrx_start;
  unsigned long next_floor_calib_time;
  unsigned long next_agc_reset_time;
  bool prev_isrecv_mode;
  uint32_t n_sent_flood;
  uint32_t n_sent_direct;
  uint32_t n_recv_flood;
  uint32_t n_recv_direct;
  unsigned long tx_budget_ms;
  unsigned long last_budget_update;
  unsigned long duty_cycle_window_ms;
  uint16_t err_flags;

  struct meshcore_packet_queue_manager *packet_manager;
  struct meshcore_mesh *owner_mesh;
};

void meshcore_dispatcher_init(
    struct meshcore_dispatcher *dispatcher,
    struct meshcore_packet_queue_manager *packet_manager,
    struct meshcore_mesh *owner_mesh);
void meshcore_dispatcher_begin(struct meshcore_dispatcher *dispatcher);
void meshcore_dispatcher_loop(struct meshcore_dispatcher *dispatcher);
int meshcore_dispatcher_receive_raw(struct meshcore_dispatcher *dispatcher,
                                    const uint8_t *raw, size_t len,
                                    int16_t rssi_dbm, int8_t snr_q4);
bool meshcore_dispatcher_tx_done(struct meshcore_dispatcher *dispatcher,
                                 bool success);
struct meshcore_packet *meshcore_dispatcher_obtain_new_packet(
    struct meshcore_dispatcher *dispatcher);
void meshcore_dispatcher_release_packet(struct meshcore_dispatcher *dispatcher,
                                        struct meshcore_packet *packet);
void meshcore_dispatcher_send_packet(struct meshcore_dispatcher *dispatcher,
                                     struct meshcore_packet *packet,
                                     uint8_t priority,
                                     uint32_t delay_millis);
bool meshcore_dispatcher_next_deadline_get(
    const struct meshcore_dispatcher *dispatcher, uint32_t now,
    uint32_t *deadline_ms);
unsigned long meshcore_dispatcher_get_total_air_time(const struct meshcore_dispatcher *dispatcher);
unsigned long meshcore_dispatcher_get_receive_air_time(const struct meshcore_dispatcher *dispatcher);
unsigned long meshcore_dispatcher_get_remaining_tx_budget(const struct meshcore_dispatcher *dispatcher);
uint32_t meshcore_dispatcher_get_num_sent_flood(const struct meshcore_dispatcher *dispatcher);
uint32_t meshcore_dispatcher_get_num_sent_direct( const struct meshcore_dispatcher *dispatcher);
uint32_t meshcore_dispatcher_get_num_recv_flood(const struct meshcore_dispatcher *dispatcher);
uint32_t meshcore_dispatcher_get_num_recv_direct(const struct meshcore_dispatcher *dispatcher);
uint16_t meshcore_dispatcher_get_err_flags(const struct meshcore_dispatcher *dispatcher);
void meshcore_dispatcher_reset_stats(struct meshcore_dispatcher *dispatcher);
bool meshcore_dispatcher_millis_has_now_passed(
    const struct meshcore_dispatcher *dispatcher, unsigned long timestamp);
unsigned long meshcore_dispatcher_future_millis(
    const struct meshcore_dispatcher *dispatcher, int millis_from_now);

#ifdef __cplusplus
}
#endif

#endif /* MESHCORE_CORE_DISPATCHER_H_ */
