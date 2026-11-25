#ifndef TECLA_GATEWAY_COMMAND_QUEUE_H
#define TECLA_GATEWAY_COMMAND_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY 8U
#define TECLA_GATEWAY_COMMAND_DEADLETTER_CAPACITY 8U
#define TECLA_GATEWAY_COMMAND_ID_BYTES 16U
#define TECLA_GATEWAY_COMMAND_PAYLOAD_MAX 192U
#define TECLA_GATEWAY_COMMAND_LEASE_MAX_MS 30000U
#define TECLA_GATEWAY_COMMAND_RETRY_BASE_MS 1000U
#define TECLA_GATEWAY_COMMAND_RETRY_MAX_MS 60000U
#define TECLA_GATEWAY_COMMAND_GAP_MS 1000U

typedef enum {
    TECLA_GATEWAY_CMD_RECEIVED_GATEWAY = 1,
    TECLA_GATEWAY_CMD_WAITING_NODE_ACK = 2,
    TECLA_GATEWAY_CMD_RECEIVED_NODE = 3,
    TECLA_GATEWAY_CMD_APPLIED = 4,
    TECLA_GATEWAY_CMD_REJECTED = 5,
    TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN = 6,
    TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED = 7
} tecla_gateway_command_state_t;

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t queue_sequence;
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint32_t config_revision_target;
    uint32_t config_revision_applied;
    uint8_t last_delivery_lease_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint64_t expires_at_epoch_s;
    uint64_t expired_at_epoch_s;
    uint16_t payload_length;
    uint8_t command_type;
    uint8_t state;
    uint8_t result_code;
    uint32_t attempts;
    uint8_t node_boot_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t node_sequence;
    uint8_t payload[TECLA_GATEWAY_COMMAND_PAYLOAD_MAX];
} tecla_gateway_command_t;

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint8_t node_boot_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t node_sequence;
    tecla_gateway_command_state_t state;
    uint8_t result_code;
    uint32_t config_revision_applied;
} tecla_gateway_command_ack_t;

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint32_t config_revision_target;
    uint8_t delivery_lease_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint64_t deadline_monotonic_ms;
    uint32_t delivery_attempts;
    uint8_t radio_send_budget;
    bool delivery_authorized;
} tecla_gateway_dispatch_authorization_t;

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint32_t config_revision_target;
    uint64_t expired_at_epoch_s;
    bool server_reports_unknown;
} tecla_gateway_server_unknown_t;

typedef enum {
    TECLA_GATEWAY_RESULT_STATUS_UNKNOWN = 1U,
    TECLA_GATEWAY_ACK_STAGE_UNKNOWN_CONFIRMED = 1U
} tecla_gateway_server_ack_value_t;

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint32_t config_revision_target;
    uint8_t result_status;
    uint8_t ack_stage;
    bool gateway_queue_terminal;
    bool remove_reported;
    uint64_t expired_at_epoch_s;
} tecla_gateway_unknown_server_ack_t;

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint32_t config_revision_target;
    bool result_unknown_final;
    bool remove_reported;
} tecla_gateway_unknown_final_server_ack_t;

/* Call after nvs_flash_init(). Unsupported snapshots fail closed and are preserved. */
esp_err_t tecla_gateway_command_queue_init(uint8_t *pending_count,
                                           uint8_t *recovered_inflight);

/* Identical duplicate command_id is idempotent; conflicts/full queue reject. */
esp_err_t tecla_gateway_command_queue_enqueue(
    const tecla_gateway_command_t *command, uint32_t *queue_sequence_out);

/* Active FIFO only. Deadletters are separately inspectable. */
esp_err_t tecla_gateway_command_queue_peek(tecla_gateway_command_t *command_out);
esp_err_t tecla_gateway_command_queue_mark_dispatch_authorized(
    uint32_t queue_sequence,
    const tecla_gateway_dispatch_authorization_t *authorization,
    uint64_t now_monotonic_ms);
/* Timeout does not authorize resend; every retry needs a new server status/lease/dispatch. */
esp_err_t tecla_gateway_command_queue_mark_retry(uint32_t queue_sequence);
esp_err_t tecla_gateway_command_queue_mark_result(
    uint32_t queue_sequence, const tecla_gateway_command_ack_t *ack);
esp_err_t tecla_gateway_command_queue_mark_server_unknown(
    uint32_t queue_sequence, const tecla_gateway_server_unknown_t *status);
/* Move UNKNOWN out of the active FIFO only after durable UNKNOWN_CONFIRMED server ACK. */
esp_err_t tecla_gateway_command_queue_release_unknown(
    uint32_t queue_sequence, const tecla_gateway_unknown_server_ack_t *ack);
/* Late result may target any retained deadletter, not just the active head. */
esp_err_t tecla_gateway_command_queue_mark_late_result(
    const tecla_gateway_command_ack_t *ack);
/* Remove a deadletter only after the API's late-ACK window is durably closed. */
esp_err_t tecla_gateway_command_queue_finalize_unknown_deadletter(
    const tecla_gateway_unknown_final_server_ack_t *ack);
esp_err_t tecla_gateway_command_queue_peek_deadletter(
    uint8_t offset, tecla_gateway_command_t *command_out);
/* Remove APPLIED/REJECTED only after the backend durably confirms that result. */
esp_err_t tecla_gateway_command_queue_remove_reported(uint32_t queue_sequence);
esp_err_t tecla_gateway_command_queue_get_count(uint8_t *count_out);
esp_err_t tecla_gateway_command_queue_get_deadletter_count(uint8_t *count_out);
uint32_t tecla_gateway_command_queue_retry_delay_ms(uint32_t attempts);

#endif
