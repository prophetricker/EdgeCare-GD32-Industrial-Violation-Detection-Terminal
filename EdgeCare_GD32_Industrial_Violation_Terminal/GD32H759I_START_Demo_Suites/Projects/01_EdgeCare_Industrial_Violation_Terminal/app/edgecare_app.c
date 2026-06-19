/*!
    \file    edgecare_app.c
    \brief   EdgeCare application state machine
*/

#include "edgecare_app.h"
#include "../board/board_config.h"
#include "../bsp/bsp_alarm.h"
#include "../bsp/bsp_camera_ov5640.h"
#include "../bsp/bsp_radar_ld2410.h"
#include "../bsp/bsp_voice_vw553.h"
#include "../model/edgecare_infer.h"
#include "../model/edgecare_model_baseline.h"
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
    uint8_t voice_alarm_latched;
    uint32_t infer_ms;
    uint8_t confidence_percent;
    uint32_t voice_last_trigger_ms;
} edgecare_context_t;

static edgecare_context_t g_edgecare = {
    EDGECARE_STATE_IDLE,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U
};

static edgecare_vote_state_t g_vote_state;
static uint8_t g_model_input_gray[MODEL_INPUT_BYTES] __attribute__((aligned(32)));
#if EDGECARE_ENABLE_GRAY96_DUMP && EDGECARE_ENABLE_GRAY96_DUMP_VARIANTS
static uint8_t g_debug_gray_variant[MODEL_INPUT_BYTES] __attribute__((aligned(32)));
#endif
#if EDGECARE_ENABLE_GRAY96_DUMP && EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP
static uint8_t g_debug_byte_plane[CAMERA_DIAG_PLANE_BYTES] __attribute__((aligned(32)));
#endif

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

static void edgecare_voice_on_alarm_update(uint8_t alarm_active)
{
    if(!alarm_active) {
        g_edgecare.voice_alarm_latched = 0U;
        return;
    }

    if(!g_edgecare.voice_alarm_latched) {
        uint32_t elapsed_ms = g_edgecare.ts_ms - g_edgecare.voice_last_trigger_ms;

        if((0U == g_edgecare.voice_last_trigger_ms) ||
           (elapsed_ms >= EDGECARE_VOICE_COOLDOWN_MS)) {
            bsp_voice_vw553_send_danger();
            g_edgecare.voice_last_trigger_ms = g_edgecare.ts_ms;
            printf("voice_probe: event=danger_cmd target=vw553 uart=USART1 tx=PA2 rx=PA3 command=DANGER cooldown_ms=%lu\r\n",
                   (unsigned long)EDGECARE_VOICE_COOLDOWN_MS);
        }

        g_edgecare.voice_alarm_latched = 1U;
    }
}

#if EDGECARE_ENABLE_GRAY96_DUMP
static void edgecare_compute_image_stats(const uint8_t *image,
                                         uint32_t bytes,
                                         edgecare_preprocess_gray96_stats_t *stats)
{
    uint32_t i;
    uint32_t checksum = 0U;
    uint32_t sum = 0U;

    stats->min_value = 0xFFU;
    stats->max_value = 0U;

    for(i = 0U; i < bytes; i++) {
        uint8_t value = image[i];

        sum += value;
        checksum ^= ((uint32_t)value << ((i & 3U) * 8U));
        checksum = (checksum << 3U) | (checksum >> 29U);

        if(value < stats->min_value) {
            stats->min_value = value;
        }
        if(value > stats->max_value) {
            stats->max_value = value;
        }
    }

    stats->mean = (0U == bytes) ? 0U : (sum / bytes);
    stats->checksum = checksum;
    stats->center[0] = (bytes > 0U) ? image[bytes / 2U] : 0U;
    stats->center[1] = (bytes > 1U) ? image[(bytes / 2U) + 1U] : 0U;
    stats->center[2] = (bytes > 2U) ? image[(bytes / 2U) + 2U] : 0U;
    stats->center[3] = (bytes > 3U) ? image[(bytes / 2U) + 3U] : 0U;
}

static void edgecare_dump_image(const char *kind,
                                const char *variant,
                                const uint8_t *image,
                                uint32_t width,
                                uint32_t height,
                                uint32_t bytes,
                                const edgecare_preprocess_gray96_stats_t *stats)
{
    uint32_t offset;

    printf("image_dump_begin: dev=%s kind=%s variant=%s width=%lu height=%lu bytes=%lu checksum=0x%08lX format=hex8\r\n",
           EDGECARE_DEVICE_ID,
           kind,
           variant,
           (unsigned long)width,
           (unsigned long)height,
           (unsigned long)bytes,
           (unsigned long)stats->checksum);

    for(offset = 0U; offset < bytes; offset += EDGECARE_GRAY96_DUMP_CHUNK_BYTES) {
        uint32_t i;
        uint32_t chunk_bytes = EDGECARE_GRAY96_DUMP_CHUNK_BYTES;

        if((offset + chunk_bytes) > bytes) {
            chunk_bytes = bytes - offset;
        }

        printf("image_dump_data: offset=%lu hex=", (unsigned long)offset);
        for(i = 0U; i < chunk_bytes; i++) {
            printf("%02X", image[offset + i]);
        }
        printf("\r\n");
    }

    printf("image_dump_end: bytes=%lu\r\n", (unsigned long)bytes);
}

static void edgecare_dump_gray96(const char *variant,
                                 const uint8_t *gray96,
                                 const edgecare_preprocess_gray96_stats_t *stats)
{
    edgecare_dump_image("gray96",
                        variant,
                        gray96,
                        MODEL_INPUT_WIDTH,
                        MODEL_INPUT_HEIGHT,
                        MODEL_INPUT_BYTES,
                        stats);
}
#endif /* EDGECARE_ENABLE_GRAY96_DUMP */

static void edgecare_preprocess_current_frame(edgecare_preprocess_gray96_stats_t *stats)
{
#if EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565 && !EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN
    edgecare_preprocess_gray96_from_rgb565(bsp_camera_ov5640_frame(),
                                           g_model_input_gray,
                                           stats);
#elif (EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV || EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF) && !EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN
    edgecare_preprocess_gray96_from_yuyv(bsp_camera_ov5640_frame(),
                                         g_model_input_gray,
                                         stats);
#else
    edgecare_preprocess_gray96_from_raw8_stride(bsp_camera_ov5640_frame(),
                                                CAMERA_RAW8_BYTE_STRIDE,
                                                CAMERA_RAW8_BYTE_PHASE,
                                                g_model_input_gray,
                                                stats);
#endif
}

static void edgecare_preprocess_gray96_probe(void)
{
    edgecare_preprocess_gray96_stats_t stats;
    edgecare_infer_result_t infer_result;

    edgecare_preprocess_current_frame(&stats);

#if EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN
    printf("preprocess_gray96: source=OV5640_DVP_PATTERN_NOT_REAL_SCENE scale=raw qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565
    printf("preprocess_gray96: source=OV5640_RGB565 scale=raw qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV
#if EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION
    printf("preprocess_gray96: source=OV5640_ISP_YUV422_Y02 scale=lshift2 qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#else
    printf("preprocess_gray96: source=OV5640_ISP_YUV422_Y02 scale=raw qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#endif
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF
#if EDGECARE_ENABLE_GRAY96_LSHIFT2_COMPENSATION
    printf("preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=lshift2 qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#else
    printf("preprocess_gray96: source=OV5640_JPEG_TO_YUV_REF_Y02 scale=raw qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#endif
#else
    printf("preprocess_gray96: source=OV5640_SNR_RAW8 scale=raw qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#endif
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
#if EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN
    edgecare_preprocess_gray96_stats_t frame_stats;

    edgecare_compute_image_stats(bsp_camera_ov5640_frame(), CAMERA_CAPTURE_BYTES, &frame_stats);
    edgecare_dump_gray96("dvp_pattern_stride2_phase0", g_model_input_gray, &stats);
    edgecare_dump_image("frame_bytes",
                        "dvp_pattern_640x240",
                        bsp_camera_ov5640_frame(),
                        CAMERA_FRAME_WIDTH * CAMERA_FRAME_BPP,
                        CAMERA_FRAME_HEIGHT,
                        CAMERA_CAPTURE_BYTES,
                        &frame_stats);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV
    edgecare_dump_gray96("yuv422_y02", g_model_input_gray, &stats);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF
    edgecare_dump_gray96("jpeg_to_yuv_ref_y02", g_model_input_gray, &stats);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565
    edgecare_dump_gray96("rgb565_luma", g_model_input_gray, &stats);
#else
    edgecare_dump_gray96("raw8_stride2_phase0", g_model_input_gray, &stats);
#endif
#if EDGECARE_ENABLE_GRAY96_DUMP_VARIANTS
    edgecare_preprocess_gray96_stats_t variant_stats;

#if EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN
    edgecare_preprocess_gray96_from_raw8_stride(bsp_camera_ov5640_frame(),
                                                CAMERA_RAW8_BYTE_STRIDE,
                                                1U,
                                                g_debug_gray_variant,
                                                &variant_stats);
    printf("preprocess_gray96_variant: source=OV5640_DVP_PATTERN_STRIDE2_PHASE1_NOT_REAL_SCENE qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
           (unsigned int)CAMERA_FRAME_WIDTH,
           (unsigned int)CAMERA_FRAME_HEIGHT,
           (unsigned int)MODEL_INPUT_WIDTH,
           (unsigned int)MODEL_INPUT_HEIGHT,
           (unsigned long)MODEL_INPUT_BYTES,
           variant_stats.min_value,
           variant_stats.max_value,
           (unsigned long)variant_stats.mean,
           (unsigned long)variant_stats.checksum,
           variant_stats.center[0],
           variant_stats.center[1],
           variant_stats.center[2],
           variant_stats.center[3]);
    edgecare_dump_gray96("dvp_pattern_stride2_phase1", g_debug_gray_variant, &variant_stats);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV || EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF
    edgecare_preprocess_gray96_from_yuv422_phase(bsp_camera_ov5640_frame(),
                                                 1U,
                                                 g_debug_gray_variant,
                                                 &variant_stats);
#if EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF
    printf("preprocess_gray96_variant: source=OV5640_JPEG_TO_YUV_REF_Y13 qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#else
    printf("preprocess_gray96_variant: source=YUV422_Y13 qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
#endif
           (unsigned int)CAMERA_FRAME_WIDTH,
           (unsigned int)CAMERA_FRAME_HEIGHT,
           (unsigned int)MODEL_INPUT_WIDTH,
           (unsigned int)MODEL_INPUT_HEIGHT,
           (unsigned long)MODEL_INPUT_BYTES,
           variant_stats.min_value,
           variant_stats.max_value,
           (unsigned long)variant_stats.mean,
           (unsigned long)variant_stats.checksum,
           variant_stats.center[0],
           variant_stats.center[1],
           variant_stats.center[2],
           variant_stats.center[3]);
    edgecare_dump_gray96("yuv422_y13", g_debug_gray_variant, &variant_stats);
#else
    edgecare_preprocess_gray96_from_raw8_stride(bsp_camera_ov5640_frame(),
                                                CAMERA_RAW8_BYTE_STRIDE,
                                                1U,
                                                g_debug_gray_variant,
                                                &variant_stats);
    printf("preprocess_gray96_variant: source=OV5640_SNR_RAW8_STRIDE2_PHASE1 qvga=%ux%u out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
           (unsigned int)CAMERA_FRAME_WIDTH,
           (unsigned int)CAMERA_FRAME_HEIGHT,
           (unsigned int)MODEL_INPUT_WIDTH,
           (unsigned int)MODEL_INPUT_HEIGHT,
           (unsigned long)MODEL_INPUT_BYTES,
           variant_stats.min_value,
           variant_stats.max_value,
           (unsigned long)variant_stats.mean,
           (unsigned long)variant_stats.checksum,
           variant_stats.center[0],
           variant_stats.center[1],
           variant_stats.center[2],
           variant_stats.center[3]);
    edgecare_dump_gray96("raw8_stride2_phase1", g_debug_gray_variant, &variant_stats);
#endif
#endif
#if EDGECARE_ENABLE_CAMERA_BYTE_PLANE_DUMP
    {
        edgecare_preprocess_gray96_stats_t plane_stats;
        static const char *const plane_names[4] = {
            "byte_plane0",
            "byte_plane1",
            "byte_plane2",
            "byte_plane3"
        };
        uint32_t phase;

        for(phase = 0U; phase < 4U; phase++) {
            edgecare_preprocess_byte_plane_from_frame(bsp_camera_ov5640_frame(),
                                                      (uint8_t)phase,
                                                      g_debug_byte_plane,
                                                      &plane_stats);
            printf("preprocess_byte_plane: source=raw_qvga phase=%lu out=%ux%u bytes=%lu min=%u max=%u mean=%lu checksum=0x%08lX center=%u,%u,%u,%u\r\n",
                   (unsigned long)phase,
                   (unsigned int)CAMERA_DIAG_PLANE_WIDTH,
                   (unsigned int)CAMERA_DIAG_PLANE_HEIGHT,
                   (unsigned long)CAMERA_DIAG_PLANE_BYTES,
                   plane_stats.min_value,
                   plane_stats.max_value,
                   (unsigned long)plane_stats.mean,
                   (unsigned long)plane_stats.checksum,
                   plane_stats.center[0],
                   plane_stats.center[1],
                   plane_stats.center[2],
                   plane_stats.center[3]);
            edgecare_dump_image("byte_plane",
                                plane_names[phase],
                                g_debug_byte_plane,
                                CAMERA_DIAG_PLANE_WIDTH,
                                CAMERA_DIAG_PLANE_HEIGHT,
                                CAMERA_DIAG_PLANE_BYTES,
                                &plane_stats);
        }
    }
#endif
#endif

    if(edgecare_infer(g_model_input_gray, &infer_result)) {
        uint8_t voted_alarm;

        edgecare_vote_reset(&g_vote_state);
        voted_alarm = edgecare_vote_update(&g_vote_state, &infer_result);
        g_edgecare.confidence_percent = infer_result.confidence_percent;
        g_edgecare.infer_ms = infer_result.inference_time_ms;
        printf("infer_probe: model=%s input=gray96 mean=%u contrast=%u conf=%u.%02u intrusion=%u placeholder=%u note=trained_baseline\r\n",
               infer_result.model_name,
               infer_result.mean,
               infer_result.contrast,
               infer_result.confidence_percent / 100U,
               infer_result.confidence_percent % 100U,
               infer_result.intrusion,
               infer_result.is_placeholder);
        printf("vote_probe: threshold=%u.%02u rule=2of3_release2 intrusion_votes=%u safe_votes=%u alarm=%u\r\n",
               (uint8_t)((EDGECARE_BASELINE_THRESHOLD_Q15 * 100U) / 32768U) / 100U,
               (uint8_t)((EDGECARE_BASELINE_THRESHOLD_Q15 * 100U) / 32768U) % 100U,
               g_vote_state.intrusion_votes,
               g_vote_state.safe_votes,
               voted_alarm);
        printf("vw553_json: {\"dev\":\"%s\",\"event\":\"zone_intrusion\",\"conf\":%u.%02u,\"lat_ms\":%lu,\"seq\":%lu,\"placeholder\":%u}\r\n",
               EDGECARE_DEVICE_ID,
               infer_result.confidence_percent / 100U,
               infer_result.confidence_percent % 100U,
               (unsigned long)infer_result.inference_time_ms,
               (unsigned long)g_edgecare.seq,
               infer_result.is_placeholder);
    }
}

static uint8_t edgecare_run_vision_frame(void)
{
    edgecare_preprocess_gray96_stats_t stats;
    edgecare_infer_result_t infer_result;
    uint8_t voted_alarm;

    if(!bsp_camera_ov5640_capture_frame()) {
        printf("vision_probe: capture=fail action=safe\r\n");
        g_edgecare.confidence_percent = 0U;
        g_edgecare.infer_ms = 0U;
        edgecare_vote_reset(&g_vote_state);
        return 0U;
    }

    edgecare_preprocess_current_frame(&stats);
    if(!edgecare_infer(g_model_input_gray, &infer_result)) {
        printf("vision_probe: capture=ok infer=fail action=safe\r\n");
        g_edgecare.confidence_percent = 0U;
        g_edgecare.infer_ms = 0U;
        edgecare_vote_reset(&g_vote_state);
        return 0U;
    }

    voted_alarm = edgecare_vote_update(&g_vote_state, &infer_result);
    g_edgecare.confidence_percent = infer_result.confidence_percent;
    g_edgecare.infer_ms = infer_result.inference_time_ms;
    printf("vision_probe: capture=ok model=%s mean=%u contrast=%u conf=%u.%02u intrusion=%u votes=%u/%u alarm=%u\r\n",
           infer_result.model_name,
           infer_result.mean,
           infer_result.contrast,
           infer_result.confidence_percent / 100U,
           infer_result.confidence_percent % 100U,
           infer_result.intrusion,
           g_vote_state.intrusion_votes,
           g_vote_state.safe_votes,
           voted_alarm);

    return voted_alarm;
}

void edgecare_app_init(void)
{
    uint8_t camera_detected;

    bsp_alarm_init();
    bsp_radar_ld2410_init();
    bsp_voice_vw553_init();
    edgecare_vote_reset(&g_vote_state);

    printf("\r\n%s boot: EdgeCare GD32H759 terminal bring-up\r\n", EDGECARE_DEVICE_ID);
    printf("log_format: [ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...\r\n");
    printf("radar_input: LD2410 OUT active-high on PF8, alarm active-low on PA8\r\n");
    printf("voice_uart: target=GD32VW553 USART1_TX=PA2 USART1_RX=PA3 baud=%lu command=DANGER\\n\r\n",
           (unsigned long)VOICE_VW553_BAUDRATE);
    printf("camera_sccb: SCL=PF1 SDA=PF0 RES=PD0 PWON=PD1\r\n");
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
        uint8_t vision_alarm = edgecare_run_vision_frame();

        g_edgecare.state = vision_alarm ? EDGECARE_STATE_ALARM : EDGECARE_STATE_IDLE;
        edgecare_alarm_set(vision_alarm);
        edgecare_voice_on_alarm_update(vision_alarm);
    } else {
        g_edgecare.state = EDGECARE_STATE_IDLE;
        g_edgecare.confidence_percent = 0U;
        edgecare_alarm_set(0U);
        edgecare_voice_on_alarm_update(0U);
        edgecare_vote_reset(&g_vote_state);
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
