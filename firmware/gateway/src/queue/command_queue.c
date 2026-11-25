#include "command_queue.h"

#include <stddef.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define QUEUE_NVS_NAMESPACE "cmd_queue"
#define QUEUE_NVS_KEY "snapshot"
#define QUEUE_SNAPSHOT_MAGIC 0x54435131UL
#define QUEUE_SNAPSHOT_VERSION 4U
#define QUEUE_SNAPSHOT_VERSION_V3 3U

typedef struct {
    uint8_t command_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t queue_sequence;
    uint32_t firmware_node_id;
    uint32_t config_revision_expected;
    uint32_t config_revision_applied;
    uint64_t expires_at_epoch_s;
    uint16_t payload_length;
    uint8_t command_type;
    uint8_t state;
    uint8_t attempts;
    uint8_t result_code;
    uint8_t node_boot_id[TECLA_GATEWAY_COMMAND_ID_BYTES];
    uint32_t node_sequence;
    uint8_t payload[TECLA_GATEWAY_COMMAND_PAYLOAD_MAX];
} tecla_gateway_command_v3_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t record_size;
    uint32_t next_queue_sequence;
    uint8_t head;
    uint8_t count;
    uint8_t reserved[2];
    tecla_gateway_command_v3_t items[TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY];
    uint32_t checksum;
} queue_snapshot_v3_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t record_size;
    uint32_t next_queue_sequence;
    uint8_t head;
    uint8_t count;
    uint8_t deadletter_count;
    uint8_t reserved;
    tecla_gateway_command_t items[TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY];
    tecla_gateway_command_t deadletters[TECLA_GATEWAY_COMMAND_DEADLETTER_CAPACITY];
    uint32_t checksum;
} queue_snapshot_t;

static queue_snapshot_t snapshot;
static queue_snapshot_t work_snapshot;
static nvs_handle_t nvs_handle;
static SemaphoreHandle_t queue_mutex;
static bool queue_ready;

static uint32_t checksum32(const uint8_t *data, size_t length)
{
    uint32_t hash = 2166136261UL;
    for (size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619UL;
    }
    return hash;
}

static uint8_t slot_for(const queue_snapshot_t *state, uint8_t offset)
{
    return (uint8_t)((state->head + offset) % TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY);
}

static bool id16_nonzero(const uint8_t id[TECLA_GATEWAY_COMMAND_ID_BYTES])
{
    uint8_t bits = 0U;
    for (size_t i = 0; i < TECLA_GATEWAY_COMMAND_ID_BYTES; ++i) bits |= id[i];
    return bits != 0U;
}

static bool command_valid(const tecla_gateway_command_t *command)
{
    return command != NULL &&
        id16_nonzero(command->command_id) &&
        command->firmware_node_id != 0U &&
        command->config_revision_target != 0U &&
        command->config_revision_target > command->config_revision_expected &&
        command->expires_at_epoch_s != 0U &&
        command->command_type != 0U &&
        command->payload_length <= TECLA_GATEWAY_COMMAND_PAYLOAD_MAX;
}

static bool active_state_valid(const tecla_gateway_command_t *item)
{
    if (!command_valid(item) || item->queue_sequence == 0U ||
        item->state < TECLA_GATEWAY_CMD_RECEIVED_GATEWAY ||
        item->state > TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN) return false;
    if (item->state == TECLA_GATEWAY_CMD_APPLIED &&
        item->config_revision_applied != item->config_revision_target) return false;
    if (item->state == TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN &&
        item->expired_at_epoch_s == 0U) return false;
    if ((item->state == TECLA_GATEWAY_CMD_RECEIVED_NODE ||
         item->state == TECLA_GATEWAY_CMD_APPLIED ||
         item->state == TECLA_GATEWAY_CMD_REJECTED) &&
        (!id16_nonzero(item->node_boot_id) ||
         item->node_sequence == UINT32_MAX)) return false;
    if ((item->state == TECLA_GATEWAY_CMD_WAITING_NODE_ACK ||
         item->state == TECLA_GATEWAY_CMD_RECEIVED_NODE) &&
        (item->attempts == 0U || !id16_nonzero(item->last_delivery_lease_id))) return false;
    return true;
}

static bool deadletter_state_valid(const tecla_gateway_command_t *item)
{
    if (!command_valid(item) || item->queue_sequence == 0U ||
        (item->state != TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED &&
         item->state != TECLA_GATEWAY_CMD_APPLIED &&
         item->state != TECLA_GATEWAY_CMD_REJECTED)) return false;
    if ((item->state == TECLA_GATEWAY_CMD_APPLIED ||
         item->state == TECLA_GATEWAY_CMD_REJECTED) &&
        (!id16_nonzero(item->node_boot_id) ||
         item->node_sequence == UINT32_MAX)) return false;
    if (item->state == TECLA_GATEWAY_CMD_APPLIED &&
        item->config_revision_applied != item->config_revision_target) return false;
    if (item->state == TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED &&
        item->expired_at_epoch_s == 0U) return false;
    return true;
}

static int8_t find_active_id(const queue_snapshot_t *state,
                             const uint8_t id[TECLA_GATEWAY_COMMAND_ID_BYTES])
{
    for (uint8_t i = 0; i < state->count; ++i) {
        const uint8_t slot = slot_for(state, i);
        if (memcmp(state->items[slot].command_id, id,
                   TECLA_GATEWAY_COMMAND_ID_BYTES) == 0) return (int8_t)slot;
    }
    return -1;
}

static int8_t find_deadletter_id(const queue_snapshot_t *state,
                                 const uint8_t id[TECLA_GATEWAY_COMMAND_ID_BYTES])
{
    for (uint8_t i = 0; i < state->deadletter_count; ++i) {
        if (memcmp(state->deadletters[i].command_id, id,
                   TECLA_GATEWAY_COMMAND_ID_BYTES) == 0) return (int8_t)i;
    }
    return -1;
}

static int8_t find_deadletter_sequence(const queue_snapshot_t *state,
                                      uint32_t sequence)
{
    for (uint8_t i = 0; i < state->deadletter_count; ++i) {
        if (state->deadletters[i].queue_sequence == sequence) return (int8_t)i;
    }
    return -1;
}

static bool sequence_in_use(const queue_snapshot_t *state, uint32_t sequence)
{
    for (uint8_t i = 0; i < state->count; ++i) {
        if (state->items[slot_for(state, i)].queue_sequence == sequence) return true;
    }
    return find_deadletter_sequence(state, sequence) >= 0;
}

static bool same_command(const tecla_gateway_command_t *stored,
                         const tecla_gateway_command_t *incoming)
{
    return stored->firmware_node_id == incoming->firmware_node_id &&
        stored->command_type == incoming->command_type &&
        stored->config_revision_expected == incoming->config_revision_expected &&
        stored->config_revision_target == incoming->config_revision_target &&
        stored->expires_at_epoch_s == incoming->expires_at_epoch_s &&
        stored->payload_length == incoming->payload_length &&
        memcmp(stored->payload, incoming->payload, incoming->payload_length) == 0;
}

static bool snapshot_valid(const queue_snapshot_t *state)
{
    if (state->magic != QUEUE_SNAPSHOT_MAGIC ||
        state->version != QUEUE_SNAPSHOT_VERSION ||
        state->record_size != sizeof(*state) ||
        state->head >= TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY ||
        state->count > TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY ||
        state->deadletter_count > TECLA_GATEWAY_COMMAND_DEADLETTER_CAPACITY ||
        state->next_queue_sequence == 0U ||
        state->checksum != checksum32((const uint8_t *)state,
                                      offsetof(queue_snapshot_t, checksum))) return false;

    for (uint8_t i = 0; i < state->count; ++i) {
        const tecla_gateway_command_t *item = &state->items[slot_for(state, i)];
        if (!active_state_valid(item)) return false;
        for (uint8_t j = 0; j < i; ++j) {
            const tecla_gateway_command_t *prior = &state->items[slot_for(state, j)];
            if (item->queue_sequence == prior->queue_sequence ||
                memcmp(item->command_id, prior->command_id,
                       TECLA_GATEWAY_COMMAND_ID_BYTES) == 0) return false;
        }
        for (uint8_t j = 0; j < state->deadletter_count; ++j) {
            const tecla_gateway_command_t *prior = &state->deadletters[j];
            if (item->queue_sequence == prior->queue_sequence ||
                memcmp(item->command_id, prior->command_id,
                       TECLA_GATEWAY_COMMAND_ID_BYTES) == 0) return false;
        }
    }
    for (uint8_t i = 0; i < state->deadletter_count; ++i) {
        const tecla_gateway_command_t *item = &state->deadletters[i];
        if (!deadletter_state_valid(item)) return false;
        for (uint8_t j = 0; j < i; ++j) {
            const tecla_gateway_command_t *prior = &state->deadletters[j];
            if (item->queue_sequence == prior->queue_sequence ||
                memcmp(item->command_id, prior->command_id,
                       TECLA_GATEWAY_COMMAND_ID_BYTES) == 0) return false;
        }
    }
    return true;
}

static esp_err_t persist_work_snapshot(void)
{
    work_snapshot.checksum = checksum32((const uint8_t *)&work_snapshot,
                                        offsetof(queue_snapshot_t, checksum));
    esp_err_t err = nvs_set_blob(nvs_handle, QUEUE_NVS_KEY, &work_snapshot,
                                 sizeof(work_snapshot));
    if (err == ESP_OK) err = nvs_commit(nvs_handle);
    if (err == ESP_OK) snapshot = work_snapshot;
    return err;
}

static esp_err_t lock_ready(void)
{
    if (!queue_ready || queue_mutex == NULL) return ESP_ERR_INVALID_STATE;
    return xSemaphoreTake(queue_mutex, portMAX_DELAY) == pdTRUE
        ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t init_failed(esp_err_t error, bool nvs_opened)
{
    if (nvs_opened) {
        nvs_close(nvs_handle);
        nvs_handle = 0;
    }
    if (queue_mutex != NULL) {
        vSemaphoreDelete(queue_mutex);
        queue_mutex = NULL;
    }
    return error;
}

static bool terminal_result(uint8_t state)
{
    return state == TECLA_GATEWAY_CMD_APPLIED ||
           state == TECLA_GATEWAY_CMD_REJECTED;
}

static bool sequence_is_newer(uint32_t candidate, uint32_t previous)
{
    return candidate > previous;
}

static void remove_active_head(queue_snapshot_t *state)
{
    memset(&state->items[state->head], 0, sizeof(state->items[state->head]));
    state->head = (uint8_t)((state->head + 1U) % TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY);
    state->count--;
}

static esp_err_t validate_ack_for_item(const tecla_gateway_command_t *item,
                                       const tecla_gateway_command_ack_t *ack,
                                       bool late)
{
    if (memcmp(item->command_id, ack->command_id, TECLA_GATEWAY_COMMAND_ID_BYTES) != 0 ||
        item->firmware_node_id != ack->firmware_node_id ||
        item->config_revision_expected != ack->config_revision_expected) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ack->state != TECLA_GATEWAY_CMD_APPLIED &&
        ack->state != TECLA_GATEWAY_CMD_REJECTED &&
        ack->state != TECLA_GATEWAY_CMD_RECEIVED_NODE) return ESP_ERR_INVALID_ARG;
    if (!id16_nonzero(ack->node_boot_id) || ack->node_sequence == UINT32_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ack->state == TECLA_GATEWAY_CMD_APPLIED &&
        ack->config_revision_applied != item->config_revision_target) {
        return ESP_ERR_INVALID_STATE;
    }
    if (ack->state == TECLA_GATEWAY_CMD_RECEIVED_NODE &&
        ack->config_revision_applied != 0U) return ESP_ERR_INVALID_STATE;

    if (late) {
        if (ack->state == TECLA_GATEWAY_CMD_RECEIVED_NODE || item->attempts == 0U) {
            return ESP_ERR_INVALID_STATE;
        }
        if (id16_nonzero(item->node_boot_id) &&
            (memcmp(item->node_boot_id, ack->node_boot_id,
                    TECLA_GATEWAY_COMMAND_ID_BYTES) != 0 ||
             !sequence_is_newer(ack->node_sequence, item->node_sequence))) {
            return ESP_ERR_INVALID_STATE;
        }
    } else if (ack->state == TECLA_GATEWAY_CMD_RECEIVED_NODE) {
        if (item->state != TECLA_GATEWAY_CMD_WAITING_NODE_ACK ||
            (id16_nonzero(item->node_boot_id) &&
             memcmp(item->node_boot_id, ack->node_boot_id,
                    TECLA_GATEWAY_COMMAND_ID_BYTES) == 0 &&
             !sequence_is_newer(ack->node_sequence, item->node_sequence))) {
            return ESP_ERR_INVALID_STATE;
        }
    } else if (item->state != TECLA_GATEWAY_CMD_RECEIVED_NODE ||
               memcmp(item->node_boot_id, ack->node_boot_id,
                      TECLA_GATEWAY_COMMAND_ID_BYTES) != 0 ||
               !sequence_is_newer(ack->node_sequence, item->node_sequence)) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

static void apply_ack(tecla_gateway_command_t *item,
                      const tecla_gateway_command_ack_t *ack)
{
    item->state = (uint8_t)ack->state;
    item->result_code = ack->result_code;
    item->config_revision_applied = ack->config_revision_applied;
    memcpy(item->node_boot_id, ack->node_boot_id, TECLA_GATEWAY_COMMAND_ID_BYTES);
    item->node_sequence = ack->node_sequence;
}

esp_err_t tecla_gateway_command_queue_init(uint8_t *pending_count,
                                           uint8_t *recovered_inflight)
{
    if (pending_count == NULL || recovered_inflight == NULL) return ESP_ERR_INVALID_ARG;
    *recovered_inflight = 0U;
    if (queue_ready) return tecla_gateway_command_queue_get_count(pending_count);

    queue_mutex = xSemaphoreCreateMutex();
    if (queue_mutex == NULL) return ESP_ERR_NO_MEM;
    esp_err_t err = nvs_open(QUEUE_NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) return init_failed(err, false);

    size_t stored_size = 0U;
    err = nvs_get_blob(nvs_handle, QUEUE_NVS_KEY, NULL, &stored_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        memset(&snapshot, 0, sizeof(snapshot));
        snapshot.magic = QUEUE_SNAPSHOT_MAGIC;
        snapshot.version = QUEUE_SNAPSHOT_VERSION;
        snapshot.record_size = (uint16_t)sizeof(snapshot);
        snapshot.next_queue_sequence = 1U;
        work_snapshot = snapshot;
        err = persist_work_snapshot();
    } else if (err == ESP_OK) {
        if (stored_size == sizeof(queue_snapshot_v3_t)) {
            return init_failed(ESP_ERR_INVALID_STATE, true);
        }
        if (stored_size != sizeof(snapshot)) return init_failed(ESP_ERR_INVALID_SIZE, true);
        err = nvs_get_blob(nvs_handle, QUEUE_NVS_KEY, &snapshot, &stored_size);
        if (err != ESP_OK) return init_failed(err, true);
        if (snapshot.version != QUEUE_SNAPSHOT_VERSION) {
            return init_failed(ESP_ERR_INVALID_STATE, true);
        }
        if (!snapshot_valid(&snapshot)) return init_failed(ESP_ERR_INVALID_CRC, true);
    }
    if (err != ESP_OK) return init_failed(err, true);

    work_snapshot = snapshot;
    for (uint8_t i = 0; i < work_snapshot.count; ++i) {
        tecla_gateway_command_t *item = &work_snapshot.items[slot_for(&work_snapshot, i)];
        if (item->state == TECLA_GATEWAY_CMD_WAITING_NODE_ACK ||
            item->state == TECLA_GATEWAY_CMD_RECEIVED_NODE) {
            item->state = TECLA_GATEWAY_CMD_RECEIVED_GATEWAY;
            if (*recovered_inflight < UINT8_MAX) (*recovered_inflight)++;
        }
    }
    if (memcmp(&work_snapshot, &snapshot, sizeof(snapshot)) != 0) {
        err = persist_work_snapshot();
        if (err != ESP_OK) return init_failed(err, true);
    }
    queue_ready = true;
    *pending_count = snapshot.count;
    return ESP_OK;
}

esp_err_t tecla_gateway_command_queue_enqueue(
    const tecla_gateway_command_t *command, uint32_t *queue_sequence_out)
{
    if (!command_valid(command) || queue_sequence_out == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    work_snapshot = snapshot;

    const int8_t active_index = find_active_id(&work_snapshot, command->command_id);
    const int8_t deadletter_index = find_deadletter_id(&work_snapshot, command->command_id);
    if (active_index >= 0 || deadletter_index >= 0) {
        const tecla_gateway_command_t *existing = active_index >= 0
            ? &work_snapshot.items[(uint8_t)active_index]
            : &work_snapshot.deadletters[(uint8_t)deadletter_index];
        if (!same_command(existing, command)) err = ESP_ERR_INVALID_ARG;
        else {
            *queue_sequence_out = existing->queue_sequence;
            err = ESP_OK;
        }
        xSemaphoreGive(queue_mutex);
        return err;
    }
    if (work_snapshot.count == TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NO_MEM;
    }

    uint32_t candidate_sequence = work_snapshot.next_queue_sequence;
    bool sequence_found = false;
    for (uint8_t attempt = 0; attempt <=
         TECLA_GATEWAY_COMMAND_QUEUE_CAPACITY + TECLA_GATEWAY_COMMAND_DEADLETTER_CAPACITY;
         ++attempt) {
        if (candidate_sequence == 0U) candidate_sequence = 1U;
        if (!sequence_in_use(&work_snapshot, candidate_sequence)) {
            sequence_found = true;
            break;
        }
        candidate_sequence++;
    }
    if (!sequence_found) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NO_MEM;
    }

    const uint8_t slot = slot_for(&work_snapshot, work_snapshot.count);
    tecla_gateway_command_t item = *command;
    item.queue_sequence = candidate_sequence;
    work_snapshot.next_queue_sequence = candidate_sequence + 1U;
    if (work_snapshot.next_queue_sequence == 0U) work_snapshot.next_queue_sequence = 1U;
    item.state = TECLA_GATEWAY_CMD_RECEIVED_GATEWAY;
    item.attempts = 0U;
    item.result_code = 0U;
    item.config_revision_applied = 0U;
    memset(item.last_delivery_lease_id, 0, TECLA_GATEWAY_COMMAND_ID_BYTES);
    memset(item.node_boot_id, 0, TECLA_GATEWAY_COMMAND_ID_BYTES);
    item.node_sequence = 0U;
    memset(item.payload + item.payload_length, 0,
           TECLA_GATEWAY_COMMAND_PAYLOAD_MAX - item.payload_length);
    work_snapshot.items[slot] = item;
    work_snapshot.count++;
    err = persist_work_snapshot();
    if (err == ESP_OK) *queue_sequence_out = item.queue_sequence;
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_peek(tecla_gateway_command_t *command_out)
{
    if (command_out == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    if (snapshot.count == 0U) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    *command_out = snapshot.items[snapshot.head];
    xSemaphoreGive(queue_mutex);
    return ESP_OK;
}

esp_err_t tecla_gateway_command_queue_mark_dispatch_authorized(
    uint32_t queue_sequence,
    const tecla_gateway_dispatch_authorization_t *authorization,
    uint64_t now_monotonic_ms)
{
    if (authorization == NULL ||
        !authorization->delivery_authorized ||
        authorization->radio_send_budget != 1U ||
        !id16_nonzero(authorization->command_id) ||
        !id16_nonzero(authorization->delivery_lease_id) ||
        authorization->firmware_node_id == 0U ||
        authorization->delivery_attempts == 0U ||
        authorization->deadline_monotonic_ms <= now_monotonic_ms ||
        authorization->deadline_monotonic_ms - now_monotonic_ms >
            TECLA_GATEWAY_COMMAND_LEASE_MAX_MS) return ESP_ERR_INVALID_ARG;

    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    if (snapshot.count == 0U) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    work_snapshot = snapshot;
    tecla_gateway_command_t *head = &work_snapshot.items[work_snapshot.head];
    if (head->queue_sequence != queue_sequence ||
        head->state != TECLA_GATEWAY_CMD_RECEIVED_GATEWAY ||
        authorization->delivery_attempts <= head->attempts ||
        memcmp(head->command_id, authorization->command_id,
               TECLA_GATEWAY_COMMAND_ID_BYTES) != 0 ||
        head->firmware_node_id != authorization->firmware_node_id ||
        head->config_revision_expected != authorization->config_revision_expected ||
        head->config_revision_target != authorization->config_revision_target ||
        memcmp(head->last_delivery_lease_id, authorization->delivery_lease_id,
               TECLA_GATEWAY_COMMAND_ID_BYTES) == 0) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    head->attempts = authorization->delivery_attempts;
    memcpy(head->last_delivery_lease_id, authorization->delivery_lease_id,
           TECLA_GATEWAY_COMMAND_ID_BYTES);
    head->state = TECLA_GATEWAY_CMD_WAITING_NODE_ACK;
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_mark_retry(uint32_t queue_sequence)
{
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    if (snapshot.count == 0U) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    work_snapshot = snapshot;
    tecla_gateway_command_t *head = &work_snapshot.items[work_snapshot.head];
    if (head->queue_sequence != queue_sequence ||
        (head->state != TECLA_GATEWAY_CMD_WAITING_NODE_ACK &&
         head->state != TECLA_GATEWAY_CMD_RECEIVED_NODE)) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    head->state = TECLA_GATEWAY_CMD_RECEIVED_GATEWAY;
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_mark_result(
    uint32_t queue_sequence, const tecla_gateway_command_ack_t *ack)
{
    if (ack == NULL || !id16_nonzero(ack->command_id) ||
        !id16_nonzero(ack->node_boot_id) || ack->firmware_node_id == 0U ||
        ack->node_sequence == UINT32_MAX ||
        (ack->state != TECLA_GATEWAY_CMD_RECEIVED_NODE &&
         ack->state != TECLA_GATEWAY_CMD_APPLIED &&
         ack->state != TECLA_GATEWAY_CMD_REJECTED)) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    if (snapshot.count == 0U) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    work_snapshot = snapshot;
    tecla_gateway_command_t *head = &work_snapshot.items[work_snapshot.head];
    if (head->queue_sequence != queue_sequence) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }

    if (head->state == (uint8_t)ack->state &&
        head->result_code == ack->result_code &&
        head->config_revision_applied == ack->config_revision_applied &&
        head->node_sequence == ack->node_sequence &&
        memcmp(head->node_boot_id, ack->node_boot_id,
               TECLA_GATEWAY_COMMAND_ID_BYTES) == 0 &&
        head->config_revision_expected == ack->config_revision_expected) {
        xSemaphoreGive(queue_mutex);
        return ESP_OK;
    }

    const bool late_unknown =
        head->state == TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN &&
        (ack->state == TECLA_GATEWAY_CMD_APPLIED ||
         ack->state == TECLA_GATEWAY_CMD_REJECTED);
    if (head->state == TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN && !late_unknown) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    err = validate_ack_for_item(head, ack, late_unknown);
    if (err != ESP_OK) {
        xSemaphoreGive(queue_mutex);
        return err;
    }
    apply_ack(head, ack);
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_mark_server_unknown(
    uint32_t queue_sequence, const tecla_gateway_server_unknown_t *status)
{
    if (status == NULL || !id16_nonzero(status->command_id) ||
        status->firmware_node_id == 0U || !status->server_reports_unknown ||
        status->expired_at_epoch_s == 0U) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    if (snapshot.count == 0U) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    work_snapshot = snapshot;
    tecla_gateway_command_t *head = &work_snapshot.items[work_snapshot.head];
    if (head->queue_sequence != queue_sequence ||
        memcmp(head->command_id, status->command_id,
               TECLA_GATEWAY_COMMAND_ID_BYTES) != 0 ||
        head->firmware_node_id != status->firmware_node_id ||
        head->config_revision_expected != status->config_revision_expected ||
        head->config_revision_target != status->config_revision_target ||
        status->expired_at_epoch_s < head->expires_at_epoch_s ||
        head->state == TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED ||
        terminal_result(head->state)) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (head->state == TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN) {
        const esp_err_t duplicate = head->expired_at_epoch_s == status->expired_at_epoch_s
            ? ESP_OK : ESP_ERR_INVALID_STATE;
        xSemaphoreGive(queue_mutex);
        return duplicate;
    }
    head->state = TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN;
    head->expired_at_epoch_s = status->expired_at_epoch_s;
    head->result_code = 0U;
    head->config_revision_applied = 0U;
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_release_unknown(
    uint32_t queue_sequence, const tecla_gateway_unknown_server_ack_t *ack)
{
    if (ack == NULL || !id16_nonzero(ack->command_id) ||
        ack->firmware_node_id == 0U ||
        ack->result_status != TECLA_GATEWAY_RESULT_STATUS_UNKNOWN ||
        ack->ack_stage != TECLA_GATEWAY_ACK_STAGE_UNKNOWN_CONFIRMED ||
        !ack->gateway_queue_terminal || !ack->remove_reported ||
        ack->expired_at_epoch_s == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    work_snapshot = snapshot;
    const int8_t existing_deadletter = find_deadletter_id(&work_snapshot, ack->command_id);
    if (existing_deadletter >= 0) {
        const tecla_gateway_command_t *existing = &work_snapshot.deadletters[(uint8_t)existing_deadletter];
        const esp_err_t duplicate_result =
            existing->state == TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED &&
            existing->queue_sequence == queue_sequence &&
            existing->firmware_node_id == ack->firmware_node_id &&
            existing->config_revision_expected == ack->config_revision_expected &&
            existing->config_revision_target == ack->config_revision_target &&
            existing->expired_at_epoch_s == ack->expired_at_epoch_s
                ? ESP_OK : ESP_ERR_INVALID_STATE;
        xSemaphoreGive(queue_mutex);
        return duplicate_result;
    }
    if (snapshot.count == 0U) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    work_snapshot = snapshot;
    tecla_gateway_command_t *head = &work_snapshot.items[work_snapshot.head];
    if (head->queue_sequence != queue_sequence ||
        head->state != TECLA_GATEWAY_CMD_NO_CONFIRMATION_UNKNOWN ||
        memcmp(head->command_id, ack->command_id,
               TECLA_GATEWAY_COMMAND_ID_BYTES) != 0 ||
        head->firmware_node_id != ack->firmware_node_id ||
        head->config_revision_expected != ack->config_revision_expected ||
        head->config_revision_target != ack->config_revision_target ||
        head->expired_at_epoch_s != ack->expired_at_epoch_s) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (work_snapshot.deadletter_count == TECLA_GATEWAY_COMMAND_DEADLETTER_CAPACITY) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NO_MEM;
    }
    head->state = TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED;
    work_snapshot.deadletters[work_snapshot.deadletter_count++] = *head;
    remove_active_head(&work_snapshot);
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_mark_late_result(
    const tecla_gateway_command_ack_t *ack)
{
    if (ack == NULL || !id16_nonzero(ack->command_id) ||
        !id16_nonzero(ack->node_boot_id) || ack->firmware_node_id == 0U ||
        ack->node_sequence == UINT32_MAX) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    work_snapshot = snapshot;
    const int8_t index = find_deadletter_id(&work_snapshot, ack->command_id);
    if (index < 0) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    tecla_gateway_command_t *item = &work_snapshot.deadletters[(uint8_t)index];
    if (item->firmware_node_id != ack->firmware_node_id ||
        item->config_revision_expected != ack->config_revision_expected) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    if (terminal_result(item->state)) {
        const bool duplicate = item->state == (uint8_t)ack->state &&
            item->result_code == ack->result_code &&
            item->config_revision_applied == ack->config_revision_applied &&
            item->node_sequence == ack->node_sequence &&
            memcmp(item->node_boot_id, ack->node_boot_id,
                   TECLA_GATEWAY_COMMAND_ID_BYTES) == 0;
        xSemaphoreGive(queue_mutex);
        return duplicate ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    if (item->state != TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    err = validate_ack_for_item(item, ack, true);
    if (err != ESP_OK) {
        xSemaphoreGive(queue_mutex);
        return err;
    }
    apply_ack(item, ack);
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_finalize_unknown_deadletter(
    const tecla_gateway_unknown_final_server_ack_t *ack)
{
    if (ack == NULL || !id16_nonzero(ack->command_id) ||
        ack->firmware_node_id == 0U || !ack->result_unknown_final ||
        !ack->remove_reported) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    work_snapshot = snapshot;
    const int8_t index = find_deadletter_id(&work_snapshot, ack->command_id);
    if (index < 0) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    const tecla_gateway_command_t *item = &work_snapshot.deadletters[(uint8_t)index];
    if (item->state != TECLA_GATEWAY_CMD_UNKNOWN_CONFIRMED ||
        item->firmware_node_id != ack->firmware_node_id ||
        item->config_revision_expected != ack->config_revision_expected ||
        item->config_revision_target != ack->config_revision_target) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    for (uint8_t i = (uint8_t)index;
         i + 1U < work_snapshot.deadletter_count; ++i) {
        work_snapshot.deadletters[i] = work_snapshot.deadletters[i + 1U];
    }
    work_snapshot.deadletter_count--;
    memset(&work_snapshot.deadletters[work_snapshot.deadletter_count], 0,
           sizeof(work_snapshot.deadletters[work_snapshot.deadletter_count]));
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_peek_deadletter(
    uint8_t offset, tecla_gateway_command_t *command_out)
{
    if (command_out == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    if (offset >= snapshot.deadletter_count) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_NOT_FOUND;
    }
    *command_out = snapshot.deadletters[offset];
    xSemaphoreGive(queue_mutex);
    return ESP_OK;
}

esp_err_t tecla_gateway_command_queue_remove_reported(uint32_t queue_sequence)
{
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    work_snapshot = snapshot;
    if (work_snapshot.count > 0U) {
        tecla_gateway_command_t *head = &work_snapshot.items[work_snapshot.head];
        if (head->queue_sequence == queue_sequence && terminal_result(head->state)) {
            remove_active_head(&work_snapshot);
            err = persist_work_snapshot();
            xSemaphoreGive(queue_mutex);
            return err;
        }
    }
    const int8_t index = find_deadletter_sequence(&work_snapshot, queue_sequence);
    if (index < 0 || !terminal_result(work_snapshot.deadletters[(uint8_t)index].state)) {
        xSemaphoreGive(queue_mutex);
        return ESP_ERR_INVALID_STATE;
    }
    for (uint8_t i = (uint8_t)index; i + 1U < work_snapshot.deadletter_count; ++i) {
        work_snapshot.deadletters[i] = work_snapshot.deadletters[i + 1U];
    }
    work_snapshot.deadletter_count--;
    memset(&work_snapshot.deadletters[work_snapshot.deadletter_count], 0,
           sizeof(work_snapshot.deadletters[work_snapshot.deadletter_count]));
    err = persist_work_snapshot();
    xSemaphoreGive(queue_mutex);
    return err;
}

esp_err_t tecla_gateway_command_queue_get_count(uint8_t *count_out)
{
    if (count_out == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    *count_out = snapshot.count;
    xSemaphoreGive(queue_mutex);
    return ESP_OK;
}

esp_err_t tecla_gateway_command_queue_get_deadletter_count(uint8_t *count_out)
{
    if (count_out == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = lock_ready();
    if (err != ESP_OK) return err;
    *count_out = snapshot.deadletter_count;
    xSemaphoreGive(queue_mutex);
    return ESP_OK;
}

uint32_t tecla_gateway_command_queue_retry_delay_ms(uint32_t attempts)
{
    uint32_t delay_ms = TECLA_GATEWAY_COMMAND_RETRY_BASE_MS;
    if (attempts == 0U) attempts = 1U;
    while (attempts > 1U && delay_ms < TECLA_GATEWAY_COMMAND_RETRY_MAX_MS) {
        delay_ms *= 2U;
        if (delay_ms > TECLA_GATEWAY_COMMAND_RETRY_MAX_MS) {
            delay_ms = TECLA_GATEWAY_COMMAND_RETRY_MAX_MS;
        }
        attempts--;
    }
    return delay_ms;
}
