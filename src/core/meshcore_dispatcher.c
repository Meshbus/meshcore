// SPDX-License-Identifier: MIT
/*
 * Copyright (c) 2026 FoBE Studio
 */

#include "meshcore_dispatcher.h"

#include <errno.h>
#include <math.h>
#include <string.h>

#include "meshcore_clock.h"
#include "meshcore_mesh.h"
#include "meshcore_platform_bridge.h"
#include "meshcore_radio.h"

#define MESHCORE_DISPATCHER_MAX_RX_DELAY_MILLIS 32000U
#define MESHCORE_DISPATCHER_MIN_TX_BUDGET_RESERVE_MS 100U
#define MESHCORE_DISPATCHER_MIN_TX_BUDGET_AIRTIME_DIV 2U
#define MESHCORE_DISPATCHER_RADIO_NONRX_TIMEOUT_MS 8000U

#ifndef MESHCORE_DISPATCHER_NOISE_FLOOR_CALIB_INTERVAL
#define MESHCORE_DISPATCHER_NOISE_FLOOR_CALIB_INTERVAL 2000U
#endif

static meshcore_dispatcher_action meshcore_dispatcher_call_on_recv_packet(
    struct meshcore_dispatcher *dispatcher, struct meshcore_packet *packet)
{
  if (dispatcher == NULL || dispatcher->owner_mesh == NULL) {
    return MESHCORE_ACTION_RELEASE;
  }

  return meshcore_mesh_on_recv_packet(dispatcher->owner_mesh, packet);
}

static void meshcore_dispatcher_bridge_log_rx_raw(struct meshcore_dispatcher *dispatcher,
                                           float snr, float rssi,
                                           const uint8_t raw[], int len)
{
  if (dispatcher != NULL) {
    meshcore_platform_bridge_dispatcher_log_rx_raw(snr, rssi, raw, len);
  }
}

static void meshcore_dispatcher_bridge_log_rx(struct meshcore_dispatcher *dispatcher,
                                       struct meshcore_packet *packet, int len,
                                       float score)
{
  if (dispatcher != NULL) {
    meshcore_platform_bridge_dispatcher_log_rx(packet, len, score);
  }
}

static void meshcore_dispatcher_bridge_log_tx(struct meshcore_dispatcher *dispatcher,
                                       struct meshcore_packet *packet, int len)
{
  if (dispatcher != NULL) {
    meshcore_platform_bridge_dispatcher_log_tx(packet, len);
  }
}

static void meshcore_dispatcher_bridge_log_tx_fail(
    struct meshcore_dispatcher *dispatcher, struct meshcore_packet *packet,
    int len)
{
  if (dispatcher != NULL) {
    meshcore_platform_bridge_dispatcher_log_tx_fail(packet, len);
  }
}

static float meshcore_dispatcher_bridge_get_airtime_budget_factor(
    const struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL) {
    return 1.0f;
  }

  return meshcore_platform_bridge_dispatcher_airtime_budget_factor_get();
}

static int meshcore_dispatcher_bridge_calc_rx_delay(
    const struct meshcore_dispatcher *dispatcher, float score,
    uint32_t air_time)
{
  if (dispatcher == NULL) {
    return (int)((powf(10.0f, 0.85f - score) - 1.0f) * (float)air_time);
  }

  return meshcore_platform_bridge_dispatcher_rx_delay_calc(score, air_time);
}

static uint32_t meshcore_dispatcher_get_cad_fail_retry_delay(
    const struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL || dispatcher->owner_mesh == NULL) {
    return 200U;
  }

  return meshcore_mesh_runtime_get_cad_fail_retry_delay(dispatcher->owner_mesh);
}

static bool meshcore_dispatcher_deadline_accumulate(uint32_t now,
                                                    uint32_t candidate,
                                                    bool *has_deadline,
                                                    uint32_t *deadline)
{
  if (has_deadline == NULL || deadline == NULL) {
    return false;
  }

  if ((int32_t)(candidate - now) <= 0) {
    *deadline = now;
    *has_deadline = true;
    return true;
  }

  if (!*has_deadline || (int32_t)(candidate - *deadline) < 0) {
    *deadline = candidate;
    *has_deadline = true;
  }

  return false;
}

static uint32_t meshcore_dispatcher_bridge_get_cad_fail_max_duration(
    const struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL) {
    return 4000U;
  }

  return meshcore_platform_bridge_dispatcher_cad_fail_max_duration_get();
}

static int meshcore_dispatcher_bridge_get_interference_threshold(
    const struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL) {
    return 0;
  }

  return meshcore_platform_bridge_dispatcher_interference_threshold_get();
}

static int meshcore_dispatcher_bridge_get_agc_reset_interval(
    const struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL) {
    return 0;
  }

  return meshcore_platform_bridge_dispatcher_agc_reset_interval_get();
}

static unsigned long meshcore_dispatcher_bridge_get_duty_cycle_window_ms(
    const struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL) {
    return 3600000UL;
  }

  return meshcore_platform_bridge_dispatcher_duty_cycle_window_ms_get();
}

static void meshcore_dispatcher_update_tx_budget(
    struct meshcore_dispatcher *dispatcher)
{
  unsigned long now;
  unsigned long elapsed;
  float duty_cycle;
  unsigned long max_budget;
  unsigned long refill;

  if (dispatcher == NULL) {
    return;
  }

  now = meshcore_clock_millis_get();
  elapsed = now - dispatcher->last_budget_update;

  duty_cycle = 1.0f / (1.0f + meshcore_dispatcher_bridge_get_airtime_budget_factor(
                                  dispatcher));
  max_budget = (unsigned long)(meshcore_dispatcher_bridge_get_duty_cycle_window_ms(
                                   dispatcher) *
                               duty_cycle);
  refill = (unsigned long)((float)elapsed * duty_cycle);

  if (refill > 0U) {
    dispatcher->tx_budget_ms += refill;
    if (dispatcher->tx_budget_ms > max_budget) {
      dispatcher->tx_budget_ms = max_budget;
    }
    dispatcher->last_budget_update = now;
  }
}

static bool meshcore_dispatcher_try_parse_packet(struct meshcore_packet *packet,
                                                 const uint8_t *raw, int len)
{
  int i = 0;
  uint8_t path_mode;
  uint8_t path_byte_len;

  if (packet == NULL || raw == NULL || len <= 0) {
    return false;
  }

  packet->header = raw[i++];
  if (meshcore_packet_get_payload_ver(packet) > PAYLOAD_VER_1) {
    return false;
  }

  if (meshcore_packet_has_transport_codes(packet)) {
    if (i + 4 > len) {
      return false;
    }
    memcpy(&packet->transport_codes[0], &raw[i], 2U);
    i += 2;
    memcpy(&packet->transport_codes[1], &raw[i], 2U);
    i += 2;
  } else {
    packet->transport_codes[0] = 0U;
    packet->transport_codes[1] = 0U;
  }

  if (i >= len) {
    return false;
  }

  packet->path_len = raw[i++];
  path_mode = (uint8_t)packet->path_len >> 6;
  if (path_mode == 3U) {
    return false;
  }

  path_byte_len = meshcore_packet_get_path_byte_len(packet);
  if (path_byte_len > MESHCORE_MAX_PATH_LEN || i + path_byte_len > len) {
    return false;
  }

  memcpy(packet->path, &raw[i], path_byte_len);
  i += path_byte_len;

  packet->payload_len = (uint16_t)(len - i);
  if (packet->payload_len > sizeof(packet->payload)) {
    return false;
  }

  memcpy(packet->payload, &raw[i], packet->payload_len);
  return true;
}

static void meshcore_dispatcher_process_recv_packet(
    struct meshcore_dispatcher *dispatcher, struct meshcore_packet *packet)
{
  meshcore_dispatcher_action action;
  uint8_t priority;
  uint32_t delay;

  action = meshcore_dispatcher_call_on_recv_packet(dispatcher, packet);
  if (action == MESHCORE_ACTION_RELEASE) {
    meshcore_packet_queue_manager_free(dispatcher->packet_manager, packet);
  } else if (action == MESHCORE_ACTION_MANUAL_HOLD) {
    return;
  } else {
    priority = (uint8_t)((action >> 24) - 1U);
    delay = action & 0x00FFFFFFU;
    meshcore_packet_queue_manager_queue_outbound(
        dispatcher->packet_manager, packet, priority,
        (uint32_t)meshcore_dispatcher_future_millis(dispatcher, (int)delay));
  }
}

int meshcore_dispatcher_receive_raw(struct meshcore_dispatcher *dispatcher,
                                    const uint8_t *raw, size_t len,
                                    int16_t rssi_dbm, int8_t snr_q4)
{
  struct meshcore_packet *packet;
  float snr = ((float)snr_q4) / 4.0f;
  float score;
  uint32_t air_time;
  int delay;

  if (dispatcher == NULL || raw == NULL || len == 0U ||
      len > MESHCORE_MAX_TRANS_UNIT_LEN) {
    return -EINVAL;
  }

  meshcore_dispatcher_bridge_log_rx_raw(dispatcher, snr,
                                        (float)rssi_dbm, raw, (int)len);

  packet = meshcore_packet_queue_manager_alloc_new(dispatcher->packet_manager);
  if (packet == NULL) {
    dispatcher->err_flags |= MESHCORE_ERR_EVENT_FULL;
    return -ENOBUFS;
  }

  if (!meshcore_dispatcher_try_parse_packet(packet, raw, (int)len)) {
    meshcore_packet_queue_manager_free(dispatcher->packet_manager, packet);
    return -EINVAL;
  }

  packet->snr_q4 = snr_q4;
  score = meshcore_radio_packet_score(snr, (int)len);
  air_time = meshcore_radio_get_est_airtime((int)len);
  dispatcher->rx_air_time += air_time;

  meshcore_dispatcher_bridge_log_rx(dispatcher, packet,
                             meshcore_packet_get_raw_length(packet), score);

  if (meshcore_packet_is_route_flood(packet)) {
    dispatcher->n_recv_flood++;

    delay = meshcore_dispatcher_bridge_calc_rx_delay(dispatcher, score, air_time);
    if (delay < 50) {
      meshcore_dispatcher_process_recv_packet(dispatcher, packet);
    } else {
      if ((uint32_t)delay > MESHCORE_DISPATCHER_MAX_RX_DELAY_MILLIS) {
        delay = (int)MESHCORE_DISPATCHER_MAX_RX_DELAY_MILLIS;
      }
      meshcore_packet_queue_manager_queue_inbound(
          dispatcher->packet_manager, packet,
          (uint32_t)meshcore_dispatcher_future_millis(dispatcher, delay));
    }
  } else {
    dispatcher->n_recv_direct++;
    meshcore_dispatcher_process_recv_packet(dispatcher, packet);
  }

  return 0;
}

bool meshcore_dispatcher_tx_done(struct meshcore_dispatcher *dispatcher,
                                 bool success)
{
  long air_time;
  float duty_cycle;
  unsigned long needed;
  int agc_reset_interval;

  if (dispatcher == NULL || dispatcher->outbound == NULL) {
    return false;
  }

  agc_reset_interval = meshcore_dispatcher_bridge_get_agc_reset_interval(dispatcher);
  if (success) {
    air_time = (long)(meshcore_clock_millis_get() - dispatcher->outbound_start);
    dispatcher->total_air_time += (unsigned long)air_time;

    meshcore_dispatcher_update_tx_budget(dispatcher);
    if ((unsigned long)air_time > dispatcher->tx_budget_ms) {
      dispatcher->tx_budget_ms = 0UL;
    } else {
      dispatcher->tx_budget_ms -= (unsigned long)air_time;
    }

    if (dispatcher->tx_budget_ms <
        MESHCORE_DISPATCHER_MIN_TX_BUDGET_RESERVE_MS) {
      duty_cycle = 1.0f / (1.0f + meshcore_dispatcher_bridge_get_airtime_budget_factor(
                                       dispatcher));
      needed = MESHCORE_DISPATCHER_MIN_TX_BUDGET_RESERVE_MS -
               dispatcher->tx_budget_ms;
      dispatcher->next_tx_time = meshcore_dispatcher_future_millis(
          dispatcher, (int)((float)needed / duty_cycle));
    } else {
      dispatcher->next_tx_time = meshcore_clock_millis_get();
    }

    meshcore_dispatcher_bridge_log_tx(
        dispatcher, dispatcher->outbound,
        meshcore_packet_get_raw_length(dispatcher->outbound));
    if (meshcore_packet_is_route_flood(dispatcher->outbound)) {
      dispatcher->n_sent_flood++;
    } else {
      dispatcher->n_sent_direct++;
    }
  } else {
    meshcore_dispatcher_bridge_log_tx_fail(
        dispatcher, dispatcher->outbound,
        meshcore_packet_get_raw_length(dispatcher->outbound));
  }

  meshcore_dispatcher_release_packet(dispatcher, dispatcher->outbound);
  dispatcher->outbound = NULL;

  if (agc_reset_interval > 0) {
    dispatcher->next_agc_reset_time =
        meshcore_dispatcher_future_millis(dispatcher, agc_reset_interval);
  }

  return true;
}

static void meshcore_dispatcher_check_send(struct meshcore_dispatcher *dispatcher)
{
  unsigned long now;
  uint32_t est_airtime;
  float duty_cycle;
  unsigned long needed;
  uint8_t raw[MESHCORE_MAX_TRANS_UNIT_LEN];
  uint8_t len;
  uint32_t max_airtime;

  now = meshcore_clock_millis_get();
  if (meshcore_packet_queue_manager_get_outbound_count(dispatcher->packet_manager,
                                                       (uint32_t)now) == 0) {
    return;
  }

  meshcore_dispatcher_update_tx_budget(dispatcher);

  est_airtime = meshcore_radio_get_est_airtime(MESHCORE_MAX_TRANS_UNIT_LEN);
  if (dispatcher->tx_budget_ms <
      est_airtime / MESHCORE_DISPATCHER_MIN_TX_BUDGET_AIRTIME_DIV) {
    duty_cycle = 1.0f / (1.0f + meshcore_dispatcher_bridge_get_airtime_budget_factor(
                                     dispatcher));
    needed = est_airtime / MESHCORE_DISPATCHER_MIN_TX_BUDGET_AIRTIME_DIV -
             dispatcher->tx_budget_ms;
    dispatcher->next_tx_time =
        meshcore_dispatcher_future_millis(dispatcher,
                                          (int)((float)needed / duty_cycle));
    return;
  }

  if (!meshcore_dispatcher_millis_has_now_passed(dispatcher,
                                                 dispatcher->next_tx_time)) {
    return;
  }

  if (meshcore_radio_is_receiving()) {
    if (dispatcher->cad_busy_start == 0UL) {
      dispatcher->cad_busy_start = now;
    }

    if (now - dispatcher->cad_busy_start >
        meshcore_dispatcher_bridge_get_cad_fail_max_duration(dispatcher)) {
      dispatcher->err_flags |= MESHCORE_ERR_EVENT_CAD_TIMEOUT;
    } else {
      dispatcher->next_tx_time = meshcore_dispatcher_future_millis(
          dispatcher,
          (int)meshcore_dispatcher_get_cad_fail_retry_delay(dispatcher));
      return;
    }
  }
  dispatcher->cad_busy_start = 0UL;

  dispatcher->outbound = meshcore_packet_queue_manager_get_next_outbound(
      dispatcher->packet_manager, (uint32_t)now);
  if (dispatcher->outbound == NULL) {
    return;
  }

  len = meshcore_packet_write_to(dispatcher->outbound, raw);
  if (len == 0U) {
    meshcore_packet_queue_manager_free(dispatcher->packet_manager,
                                       dispatcher->outbound);
    dispatcher->outbound = NULL;
    return;
  }

  max_airtime = meshcore_radio_get_est_airtime(len) * 3U / 2U;
  dispatcher->outbound_start = now;
  if (!meshcore_radio_start_send_raw(raw, len)) {
    meshcore_dispatcher_bridge_log_tx_fail(
        dispatcher, dispatcher->outbound,
        meshcore_packet_get_raw_length(dispatcher->outbound));
    meshcore_dispatcher_release_packet(dispatcher, dispatcher->outbound);
    dispatcher->outbound = NULL;
    return;
  }

  dispatcher->outbound_expiry =
      meshcore_dispatcher_future_millis(dispatcher, (int)max_airtime);
}

void meshcore_dispatcher_init(
    struct meshcore_dispatcher *dispatcher,
    struct meshcore_packet_queue_manager *packet_manager,
    struct meshcore_mesh *owner_mesh)
{
  if (dispatcher == NULL) {
    return;
  }

  memset(dispatcher, 0, sizeof(*dispatcher));
  dispatcher->packet_manager = packet_manager;
  dispatcher->owner_mesh = owner_mesh;
  dispatcher->next_tx_time = meshcore_clock_millis_get();
  dispatcher->prev_isrecv_mode = true;
  dispatcher->duty_cycle_window_ms = 3600000UL;
}

void meshcore_dispatcher_begin(struct meshcore_dispatcher *dispatcher)
{
  unsigned long now;
  float duty_cycle;

  if (dispatcher == NULL) {
    return;
  }

  now = meshcore_clock_millis_get();
  dispatcher->n_sent_flood = 0U;
  dispatcher->n_sent_direct = 0U;
  dispatcher->n_recv_flood = 0U;
  dispatcher->n_recv_direct = 0U;
  dispatcher->err_flags = 0U;
  dispatcher->radio_nonrx_start = now;

  dispatcher->duty_cycle_window_ms =
      meshcore_dispatcher_bridge_get_duty_cycle_window_ms(dispatcher);
  duty_cycle = 1.0f / (1.0f + meshcore_dispatcher_bridge_get_airtime_budget_factor(
                                   dispatcher));
  dispatcher->tx_budget_ms =
      (unsigned long)((float)dispatcher->duty_cycle_window_ms * duty_cycle);
  dispatcher->last_budget_update = now;
  dispatcher->next_tx_time = now;

  meshcore_radio_begin();
  dispatcher->prev_isrecv_mode = meshcore_radio_is_in_recv_mode();
}

void meshcore_dispatcher_loop(struct meshcore_dispatcher *dispatcher)
{
  bool is_recv;
  int agc_reset_interval;
  unsigned long now;
  struct meshcore_packet *packet;

  if (dispatcher == NULL || dispatcher->packet_manager == NULL) {
    return;
  }

  if (meshcore_dispatcher_millis_has_now_passed(
          dispatcher, dispatcher->next_floor_calib_time)) {
    meshcore_radio_trigger_noise_floor_calibrate(
        meshcore_dispatcher_bridge_get_interference_threshold(dispatcher));
    dispatcher->next_floor_calib_time = meshcore_dispatcher_future_millis(
        dispatcher, (int)MESHCORE_DISPATCHER_NOISE_FLOOR_CALIB_INTERVAL);
  }

  agc_reset_interval = meshcore_dispatcher_bridge_get_agc_reset_interval(dispatcher);

  is_recv = meshcore_radio_is_in_recv_mode();
  if (is_recv != dispatcher->prev_isrecv_mode) {
    dispatcher->prev_isrecv_mode = is_recv;
    if (!is_recv) {
      dispatcher->radio_nonrx_start = meshcore_clock_millis_get();
    }
  }

  now = meshcore_clock_millis_get();
  if (!is_recv &&
      now - dispatcher->radio_nonrx_start > MESHCORE_DISPATCHER_RADIO_NONRX_TIMEOUT_MS) {
    dispatcher->err_flags |= MESHCORE_ERR_EVENT_STARTRX_TIMEOUT;
  }

  if (dispatcher->outbound != NULL) {
    if (meshcore_dispatcher_millis_has_now_passed(
            dispatcher, dispatcher->outbound_expiry)) {
      (void)meshcore_dispatcher_tx_done(dispatcher, false);
    } else {
      return;
    }
  }

  if (agc_reset_interval > 0 &&
      meshcore_dispatcher_millis_has_now_passed(dispatcher,
                                                dispatcher->next_agc_reset_time)) {
    meshcore_radio_reset_agc();
    dispatcher->next_agc_reset_time =
        meshcore_dispatcher_future_millis(dispatcher, agc_reset_interval);
  }

  packet = meshcore_packet_queue_manager_get_next_inbound(
      dispatcher->packet_manager, (uint32_t)meshcore_clock_millis_get());
  if (packet != NULL) {
    meshcore_dispatcher_process_recv_packet(dispatcher, packet);
  }

  meshcore_dispatcher_check_send(dispatcher);
}

struct meshcore_packet *meshcore_dispatcher_obtain_new_packet(
    struct meshcore_dispatcher *dispatcher)
{
  struct meshcore_packet *packet;

  if (dispatcher == NULL || dispatcher->packet_manager == NULL) {
    return NULL;
  }

  packet = meshcore_packet_queue_manager_alloc_new(dispatcher->packet_manager);
  if (packet == NULL) {
    dispatcher->err_flags |= MESHCORE_ERR_EVENT_FULL;
    return NULL;
  }

  packet->payload_len = 0U;
  packet->path_len = 0U;
  packet->snr_q4 = 0;
  return packet;
}

void meshcore_dispatcher_release_packet(struct meshcore_dispatcher *dispatcher,
                                        struct meshcore_packet *packet)
{
  if (dispatcher == NULL || dispatcher->packet_manager == NULL) {
    return;
  }

  meshcore_packet_queue_manager_free(dispatcher->packet_manager, packet);
}

void meshcore_dispatcher_send_packet(struct meshcore_dispatcher *dispatcher,
                                     struct meshcore_packet *packet,
                                     uint8_t priority,
                                     uint32_t delay_millis)
{
  if (dispatcher == NULL || dispatcher->packet_manager == NULL ||
      packet == NULL) {
    return;
  }

  if (!meshcore_packet_is_valid_path_len((uint8_t)packet->path_len) ||
      packet->payload_len > MESHCORE_PACKET_PAYLOAD_MAX_LEN) {
    meshcore_packet_queue_manager_free(dispatcher->packet_manager, packet);
    return;
  }

  meshcore_packet_queue_manager_queue_outbound(
      dispatcher->packet_manager, packet, priority,
      (uint32_t)meshcore_dispatcher_future_millis(dispatcher,
                                                  (int)delay_millis));
}

bool meshcore_dispatcher_next_deadline_get(
    const struct meshcore_dispatcher *dispatcher, uint32_t now,
    uint32_t *deadline_ms)
{
  bool has_deadline = false;
  uint32_t deadline = 0U;
  uint32_t scheduled_for = 0U;

  if (dispatcher == NULL || dispatcher->packet_manager == NULL ||
      deadline_ms == NULL) {
    return false;
  }

  if (dispatcher->outbound != NULL) {
    if (meshcore_dispatcher_deadline_accumulate(
            now, (uint32_t)dispatcher->outbound_expiry, &has_deadline,
            &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (meshcore_packet_queue_manager_next_outbound_scheduled_get(
          dispatcher->packet_manager, now, &scheduled_for)) {
    if (meshcore_dispatcher_deadline_accumulate(now, scheduled_for,
                                                &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (meshcore_packet_queue_manager_next_inbound_scheduled_get(
          dispatcher->packet_manager, now, &scheduled_for)) {
    if (meshcore_dispatcher_deadline_accumulate(now, scheduled_for,
                                                &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (meshcore_packet_queue_manager_get_outbound_total(
          dispatcher->packet_manager) > 0) {
    if (meshcore_dispatcher_deadline_accumulate(
            now, (uint32_t)dispatcher->next_tx_time, &has_deadline,
            &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (meshcore_dispatcher_deadline_accumulate(
          now, (uint32_t)dispatcher->next_floor_calib_time, &has_deadline,
          &deadline)) {
    *deadline_ms = deadline;
    return true;
  }

  if (dispatcher->next_agc_reset_time != 0UL) {
    if (meshcore_dispatcher_deadline_accumulate(
            now, (uint32_t)dispatcher->next_agc_reset_time, &has_deadline,
            &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (!meshcore_radio_is_in_recv_mode()) {
    uint32_t nonrx_deadline =
        (uint32_t)(dispatcher->radio_nonrx_start +
                   MESHCORE_DISPATCHER_RADIO_NONRX_TIMEOUT_MS);

    if (meshcore_dispatcher_deadline_accumulate(now, nonrx_deadline,
                                                &has_deadline, &deadline)) {
      *deadline_ms = deadline;
      return true;
    }
  }

  if (!has_deadline) {
    return false;
  }

  *deadline_ms = deadline;
  return true;
}

unsigned long meshcore_dispatcher_get_total_air_time(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0UL : dispatcher->total_air_time;
}

unsigned long meshcore_dispatcher_get_receive_air_time(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0UL : dispatcher->rx_air_time;
}

unsigned long meshcore_dispatcher_get_remaining_tx_budget(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0UL : dispatcher->tx_budget_ms;
}

uint32_t meshcore_dispatcher_get_num_sent_flood(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0U : dispatcher->n_sent_flood;
}

uint32_t meshcore_dispatcher_get_num_sent_direct(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0U : dispatcher->n_sent_direct;
}

uint32_t meshcore_dispatcher_get_num_recv_flood(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0U : dispatcher->n_recv_flood;
}

uint32_t meshcore_dispatcher_get_num_recv_direct(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0U : dispatcher->n_recv_direct;
}

uint16_t meshcore_dispatcher_get_err_flags(
    const struct meshcore_dispatcher *dispatcher)
{
  return dispatcher == NULL ? 0U : dispatcher->err_flags;
}

void meshcore_dispatcher_reset_stats(struct meshcore_dispatcher *dispatcher)
{
  if (dispatcher == NULL) {
    return;
  }

  dispatcher->n_sent_flood = 0U;
  dispatcher->n_sent_direct = 0U;
  dispatcher->n_recv_flood = 0U;
  dispatcher->n_recv_direct = 0U;
  dispatcher->err_flags = 0U;
}

bool meshcore_dispatcher_millis_has_now_passed(
    const struct meshcore_dispatcher *dispatcher, unsigned long timestamp)
{
  (void)dispatcher;
  return (long)(meshcore_clock_millis_get() - timestamp) > 0;
}

unsigned long meshcore_dispatcher_future_millis(
    const struct meshcore_dispatcher *dispatcher, int millis_from_now)
{
  (void)dispatcher;
  return meshcore_clock_millis_get() + (unsigned long)millis_from_now;
}
