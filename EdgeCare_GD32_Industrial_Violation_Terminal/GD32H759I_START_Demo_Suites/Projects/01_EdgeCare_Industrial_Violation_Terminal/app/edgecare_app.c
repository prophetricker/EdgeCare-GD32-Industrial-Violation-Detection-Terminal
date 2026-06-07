/*!
    \file    edgecare_app.c
    \brief   EdgeCare application state machine
*/

#include "edgecare_app.h"
#include "../board/board_config.h"
#include "../bsp/bsp_alarm.h"
#include "../bsp/bsp_camera_ov5640.h"
#include "../bsp/bsp_radar_ld2410.h"
#include "../model/edgecare_infer.h"
#include "../platform/edgecare_log.h"
#include "../vision/edgecare_preprocess.h"
#include <stdio.h>

typedef enum {
    EDGECARE_STATE_IDLE = 0,
    EDGECARE_STATE_ALARM
} edgecare_state_t;

typedef struct {
    edgecare_state_t state;
    uint32_t ts_ms;
    uint32_t seq;
    uint8_t radar_triggered;
    uint8_t alarm_active;
    uint32_t infer_ms;
    uint8_t confidence_percent;
} edgecare_context_t;

static edgecare_context_t g_edgecare = {
    EDGECARE_STATE_IDLE,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U
};

static uint8_t g_model_input_gray[MODEL_INPUT_BYTES] __attribute__((aligned(32)));

static const char *edgecare_state_name(edgecare_state_t state)
{
    switch(state) {
    case EDGECARE_STATE_IDLE:
        return "IDLE";
    case EDGECARE_STATE_ALARM:
        return "ALARM";
    default:
        return "UNKNOWN";
    }
}

static void edgecare_alarm_set(uint8_t active)
{
    g_edgecare.alarm_active = active ? 1U : 0U;
    bsp_alarm_set_active(g_edgecare.alarm_active);
}

#if EDGECARE_ENABLE_GRAY96_DUMP
static void edgecare_dump_gray96(const uint8_t *gray96,
                                 const edgecare_preprocess_gray96_stats_t *stats)
{
    uint32_t offset;

    printf("gray96_dump_begin: dev=%s width=%u height=%u bytes=%lu checksum=0x%08lX format=hex8\r\n",
           EDGECARE_DEVICE_ID,
           (unsigned int)MODEL_INPUT_WIDTH,
           (unsigned int)MODEL_INPUT_HEIGHT,
           (unsigned long)MODEL_INPUT_BYTES,
           (unsigned long)stats->checksum);

    for(offset = 0U; offset < MODEL_INPUT_BYTES; offset += EDGECARE_GRAY96_DUMP_CHUNK_BYTES) {
        uint32_t i;
        uint32_t chunk_bytes = EDGECARE_GRAY96_DUMP_CHUNK_BYTES;

        if((offset + chunk_bytes) > MODEL_INPUT_BYTES) {
            chunk_bytes = MODEL_INPUT_BYTES - offset;
        }

        printf("gray96_dump_data: offset=%lu hex=", (unsigned long)offset);
        for(i = 0U; i < chunk_bytes; i++) {
            printf("%02X", gray96[offset + i]);
        }
        printf("\r\n");
    }

    printf("gray96_dump_end: bytes=%lu\r\n", (unsigned long)MODEL_INPUT_BYTES);
}
#endif /* EDGECARE_ENABLE_GRAY96_DUMP */

static void edgecare_preprocess_gray96_probe(void)
{
    edgecare_preprocess_gray96_stats_t stats;
    edgecare_infer_result_t infer_result;

    edgecare_preprocess_gray96_from_yuyv(bsp_camera_ov5640_frame(),
                                         g_model_input_gray,
                                         &stats);

    printf("preprocess_gray96: source=YUYV_Y qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
           (unsigned int)CAMERA_FRAME_WIDTH,
           (unsigned int)CAMERA_FRAME_HEIGHT,
           (unsigned int)MODEL_INPUT_WIDTH,
           (unsigned int)MODEL_INPUT_HEIGHT,
           (unsigned long)MODEL_INPUT_BYTES,
           stats.min_value,
           stats.max_value,
           (unsigned long)stats.mean,
           (unsigned long)stats.checksum,
           stats.center[0],
           stats.center[1],
           stats.center[2],
           stats.center[3]);

#if EDGECARE_ENABLE_GRAY96_DUMP
    edgecare_dump_gray96(g_model_input_gray, &stats);
#endif

    if(edgecare_infer(g_model_input_gray, &infer_result)) {
        g_edgecare.confidence_percent = infer_result.confidence_percent;
        g_edgecare.infer_ms = 1U;
        printf("infer_probe: model=stat_placeholder input=gray96 mean=%u contrast=%u conf=%u.%02u intrusion=%u note=replace_with_trained_model\r\n",
               infer_result.mean,
               infer_result.contrast,
               infer_result.confidence_percent / 100U,
               infer_result.confidence_percent % 100U,
               infer_result.intrusion);
    }
}

void edgecare_app_init(void)
{
    uint8_t camera_detected;

    bsp_alarm_init();
    bsp_radar_ld2410_init();

    printf("\r\n%s boot: EdgeCare GD32H759 terminal bring-up\r\n", EDGECARE_DEVICE_ID);
    printf("log_format: [ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...\r\n");
    printf("radar_input: LD2410 OUT active-high on PF8, alarm active-low on PA8\r\n");
    printf("camera_sccb: SCL=PB10 SDA=PB11 RES=PD0 PWON=PD1\r\n");
    printf("camera_capture_probe: enabled; expect one camera_capture line before periodic state logs\r\n");

    camera_detected = bsp_camera_ov5640_id_probe();
    if(camera_detected) {
        (void)bsp_camera_ov5640_qvga_stream_init();
    } else {
        printf("camera_init: skipped because OV5640 ID was not confirmed\r\n");
    }

    if(bsp_camera_ov5640_capture_probe()) {
        edgecare_preprocess_gray96_probe();
    }
}

void edgecare_app_step(void)
{
    g_edgecare.radar_triggered = bsp_radar_ld2410_is_triggered();
    g_edgecare.infer_ms = 0U;

    if(g_edgecare.radar_triggered) {
        g_edgecare.state = EDGECARE_STATE_ALARM;
        g_edgecare.confidence_percent = 100U;
        edgecare_alarm_set(1U);
    } else {
        g_edgecare.state = EDGECARE_STATE_IDLE;
        g_edgecare.confidence_percent = 0U;
        edgecare_alarm_set(0U);
    }

    g_edgecare.seq++;
    edgecare_log_status(g_edgecare.ts_ms,
                        edgecare_state_name(g_edgecare.state),
                        g_edgecare.radar_triggered,
                        g_edgecare.infer_ms,
                        g_edgecare.confidence_percent,
                        g_edgecare.alarm_active,
                        g_edgecare.seq);
}

void edgecare_app_advance_time(uint32_t elapsed_ms)
{
    g_edgecare.ts_ms += elapsed_ms;
}
