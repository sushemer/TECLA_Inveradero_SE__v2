#include "../tecla_wire_v3.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *p, uint32_t v)
{
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
static void put_float(uint8_t *p, float f)
{
    uint32_t bits; memcpy(&bits, &f, sizeof(bits)); put_u32(p, bits);
}
static void init_header(tecla_wire_v3_header_t *h, uint8_t type)
{
    memset(h, 0, sizeof(*h)); h->firmware_node_id = 0x78563412U; h->type = type; h->seq = 0x04030201U;
    for (size_t i=0; i<sizeof(h->boot_id); ++i) h->boot_id[i]=(uint8_t)(i+1U);
    h->boot_id[6]=(uint8_t)((h->boot_id[6]&0x0fU)|0x40U);
    h->boot_id[8]=(uint8_t)((h->boot_id[8]&0x3fU)|0x80U);
}
static void set_uuid_v4(uint8_t *p, uint8_t seed)
{
    for(size_t i=0U;i<16U;++i)p[i]=(uint8_t)(seed+i);
    p[6]=(uint8_t)((p[6]&0x0fU)|0x40U);p[8]=(uint8_t)((p[8]&0x3fU)|0x80U);
}

static void roundtrip(tecla_wire_v3_header_t *h, const uint8_t *payload, size_t plen, size_t total)
{
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE]; size_t frame_len=0U;
    assert(tecla_wire_v3_encode(h,payload,plen,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_OK);
    assert(frame_len==total && frame[0]==3U && frame[1]==h->type);
    assert(frame[2]==(uint8_t)total && frame[3]==0U);
    assert(frame[4]==0x12U && frame[5]==0x34U && frame[6]==0x56U && frame[7]==0x78U);
    assert(frame[8]==1U && frame[13]==6U && frame[14]==0x47U && frame[16]==0x89U);
    assert(frame[23]==16U);
    assert(frame[24]==1U && frame[25]==2U && frame[26]==3U && frame[27]==4U);
    tecla_wire_v3_frame_view_t view;
    assert(tecla_wire_v3_decode(frame,frame_len,&view)==TECLA_WIRE_V3_OK);
    assert(view.header.type==h->type && view.header.firmware_node_id==h->firmware_node_id &&
           view.header.seq==h->seq && view.payload_len==plen &&
           memcmp(view.payload,payload,plen)==0);
}

static size_t typed_roundtrip(tecla_wire_v3_header_t *header,
                              const tecla_wire_v3_message_t *message,
                              tecla_wire_v3_message_t *decoded)
{
    uint8_t frame[TECLA_WIRE_V3_MAX_FRAME_SIZE];
    size_t frame_len = 0U;
    tecla_wire_v3_header_t parsed_header;
    assert(tecla_wire_v3_encode_typed(header, message, frame, sizeof(frame),
                                      &frame_len) == TECLA_WIRE_V3_OK);
    assert(tecla_wire_v3_decode_typed(frame, frame_len, &parsed_header, decoded) ==
           TECLA_WIRE_V3_OK);
    assert(parsed_header.type == header->type &&
           parsed_header.firmware_node_id == header->firmware_node_id &&
           parsed_header.seq == header->seq);
    return frame_len;
}

int main(void)
{
    tecla_wire_v3_header_t h; uint8_t p[222]={0};

    init_header(&h,TECLA_WIRE_V3_CAPABILITIES);
    p[0]=8U; size_t o=1U;
    for (uint8_t s=1U;s<=8U;++s) { p[o++]=s; p[o++]=7U; for(uint8_t m=1U;m<=7U;++m)p[o++]=m; }
    roundtrip(&h,p,o,101U);

    init_header(&h,TECLA_WIRE_V3_HEARTBEAT); memset(p,0,86U); p[4]=1U;
    p[22]=0x12U;p[54]=0x34U;
    roundtrip(&h,p,86U,114U);

    init_header(&h,TECLA_WIRE_V3_TELEMETRY); memset(p,0,139U);
    p[4]=1U; p[5]=1U; put_u16(p+6U,600U); p[16]=0U; p[17]=255U; p[18]=8U;
    for(size_t i=19U;i<83U;++i)p[i]=(uint8_t)(i-18U);
    for(uint8_t i=0U;i<8U;++i){uint8_t *r=p+83U+(size_t)i*7U;r[0]=(uint8_t)(i+1U);r[1]=1U;r[2]=1U;put_float(r+3U,(float)i+0.5f);}
    roundtrip(&h,p,139U,167U);

    init_header(&h,TECLA_WIRE_V3_TELEMETRY_ACK); p[0]=1U;p[1]=0U;
    roundtrip(&h,p,2U,30U);

    init_header(&h,TECLA_WIRE_V3_COMMAND_CONFIG); memset(p,0,120U);
    set_uuid_v4(p,0x20U); p[20]=2U; p[24]=1U; put_u16(p+25U,600U); put_u32(p+27U,600U); p[31]=8U;
    for(uint8_t i=0U;i<8U;++i){p[96U+(size_t)i*3U]=(uint8_t)(i+1U);p[97U+(size_t)i*3U]=1U;p[98U+(size_t)i*3U]=1U;}
    roundtrip(&h,p,120U,148U);

    init_header(&h,TECLA_WIRE_V3_COMMAND_ACK); memset(p,0,30U);set_uuid_v4(p,0x40U);p[20]=2U;p[24]=2U;p[28]=2U;
    roundtrip(&h,p,30U,58U);

    init_header(&h,TECLA_WIRE_V3_MANIFEST_CONFIG); memset(p,0,96U);
    p[0]=1U;put_u16(p+1U,600U);put_u32(p+3U,600U);p[7]=8U;p[8]=0x56U;p[40]=0x78U;
    for(uint8_t i=0U;i<8U;++i){p[72U+(size_t)i*3U]=(uint8_t)(i+1U);p[73U+(size_t)i*3U]=1U;p[74U+(size_t)i*3U]=1U;}
    roundtrip(&h,p,96U,124U);

    init_header(&h,TECLA_WIRE_V3_MANIFEST_ACK); memset(p,0,66U);p[0]=0x56U;p[32]=0x78U;p[64]=1U;
    roundtrip(&h,p,66U,94U);

    uint8_t frame[251]={0}; size_t frame_len=0U; tecla_wire_v3_frame_view_t view;
    init_header(&h,TECLA_WIRE_V3_HEARTBEAT); memset(p,0,222U);
    /* A valid frame at the ESP-NOW v1 app-data limit is intentionally not a valid
       heartbeat; a type cannot consume arbitrary padding. */
    assert(tecla_wire_v3_encode(&h,p,222U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_FIELD);
    assert(tecla_wire_v3_encode(&h,p,222U,frame,249U,&frame_len)==TECLA_WIRE_V3_ERR_FIELD);
    assert(tecla_wire_v3_encode(&h,p,223U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_LENGTH);

    init_header(&h,TECLA_WIRE_V3_HEARTBEAT); memset(p,0,86U);p[4]=1U;
    assert(tecla_wire_v3_encode(&h,p,86U,frame,113U,&frame_len)==TECLA_WIRE_V3_ERR_CAPACITY);
    assert(tecla_wire_v3_encode(&h,p,86U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_OK);
    frame[0]=2U; assert(tecla_wire_v3_decode(frame,114U,&view)==TECLA_WIRE_V3_ERR_VERSION);
    frame[0]=3U; frame[1]=99U; assert(tecla_wire_v3_decode(frame,114U,&view)==TECLA_WIRE_V3_ERR_TYPE);
    frame[1]=2U; frame[2]=113U; assert(tecla_wire_v3_decode(frame,114U,&view)==TECLA_WIRE_V3_ERR_LENGTH);
    frame[2]=114U; frame[4]=0U;frame[5]=0U;frame[6]=0U;frame[7]=0U;
    assert(tecla_wire_v3_decode(frame,114U,&view)==TECLA_WIRE_V3_ERR_FIELD);
    assert(tecla_wire_v3_decode(frame,251U,&view)==TECLA_WIRE_V3_ERR_LENGTH);

    init_header(&h,TECLA_WIRE_V3_TELEMETRY); memset(p,0,90U);
    p[4]=1U;p[5]=1U;put_u16(p+6U,600U);p[17]=50U;p[18]=1U;
    p[83]=1U;p[84]=1U;p[85]=1U;put_u32(p+86U,0x7fc00000U);
    assert(tecla_wire_v3_encode(&h,p,90U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_FIELD);
    put_float(p+86U,23.5f);p[16]=1U;
    assert(tecla_wire_v3_encode(&h,p,90U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_FIELD);
    p[16]=0U;p[18]=2U;
    assert(tecla_wire_v3_encode(&h,p,90U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_FIELD);
    p[18]=1U;p[6]=1U;p[7]=0U;
    assert(tecla_wire_v3_encode(&h,p,90U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_FIELD);

    init_header(&h,TECLA_WIRE_V3_HEARTBEAT);memset(p,0,86U);p[4]=1U;
    h.seq=UINT32_MAX;
    assert(tecla_wire_v3_encode(&h,p,86U,frame,sizeof(frame),&frame_len)==TECLA_WIRE_V3_ERR_FIELD);
    h.seq=1U;
    char hash_hex[64]; char hash_encoded[65]; uint8_t hash_raw[32];
    for(size_t i=0U;i<sizeof(hash_hex);++i)hash_hex[i]=(i%2U==0U)?'a':'f';
    assert(tecla_wire_v3_decode_sha256_hex(hash_hex,sizeof(hash_hex),hash_raw)==TECLA_WIRE_V3_OK);
    for(size_t i=0U;i<sizeof(hash_raw);++i)assert(hash_raw[i]==0xafU);
    assert(tecla_wire_v3_encode_sha256_hex(hash_raw,hash_encoded)==TECLA_WIRE_V3_OK);
    assert(strcmp(hash_encoded,"afafafafafafafafafafafafafafafafafafafafafafafafafafafafafafafaf")==0);
    hash_hex[9]='x';assert(tecla_wire_v3_decode_sha256_hex(hash_hex,sizeof(hash_hex),hash_raw)==TECLA_WIRE_V3_ERR_FIELD);
    assert(tecla_wire_v3_decode_sha256_hex(hash_hex,63U,hash_raw)==TECLA_WIRE_V3_ERR_LENGTH);

    /* Typed builders/parsers cover all message variants without packed structs. */
    tecla_wire_v3_message_t typed = {0}, decoded = {0};
    init_header(&h, TECLA_WIRE_V3_CAPABILITIES);
    typed.capabilities.slot_count = 1U;
    typed.capabilities.slots[0].slot_code = 1U;
    typed.capabilities.slots[0].measurement_count = 2U;
    typed.capabilities.slots[0].measurement_codes[0] = 1U;
    typed.capabilities.slots[0].measurement_codes[1] = 2U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 33U);
    assert(decoded.capabilities.slot_count == 1U &&
           decoded.capabilities.slots[0].measurement_codes[1] == 2U);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_HEARTBEAT);
    typed.heartbeat.effective_mode = 1U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 114U);
    assert(decoded.heartbeat.effective_mode == 1U && !decoded.heartbeat.last_command_present);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_TELEMETRY);
    typed.telemetry.effective_mode = 1U; typed.telemetry.sample_interval_s = 600U;
    typed.telemetry.battery_percent = UINT8_MAX; typed.telemetry.reading_count = 1U;
    typed.telemetry.readings[0] = (tecla_wire_v3_reading_t){1U, 1U, 1U, 23.5f};
    assert(typed_roundtrip(&h, &typed, &decoded) == 118U);
    assert(decoded.telemetry.reading_count == 1U &&
           decoded.telemetry.readings[0].value == 23.5f);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_TELEMETRY_ACK);
    typed.telemetry_ack.scope = 1U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 30U);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_COMMAND_CONFIG);
    set_uuid_v4(typed.command_config.command_id, 0x20U);
    typed.command_config.command_config_revision_target = 1U;
    typed.command_config.effective_mode = 1U;
    typed.command_config.sample_interval_s = 600U;
    typed.command_config.management_poll_interval_s = 600U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 124U);
    assert(decoded.command_config.command_config_revision_target == 1U);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_COMMAND_ACK);
    set_uuid_v4(typed.command_ack.command_id, 0x10U);
    typed.command_ack.command_config_revision_expected = 1U;
    typed.command_ack.command_config_revision_target = 2U;
    typed.command_ack.command_config_revision_applied = 2U;
    typed.command_ack.stage = 2U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 58U);
    static const uint8_t command_ack_payload_golden[30] = {
        0x10,0x11,0x12,0x13,0x14,0x15,0x46,0x17,0x98,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f,
        0x01,0x00,0x00,0x00, 0x02,0x00,0x00,0x00, 0x02,0x00,0x00,0x00, 0x02,0x00
    };
    uint8_t typed_frame[TECLA_WIRE_V3_MAX_FRAME_SIZE]; size_t typed_len = 0U;
    assert(tecla_wire_v3_encode_typed(&h, &typed, typed_frame, sizeof(typed_frame),
                                      &typed_len) == TECLA_WIRE_V3_OK);
    assert(typed_len == 58U &&
           memcmp(typed_frame + TECLA_WIRE_V3_HEADER_SIZE,
                  command_ack_payload_golden, sizeof(command_ack_payload_golden)) == 0);
    assert(decoded.command_ack.command_config_revision_applied == 2U);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_MANIFEST_CONFIG);
    typed.manifest_config.effective_mode = 1U;
    typed.manifest_config.sample_interval_s = 600U;
    typed.manifest_config.management_poll_interval_s = 600U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 100U);

    memset(&typed, 0, sizeof(typed)); init_header(&h, TECLA_WIRE_V3_MANIFEST_ACK);
    typed.manifest_ack.stage = 1U;
    assert(typed_roundtrip(&h, &typed, &decoded) == 94U);

    puts("PASS wire_v3_bytewise_golden_lengths_offsets_malformed_limit");
    return 0;
}
