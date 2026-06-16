/*!
    \file    bsp_camera_ov5640.c
    \brief   OV5640 SCCB/DCI/DMA camera bring-up for EdgeCare
*/

#include "bsp_camera_ov5640.h"
#include "../board/board_config.h"
#include "gd32h7xx.h"
#include "gd32h759i_start.h"
#include "systick.h"
#include <stdio.h>
#include <string.h>

#define DCI_DATA_ADDRESS       ((uint32_t)&DCI_DATA)
typedef struct {
    uint16_t reg;
    uint8_t value;
} camera_reg8_t;

typedef struct {
    const char *tag;
    uint32_t clock_polarity;
    uint32_t hsync_polarity;
    uint32_t vsync_polarity;
} camera_capture_variant_t;

typedef struct {
    uint8_t valid;
    uint8_t best_phase;
    uint32_t row_range;
    uint32_t col_range;
    uint32_t neighbor_delta;
    uint32_t score;
} camera_frame_quality_t;

typedef struct {
    uint32_t pclk_edges;
    uint32_t href_edges;
    uint32_t sync_edges;
    uint32_t href_high_samples;
    uint32_t sync_high_samples;
    uint8_t pclk_level;
    uint8_t href_level;
    uint8_t sync_level;
} camera_dvp_gpio_sample_t;

typedef struct {
    uint32_t samples;
    uint32_t nonzero;
    uint32_t changes;
    uint32_t checksum;
    uint32_t first_count;
    uint8_t first[4];
    uint8_t last[4];
    uint8_t min_value;
    uint8_t max_value;
    uint8_t previous_value;
    uint8_t have_previous;
} camera_raw_sample_stats_t;

typedef struct {
    const char *tag;
    uint8_t reg_471b;
    uint8_t reg_471d;
    uint8_t reg_4730;
    uint8_t reg_4740;
} camera_timing_variant_t;

typedef struct {
    const char *tag;
    uint8_t reg_300e;
    uint8_t reg_302e;
    uint8_t reg_4713;
} camera_dvp_mode_variant_t;

typedef struct {
    const char *tag;
    uint8_t reg_460b;
    uint8_t reg_460c;
    uint8_t reg_3824;
    uint8_t reg_471d;
    uint8_t reg_4740;
    uint32_t clock_polarity;
    uint32_t hsync_polarity;
    uint32_t vsync_polarity;
} camera_jpeg_to_yuv_tune_variant_t;

typedef struct {
    uint8_t reg_3000;
    uint8_t reg_3002;
    uint8_t reg_3004;
    uint8_t reg_3006;
    uint8_t reg_3007;
    uint8_t reg_3008;
    uint8_t reg_3034;
    uint8_t reg_3035;
    uint8_t reg_3036;
    uint8_t reg_3037;
    uint8_t reg_3500;
    uint8_t reg_3501;
    uint8_t reg_3502;
    uint8_t reg_3503;
    uint8_t reg_350a;
    uint8_t reg_350b;
    uint8_t reg_3a00;
    uint8_t reg_3a13;
    uint8_t reg_3821;
    uint8_t reg_3824;
    uint8_t reg_4000;
    uint8_t reg_4001;
    uint8_t reg_4004;
    uint8_t reg_4005;
    uint8_t reg_4300;
    uint8_t reg_460b;
    uint8_t reg_460c;
    uint8_t reg_4740;
    uint8_t reg_4741;
    uint8_t reg_4745;
    uint8_t reg_5000;
    uint8_t reg_5001;
    uint8_t reg_501f;
    uint8_t reg_503d;
    uint8_t reg_5584;
} camera_isp_path_regs_t;

typedef struct {
    uint8_t reg_3800;
    uint8_t reg_3801;
    uint8_t reg_3802;
    uint8_t reg_3803;
    uint8_t reg_3804;
    uint8_t reg_3805;
    uint8_t reg_3806;
    uint8_t reg_3807;
    uint8_t reg_3808;
    uint8_t reg_3809;
    uint8_t reg_380a;
    uint8_t reg_380b;
    uint8_t reg_380c;
    uint8_t reg_380d;
    uint8_t reg_380e;
    uint8_t reg_380f;
    uint8_t reg_3810;
    uint8_t reg_3811;
    uint8_t reg_3812;
    uint8_t reg_3813;
    uint8_t reg_3814;
    uint8_t reg_3815;
} camera_window_regs_t;

typedef struct {
    uint32_t polls;
    uint32_t ctl_first;
    uint32_t ctl_last;
    uint32_t stat0_first;
    uint32_t stat0_last;
    uint32_t stat0_changes;
    uint32_t stat1_first;
    uint32_t stat1_last;
    uint32_t stat1_changes;
    uint32_t intf_first;
    uint32_t intf_last;
    uint32_t intf_changes;
    uint32_t hs_high_polls;
    uint32_t vs_high_polls;
    uint32_t fv_high_polls;
    uint32_t ef_seen;
    uint32_t ovr_seen;
    uint32_t vsif_seen;
    uint32_t elif_seen;
    uint32_t dma_count_start;
    uint32_t dma_count_min;
    uint32_t dma_count_last;
    uint32_t dma_count_changes;
    uint32_t dma_chctl_last;
    uint32_t dma_chfctl_last;
    uint32_t dmamux_last;
} camera_dci_status_probe_t;

#define CAMERA_TIMING_SWEEP_MIN_HREF_EDGES 100U
#define CAMERA_DCI_STATUS_PROBE_POLL_MASK 0x3FFU
#define CAMERA_CAPTURE_MODE_SWEEP_WINDOW_MS 80U
#define CAMERA_CAPTURE_MODE_SWEEP_POLLS_PER_MS 512U

#if EDGECARE_ENABLE_CAMERA_SNAPSHOT_CAPTURE
#define CAMERA_DCI_CAPTURE_MODE_DEFAULT DCI_CAPTURE_MODE_SNAPSHOT
#else
#define CAMERA_DCI_CAPTURE_MODE_DEFAULT DCI_CAPTURE_MODE_CONTINUOUS
#endif

static uint32_t g_camera_capture_buffer[CAMERA_CAPTURE_WORDS] __attribute__((aligned(32)));
static void camera_log_dvp_registers(const char *stage);
static void camera_dci_gpio_init(void);
static void camera_dci_dma_init(uint32_t capture_mode,
                                uint32_t clock_polarity,
                                uint32_t hsync_polarity,
                                uint32_t vsync_polarity);
static void camera_apply_st_dvp_reference_path(void);
static void camera_apply_rgb565_capture_path(void);
static void camera_apply_jpeg_to_yuv_ref_capture_path(void);
static void camera_window_readback_log(const char *tag);
static void camera_ov5640_jpeg_to_yuv_tune_sweep_probe(void);
static void camera_apply_raw_capture_path(void);
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
static void camera_dci_dma_summary_log(const char *tag);
static void camera_dci_status_probe_begin(camera_dci_status_probe_t *probe);
static void camera_dci_status_probe_poll(camera_dci_status_probe_t *probe);
static void camera_forced_sync_dci_probe(const char *tag, uint8_t href_value, uint8_t vsync_value);
static void camera_dci_sync_matrix_probe(void);
static void camera_capture_mode_sweep_probe(void);
#endif

static const camera_reg8_t g_ov5640_qvga_yuv_probe_regs[] = {
    {0x3103U, 0x11U},
    {0x3008U, 0x42U},
    {0x3103U, 0x03U},
    {0x3017U, 0xFFU},
    {0x3018U, 0xFFU},
    {0x3034U, 0x1AU},
    {0x3035U, 0x11U},
    {0x3036U, 0x46U},
    {0x3037U, 0x13U},
    {0x3108U, 0x01U},
    {0x3630U, 0x36U},
    {0x3631U, 0x0EU},
    {0x3632U, 0xE2U},
    {0x3633U, 0x12U},
    {0x3621U, 0xE0U},
    {0x3704U, 0xA0U},
    {0x3703U, 0x5AU},
    {0x3715U, 0x78U},
    {0x3717U, 0x01U},
    {0x370BU, 0x60U},
    {0x3705U, 0x1AU},
    {0x3905U, 0x02U},
    {0x3906U, 0x10U},
    {0x3901U, 0x0AU},
    {0x3731U, 0x12U},
    {0x3600U, 0x08U},
    {0x3601U, 0x33U},
    {0x302DU, 0x60U},
    {0x3620U, 0x52U},
    {0x371BU, 0x20U},
    {0x471CU, 0x50U},
    {0x3A13U, 0x43U},
    {0x3A18U, 0x00U},
    {0x3A19U, 0xF8U},
    {0x3635U, 0x13U},
    {0x3636U, 0x03U},
    {0x3634U, 0x40U},
    {0x3622U, 0x01U},
    {0x3C01U, 0x34U},
    {0x3C04U, 0x28U},
    {0x3C05U, 0x98U},
    {0x3C06U, 0x00U},
    {0x3C07U, 0x08U},
    {0x3C08U, 0x00U},
    {0x3C09U, 0x1CU},
    {0x3C0AU, 0x9CU},
    {0x3C0BU, 0x40U},
    {0x3820U, 0x41U},
    {0x3821U, 0x07U},
    {0x3814U, 0x31U},
    {0x3815U, 0x31U},
    {0x3000U, 0x00U},
    {0x3002U, 0x1CU},
    {0x3004U, 0xFFU},
    {0x3006U, 0xC3U},
    {0x300EU, 0x58U},
    {0x302EU, 0x00U},
    {0x4300U, 0x30U},
    {0x501FU, 0x00U},
    {0x4713U, 0x03U},
    {0x4407U, 0x04U},
    {0x460BU, 0x35U},
    {0x460CU, 0x22U},
    {0x3824U, 0x02U},
    {0x5000U, 0xA7U},
    {0x5001U, 0xA3U},
    {0x3800U, 0x00U},
    {0x3801U, 0x00U},
    {0x3802U, 0x00U},
    {0x3803U, 0x04U},
    {0x3804U, 0x0AU},
    {0x3805U, 0x3FU},
    {0x3806U, 0x07U},
    {0x3807U, 0x9BU},
    {0x3808U, 0x01U},
    {0x3809U, 0x40U},
    {0x380AU, 0x00U},
    {0x380BU, 0xF0U},
    {0x380CU, 0x07U},
    {0x380DU, 0x68U},
    {0x380EU, 0x03U},
    {0x380FU, 0xD8U},
    {0x3810U, 0x00U},
    {0x3811U, 0x10U},
    {0x3812U, 0x00U},
    {0x3813U, 0x06U},
    {0x3618U, 0x00U},
    {0x3612U, 0x29U},
    {0x3708U, 0x64U},
    {0x3709U, 0x52U},
    {0x370CU, 0x03U},
    {0x3A02U, 0x03U},
    {0x3A03U, 0xD8U},
    {0x3A08U, 0x01U},
    {0x3A09U, 0x27U},
    {0x3A0AU, 0x00U},
    {0x3A0BU, 0xF6U},
    {0x3A0EU, 0x03U},
    {0x3A0DU, 0x04U},
    {0x3A14U, 0x03U},
    {0x3A15U, 0xD8U},
    {0x4001U, 0x02U},
    {0x4004U, 0x02U},
    {0x3008U, 0x02U}
};

/* OV5640 software application notes 13.1.1 VGA Preview, with soft reset handled before the table. */
static const camera_reg8_t g_ov5640_vga_yuv_reference_regs[] = {
    {0x3103U, 0x11U},
    {0x3008U, 0x42U},
    {0x3103U, 0x03U},
    {0x3017U, 0xFFU},
    {0x3018U, 0xFFU},
    {0x3034U, 0x1AU},
    {0x3035U, 0x11U},
    {0x3036U, 0x46U},
    {0x3037U, 0x13U},
    {0x3108U, 0x01U},
    {0x3630U, 0x36U},
    {0x3631U, 0x0EU},
    {0x3632U, 0xE2U},
    {0x3633U, 0x12U},
    {0x3621U, 0xE0U},
    {0x3704U, 0xA0U},
    {0x3703U, 0x5AU},
    {0x3715U, 0x78U},
    {0x3717U, 0x01U},
    {0x370BU, 0x60U},
    {0x3705U, 0x1AU},
    {0x3905U, 0x02U},
    {0x3906U, 0x10U},
    {0x3901U, 0x0AU},
    {0x3731U, 0x12U},
    {0x3600U, 0x08U},
    {0x3601U, 0x33U},
    {0x302DU, 0x60U},
    {0x3620U, 0x52U},
    {0x371BU, 0x20U},
    {0x471CU, 0x50U},
    {0x3A13U, 0x43U},
    {0x3A18U, 0x00U},
    {0x3A19U, 0xF8U},
    {0x3635U, 0x13U},
    {0x3636U, 0x03U},
    {0x3634U, 0x40U},
    {0x3622U, 0x01U},
    {0x3C01U, 0x34U},
    {0x3C04U, 0x28U},
    {0x3C05U, 0x98U},
    {0x3C06U, 0x00U},
    {0x3C07U, 0x08U},
    {0x3C08U, 0x00U},
    {0x3C09U, 0x1CU},
    {0x3C0AU, 0x9CU},
    {0x3C0BU, 0x40U},
    {0x3820U, 0x41U},
    {0x3821U, 0x07U},
    {0x3814U, 0x31U},
    {0x3815U, 0x31U},
    {0x3800U, 0x00U},
    {0x3801U, 0x00U},
    {0x3802U, 0x00U},
    {0x3803U, 0x04U},
    {0x3804U, 0x0AU},
    {0x3805U, 0x3FU},
    {0x3806U, 0x07U},
    {0x3807U, 0x9BU},
    {0x3808U, 0x02U},
    {0x3809U, 0x80U},
    {0x380AU, 0x01U},
    {0x380BU, 0xE0U},
    {0x380CU, 0x07U},
    {0x380DU, 0x68U},
    {0x380EU, 0x03U},
    {0x380FU, 0xD8U},
    {0x3810U, 0x00U},
    {0x3811U, 0x10U},
    {0x3812U, 0x00U},
    {0x3813U, 0x06U},
    {0x3618U, 0x00U},
    {0x3612U, 0x29U},
    {0x3708U, 0x64U},
    {0x3709U, 0x52U},
    {0x370CU, 0x03U},
    {0x3A02U, 0x03U},
    {0x3A03U, 0xD8U},
    {0x3A08U, 0x01U},
    {0x3A09U, 0x27U},
    {0x3A0AU, 0x00U},
    {0x3A0BU, 0xF6U},
    {0x3A0EU, 0x03U},
    {0x3A0DU, 0x04U},
    {0x3A14U, 0x03U},
    {0x3A15U, 0xD8U},
    {0x4001U, 0x02U},
    {0x4004U, 0x02U},
    {0x3000U, 0x00U},
    {0x3002U, 0x1CU},
    {0x3004U, 0xFFU},
    {0x3006U, 0xC3U},
    {0x300EU, 0x58U},
    {0x302EU, 0x00U},
    {0x4300U, 0x30U},
    {0x501FU, 0x00U},
    {0x4713U, 0x03U},
    {0x4407U, 0x04U},
    {0x440EU, 0x00U},
    {0x460BU, 0x35U},
    {0x460CU, 0x22U},
    {0x3824U, 0x02U},
    {0x5000U, 0xA7U},
    {0x5001U, 0xA3U},
    {0x5180U, 0xFFU},
    {0x5181U, 0xF2U},
    {0x5182U, 0x00U},
    {0x5183U, 0x14U},
    {0x5184U, 0x25U},
    {0x5185U, 0x24U},
    {0x5186U, 0x09U},
    {0x5187U, 0x09U},
    {0x5188U, 0x09U},
    {0x5189U, 0x75U},
    {0x518AU, 0x54U},
    {0x518BU, 0xE0U},
    {0x518CU, 0xB2U},
    {0x518DU, 0x42U},
    {0x518EU, 0x3DU},
    {0x518FU, 0x56U},
    {0x5190U, 0x46U},
    {0x5191U, 0xF8U},
    {0x5192U, 0x04U},
    {0x5193U, 0x70U},
    {0x5194U, 0xF0U},
    {0x5195U, 0xF0U},
    {0x5196U, 0x03U},
    {0x5197U, 0x01U},
    {0x5198U, 0x04U},
    {0x5199U, 0x12U},
    {0x519AU, 0x04U},
    {0x519BU, 0x00U},
    {0x519CU, 0x06U},
    {0x519DU, 0x82U},
    {0x519EU, 0x38U},
    {0x5381U, 0x1EU},
    {0x5382U, 0x5BU},
    {0x5383U, 0x08U},
    {0x5384U, 0x0AU},
    {0x5385U, 0x7EU},
    {0x5386U, 0x88U},
    {0x5387U, 0x7CU},
    {0x5388U, 0x6CU},
    {0x5389U, 0x10U},
    {0x538AU, 0x01U},
    {0x538BU, 0x98U},
    {0x5300U, 0x08U},
    {0x5301U, 0x30U},
    {0x5302U, 0x10U},
    {0x5303U, 0x00U},
    {0x5304U, 0x08U},
    {0x5305U, 0x30U},
    {0x5306U, 0x08U},
    {0x5307U, 0x16U},
    {0x5309U, 0x08U},
    {0x530AU, 0x30U},
    {0x530BU, 0x04U},
    {0x530CU, 0x06U},
    {0x5480U, 0x01U},
    {0x5481U, 0x08U},
    {0x5482U, 0x14U},
    {0x5483U, 0x28U},
    {0x5484U, 0x51U},
    {0x5485U, 0x65U},
    {0x5486U, 0x71U},
    {0x5487U, 0x7DU},
    {0x5488U, 0x87U},
    {0x5489U, 0x91U},
    {0x548AU, 0x9AU},
    {0x548BU, 0xAAU},
    {0x548CU, 0xB8U},
    {0x548DU, 0xCDU},
    {0x548EU, 0xDDU},
    {0x548FU, 0xEAU},
    {0x5490U, 0x1DU},
    {0x5580U, 0x02U},
    {0x5583U, 0x40U},
    {0x5584U, 0x10U},
    {0x5589U, 0x10U},
    {0x558AU, 0x00U},
    {0x558BU, 0xF8U},
    {0x5800U, 0x23U},
    {0x5801U, 0x14U},
    {0x5802U, 0x0FU},
    {0x5803U, 0x0FU},
    {0x5804U, 0x12U},
    {0x5805U, 0x26U},
    {0x5806U, 0x0CU},
    {0x5807U, 0x08U},
    {0x5808U, 0x05U},
    {0x5809U, 0x05U},
    {0x580AU, 0x08U},
    {0x580BU, 0x0DU},
    {0x580CU, 0x08U},
    {0x580DU, 0x03U},
    {0x580EU, 0x00U},
    {0x580FU, 0x00U},
    {0x5810U, 0x03U},
    {0x5811U, 0x09U},
    {0x5812U, 0x07U},
    {0x5813U, 0x03U},
    {0x5814U, 0x00U},
    {0x5815U, 0x01U},
    {0x5816U, 0x03U},
    {0x5817U, 0x08U},
    {0x5818U, 0x0DU},
    {0x5819U, 0x08U},
    {0x581AU, 0x05U},
    {0x581BU, 0x06U},
    {0x581CU, 0x08U},
    {0x581DU, 0x0EU},
    {0x581EU, 0x29U},
    {0x581FU, 0x17U},
    {0x5820U, 0x11U},
    {0x5821U, 0x11U},
    {0x5822U, 0x15U},
    {0x5823U, 0x28U},
    {0x5824U, 0x46U},
    {0x5825U, 0x26U},
    {0x5826U, 0x08U},
    {0x5827U, 0x26U},
    {0x5828U, 0x64U},
    {0x5829U, 0x26U},
    {0x582AU, 0x24U},
    {0x582BU, 0x22U},
    {0x582CU, 0x24U},
    {0x582DU, 0x24U},
    {0x582EU, 0x06U},
    {0x582FU, 0x22U},
    {0x5830U, 0x40U},
    {0x5831U, 0x42U},
    {0x5832U, 0x24U},
    {0x5833U, 0x26U},
    {0x5834U, 0x24U},
    {0x5835U, 0x22U},
    {0x5836U, 0x22U},
    {0x5837U, 0x26U},
    {0x5838U, 0x44U},
    {0x5839U, 0x24U},
    {0x583AU, 0x26U},
    {0x583BU, 0x28U},
    {0x583CU, 0x42U},
    {0x583DU, 0xCEU},
    {0x5025U, 0x00U},
    {0x3A0FU, 0x30U},
    {0x3A10U, 0x28U},
    {0x3A1BU, 0x30U},
    {0x3A1EU, 0x26U},
    {0x3A11U, 0x60U},
    {0x3A1FU, 0x14U},
    {0x3008U, 0x02U},
    {0x3035U, 0x21U},
    {0x3C01U, 0xB4U},
    {0x3C00U, 0x04U},
    {0x3A19U, 0x7CU},
    {0x5800U, 0x2CU},
    {0x5801U, 0x17U},
    {0x5802U, 0x11U},
    {0x5803U, 0x11U},
    {0x5804U, 0x15U},
    {0x5805U, 0x29U},
    {0x5806U, 0x08U},
    {0x5807U, 0x06U},
    {0x5808U, 0x04U},
    {0x5809U, 0x04U},
    {0x580AU, 0x05U},
    {0x580BU, 0x07U},
    {0x580CU, 0x06U},
    {0x580DU, 0x03U},
    {0x580EU, 0x01U},
    {0x580FU, 0x01U},
    {0x5810U, 0x03U},
    {0x5811U, 0x06U},
    {0x5812U, 0x06U},
    {0x5813U, 0x02U},
    {0x5814U, 0x01U},
    {0x5815U, 0x01U},
    {0x5816U, 0x04U},
    {0x5817U, 0x07U},
    {0x5818U, 0x06U},
    {0x5819U, 0x07U},
    {0x581AU, 0x06U},
    {0x581BU, 0x06U},
    {0x581CU, 0x06U},
    {0x581DU, 0x0EU},
    {0x581EU, 0x31U},
    {0x581FU, 0x12U},
    {0x5820U, 0x11U},
    {0x5821U, 0x11U},
    {0x5822U, 0x11U},
    {0x5823U, 0x2FU},
    {0x5824U, 0x12U},
    {0x5825U, 0x25U},
    {0x5826U, 0x39U},
    {0x5827U, 0x29U},
    {0x5828U, 0x27U},
    {0x5829U, 0x39U},
    {0x582AU, 0x26U},
    {0x582BU, 0x33U},
    {0x582CU, 0x24U},
    {0x582DU, 0x39U},
    {0x582EU, 0x28U},
    {0x582FU, 0x21U},
    {0x5830U, 0x40U},
    {0x5831U, 0x21U},
    {0x5832U, 0x17U},
    {0x5833U, 0x17U},
    {0x5834U, 0x15U},
    {0x5835U, 0x11U},
    {0x5836U, 0x24U},
    {0x5837U, 0x27U},
    {0x5838U, 0x26U},
    {0x5839U, 0x26U},
    {0x583AU, 0x26U},
    {0x583BU, 0x28U},
    {0x583CU, 0x14U},
    {0x583DU, 0xEEU},
    {0x4005U, 0x1AU},
    {0x5381U, 0x26U},
    {0x5382U, 0x50U},
    {0x5383U, 0x0CU},
    {0x5384U, 0x09U},
    {0x5385U, 0x74U},
    {0x5386U, 0x7DU},
    {0x5387U, 0x7EU},
    {0x5388U, 0x75U},
    {0x5389U, 0x09U},
    {0x538BU, 0x98U},
    {0x538AU, 0x01U},
    {0x5580U, 0x02U},
    {0x5588U, 0x01U},
    {0x5583U, 0x40U},
    {0x5584U, 0x10U},
    {0x5589U, 0x0FU},
    {0x558AU, 0x00U},
    {0x558BU, 0x3FU},
    {0x5308U, 0x25U},
    {0x5304U, 0x08U},
    {0x5305U, 0x30U},
    {0x5306U, 0x10U},
    {0x5307U, 0x20U},
    {0x5180U, 0xFFU},
    {0x5181U, 0xF2U},
    {0x5182U, 0x11U},
    {0x5183U, 0x14U},
    {0x5184U, 0x25U},
    {0x5185U, 0x24U},
    {0x5186U, 0x10U},
    {0x5187U, 0x12U},
    {0x5188U, 0x10U},
    {0x5189U, 0x80U},
    {0x518AU, 0x54U},
    {0x518BU, 0xB8U},
    {0x518CU, 0xB2U},
    {0x518DU, 0x42U},
    {0x518EU, 0x3AU},
    {0x518FU, 0x56U},
    {0x5190U, 0x46U},
    {0x5191U, 0xF0U},
    {0x5192U, 0x0FU},
    {0x5193U, 0x70U},
    {0x5194U, 0xF0U},
    {0x5195U, 0xF0U},
    {0x5196U, 0x03U},
    {0x5197U, 0x01U},
    {0x5198U, 0x06U},
    {0x5199U, 0x62U},
    {0x519AU, 0x04U},
    {0x519BU, 0x00U},
    {0x519CU, 0x04U},
    {0x519DU, 0xE7U},
    {0x519EU, 0x38U},
};

/* OV5640 software application notes 13.1.2 Other size preview: QVGA output window. */
static const camera_reg8_t g_ov5640_qvga_output_regs[] = {
    {0x3800U, 0x00U},
    {0x3801U, 0x00U},
    {0x3802U, 0x00U},
    {0x3803U, 0x04U},
    {0x3804U, 0x0AU},
    {0x3805U, 0x3FU},
    {0x3806U, 0x07U},
    {0x3807U, 0x9BU},
    {0x3808U, 0x01U},
    {0x3809U, 0x40U},
    {0x380AU, 0x00U},
    {0x380BU, 0xF0U},
    {0x380CU, 0x07U},
    {0x380DU, 0x68U},
    {0x380EU, 0x03U},
    {0x380FU, 0xD8U},
};

/* ST's OV5640 BSP DVP-mode overlay, kept separate from the app-note base table for A/B diagnostics. */
static const camera_reg8_t g_ov5640_st_dvp_reference_regs[] = {
    {0x3017U, 0xFFU},
    {0x3018U, 0xF3U},
    {0x302EU, 0x00U},
    {0x471CU, 0x50U},
    {0x300EU, 0x58U},
    {0x3034U, 0x18U},
    {0x3035U, 0x41U},
    {0x3036U, 0x60U},
    {0x3037U, 0x13U},
    {0x3108U, 0x01U},
    {0x3808U, 0x01U},
    {0x3809U, 0x40U},
    {0x380AU, 0x00U},
    {0x380BU, 0xF0U},
    {0x4300U, 0x30U},
    {0x501FU, 0x00U},
    {0x4740U, 0x22U}
};

static uint8_t camera_wait_flag(uint32_t flag)
{
    uint32_t timeout = CAMERA_TIMEOUT;

    while(SET != i2c_flag_get(CAMERA_SCCB, flag)) {
        if(SET == i2c_flag_get(CAMERA_SCCB, I2C_FLAG_NACK)) {
            i2c_flag_clear(CAMERA_SCCB, I2C_FLAG_NACK);
            return 0U;
        }

        if(0U == timeout--) {
            return 0U;
        }
    }

    return 1U;
}

static void camera_sccb_init(void)
{
    rcu_i2c_clock_config(CAMERA_SCCB_IDX, RCU_I2CSRC_IRC64MDIV);
    rcu_periph_clock_enable(CAMERA_SCCB_GPIO_RCU);
    rcu_periph_clock_enable(CAMERA_CTRL_GPIO_RCU);
    rcu_periph_clock_enable(CAMERA_SCCB_RCU);

    gpio_af_set(CAMERA_SCCB_GPIO_PORT, CAMERA_SCCB_AF, CAMERA_SCCB_SCL_PIN);
    gpio_af_set(CAMERA_SCCB_GPIO_PORT, CAMERA_SCCB_AF, CAMERA_SCCB_SDA_PIN);

    gpio_mode_set(CAMERA_SCCB_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  CAMERA_SCCB_SCL_PIN | CAMERA_SCCB_SDA_PIN);
    gpio_output_options_set(CAMERA_SCCB_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_12MHZ,
                            CAMERA_SCCB_SCL_PIN | CAMERA_SCCB_SDA_PIN);

    gpio_mode_set(CAMERA_CTRL_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  CAMERA_RES_PIN | CAMERA_PWON_PIN);
    gpio_output_options_set(CAMERA_CTRL_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ,
                            CAMERA_RES_PIN | CAMERA_PWON_PIN);

#if EDGECARE_CAMERA_PWON_ACTIVE_HIGH
    gpio_bit_set(CAMERA_CTRL_GPIO_PORT, CAMERA_PWON_PIN);
#else
    gpio_bit_reset(CAMERA_CTRL_GPIO_PORT, CAMERA_PWON_PIN);
#endif
    gpio_bit_reset(CAMERA_CTRL_GPIO_PORT, CAMERA_RES_PIN);
    delay_1ms(5U);
    gpio_bit_set(CAMERA_CTRL_GPIO_PORT, CAMERA_RES_PIN);
    delay_1ms(20U);

    printf("camera_sccb: SCL=PF1 SDA=PF0 RES=PD0 PWON=PD1 PWON_active_high=%u\r\n",
           (unsigned int)EDGECARE_CAMERA_PWON_ACTIVE_HIGH);

    i2c_deinit(CAMERA_SCCB);
    i2c_timing_config(CAMERA_SCCB, 0U, 6U, 0U);
    i2c_master_clock_config(CAMERA_SCCB, 0x26U, 0x73U);
    i2c_address_config(CAMERA_SCCB, 0x00U, I2C_ADDFORMAT_7BITS);
    i2c_enable(CAMERA_SCCB);
}

static uint8_t camera_sccb_read_reg8(uint8_t reg, uint8_t *data)
{
    if(SET == i2c_flag_get(CAMERA_SCCB, I2C_FLAG_I2CBSY)) {
        return 0U;
    }

    i2c_master_addressing(CAMERA_SCCB, CAMERA_SCCB_ADDR, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(CAMERA_SCCB, 0x1U);
    i2c_automatic_end_enable(CAMERA_SCCB);
    i2c_start_on_bus(CAMERA_SCCB);

    i2c_data_transmit(CAMERA_SCCB, reg);
    if(!camera_wait_flag(I2C_FLAG_TI)) {
        return 0U;
    }
    if(!camera_wait_flag(I2C_FLAG_STPDET)) {
        return 0U;
    }
    i2c_flag_clear(CAMERA_SCCB, I2C_FLAG_STPDET);

    i2c_master_addressing(CAMERA_SCCB, CAMERA_SCCB_ADDR, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(CAMERA_SCCB, 0x1U);
    i2c_automatic_end_enable(CAMERA_SCCB);
    delay_1ms(1U);
    i2c_start_on_bus(CAMERA_SCCB);

    if(!camera_wait_flag(I2C_FLAG_RBNE)) {
        return 0U;
    }

    *data = (uint8_t)i2c_data_receive(CAMERA_SCCB);

    if(!camera_wait_flag(I2C_FLAG_STPDET)) {
        return 0U;
    }
    i2c_flag_clear(CAMERA_SCCB, I2C_FLAG_STPDET);

    return 1U;
}

static uint8_t camera_sccb_read_reg16(uint16_t reg, uint8_t *data)
{
    if(SET == i2c_flag_get(CAMERA_SCCB, I2C_FLAG_I2CBSY)) {
        return 0U;
    }

    i2c_master_addressing(CAMERA_SCCB, CAMERA_SCCB_ADDR, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(CAMERA_SCCB, 0x2U);
    i2c_automatic_end_enable(CAMERA_SCCB);
    i2c_start_on_bus(CAMERA_SCCB);

    i2c_data_transmit(CAMERA_SCCB, (uint8_t)(reg >> 8U));
    if(!camera_wait_flag(I2C_FLAG_TI)) {
        return 0U;
    }

    i2c_data_transmit(CAMERA_SCCB, (uint8_t)(reg & 0xFFU));
    if(!camera_wait_flag(I2C_FLAG_TI)) {
        return 0U;
    }

    if(!camera_wait_flag(I2C_FLAG_STPDET)) {
        return 0U;
    }
    i2c_flag_clear(CAMERA_SCCB, I2C_FLAG_STPDET);

    i2c_master_addressing(CAMERA_SCCB, CAMERA_SCCB_ADDR, I2C_MASTER_RECEIVE);
    i2c_transfer_byte_number_config(CAMERA_SCCB, 0x1U);
    i2c_automatic_end_enable(CAMERA_SCCB);
    delay_1ms(1U);
    i2c_start_on_bus(CAMERA_SCCB);

    if(!camera_wait_flag(I2C_FLAG_RBNE)) {
        return 0U;
    }

    *data = (uint8_t)i2c_data_receive(CAMERA_SCCB);

    if(!camera_wait_flag(I2C_FLAG_STPDET)) {
        return 0U;
    }
    i2c_flag_clear(CAMERA_SCCB, I2C_FLAG_STPDET);

    return 1U;
}

static uint8_t camera_sccb_write_reg16(uint16_t reg, uint8_t data)
{
    if(SET == i2c_flag_get(CAMERA_SCCB, I2C_FLAG_I2CBSY)) {
        return 0U;
    }

    i2c_master_addressing(CAMERA_SCCB, CAMERA_SCCB_ADDR, I2C_MASTER_TRANSMIT);
    i2c_transfer_byte_number_config(CAMERA_SCCB, 0x3U);
    i2c_automatic_end_enable(CAMERA_SCCB);
    i2c_start_on_bus(CAMERA_SCCB);

    i2c_data_transmit(CAMERA_SCCB, (uint8_t)(reg >> 8U));
    if(!camera_wait_flag(I2C_FLAG_TI)) {
        return 0U;
    }

    i2c_data_transmit(CAMERA_SCCB, (uint8_t)(reg & 0xFFU));
    if(!camera_wait_flag(I2C_FLAG_TI)) {
        return 0U;
    }

    i2c_data_transmit(CAMERA_SCCB, data);
    if(!camera_wait_flag(I2C_FLAG_TI)) {
        return 0U;
    }

    if(!camera_wait_flag(I2C_FLAG_STPDET)) {
        return 0U;
    }
    i2c_flag_clear(CAMERA_SCCB, I2C_FLAG_STPDET);

    return 1U;
}

static void camera_log_dvp_registers(const char *stage)
{
    uint8_t reg_3007 = 0U;
    uint8_t reg_300e = 0U;
    uint8_t reg_301a = 0U;
    uint8_t reg_3019 = 0U;
    uint8_t reg_301b = 0U;
    uint8_t reg_3017 = 0U;
    uint8_t reg_3018 = 0U;
    uint8_t reg_301d = 0U;
    uint8_t reg_302e = 0U;
    uint8_t reg_4300 = 0U;
    uint8_t reg_501f = 0U;
    uint8_t reg_4709 = 0U;
    uint8_t reg_470a = 0U;
    uint8_t reg_470b = 0U;
    uint8_t reg_460b = 0U;
    uint8_t reg_460c = 0U;
    uint8_t reg_3824 = 0U;
    uint8_t reg_4713 = 0U;
    uint8_t reg_471b = 0U;
    uint8_t reg_471c = 0U;
    uint8_t reg_471d = 0U;
    uint8_t reg_471f = 0U;
    uint8_t reg_4730 = 0U;
    uint8_t reg_4740 = 0U;
    uint8_t reg_4741 = 0U;
    uint8_t reg_4745 = 0U;
    uint8_t reg_503d = 0U;
    uint8_t reg_3051 = 0U;

    (void)camera_sccb_read_reg16(0x3007U, &reg_3007);
    (void)camera_sccb_read_reg16(0x300EU, &reg_300e);
    (void)camera_sccb_read_reg16(0x3019U, &reg_3019);
    (void)camera_sccb_read_reg16(0x301AU, &reg_301a);
    (void)camera_sccb_read_reg16(0x301BU, &reg_301b);
    (void)camera_sccb_read_reg16(0x3017U, &reg_3017);
    (void)camera_sccb_read_reg16(0x3018U, &reg_3018);
    (void)camera_sccb_read_reg16(0x301DU, &reg_301d);
    (void)camera_sccb_read_reg16(0x302EU, &reg_302e);
    (void)camera_sccb_read_reg16(0x4300U, &reg_4300);
    (void)camera_sccb_read_reg16(0x501FU, &reg_501f);
    (void)camera_sccb_read_reg16(0x4709U, &reg_4709);
    (void)camera_sccb_read_reg16(0x470AU, &reg_470a);
    (void)camera_sccb_read_reg16(0x470BU, &reg_470b);
    (void)camera_sccb_read_reg16(0x460BU, &reg_460b);
    (void)camera_sccb_read_reg16(0x460CU, &reg_460c);
    (void)camera_sccb_read_reg16(0x3824U, &reg_3824);
    (void)camera_sccb_read_reg16(0x4713U, &reg_4713);
    (void)camera_sccb_read_reg16(0x471BU, &reg_471b);
    (void)camera_sccb_read_reg16(0x471CU, &reg_471c);
    (void)camera_sccb_read_reg16(0x471DU, &reg_471d);
    (void)camera_sccb_read_reg16(0x471FU, &reg_471f);
    (void)camera_sccb_read_reg16(0x4730U, &reg_4730);
    (void)camera_sccb_read_reg16(0x4740U, &reg_4740);
    (void)camera_sccb_read_reg16(0x4741U, &reg_4741);
    (void)camera_sccb_read_reg16(0x4745U, &reg_4745);
    (void)camera_sccb_read_reg16(0x503DU, &reg_503d);
    (void)camera_sccb_read_reg16(0x3051U, &reg_3051);

    printf("camera_dvp_regs[%s]: 300e=0x%02X 301a=0x%02X 3017=0x%02X 3018=0x%02X 301d=0x%02X 3051=0x%02X 4300=0x%02X 501f=0x%02X\r\n",
           stage,
           reg_300e,
           reg_301a,
           reg_3017,
           reg_3018,
           reg_301d,
           reg_3051,
           reg_4300,
           reg_501f);
    printf("camera_dvp_regs[%s]: 460b=0x%02X 460c=0x%02X 3824=0x%02X 4713=0x%02X 471b=0x%02X 471d=0x%02X 4730=0x%02X 4740=0x%02X 4741=0x%02X 4745=0x%02X 503d=0x%02X\r\n",
           stage,
           reg_460b,
           reg_460c,
           reg_3824,
           reg_4713,
           reg_471b,
           reg_471d,
           reg_4730,
           reg_4740,
           reg_4741,
           reg_4745,
           reg_503d);
    printf("camera_dvp_ext_regs[%s]: 3007=0x%02X 3019=0x%02X 301b=0x%02X 302e=0x%02X 4709=0x%02X 470a=0x%02X 470b=0x%02X 471c=0x%02X 471f=0x%02X\r\n",
           stage,
           reg_3007,
           reg_3019,
           reg_301b,
           reg_302e,
           reg_4709,
           reg_470a,
           reg_470b,
           reg_471c,
           reg_471f);
}

uint8_t bsp_camera_ov5640_id_probe(void)
{
    uint8_t ov5640_id_h = 0U;
    uint8_t ov5640_id_l = 0U;
    uint8_t ov2640_id_h = 0U;
    uint8_t ov2640_id_l = 0U;
    uint8_t ov5640_detected = 0U;

    camera_sccb_init();

    if(camera_sccb_read_reg16(0x300AU, &ov5640_id_h) &&
       camera_sccb_read_reg16(0x300BU, &ov5640_id_l)) {
        printf("camera_id: ov5640_regs[0x300A,0x300B]=0x%02X 0x%02X\r\n",
               ov5640_id_h, ov5640_id_l);
        ov5640_detected = ((0x56U == ov5640_id_h) && (0x40U == ov5640_id_l)) ? 1U : 0U;
    } else {
        printf("camera_id: OV5640 ID read timeout/no ack on PF1/PF0\r\n");
    }

    if((0U == ov5640_detected) &&
       camera_sccb_read_reg8(0x0AU, &ov2640_id_h) &&
       camera_sccb_read_reg8(0x0BU, &ov2640_id_l)) {
        printf("camera_id: ov2640_regs[0x0A,0x0B]=0x%02X 0x%02X\r\n",
               ov2640_id_h, ov2640_id_l);
    }

    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TC)) {
    }

    return ov5640_detected;
}

uint8_t bsp_camera_ov5640_qvga_stream_init(void)
{
    const camera_reg8_t *init_regs;
    uint32_t init_reg_count;
    const char *init_path;
    uint32_t i;
    uint8_t stream_ctl = 0U;
    uint8_t width_h = 0U;
    uint8_t width_l = 0U;
    uint8_t height_h = 0U;
    uint8_t height_l = 0U;
    uint8_t polarity = 0U;
    uint8_t test_pattern_503d = 0U;
    uint8_t test_pattern_4741 = 0U;
    uint8_t vfifo = 0U;
    uint8_t pclk_div = 0U;

#if EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT
    init_regs = g_ov5640_vga_yuv_reference_regs;
    init_reg_count = sizeof(g_ov5640_vga_yuv_reference_regs) / sizeof(g_ov5640_vga_yuv_reference_regs[0]);
    init_path = "full_reference_vga_then_qvga";
#else
    init_regs = g_ov5640_qvga_yuv_probe_regs;
    init_reg_count = sizeof(g_ov5640_qvga_yuv_probe_regs) / sizeof(g_ov5640_qvga_yuv_probe_regs[0]);
    init_path = "minimal_probe_qvga";
#endif

    printf("camera_init: ov5640 qvga yuv probe regs=%lu source=OV5640 app note 13.1.1/13.1.2 camera_init_path=%s\r\n",
           (unsigned long)init_reg_count,
           init_path);

    if(!camera_sccb_write_reg16(0x3008U, 0x82U)) {
        printf("camera_init: soft_reset write failed\r\n");
        return 0U;
    }
    delay_1ms(10U);

    for(i = 0U; i < init_reg_count; i++) {
        if(!camera_sccb_write_reg16(init_regs[i].reg, init_regs[i].value)) {
            printf("camera_init: write_failed reg=0x%04lX value=0x%02X index=%lu path=%s\r\n",
                   (unsigned long)init_regs[i].reg,
                   init_regs[i].value,
                   (unsigned long)i,
                   init_path);
            return 0U;
        }
        if(0x3008U == init_regs[i].reg) {
            delay_1ms(5U);
        }
    }

#if EDGECARE_ENABLE_OV5640_FULL_REFERENCE_INIT
    for(i = 0U; i < (sizeof(g_ov5640_qvga_output_regs) / sizeof(g_ov5640_qvga_output_regs[0])); i++) {
        if(!camera_sccb_write_reg16(g_ov5640_qvga_output_regs[i].reg,
                                    g_ov5640_qvga_output_regs[i].value)) {
            printf("camera_init: qvga_override_failed reg=0x%04lX value=0x%02X index=%lu\r\n",
                   (unsigned long)g_ov5640_qvga_output_regs[i].reg,
                   g_ov5640_qvga_output_regs[i].value,
                   (unsigned long)i);
            return 0U;
        }
    }
#endif

#if EDGECARE_ENABLE_CAMERA_SLOW_PCLK
    (void)camera_sccb_write_reg16(0x460CU, 0x22U);
    (void)camera_sccb_write_reg16(0x3824U, EDGECARE_CAMERA_PCLK_DIV_REG);
#endif
    (void)camera_sccb_write_reg16(0x4730U, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, EDGECARE_CAMERA_DVP_POLARITY_REG);
    (void)camera_sccb_write_reg16(0x4745U, EDGECARE_CAMERA_DATA_ORDER_DEFAULT);

#if EDGECARE_ENABLE_CAMERA_TEST_PATTERN
#if EDGECARE_CAMERA_TEST_PATTERN_MODE == 1U
    (void)camera_sccb_write_reg16(0x503DU, 0x80U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#elif EDGECARE_CAMERA_TEST_PATTERN_MODE == 2U
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x05U);
#else
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#endif
#else
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#endif

    delay_1ms(100U);

    (void)camera_sccb_read_reg16(0x3008U, &stream_ctl);
    (void)camera_sccb_read_reg16(0x3808U, &width_h);
    (void)camera_sccb_read_reg16(0x3809U, &width_l);
    (void)camera_sccb_read_reg16(0x380AU, &height_h);
    (void)camera_sccb_read_reg16(0x380BU, &height_l);
    (void)camera_sccb_read_reg16(0x4740U, &polarity);
    (void)camera_sccb_read_reg16(0x503DU, &test_pattern_503d);
    (void)camera_sccb_read_reg16(0x4741U, &test_pattern_4741);
    (void)camera_sccb_read_reg16(0x460CU, &vfifo);
    (void)camera_sccb_read_reg16(0x3824U, &pclk_div);
    printf("camera_init: readback 3008=0x%02X size=%ux%u polarity_4740=0x%02X test_503d=0x%02X test_4741=0x%02X vfifo_460c=0x%02X pclkdiv_3824=0x%02X\r\n",
           stream_ctl,
           (uint16_t)(((uint16_t)width_h << 8U) | width_l),
           (uint16_t)(((uint16_t)height_h << 8U) | height_l),
           polarity,
           test_pattern_503d,
           test_pattern_4741,
           vfifo,
           pclk_div);
    camera_log_dvp_registers("after_init");

    return 1U;
}

static camera_dvp_gpio_sample_t camera_dvp_gpio_measure(uint32_t bursts, uint32_t samples_per_burst)
{
    uint32_t burst;
    uint32_t i;
    camera_dvp_gpio_sample_t sample = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U};
    FlagStatus pclk_prev;
    FlagStatus href_prev;
    FlagStatus sync_prev;
    FlagStatus pclk_now;
    FlagStatus href_now;
    FlagStatus sync_now;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_7);

    pclk_prev = gpio_input_bit_get(GPIOA, GPIO_PIN_6);
    href_prev = gpio_input_bit_get(GPIOA, GPIO_PIN_4);
    sync_prev = gpio_input_bit_get(GPIOB, GPIO_PIN_7);

    for(burst = 0U; burst < bursts; burst++) {
        for(i = 0U; i < samples_per_burst; i++) {
            pclk_now = gpio_input_bit_get(GPIOA, GPIO_PIN_6);
            href_now = gpio_input_bit_get(GPIOA, GPIO_PIN_4);
            sync_now = gpio_input_bit_get(GPIOB, GPIO_PIN_7);

            if(SET == href_now) {
                sample.href_high_samples++;
            }
            if(SET == sync_now) {
                sample.sync_high_samples++;
            }

            if(pclk_now != pclk_prev) {
                sample.pclk_edges++;
                pclk_prev = pclk_now;
            }
            if(href_now != href_prev) {
                sample.href_edges++;
                href_prev = href_now;
            }
            if(sync_now != sync_prev) {
                sample.sync_edges++;
                sync_prev = sync_now;
            }
        }
    }

    sample.pclk_level = (SET == pclk_prev) ? 1U : 0U;
    sample.href_level = (SET == href_prev) ? 1U : 0U;
    sample.sync_level = (SET == sync_prev) ? 1U : 0U;

    return sample;
}

static void camera_dvp_gpio_activity_probe(void)
{
    camera_dvp_gpio_sample_t sample = camera_dvp_gpio_measure(80U, 50000U);

    printf("camera_dvp_gpio: bursts=80 samples_per_burst=50000 pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu pclk=%u href=%u sync=%u\r\n",
           (unsigned long)sample.pclk_edges,
           (unsigned long)sample.href_edges,
           (unsigned long)sample.sync_edges,
           (unsigned long)sample.href_high_samples,
           (unsigned long)sample.sync_high_samples,
           sample.pclk_level,
           sample.href_level,
           sample.sync_level);
}

static void camera_sync_sweep_log(const char *tag, uint8_t href_value, uint8_t vsync_value)
{
    camera_dvp_gpio_sample_t sample = camera_dvp_gpio_measure(8U, 20000U);

    printf("camera_sync_sweep[%s]: forced_href=%u forced_vsync=%u samples=%lu pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu pclk=%u href=%u sync=%u\r\n",
           tag,
           href_value,
           vsync_value,
           (unsigned long)(8U * 20000U),
           (unsigned long)sample.pclk_edges,
           (unsigned long)sample.href_edges,
           (unsigned long)sample.sync_edges,
           (unsigned long)sample.href_high_samples,
           (unsigned long)sample.sync_high_samples,
           sample.pclk_level,
           sample.href_level,
           sample.sync_level);
}

#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
static void camera_forced_sync_dci_probe(const char *tag, uint8_t href_value, uint8_t vsync_value)
{
    dci_parameter_struct dci_struct;
    camera_dci_status_probe_t probe;
    uint32_t i;

    camera_dci_gpio_init();
    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_DMAMUX);

    dci_deinit();
    dci_struct.capture_mode = DCI_CAPTURE_MODE_CONTINUOUS;
    dci_struct.clock_polarity = DCI_CK_POLARITY_FALLING;
    dci_struct.hsync_polarity = DCI_HSYNC_POLARITY_LOW;
    dci_struct.vsync_polarity = DCI_VSYNC_POLARITY_HIGH;
    dci_struct.frame_rate = DCI_FRAME_RATE_ALL;
    dci_struct.interface_format = DCI_INTERFACE_FORMAT_8BITS;
    dci_init(&dci_struct);

    dci_enable();
    dci_capture_enable();
    camera_dci_status_probe_begin(&probe);
    camera_dci_dma_summary_log(tag);
    for(i = 0U; i < 512U; i++) {
        camera_dci_status_probe_poll(&probe);
    }
    dci_capture_disable();
    dci_disable();

    printf("camera_forced_sync_dci[%s]: forced_href=%u forced_vsync=%u polls=%lu stat0=%08lX->%08lX chg=%lu hs_polls=%lu vs_polls=%lu fv_polls=%lu ctl=%08lX->%08lX\r\n",
           tag,
           href_value,
           vsync_value,
           (unsigned long)probe.polls,
           (unsigned long)probe.stat0_first,
           (unsigned long)probe.stat0_last,
           (unsigned long)probe.stat0_changes,
           (unsigned long)probe.hs_high_polls,
           (unsigned long)probe.vs_high_polls,
           (unsigned long)probe.fv_high_polls,
           (unsigned long)probe.ctl_first,
           (unsigned long)probe.ctl_last);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
}
#endif

static void camera_ov5640_sync_output_sweep(void)
{
#if EDGECARE_ENABLE_CAMERA_SYNC_OUTPUT_SWEEP
    uint8_t saved_3017 = 0U;
    uint8_t saved_301a = 0U;
    uint8_t saved_301d = 0U;
    uint8_t saved_3051 = 0U;
    uint8_t read_3017 = 0U;
    uint8_t read_301a = 0U;
    uint8_t read_301d = 0U;
    static const uint8_t force_values[] = {
        0x00U,
        0x20U,
        0x40U,
        0x60U
    };
    static const char *const force_tags[] = {
        "force_h0_v0",
        "force_h1_v0",
        "force_h0_v1",
        "force_h1_v1"
    };
    uint32_t i;

    (void)camera_sccb_read_reg16(0x3017U, &saved_3017);
    (void)camera_sccb_read_reg16(0x301AU, &saved_301a);
    (void)camera_sccb_read_reg16(0x301DU, &saved_301d);
    (void)camera_sccb_read_reg16(0x3051U, &saved_3051);

    printf("camera_sync_sweep: enabled source=OV5640_datasheet_3017_301d_301a saved_3017=0x%02X saved_301a=0x%02X saved_301d=0x%02X saved_3051=0x%02X note=PA4_reports_href PB7_reports_sync\r\n",
           saved_3017,
           saved_301a,
           saved_301d,
           saved_3051);

    camera_sync_sweep_log("normal_before", 2U, 2U);

    (void)camera_sccb_write_reg16(0x3017U, (uint8_t)(saved_3017 | 0x70U));
    (void)camera_sccb_read_reg16(0x3017U, &read_3017);
    printf("camera_sync_sweep: set_output_dir read_3017=0x%02X mask_vs_href_pclk=0x70\r\n", read_3017);
    delay_1ms(10U);
    camera_sync_sweep_log("normal_output_dir", 2U, 2U);

    (void)camera_sccb_write_reg16(0x301DU, (uint8_t)(saved_301d | 0x60U));
    (void)camera_sccb_read_reg16(0x301DU, &read_301d);
    printf("camera_sync_sweep: set_reg_control read_301d=0x%02X mask_vs_href=0x60\r\n", read_301d);

    for(i = 0U; i < (sizeof(force_values) / sizeof(force_values[0])); i++) {
        uint8_t forced = (uint8_t)((saved_301a & (uint8_t)~0x60U) | force_values[i]);

        (void)camera_sccb_write_reg16(0x301AU, forced);
        delay_1ms(10U);
        (void)camera_sccb_read_reg16(0x301AU, &read_301a);
        printf("camera_sync_sweep: set_output_value tag=%s read_301a=0x%02X\r\n",
               force_tags[i],
               read_301a);
        camera_sync_sweep_log(force_tags[i],
                              (0U != (force_values[i] & 0x20U)) ? 1U : 0U,
                              (0U != (force_values[i] & 0x40U)) ? 1U : 0U);
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
        camera_forced_sync_dci_probe(force_tags[i],
                                     (0U != (force_values[i] & 0x20U)) ? 1U : 0U,
                                     (0U != (force_values[i] & 0x40U)) ? 1U : 0U);
#endif
    }

    (void)camera_sccb_write_reg16(0x301AU, saved_301a);
    (void)camera_sccb_write_reg16(0x301DU, saved_301d);
    (void)camera_sccb_write_reg16(0x3017U, saved_3017);
    (void)camera_sccb_write_reg16(0x3051U, saved_3051);
    delay_1ms(20U);
    camera_sync_sweep_log("restored", 2U, 2U);
#endif
}

static uint32_t camera_timing_sweep_score(const camera_dvp_gpio_sample_t *sample, uint32_t total_samples)
{
    uint32_t score;
    uint32_t href_low_samples;
    uint32_t href_balance;

    if(0U == sample->pclk_edges) {
        return 0U;
    }

    href_low_samples = total_samples - sample->href_high_samples;
    href_balance = (sample->href_high_samples < href_low_samples) ? sample->href_high_samples : href_low_samples;

    score = sample->pclk_edges / 64U;
    score += sample->href_edges * 20U;
    score += sample->sync_edges * 80U;
    score += href_balance / 128U;

    if(sample->href_edges >= 100U) {
        score += 5000U;
    }
    if(sample->sync_edges > 0U) {
        score += 2000U;
    }
    if((sample->href_high_samples > 0U) && (sample->href_high_samples < total_samples)) {
        score += 500U;
    }

    return score;
}

static void camera_ov5640_timing_sweep(void)
{
#if EDGECARE_ENABLE_CAMERA_TIMING_SWEEP
    static const camera_timing_variant_t variants[] = {
        {"href_default_vsync0", 0x02U, 0x00U, 0x00U, 0x20U},
        {"href_default_vsync1", 0x02U, 0x01U, 0x00U, 0x20U},
        {"hsync_enable_vsync0", 0x03U, 0x00U, 0x00U, 0x20U},
        {"hsync_enable_vsync1", 0x03U, 0x01U, 0x00U, 0x20U},
        {"href_pol_low", 0x02U, 0x00U, 0x00U, 0x22U},
        {"vsync_pol_high", 0x02U, 0x00U, 0x00U, 0x21U},
        {"no_pclk_fall", 0x02U, 0x00U, 0x00U, 0x00U},
        {"pclk_gate_href", 0x02U, 0x00U, 0x00U, 0x24U}
    };
    const uint32_t bursts = 8U;
    const uint32_t samples_per_burst = 20000U;
    const uint32_t total_samples = bursts * samples_per_burst;
    uint8_t saved_471b = 0U;
    uint8_t saved_471d = 0U;
    uint8_t saved_4730 = 0U;
    uint8_t saved_4740 = 0U;
    camera_dvp_gpio_sample_t baseline;
    camera_dvp_gpio_sample_t best_sample;
    uint32_t baseline_score;
    uint32_t best_score;
    uint32_t best_index = 0U;
    uint8_t best_valid = 0U;
    uint8_t apply_best = 0U;
    const char *apply_reason = "none";
    uint32_t i;

    (void)camera_sccb_read_reg16(0x471BU, &saved_471b);
    (void)camera_sccb_read_reg16(0x471DU, &saved_471d);
    (void)camera_sccb_read_reg16(0x4730U, &saved_4730);
    (void)camera_sccb_read_reg16(0x4740U, &saved_4740);

    baseline = camera_dvp_gpio_measure(bursts, samples_per_burst);
    baseline_score = camera_timing_sweep_score(&baseline, total_samples);
    best_sample = baseline;
    best_score = baseline_score;

    printf("camera_timing_sweep: enabled source=OV5640_datasheet_471b_471d_4730_4740 saved_471b=0x%02X saved_471d=0x%02X saved_4730=0x%02X saved_4740=0x%02X samples=%lu\r\n",
           saved_471b,
           saved_471d,
           saved_4730,
           saved_4740,
           (unsigned long)total_samples);
    printf("camera_timing_sweep[baseline]: 471b=0x%02X 471d=0x%02X 4730=0x%02X 4740=0x%02X pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu score=%lu pclk=%u href=%u sync=%u\r\n",
           saved_471b,
           saved_471d,
           saved_4730,
           saved_4740,
           (unsigned long)baseline.pclk_edges,
           (unsigned long)baseline.href_edges,
           (unsigned long)baseline.sync_edges,
           (unsigned long)baseline.href_high_samples,
           (unsigned long)baseline.sync_high_samples,
           (unsigned long)baseline_score,
           baseline.pclk_level,
           baseline.href_level,
           baseline.sync_level);

    for(i = 0U; i < (sizeof(variants) / sizeof(variants[0])); i++) {
        camera_dvp_gpio_sample_t sample;
        uint32_t score;

        (void)camera_sccb_write_reg16(0x471BU, variants[i].reg_471b);
        (void)camera_sccb_write_reg16(0x471DU, variants[i].reg_471d);
        (void)camera_sccb_write_reg16(0x4730U, variants[i].reg_4730);
        (void)camera_sccb_write_reg16(0x4740U, variants[i].reg_4740);
        delay_1ms(20U);

        sample = camera_dvp_gpio_measure(bursts, samples_per_burst);
        score = camera_timing_sweep_score(&sample, total_samples);

        printf("camera_timing_sweep[%s]: 471b=0x%02X 471d=0x%02X 4730=0x%02X 4740=0x%02X pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu score=%lu pclk=%u href=%u sync=%u\r\n",
               variants[i].tag,
               variants[i].reg_471b,
               variants[i].reg_471d,
               variants[i].reg_4730,
               variants[i].reg_4740,
               (unsigned long)sample.pclk_edges,
               (unsigned long)sample.href_edges,
               (unsigned long)sample.sync_edges,
               (unsigned long)sample.href_high_samples,
               (unsigned long)sample.sync_high_samples,
               (unsigned long)score,
               sample.pclk_level,
               sample.href_level,
               sample.sync_level);

        if(score > best_score) {
            best_valid = 1U;
            best_index = i;
            best_score = score;
            best_sample = sample;
        }
    }

    if((0U != best_valid) &&
       (best_sample.href_edges >= CAMERA_TIMING_SWEEP_MIN_HREF_EDGES) &&
       (best_score > (baseline_score + 100U)) &&
       ((best_sample.href_edges > (baseline.href_edges + 20U)) ||
        (best_sample.sync_edges > baseline.sync_edges))) {
        apply_best = 1U;
        apply_reason = "href_edges";
#if EDGECARE_ENABLE_CAMERA_APPLY_VSYNC_EDGE_TIMING
    } else if((0U != best_valid) &&
              (best_sample.sync_edges > baseline.sync_edges) &&
              (best_sample.sync_edges >= 2U) &&
              (best_score > (baseline_score + 100U))) {
        apply_best = 1U;
        apply_reason = "vsync_edges_diag";
#endif
    } else {
        apply_best = 0U;
    }

    if(0U != apply_best) {
        (void)camera_sccb_write_reg16(0x471BU, variants[best_index].reg_471b);
        (void)camera_sccb_write_reg16(0x471DU, variants[best_index].reg_471d);
        (void)camera_sccb_write_reg16(0x4730U, variants[best_index].reg_4730);
        (void)camera_sccb_write_reg16(0x4740U, variants[best_index].reg_4740);
        delay_1ms(20U);
        printf("camera_timing_sweep_best: tag=%s apply=1 reason=%s baseline_score=%lu best_score=%lu href_edges=%lu sync_edges=%lu\r\n",
               variants[best_index].tag,
               apply_reason,
               (unsigned long)baseline_score,
               (unsigned long)best_score,
               (unsigned long)best_sample.href_edges,
               (unsigned long)best_sample.sync_edges);
    } else {
        (void)camera_sccb_write_reg16(0x471BU, saved_471b);
        (void)camera_sccb_write_reg16(0x471DU, saved_471d);
        (void)camera_sccb_write_reg16(0x4730U, saved_4730);
        (void)camera_sccb_write_reg16(0x4740U, saved_4740);
        delay_1ms(20U);
        printf("camera_timing_sweep_best: tag=none apply=0 baseline_score=%lu best_score=%lu href_edges=%lu sync_edges=%lu\r\n",
               (unsigned long)baseline_score,
               (unsigned long)best_score,
               (unsigned long)best_sample.href_edges,
               (unsigned long)best_sample.sync_edges);
    }
#endif
}

static void camera_ov5640_dvp_mode_sweep(void)
{
#if EDGECARE_ENABLE_CAMERA_DVP_MODE_SWEEP
    static const camera_dvp_mode_variant_t variants[] = {
        {"app_note_ref", 0x58U, 0x00U, 0x03U},
        {"linux_302e08", 0x58U, 0x08U, 0x03U},
        {"jpeg_mode2", 0x58U, 0x00U, 0x02U},
        {"jpeg_mode2_302e08", 0x58U, 0x08U, 0x02U},
        {"mipi_ctrl40", 0x40U, 0x00U, 0x03U},
        {"mipi_ctrl40_302e08", 0x40U, 0x08U, 0x03U}
    };
    const uint32_t bursts = 8U;
    const uint32_t samples_per_burst = 20000U;
    const uint32_t total_samples = bursts * samples_per_burst;
    uint8_t saved_300e = 0U;
    uint8_t saved_302e = 0U;
    uint8_t saved_4713 = 0U;
    camera_dvp_gpio_sample_t baseline;
    camera_dvp_gpio_sample_t best_sample;
    uint32_t baseline_score;
    uint32_t best_score;
    uint32_t best_index = 0U;
    uint8_t best_valid = 0U;
    uint32_t i;

    (void)camera_sccb_read_reg16(0x300EU, &saved_300e);
    (void)camera_sccb_read_reg16(0x302EU, &saved_302e);
    (void)camera_sccb_read_reg16(0x4713U, &saved_4713);

    baseline = camera_dvp_gpio_measure(bursts, samples_per_burst);
    baseline_score = camera_timing_sweep_score(&baseline, total_samples);
    best_sample = baseline;
    best_score = baseline_score;

    printf("camera_dvp_mode_sweep: enabled source=OV5640_300e_302e_4713 saved_300e=0x%02X saved_302e=0x%02X saved_4713=0x%02X samples=%lu\r\n",
           saved_300e,
           saved_302e,
           saved_4713,
           (unsigned long)total_samples);
    printf("camera_dvp_mode_sweep[baseline]: 300e=0x%02X 302e=0x%02X 4713=0x%02X pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu score=%lu pclk=%u href=%u sync=%u\r\n",
           saved_300e,
           saved_302e,
           saved_4713,
           (unsigned long)baseline.pclk_edges,
           (unsigned long)baseline.href_edges,
           (unsigned long)baseline.sync_edges,
           (unsigned long)baseline.href_high_samples,
           (unsigned long)baseline.sync_high_samples,
           (unsigned long)baseline_score,
           baseline.pclk_level,
           baseline.href_level,
           baseline.sync_level);

    for(i = 0U; i < (sizeof(variants) / sizeof(variants[0])); i++) {
        camera_dvp_gpio_sample_t sample;
        uint32_t score;
        uint8_t read_300e = 0U;
        uint8_t read_302e = 0U;
        uint8_t read_4713 = 0U;

        (void)camera_sccb_write_reg16(0x300EU, variants[i].reg_300e);
        (void)camera_sccb_write_reg16(0x302EU, variants[i].reg_302e);
        (void)camera_sccb_write_reg16(0x4713U, variants[i].reg_4713);
        delay_1ms(20U);

        (void)camera_sccb_read_reg16(0x300EU, &read_300e);
        (void)camera_sccb_read_reg16(0x302EU, &read_302e);
        (void)camera_sccb_read_reg16(0x4713U, &read_4713);
        sample = camera_dvp_gpio_measure(bursts, samples_per_burst);
        score = camera_timing_sweep_score(&sample, total_samples);

        printf("camera_dvp_mode_sweep[%s]: 300e=0x%02X/read0x%02X 302e=0x%02X/read0x%02X 4713=0x%02X/read0x%02X pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu score=%lu pclk=%u href=%u sync=%u\r\n",
               variants[i].tag,
               variants[i].reg_300e,
               read_300e,
               variants[i].reg_302e,
               read_302e,
               variants[i].reg_4713,
               read_4713,
               (unsigned long)sample.pclk_edges,
               (unsigned long)sample.href_edges,
               (unsigned long)sample.sync_edges,
               (unsigned long)sample.href_high_samples,
               (unsigned long)sample.sync_high_samples,
               (unsigned long)score,
               sample.pclk_level,
               sample.href_level,
               sample.sync_level);

        if(score > best_score) {
            best_valid = 1U;
            best_index = i;
            best_score = score;
            best_sample = sample;
        }
    }

    if((0U != best_valid) &&
       (best_sample.href_edges >= CAMERA_TIMING_SWEEP_MIN_HREF_EDGES) &&
       (best_score > (baseline_score + 100U)) &&
       (best_sample.href_edges > (baseline.href_edges + 20U))) {
        (void)camera_sccb_write_reg16(0x300EU, variants[best_index].reg_300e);
        (void)camera_sccb_write_reg16(0x302EU, variants[best_index].reg_302e);
        (void)camera_sccb_write_reg16(0x4713U, variants[best_index].reg_4713);
        delay_1ms(20U);
        printf("camera_dvp_mode_sweep_best: tag=%s apply=1 baseline_score=%lu best_score=%lu href_edges=%lu sync_edges=%lu\r\n",
               variants[best_index].tag,
               (unsigned long)baseline_score,
               (unsigned long)best_score,
               (unsigned long)best_sample.href_edges,
               (unsigned long)best_sample.sync_edges);
    } else {
        (void)camera_sccb_write_reg16(0x300EU, saved_300e);
        (void)camera_sccb_write_reg16(0x302EU, saved_302e);
        (void)camera_sccb_write_reg16(0x4713U, saved_4713);
        delay_1ms(20U);
        printf("camera_dvp_mode_sweep_best: tag=none apply=0 baseline_score=%lu best_score=%lu href_edges=%lu sync_edges=%lu\r\n",
               (unsigned long)baseline_score,
               (unsigned long)best_score,
               (unsigned long)best_sample.href_edges,
               (unsigned long)best_sample.sync_edges);
    }
#endif
}

static uint8_t camera_raw_data_bus_read(void)
{
    uint8_t value = 0U;

    if(SET == gpio_input_bit_get(GPIOC, GPIO_PIN_6)) {
        value |= 0x01U;
    }
    if(SET == gpio_input_bit_get(GPIOC, GPIO_PIN_7)) {
        value |= 0x02U;
    }
    if(SET == gpio_input_bit_get(GPIOC, GPIO_PIN_8)) {
        value |= 0x04U;
    }
    if(SET == gpio_input_bit_get(GPIOG, GPIO_PIN_11)) {
        value |= 0x08U;
    }
    if(SET == gpio_input_bit_get(GPIOC, GPIO_PIN_11)) {
        value |= 0x10U;
    }
    if(SET == gpio_input_bit_get(GPIOB, GPIO_PIN_6)) {
        value |= 0x20U;
    }
    if(SET == gpio_input_bit_get(GPIOE, GPIO_PIN_5)) {
        value |= 0x40U;
    }
    if(SET == gpio_input_bit_get(GPIOB, GPIO_PIN_9)) {
        value |= 0x80U;
    }

    return value;
}

static void camera_data_bus_gpio_input_init(uint32_t pupd)
{
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOG);

    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, pupd, GPIO_PIN_6 | GPIO_PIN_9);
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, pupd, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11);
    gpio_mode_set(GPIOE, GPIO_MODE_INPUT, pupd, GPIO_PIN_5);
    gpio_mode_set(GPIOG, GPIO_MODE_INPUT, pupd, GPIO_PIN_11);
}

static uint8_t camera_data_pad_value_301a(uint8_t expected, uint8_t map_d9_d2)
{
    if(0U != map_d9_d2) {
        return (uint8_t)((expected >> 4U) & 0x0FU);
    }

    return (uint8_t)((expected >> 6U) & 0x03U);
}

static uint8_t camera_data_pad_value_301b(uint8_t expected, uint8_t map_d9_d2)
{
    if(0U != map_d9_d2) {
        return (uint8_t)((expected & 0x0FU) << 4U);
    }

    return (uint8_t)((expected & 0x3FU) << 2U);
}

static void camera_ov5640_data_pad_sweep(void)
{
#if EDGECARE_ENABLE_CAMERA_DATA_PAD_SWEEP
    static const uint8_t patterns[] = {
        0x00U,
        0xFFU,
        0xAAU,
        0x55U,
        0x01U,
        0x02U,
        0x04U,
        0x08U,
        0x10U,
        0x20U,
        0x40U,
        0x80U
    };
    static const uint8_t map_d9_d2_values[] = {
        0U,
        1U
    };
    static const char *const map_tags[] = {
        "pad_d7_d0",
        "pad_d9_d2"
    };
    uint8_t saved_3017 = 0U;
    uint8_t saved_3018 = 0U;
    uint8_t saved_301a = 0U;
    uint8_t saved_301b = 0U;
    uint8_t saved_301d = 0U;
    uint8_t saved_301e = 0U;
    uint8_t read_3017 = 0U;
    uint8_t read_3018 = 0U;
    uint8_t read_301a = 0U;
    uint8_t read_301b = 0U;
    uint8_t read_301d = 0U;
    uint8_t read_301e = 0U;
    uint32_t map_index;
    uint32_t pattern_index;

    camera_data_bus_gpio_input_init(GPIO_PUPD_NONE);

    (void)camera_sccb_read_reg16(0x3017U, &saved_3017);
    (void)camera_sccb_read_reg16(0x3018U, &saved_3018);
    (void)camera_sccb_read_reg16(0x301AU, &saved_301a);
    (void)camera_sccb_read_reg16(0x301BU, &saved_301b);
    (void)camera_sccb_read_reg16(0x301DU, &saved_301d);
    (void)camera_sccb_read_reg16(0x301EU, &saved_301e);

    printf("camera_data_pad_sweep: enabled source=OV5640_datasheet_3017_3018_301a_301b_301d_301e saved_3017=0x%02X saved_3018=0x%02X saved_301a=0x%02X saved_301b=0x%02X saved_301d=0x%02X saved_301e=0x%02X note=read_mcu_D0_D7\r\n",
           saved_3017,
           saved_3018,
           saved_301a,
           saved_301b,
           saved_301d,
           saved_301e);

    (void)camera_sccb_write_reg16(0x3017U, (uint8_t)(saved_3017 | 0x0FU));
    (void)camera_sccb_write_reg16(0x3018U, (uint8_t)(saved_3018 | 0xFCU));
    (void)camera_sccb_write_reg16(0x301DU, (uint8_t)(saved_301d | 0x0FU));
    (void)camera_sccb_write_reg16(0x301EU, (uint8_t)(saved_301e | 0xFCU));
    delay_1ms(10U);
    (void)camera_sccb_read_reg16(0x3017U, &read_3017);
    (void)camera_sccb_read_reg16(0x3018U, &read_3018);
    (void)camera_sccb_read_reg16(0x301DU, &read_301d);
    (void)camera_sccb_read_reg16(0x301EU, &read_301e);
    printf("camera_data_pad_sweep: set_output_dir read_3017=0x%02X read_3018=0x%02X read_301d=0x%02X read_301e=0x%02X mask_d9_d0=3017:0x0F_3018:0xFC_301d:0x0F_301e:0xFC\r\n",
           read_3017,
           read_3018,
           read_301d,
           read_301e);

    for(map_index = 0U; map_index < (sizeof(map_d9_d2_values) / sizeof(map_d9_d2_values[0])); map_index++) {
        uint32_t full_matches = 0U;
        uint32_t strong_matches = 0U;
        uint32_t bit_tests = 0U;
        uint8_t present_mask = 0U;
        uint8_t strong_mask = 0U;
        uint8_t missing_mask = 0U;
        uint8_t extra_mask = 0U;
        uint8_t zero_float_mask = 0U;
        uint8_t ff_missing_mask = 0U;

        for(pattern_index = 0U; pattern_index < (sizeof(patterns) / sizeof(patterns[0])); pattern_index++) {
            uint8_t expected = patterns[pattern_index];
            uint8_t force_301a = (uint8_t)((saved_301a & (uint8_t)~0x0FU) |
                                           camera_data_pad_value_301a(expected, map_d9_d2_values[map_index]));
            uint8_t force_301b = (uint8_t)((saved_301b & (uint8_t)~0xFCU) |
                                           camera_data_pad_value_301b(expected, map_d9_d2_values[map_index]));
            uint8_t read_none;
            uint8_t read_down;
            uint8_t read_up;
            uint8_t match;
            uint8_t strong_match;

            (void)camera_sccb_write_reg16(0x301AU, force_301a);
            (void)camera_sccb_write_reg16(0x301BU, force_301b);
            delay_1ms(5U);
            (void)camera_sccb_read_reg16(0x301AU, &read_301a);
            (void)camera_sccb_read_reg16(0x301BU, &read_301b);
            camera_data_bus_gpio_input_init(GPIO_PUPD_NONE);
            delay_1ms(1U);
            read_none = camera_raw_data_bus_read();
            camera_data_bus_gpio_input_init(GPIO_PUPD_PULLDOWN);
            delay_1ms(1U);
            read_down = camera_raw_data_bus_read();
            camera_data_bus_gpio_input_init(GPIO_PUPD_PULLUP);
            delay_1ms(1U);
            read_up = camera_raw_data_bus_read();
            camera_data_bus_gpio_input_init(GPIO_PUPD_NONE);

            match = (expected == read_none) ? 1U : 0U;
            strong_match = (expected == read_down) ? 1U : 0U;
            if(match != 0U) {
                full_matches++;
            }
            if(strong_match != 0U) {
                strong_matches++;
            }
            if(expected == 0x00U) {
                zero_float_mask = read_none;
            } else if(expected == 0xFFU) {
                ff_missing_mask = (uint8_t)(~read_down);
            } else if((expected & (uint8_t)(expected - 1U)) == 0U) {
                bit_tests++;
                present_mask = (uint8_t)(present_mask | read_down);
                if((read_down & expected) != 0U) {
                    strong_mask = (uint8_t)(strong_mask | expected);
                } else {
                    missing_mask = (uint8_t)(missing_mask | expected);
                }
                extra_mask = (uint8_t)(extra_mask | (uint8_t)(read_down & (uint8_t)~expected));
            }

            printf("camera_data_pad_sweep[%s_%02X]: map=%s expected=0x%02X read=0x%02X match=%u strong_match=%u read_none=0x%02X read_down=0x%02X read_up=0x%02X read_301a=0x%02X read_301b=0x%02X\r\n",
                   map_tags[map_index],
                   expected,
                   map_tags[map_index],
                   expected,
                   read_none,
                   match,
                   strong_match,
                   read_none,
                   read_down,
                   read_up,
                   read_301a,
                   read_301b);
        }

        printf("camera_data_pad_summary[%s]: bit_tests=%lu full_matches=%lu strong_matches=%lu present_mask=0x%02X strong_mask=0x%02X missing_mask=0x%02X extra_mask=0x%02X zero_float_mask=0x%02X ff_missing_mask=0x%02X note=use_strong_mask_for_camera_driven_bits\r\n",
               map_tags[map_index],
               (unsigned long)bit_tests,
               (unsigned long)full_matches,
               (unsigned long)strong_matches,
               present_mask,
               strong_mask,
               missing_mask,
               extra_mask,
               zero_float_mask,
               ff_missing_mask);
    }

    (void)camera_sccb_write_reg16(0x301BU, saved_301b);
    (void)camera_sccb_write_reg16(0x301AU, saved_301a);
    (void)camera_sccb_write_reg16(0x301EU, saved_301e);
    (void)camera_sccb_write_reg16(0x301DU, saved_301d);
    (void)camera_sccb_write_reg16(0x3018U, saved_3018);
    (void)camera_sccb_write_reg16(0x3017U, saved_3017);
    delay_1ms(20U);
    (void)camera_sccb_read_reg16(0x3017U, &read_3017);
    (void)camera_sccb_read_reg16(0x3018U, &read_3018);
    (void)camera_sccb_read_reg16(0x301AU, &read_301a);
    (void)camera_sccb_read_reg16(0x301BU, &read_301b);
    (void)camera_sccb_read_reg16(0x301DU, &read_301d);
    (void)camera_sccb_read_reg16(0x301EU, &read_301e);
    printf("camera_data_pad_sweep: restored 3017=0x%02X/read0x%02X 3018=0x%02X/read0x%02X 301a=0x%02X/read0x%02X 301b=0x%02X/read0x%02X 301d=0x%02X/read0x%02X 301e=0x%02X/read0x%02X\r\n",
           saved_3017,
           read_3017,
           saved_3018,
           read_3018,
           saved_301a,
           read_301a,
           saved_301b,
           read_301b,
           saved_301d,
           read_301d,
           saved_301e,
           read_301e);
#endif
}

static void camera_raw_sample_stats_add(camera_raw_sample_stats_t *stats, uint8_t data)
{
    if(stats->first_count < 4U) {
        stats->first[stats->first_count] = data;
        stats->first_count++;
    }

    stats->last[0] = stats->last[1];
    stats->last[1] = stats->last[2];
    stats->last[2] = stats->last[3];
    stats->last[3] = data;

    if(0U != data) {
        stats->nonzero++;
    }
    if(0U == stats->have_previous) {
        stats->previous_value = data;
        stats->min_value = data;
        stats->max_value = data;
        stats->have_previous = 1U;
    } else {
        if(data != stats->previous_value) {
            stats->changes++;
        }
        stats->previous_value = data;
        if(data < stats->min_value) {
            stats->min_value = data;
        }
        if(data > stats->max_value) {
            stats->max_value = data;
        }
    }

    stats->checksum = ((stats->checksum << 5U) | (stats->checksum >> 27U)) ^ data;
    stats->samples++;
}

static void camera_raw_sample_stats_finish(camera_raw_sample_stats_t *stats)
{
    if(0U == stats->have_previous) {
        stats->min_value = 0U;
    }
}

static void camera_raw_sample_stats_log(const char *tag, const camera_raw_sample_stats_t *stats)
{
    printf("camera_raw_pclk_sample[%s]: samples=%lu nonzero=%lu changes=%lu min=0x%02X max=0x%02X checksum=0x%08lX first=%02X,%02X,%02X,%02X last=%02X,%02X,%02X,%02X\r\n",
           tag,
           (unsigned long)stats->samples,
           (unsigned long)stats->nonzero,
           (unsigned long)stats->changes,
           stats->min_value,
           stats->max_value,
           (unsigned long)stats->checksum,
           stats->first[0],
           stats->first[1],
           stats->first[2],
           stats->first[3],
           stats->last[0],
           stats->last[1],
           stats->last[2],
           stats->last[3]);
}

static void camera_raw_pclk_sample_probe_tag(const char *source_tag,
                                             const char *all_tag,
                                             const char *href_high_tag,
                                             const char *href_low_tag)
{
#if EDGECARE_ENABLE_CAMERA_RAW_PCLK_SAMPLE
    const uint32_t max_edges = 32768U;
    const uint32_t max_loops = 4000000U;
    uint32_t loops = 0U;
    uint32_t pclk_edges = 0U;
    uint32_t href_gated = 0U;
    uint32_t href_low_gated = 0U;
    uint32_t sync_high = 0U;
    camera_raw_sample_stats_t all_stats = {0U};
    camera_raw_sample_stats_t href_high_stats = {0U};
    camera_raw_sample_stats_t href_low_stats = {0U};
    FlagStatus pclk_prev;

    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOG);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_9);
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11);
    gpio_mode_set(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_mode_set(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);

    pclk_prev = gpio_input_bit_get(GPIOA, GPIO_PIN_6);

    while((pclk_edges < max_edges) && (loops < max_loops)) {
        FlagStatus pclk_now = gpio_input_bit_get(GPIOA, GPIO_PIN_6);

        if(pclk_now != pclk_prev) {
            FlagStatus href_now;
            FlagStatus sync_now;
            uint8_t data;

            pclk_prev = pclk_now;
            pclk_edges++;
            href_now = gpio_input_bit_get(GPIOA, GPIO_PIN_4);
            sync_now = gpio_input_bit_get(GPIOB, GPIO_PIN_7);
            data = camera_raw_data_bus_read();

            if(SET == sync_now) {
                sync_high++;
            }

            camera_raw_sample_stats_add(&all_stats, data);
            if(SET == href_now) {
                href_gated++;
                camera_raw_sample_stats_add(&href_high_stats, data);
            } else {
                href_low_gated++;
                camera_raw_sample_stats_add(&href_low_stats, data);
            }
        }
        loops++;
    }

    camera_raw_sample_stats_finish(&all_stats);
    camera_raw_sample_stats_finish(&href_high_stats);
    camera_raw_sample_stats_finish(&href_low_stats);

    if(NULL != source_tag) {
        printf("camera_raw_pclk_sample[%s]: enabled max_edges=%lu max_loops=%lu loops=%lu pclk_edges=%lu href_gated=%lu href_low_gated=%lu sync_high_edges=%lu all_changes=%lu high_changes=%lu low_changes=%lu high_min=0x%02X high_max=0x%02X low_min=0x%02X low_max=0x%02X\r\n",
               source_tag,
               (unsigned long)max_edges,
               (unsigned long)max_loops,
               (unsigned long)loops,
               (unsigned long)pclk_edges,
               (unsigned long)href_gated,
               (unsigned long)href_low_gated,
               (unsigned long)sync_high,
               (unsigned long)all_stats.changes,
               (unsigned long)href_high_stats.changes,
               (unsigned long)href_low_stats.changes,
               href_high_stats.min_value,
               href_high_stats.max_value,
               href_low_stats.min_value,
               href_low_stats.max_value);
    } else {
        printf("camera_raw_pclk_sample: enabled max_edges=%lu max_loops=%lu loops=%lu pclk_edges=%lu href_gated=%lu href_low_gated=%lu sync_high_edges=%lu all_changes=%lu high_changes=%lu low_changes=%lu high_min=0x%02X high_max=0x%02X low_min=0x%02X low_max=0x%02X\r\n",
               (unsigned long)max_edges,
               (unsigned long)max_loops,
               (unsigned long)loops,
               (unsigned long)pclk_edges,
               (unsigned long)href_gated,
               (unsigned long)href_low_gated,
               (unsigned long)sync_high,
               (unsigned long)all_stats.changes,
               (unsigned long)href_high_stats.changes,
               (unsigned long)href_low_stats.changes,
               href_high_stats.min_value,
               href_high_stats.max_value,
               href_low_stats.min_value,
               href_low_stats.max_value);
    }
    camera_raw_sample_stats_log(all_tag, &all_stats);
    camera_raw_sample_stats_log(href_high_tag, &href_high_stats);
    camera_raw_sample_stats_log(href_low_tag, &href_low_stats);
#endif
}

static void camera_raw_pclk_sample_probe(void)
{
    camera_raw_pclk_sample_probe_tag(NULL, "all", "href_high", "href_low");
}

static void camera_isp_path_regs_read(camera_isp_path_regs_t *regs)
{
    (void)camera_sccb_read_reg16(0x3000U, &regs->reg_3000);
    (void)camera_sccb_read_reg16(0x3002U, &regs->reg_3002);
    (void)camera_sccb_read_reg16(0x3004U, &regs->reg_3004);
    (void)camera_sccb_read_reg16(0x3006U, &regs->reg_3006);
    (void)camera_sccb_read_reg16(0x3007U, &regs->reg_3007);
    (void)camera_sccb_read_reg16(0x3008U, &regs->reg_3008);
    (void)camera_sccb_read_reg16(0x3034U, &regs->reg_3034);
    (void)camera_sccb_read_reg16(0x3035U, &regs->reg_3035);
    (void)camera_sccb_read_reg16(0x3036U, &regs->reg_3036);
    (void)camera_sccb_read_reg16(0x3037U, &regs->reg_3037);
    (void)camera_sccb_read_reg16(0x3500U, &regs->reg_3500);
    (void)camera_sccb_read_reg16(0x3501U, &regs->reg_3501);
    (void)camera_sccb_read_reg16(0x3502U, &regs->reg_3502);
    (void)camera_sccb_read_reg16(0x3503U, &regs->reg_3503);
    (void)camera_sccb_read_reg16(0x350AU, &regs->reg_350a);
    (void)camera_sccb_read_reg16(0x350BU, &regs->reg_350b);
    (void)camera_sccb_read_reg16(0x3A00U, &regs->reg_3a00);
    (void)camera_sccb_read_reg16(0x3A13U, &regs->reg_3a13);
    (void)camera_sccb_read_reg16(0x3821U, &regs->reg_3821);
    (void)camera_sccb_read_reg16(0x3824U, &regs->reg_3824);
    (void)camera_sccb_read_reg16(0x4000U, &regs->reg_4000);
    (void)camera_sccb_read_reg16(0x4001U, &regs->reg_4001);
    (void)camera_sccb_read_reg16(0x4004U, &regs->reg_4004);
    (void)camera_sccb_read_reg16(0x4005U, &regs->reg_4005);
    (void)camera_sccb_read_reg16(0x4300U, &regs->reg_4300);
    (void)camera_sccb_read_reg16(0x460BU, &regs->reg_460b);
    (void)camera_sccb_read_reg16(0x460CU, &regs->reg_460c);
    (void)camera_sccb_read_reg16(0x4740U, &regs->reg_4740);
    (void)camera_sccb_read_reg16(0x4741U, &regs->reg_4741);
    (void)camera_sccb_read_reg16(0x4745U, &regs->reg_4745);
    (void)camera_sccb_read_reg16(0x5000U, &regs->reg_5000);
    (void)camera_sccb_read_reg16(0x5001U, &regs->reg_5001);
    (void)camera_sccb_read_reg16(0x501FU, &regs->reg_501f);
    (void)camera_sccb_read_reg16(0x503DU, &regs->reg_503d);
    (void)camera_sccb_read_reg16(0x5584U, &regs->reg_5584);
}

static void camera_isp_path_regs_log(const char *tag)
{
    camera_isp_path_regs_t regs = {0U};
    uint16_t exposure;
    uint16_t gain;

    camera_isp_path_regs_read(&regs);
    exposure = (uint16_t)((((uint16_t)regs.reg_3500 & 0x0FU) << 12U) |
                          ((uint16_t)regs.reg_3501 << 4U) |
                          (((uint16_t)regs.reg_3502 >> 4U) & 0x0FU));
    gain = (uint16_t)((((uint16_t)regs.reg_350a & 0x03U) << 8U) | regs.reg_350b);

    printf("camera_isp_path_regs[%s]: 3000=0x%02X 3002=0x%02X 3004=0x%02X 3006=0x%02X 3007=0x%02X 3008=0x%02X 5000=0x%02X 5001=0x%02X 501f=0x%02X 503d=0x%02X 5584=0x%02X\r\n",
           tag,
           regs.reg_3000,
           regs.reg_3002,
           regs.reg_3004,
           regs.reg_3006,
           regs.reg_3007,
           regs.reg_3008,
           regs.reg_5000,
           regs.reg_5001,
           regs.reg_501f,
           regs.reg_503d,
           regs.reg_5584);
    printf("camera_isp_path_regs[%s]: pll=3034:0x%02X 3035:0x%02X 3036:0x%02X 3037:0x%02X\r\n",
           tag,
           regs.reg_3034,
           regs.reg_3035,
           regs.reg_3036,
           regs.reg_3037);
    printf("camera_isp_path_regs[%s]: 3500=0x%02X 3501=0x%02X 3502=0x%02X exp=0x%04X 3503=0x%02X 350a=0x%02X 350b=0x%02X gain=0x%03X 3a00=0x%02X 3a13=0x%02X 4000=0x%02X 4001=0x%02X 4004=0x%02X 4005=0x%02X\r\n",
           tag,
           regs.reg_3500,
           regs.reg_3501,
           regs.reg_3502,
           exposure,
           regs.reg_3503,
           regs.reg_350a,
           regs.reg_350b,
           gain,
           regs.reg_3a00,
           regs.reg_3a13,
           regs.reg_4000,
           regs.reg_4001,
           regs.reg_4004,
           regs.reg_4005);
    printf("camera_isp_path_regs[%s]: 3821=0x%02X 3824=0x%02X 4300=0x%02X 460b=0x%02X 460c=0x%02X 4740=0x%02X 4741=0x%02X 4745=0x%02X\r\n",
           tag,
           regs.reg_3821,
           regs.reg_3824,
           regs.reg_4300,
           regs.reg_460b,
           regs.reg_460c,
           regs.reg_4740,
           regs.reg_4741,
           regs.reg_4745);
}

static void camera_isp_path_regs_restore(const camera_isp_path_regs_t *regs)
{
    (void)camera_sccb_write_reg16(0x3000U, regs->reg_3000);
    (void)camera_sccb_write_reg16(0x3002U, regs->reg_3002);
    (void)camera_sccb_write_reg16(0x3004U, regs->reg_3004);
    (void)camera_sccb_write_reg16(0x3006U, regs->reg_3006);
    (void)camera_sccb_write_reg16(0x3007U, regs->reg_3007);
    (void)camera_sccb_write_reg16(0x3034U, regs->reg_3034);
    (void)camera_sccb_write_reg16(0x3035U, regs->reg_3035);
    (void)camera_sccb_write_reg16(0x3036U, regs->reg_3036);
    (void)camera_sccb_write_reg16(0x3037U, regs->reg_3037);
    (void)camera_sccb_write_reg16(0x3500U, regs->reg_3500);
    (void)camera_sccb_write_reg16(0x3501U, regs->reg_3501);
    (void)camera_sccb_write_reg16(0x3502U, regs->reg_3502);
    (void)camera_sccb_write_reg16(0x3503U, regs->reg_3503);
    (void)camera_sccb_write_reg16(0x350AU, regs->reg_350a);
    (void)camera_sccb_write_reg16(0x350BU, regs->reg_350b);
    (void)camera_sccb_write_reg16(0x3A00U, regs->reg_3a00);
    (void)camera_sccb_write_reg16(0x3A13U, regs->reg_3a13);
    (void)camera_sccb_write_reg16(0x3821U, regs->reg_3821);
    (void)camera_sccb_write_reg16(0x3824U, regs->reg_3824);
    (void)camera_sccb_write_reg16(0x4000U, regs->reg_4000);
    (void)camera_sccb_write_reg16(0x4001U, regs->reg_4001);
    (void)camera_sccb_write_reg16(0x4004U, regs->reg_4004);
    (void)camera_sccb_write_reg16(0x4005U, regs->reg_4005);
    (void)camera_sccb_write_reg16(0x4300U, regs->reg_4300);
    (void)camera_sccb_write_reg16(0x460BU, regs->reg_460b);
    (void)camera_sccb_write_reg16(0x460CU, regs->reg_460c);
    (void)camera_sccb_write_reg16(0x4740U, regs->reg_4740);
    (void)camera_sccb_write_reg16(0x4741U, regs->reg_4741);
    (void)camera_sccb_write_reg16(0x4745U, regs->reg_4745);
    (void)camera_sccb_write_reg16(0x5000U, regs->reg_5000);
    (void)camera_sccb_write_reg16(0x5001U, regs->reg_5001);
    (void)camera_sccb_write_reg16(0x501FU, regs->reg_501f);
    (void)camera_sccb_write_reg16(0x503DU, regs->reg_503d);
    (void)camera_sccb_write_reg16(0x5584U, regs->reg_5584);
}

static void camera_window_regs_read(camera_window_regs_t *regs)
{
    (void)camera_sccb_read_reg16(0x3800U, &regs->reg_3800);
    (void)camera_sccb_read_reg16(0x3801U, &regs->reg_3801);
    (void)camera_sccb_read_reg16(0x3802U, &regs->reg_3802);
    (void)camera_sccb_read_reg16(0x3803U, &regs->reg_3803);
    (void)camera_sccb_read_reg16(0x3804U, &regs->reg_3804);
    (void)camera_sccb_read_reg16(0x3805U, &regs->reg_3805);
    (void)camera_sccb_read_reg16(0x3806U, &regs->reg_3806);
    (void)camera_sccb_read_reg16(0x3807U, &regs->reg_3807);
    (void)camera_sccb_read_reg16(0x3808U, &regs->reg_3808);
    (void)camera_sccb_read_reg16(0x3809U, &regs->reg_3809);
    (void)camera_sccb_read_reg16(0x380AU, &regs->reg_380a);
    (void)camera_sccb_read_reg16(0x380BU, &regs->reg_380b);
    (void)camera_sccb_read_reg16(0x380CU, &regs->reg_380c);
    (void)camera_sccb_read_reg16(0x380DU, &regs->reg_380d);
    (void)camera_sccb_read_reg16(0x380EU, &regs->reg_380e);
    (void)camera_sccb_read_reg16(0x380FU, &regs->reg_380f);
    (void)camera_sccb_read_reg16(0x3810U, &regs->reg_3810);
    (void)camera_sccb_read_reg16(0x3811U, &regs->reg_3811);
    (void)camera_sccb_read_reg16(0x3812U, &regs->reg_3812);
    (void)camera_sccb_read_reg16(0x3813U, &regs->reg_3813);
    (void)camera_sccb_read_reg16(0x3814U, &regs->reg_3814);
    (void)camera_sccb_read_reg16(0x3815U, &regs->reg_3815);
}

static void camera_window_readback_log(const char *tag)
{
#if EDGECARE_ENABLE_CAMERA_WINDOW_READBACK
    camera_window_regs_t regs = {0U};
    uint16_t x_start;
    uint16_t y_start;
    uint16_t x_end;
    uint16_t y_end;
    uint16_t out_w;
    uint16_t out_h;
    uint16_t hts;
    uint16_t vts;
    uint16_t x_offset;
    uint16_t y_offset;

    camera_window_regs_read(&regs);
    x_start = (uint16_t)(((uint16_t)regs.reg_3800 << 8U) | regs.reg_3801);
    y_start = (uint16_t)(((uint16_t)regs.reg_3802 << 8U) | regs.reg_3803);
    x_end = (uint16_t)(((uint16_t)regs.reg_3804 << 8U) | regs.reg_3805);
    y_end = (uint16_t)(((uint16_t)regs.reg_3806 << 8U) | regs.reg_3807);
    out_w = (uint16_t)(((uint16_t)regs.reg_3808 << 8U) | regs.reg_3809);
    out_h = (uint16_t)(((uint16_t)regs.reg_380a << 8U) | regs.reg_380b);
    hts = (uint16_t)(((uint16_t)regs.reg_380c << 8U) | regs.reg_380d);
    vts = (uint16_t)(((uint16_t)regs.reg_380e << 8U) | regs.reg_380f);
    x_offset = (uint16_t)(((uint16_t)regs.reg_3810 << 8U) | regs.reg_3811);
    y_offset = (uint16_t)(((uint16_t)regs.reg_3812 << 8U) | regs.reg_3813);

    printf("camera_window_readback[%s]: raw=3800:%02X 3801:%02X 3802:%02X 3803:%02X 3804:%02X 3805:%02X 3806:%02X 3807:%02X 3808:%02X 3809:%02X 380a:%02X 380b:%02X\r\n",
           tag,
           regs.reg_3800,
           regs.reg_3801,
           regs.reg_3802,
           regs.reg_3803,
           regs.reg_3804,
           regs.reg_3805,
           regs.reg_3806,
           regs.reg_3807,
           regs.reg_3808,
           regs.reg_3809,
           regs.reg_380a,
           regs.reg_380b);
    printf("camera_window_readback[%s]: raw=380c:%02X 380d:%02X 380e:%02X 380f:%02X 3810:%02X 3811:%02X 3812:%02X 3813:%02X 3814:%02X 3815:%02X\r\n",
           tag,
           regs.reg_380c,
           regs.reg_380d,
           regs.reg_380e,
           regs.reg_380f,
           regs.reg_3810,
           regs.reg_3811,
           regs.reg_3812,
           regs.reg_3813,
           regs.reg_3814,
           regs.reg_3815);
    printf("camera_window_readback[%s]: crop=%u,%u-%u,%u sensor_span=%ux%u output=%ux%u hts=%u vts=%u offset=%u,%u inc=0x%02X,0x%02X\r\n",
           tag,
           x_start,
           y_start,
           x_end,
           y_end,
           (uint16_t)(x_end - x_start + 1U),
           (uint16_t)(y_end - y_start + 1U),
           out_w,
           out_h,
           hts,
           vts,
           x_offset,
           y_offset,
           regs.reg_3814,
           regs.reg_3815);
#else
    (void)tag;
#endif
}

static void camera_dci_gpio_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOE);
    rcu_periph_clock_enable(RCU_GPIOG);
    rcu_periph_clock_enable(RCU_DCI);

    /* DCI_PIXCLK(PA6), DCI_HSYNC(PA4), DCI_VSYNC(PB7). */
    gpio_af_set(GPIOA, GPIO_AF_13, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_af_set(GPIOB, GPIO_AF_13, GPIO_PIN_7);

    gpio_mode_set(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_output_options_set(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_7);

    /* D0(PC6), D1(PC7), D2(PC8), D3(PG11), D4(PC11), D5(PB6), D6(PE5), D7(PB9). */
    gpio_af_set(GPIOC, GPIO_AF_13, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11);
    gpio_af_set(GPIOG, GPIO_AF_13, GPIO_PIN_11);
    gpio_af_set(GPIOE, GPIO_AF_13, GPIO_PIN_5);
    gpio_af_set(GPIOB, GPIO_AF_13, GPIO_PIN_6 | GPIO_PIN_9);

    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11);
    gpio_mode_set(GPIOG, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOG, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_11);
    gpio_mode_set(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_5);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_5);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_9);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_6 | GPIO_PIN_9);
}

static void camera_dci_dma_init(uint32_t capture_mode,
                                uint32_t clock_polarity,
                                uint32_t hsync_polarity,
                                uint32_t vsync_polarity)
{
    dci_parameter_struct dci_struct;
    dma_single_data_parameter_struct dma_struct;

    camera_dci_gpio_init();

    dci_deinit();
    dci_struct.capture_mode = capture_mode;
    dci_struct.clock_polarity = clock_polarity;
    dci_struct.hsync_polarity = hsync_polarity;
    dci_struct.vsync_polarity = vsync_polarity;
    dci_struct.frame_rate = DCI_FRAME_RATE_ALL;
    dci_struct.interface_format = DCI_INTERFACE_FORMAT_8BITS;
    dci_init(&dci_struct);

    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_DMAMUX);

    dma_channel_disable(DMA1, DMA_CH7);
    dma_deinit(DMA1, DMA_CH7);
    dma_single_data_para_struct_init(&dma_struct);
    dma_struct.request = DMA_REQUEST_DCI;
    dma_struct.periph_addr = DCI_DATA_ADDRESS;
    dma_struct.memory0_addr = (uint32_t)g_camera_capture_buffer;
    dma_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_struct.number = CAMERA_CAPTURE_WORDS;
    dma_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_struct.periph_memory_width = DMA_PERIPH_WIDTH_32BIT;
    dma_struct.circular_mode = DMA_CIRCULAR_MODE_DISABLE;
    dma_struct.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(DMA1, DMA_CH7, &dma_struct);
}

#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
static uint32_t camera_dma_width_bits(uint32_t width_code)
{
    if(0U == width_code) {
        return 8U;
    }
    if(1U == width_code) {
        return 16U;
    }
    if(2U == width_code) {
        return 32U;
    }
    return 0U;
}

static void camera_dci_dma_summary_log(const char *tag)
{
    uint32_t dci_ctl = DCI_CTL;
    uint32_t dma_chctl = DMA_CHCTL(DMA1, DMA_CH7);
    uint32_t dma_chfctl = DMA_CHFCTL(DMA1, DMA_CH7);
    uint32_t dma_chcnt = DMA_CHCNT(DMA1, DMA_CH7);
    uint32_t dmamux = DMAMUX_RM_CHXCFG((uint32_t)DMA_CH7 + 8U);
    uint32_t dmamux_id = dmamux & DMAMUX_RM_CHXCFG_MUXID;
    uint32_t pwidth_code = (dma_chctl & DMA_CHXCTL_PWIDTH) >> 11U;
    uint32_t mwidth_code = (dma_chctl & DMA_CHXCTL_MWIDTH) >> 13U;
    uint32_t tm_code = (dma_chctl & DMA_CHXCTL_TM) >> 6U;
    uint32_t dcif_code = (dci_ctl & DCI_CTL_DCIF) >> 10U;
    uint32_t frame_rate_code = (dci_ctl & DCI_CTL_FR) >> 8U;

    printf("camera_dci_dma_summary[%s]: ctl=%08lX dma_chctl=%08lX dma_chfctl=%08lX dmamux=%08lX dmamux_id=%lu dma_req_ok=%u dma_en=%u mem_inc=%u periph_inc=%u pwidth=%lu mwidth=%lu tm=%lu tm_ok=%u cnt=%lu dci_en=%u cap=%u snap=%u ck_rising=%u hs_blank_high=%u vs_blank_high=%u frame_rate=%lu dcif=%lu dcif_ok=%u\r\n",
           tag,
           (unsigned long)dci_ctl,
           (unsigned long)dma_chctl,
           (unsigned long)dma_chfctl,
           (unsigned long)dmamux,
           (unsigned long)dmamux_id,
           (dmamux_id == (DMA_REQUEST_DCI & DMAMUX_RM_CHXCFG_MUXID)) ? 1U : 0U,
           (0U != (dma_chctl & DMA_CHXCTL_CHEN)) ? 1U : 0U,
           (0U != (dma_chctl & DMA_CHXCTL_MNAGA)) ? 1U : 0U,
           (0U != (dma_chctl & DMA_CHXCTL_PNAGA)) ? 1U : 0U,
           (unsigned long)camera_dma_width_bits(pwidth_code),
           (unsigned long)camera_dma_width_bits(mwidth_code),
           (unsigned long)tm_code,
           (DMA_PERIPH_TO_MEMORY == (dma_chctl & DMA_CHXCTL_TM)) ? 1U : 0U,
           (unsigned long)dma_chcnt,
           (0U != (dci_ctl & DCI_CTL_DCIEN)) ? 1U : 0U,
           (0U != (dci_ctl & DCI_CTL_CAP)) ? 1U : 0U,
           (0U != (dci_ctl & DCI_CTL_SNAP)) ? 1U : 0U,
           (0U != (dci_ctl & DCI_CTL_CKS)) ? 1U : 0U,
           (0U != (dci_ctl & DCI_CTL_HPS)) ? 1U : 0U,
           (0U != (dci_ctl & DCI_CTL_VPS)) ? 1U : 0U,
           (unsigned long)frame_rate_code,
           (unsigned long)dcif_code,
           (DCI_INTERFACE_FORMAT_8BITS == (dci_ctl & DCI_CTL_DCIF)) ? 1U : 0U);
}

static void camera_dci_status_probe_begin(camera_dci_status_probe_t *probe)
{
    uint32_t stat0 = DCI_STAT0;
    uint32_t stat1 = DCI_STAT1;
    uint32_t intf = DCI_INTF;
    uint32_t dma_count = DMA_CHCNT(DMA1, DMA_CH7);

    memset(probe, 0, sizeof(*probe));
    probe->ctl_first = DCI_CTL;
    probe->ctl_last = probe->ctl_first;
    probe->stat0_first = stat0;
    probe->stat0_last = stat0;
    probe->stat1_first = stat1;
    probe->stat1_last = stat1;
    probe->intf_first = intf;
    probe->intf_last = intf;
    probe->dma_count_start = dma_count;
    probe->dma_count_min = dma_count;
    probe->dma_count_last = dma_count;
    probe->dma_chctl_last = DMA_CHCTL(DMA1, DMA_CH7);
    probe->dma_chfctl_last = DMA_CHFCTL(DMA1, DMA_CH7);
    probe->dmamux_last = DMAMUX_RM_CHXCFG((uint32_t)DMA_CH7 + 8U);
}

static void camera_dci_status_probe_poll(camera_dci_status_probe_t *probe)
{
    uint32_t stat0 = DCI_STAT0;
    uint32_t stat1 = DCI_STAT1;
    uint32_t intf = DCI_INTF;
    uint32_t dma_count = DMA_CHCNT(DMA1, DMA_CH7);

    probe->polls++;
    if(stat0 != probe->stat0_last) {
        probe->stat0_changes++;
        probe->stat0_last = stat0;
    }
    if(stat1 != probe->stat1_last) {
        probe->stat1_changes++;
        probe->stat1_last = stat1;
    }
    if(intf != probe->intf_last) {
        probe->intf_changes++;
        probe->intf_last = intf;
    }
    if(dma_count != probe->dma_count_last) {
        probe->dma_count_changes++;
        probe->dma_count_last = dma_count;
        if(dma_count < probe->dma_count_min) {
            probe->dma_count_min = dma_count;
        }
    }

    if(0U != (stat0 & DCI_STAT0_HS)) {
        probe->hs_high_polls++;
    }
    if(0U != (stat0 & DCI_STAT0_VS)) {
        probe->vs_high_polls++;
    }
    if(0U != (stat0 & DCI_STAT0_FV)) {
        probe->fv_high_polls++;
    }
    if(0U != (stat1 & DCI_STAT1_EFF)) {
        probe->ef_seen = 1U;
    }
    if(0U != (stat1 & DCI_STAT1_OVRF)) {
        probe->ovr_seen = 1U;
    }
    if(0U != (intf & DCI_INTF_VSIF)) {
        probe->vsif_seen = 1U;
    }
    if(0U != (intf & DCI_INTF_ELIF)) {
        probe->elif_seen = 1U;
    }

    probe->ctl_last = DCI_CTL;
    probe->dma_chctl_last = DMA_CHCTL(DMA1, DMA_CH7);
    probe->dma_chfctl_last = DMA_CHFCTL(DMA1, DMA_CH7);
    probe->dmamux_last = DMAMUX_RM_CHXCFG((uint32_t)DMA_CH7 + 8U);
}

static void camera_dci_status_probe_log(const char *tag, const camera_dci_status_probe_t *probe)
{
    printf("camera_dci_status_probe[%s]: polls=%lu ctl=%08lX->%08lX stat0=%08lX->%08lX chg=%lu stat1=%08lX->%08lX chg=%lu intf=%08lX->%08lX chg=%lu hs_polls=%lu vs_polls=%lu fv_polls=%lu ef_seen=%lu ovr_seen=%lu vsif_seen=%lu elif_seen=%lu dma_cnt=%lu->%lu min=%lu chg=%lu dma_chctl=%08lX dma_chfctl=%08lX dmamux=%08lX\r\n",
           tag,
           (unsigned long)probe->polls,
           (unsigned long)probe->ctl_first,
           (unsigned long)probe->ctl_last,
           (unsigned long)probe->stat0_first,
           (unsigned long)probe->stat0_last,
           (unsigned long)probe->stat0_changes,
           (unsigned long)probe->stat1_first,
           (unsigned long)probe->stat1_last,
           (unsigned long)probe->stat1_changes,
           (unsigned long)probe->intf_first,
           (unsigned long)probe->intf_last,
           (unsigned long)probe->intf_changes,
           (unsigned long)probe->hs_high_polls,
           (unsigned long)probe->vs_high_polls,
           (unsigned long)probe->fv_high_polls,
           (unsigned long)probe->ef_seen,
           (unsigned long)probe->ovr_seen,
           (unsigned long)probe->vsif_seen,
           (unsigned long)probe->elif_seen,
           (unsigned long)probe->dma_count_start,
           (unsigned long)probe->dma_count_last,
           (unsigned long)probe->dma_count_min,
           (unsigned long)probe->dma_count_changes,
           (unsigned long)probe->dma_chctl_last,
           (unsigned long)probe->dma_chfctl_last,
           (unsigned long)probe->dmamux_last);
}

static void camera_dci_sync_matrix_sample(const char *tag, uint32_t hsync_polarity, uint32_t vsync_polarity)
{
    dci_parameter_struct dci_struct;
    camera_dci_status_probe_t probe;
    uint32_t i;

    camera_dci_gpio_init();
    rcu_periph_clock_enable(RCU_DMA1);
    rcu_periph_clock_enable(RCU_DMAMUX);

    dci_deinit();
    dci_struct.capture_mode = DCI_CAPTURE_MODE_CONTINUOUS;
    dci_struct.clock_polarity = DCI_CK_POLARITY_FALLING;
    dci_struct.hsync_polarity = hsync_polarity;
    dci_struct.vsync_polarity = vsync_polarity;
    dci_struct.frame_rate = DCI_FRAME_RATE_ALL;
    dci_struct.interface_format = DCI_INTERFACE_FORMAT_8BITS;
    dci_init(&dci_struct);

    dci_enable();
    dci_capture_enable();
    camera_dci_status_probe_begin(&probe);
    camera_dci_dma_summary_log(tag);
    for(i = 0U; i < 4096U; i++) {
        camera_dci_status_probe_poll(&probe);
    }
    dci_capture_disable();
    dci_disable();

    printf("camera_dci_sync_matrix[%s]: polls=%lu stat0=%08lX->%08lX chg=%lu hs_polls=%lu vs_polls=%lu fv_polls=%lu stat1=%08lX->%08lX intf=%08lX->%08lX ctl=%08lX->%08lX\r\n",
           tag,
           (unsigned long)probe.polls,
           (unsigned long)probe.stat0_first,
           (unsigned long)probe.stat0_last,
           (unsigned long)probe.stat0_changes,
           (unsigned long)probe.hs_high_polls,
           (unsigned long)probe.vs_high_polls,
           (unsigned long)probe.fv_high_polls,
           (unsigned long)probe.stat1_first,
           (unsigned long)probe.stat1_last,
           (unsigned long)probe.intf_first,
           (unsigned long)probe.intf_last,
           (unsigned long)probe.ctl_first,
           (unsigned long)probe.ctl_last);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_7);
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
    gpio_mode_set(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_mode_set(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);
}

static void camera_dci_sync_matrix_probe(void)
{
    camera_dci_sync_matrix_sample("hs_low_vs_low", DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_LOW);
    camera_dci_sync_matrix_sample("hs_low_vs_high", DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH);
    camera_dci_sync_matrix_sample("hs_high_vs_low", DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_LOW);
    camera_dci_sync_matrix_sample("hs_high_vs_high", DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_HIGH);
}

#if EDGECARE_ENABLE_CAMERA_CAPTURE_MODE_SWEEP
static void camera_capture_mode_sweep_one(const char *tag, uint32_t capture_mode)
{
    camera_dci_status_probe_t probe;
    uint32_t ms;
    uint32_t i;

    memset(g_camera_capture_buffer, 0, sizeof(g_camera_capture_buffer));
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)g_camera_capture_buffer, sizeof(g_camera_capture_buffer));
    camera_dci_dma_init(capture_mode,
                        DCI_CK_POLARITY_RISING,
                        DCI_HSYNC_POLARITY_LOW,
                        DCI_VSYNC_POLARITY_HIGH);
    dma_flag_clear(DMA1, DMA_CH7, DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_TAE | DMA_FLAG_SDE | DMA_FLAG_FEE);
    dci_interrupt_flag_clear(DCI_INT_FLAG_EF | DCI_INT_FLAG_OVR | DCI_INT_FLAG_VSYNC | DCI_INT_FLAG_EL);

    dma_channel_enable(DMA1, DMA_CH7);
    dci_enable();
    dci_capture_enable();
    camera_dci_status_probe_begin(&probe);
    camera_dci_dma_summary_log(tag);
    for(ms = 0U; ms < CAMERA_CAPTURE_MODE_SWEEP_WINDOW_MS; ms++) {
        for(i = 0U; i < CAMERA_CAPTURE_MODE_SWEEP_POLLS_PER_MS; i++) {
            camera_dci_status_probe_poll(&probe);
        }
        delay_1ms(1U);
    }
    dci_capture_disable();
    dci_disable();
    dma_channel_disable(DMA1, DMA_CH7);

    printf("camera_capture_mode_sweep[%s]: capture_mode=%s window_ms=%lu polls=%lu stat0=%08lX->%08lX chg=%lu stat1=%08lX->%08lX chg=%lu intf=%08lX->%08lX chg=%lu hs_polls=%lu vs_polls=%lu fv_polls=%lu ef_seen=%lu vsif_seen=%lu elif_seen=%lu dma_cnt=%lu->%lu min=%lu dma_chctl=%08lX dmamux=%08lX\r\n",
           tag,
           (DCI_CAPTURE_MODE_SNAPSHOT == capture_mode) ? "snapshot" : "continuous",
           (unsigned long)CAMERA_CAPTURE_MODE_SWEEP_WINDOW_MS,
           (unsigned long)probe.polls,
           (unsigned long)probe.stat0_first,
           (unsigned long)probe.stat0_last,
           (unsigned long)probe.stat0_changes,
           (unsigned long)probe.stat1_first,
           (unsigned long)probe.stat1_last,
           (unsigned long)probe.stat1_changes,
           (unsigned long)probe.intf_first,
           (unsigned long)probe.intf_last,
           (unsigned long)probe.intf_changes,
           (unsigned long)probe.hs_high_polls,
           (unsigned long)probe.vs_high_polls,
           (unsigned long)probe.fv_high_polls,
           (unsigned long)probe.ef_seen,
           (unsigned long)probe.vsif_seen,
           (unsigned long)probe.elif_seen,
           (unsigned long)probe.dma_count_start,
           (unsigned long)probe.dma_count_last,
           (unsigned long)probe.dma_count_min,
           (unsigned long)probe.dma_chctl_last,
           (unsigned long)probe.dmamux_last);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOC, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
    gpio_mode_set(GPIOE, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_mode_set(GPIOG, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_11);
}

static void camera_capture_mode_sweep_probe(void)
{
    printf("camera_capture_mode_sweep: enabled source=GD32_DCI_capture_mode_compare polarity=pclk_rising_hs_blank_low_vs_blank_high window_ms=%lu polls_per_ms=%lu\r\n",
           (unsigned long)CAMERA_CAPTURE_MODE_SWEEP_WINDOW_MS,
           (unsigned long)CAMERA_CAPTURE_MODE_SWEEP_POLLS_PER_MS);
    camera_capture_mode_sweep_one("snapshot", DCI_CAPTURE_MODE_SNAPSHOT);
    camera_capture_mode_sweep_one("continuous", DCI_CAPTURE_MODE_CONTINUOUS);
}
#else
static void camera_capture_mode_sweep_probe(void)
{
}
#endif
#endif

static uint8_t camera_frame_byte_at(uint32_t pixel_index, uint8_t phase)
{
    return ((const uint8_t *)g_camera_capture_buffer)[(pixel_index * 2U) + (uint32_t)(phase & 1U)];
}

static uint8_t camera_abs_diff_u8(uint8_t a, uint8_t b)
{
    return (a > b) ? (uint8_t)(a - b) : (uint8_t)(b - a);
}

static camera_frame_quality_t camera_estimate_frame_quality(void)
{
    camera_frame_quality_t best = {0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t phase;

    for(phase = 0U; phase < 2U; phase++) {
        uint32_t y;
        uint32_t x;
        uint32_t row_min = 0xFFFFFFFFU;
        uint32_t row_max = 0U;
        uint32_t col_min = 0xFFFFFFFFU;
        uint32_t col_max = 0U;
        uint32_t neighbor_delta = 0U;
        uint32_t neighbor_count = 0U;
        uint32_t row_sum[8] = {0U};
        uint32_t col_sum[8] = {0U};

        for(y = 0U; y < 8U; y++) {
            uint32_t src_y = ((y * 2U) + 1U) * (CAMERA_FRAME_HEIGHT / 16U);

            for(x = 0U; x < CAMERA_FRAME_WIDTH; x += 4U) {
                row_sum[y] += camera_frame_byte_at((src_y * CAMERA_FRAME_WIDTH) + x, phase);
            }
            row_sum[y] /= (CAMERA_FRAME_WIDTH / 4U);

            if(row_sum[y] < row_min) {
                row_min = row_sum[y];
            }
            if(row_sum[y] > row_max) {
                row_max = row_sum[y];
            }
        }

        for(x = 0U; x < 8U; x++) {
            uint32_t src_x = ((x * 2U) + 1U) * (CAMERA_FRAME_WIDTH / 16U);

            for(y = 0U; y < CAMERA_FRAME_HEIGHT; y += 4U) {
                col_sum[x] += camera_frame_byte_at((y * CAMERA_FRAME_WIDTH) + src_x, phase);
            }
            col_sum[x] /= (CAMERA_FRAME_HEIGHT / 4U);

            if(col_sum[x] < col_min) {
                col_min = col_sum[x];
            }
            if(col_sum[x] > col_max) {
                col_max = col_sum[x];
            }
        }

        for(y = CAMERA_FRAME_HEIGHT / 4U; y < ((CAMERA_FRAME_HEIGHT / 4U) * 3U); y += 8U) {
            uint8_t prev = camera_frame_byte_at((y * CAMERA_FRAME_WIDTH) + (CAMERA_FRAME_WIDTH / 8U), phase);

            for(x = (CAMERA_FRAME_WIDTH / 8U) + 1U; x < ((CAMERA_FRAME_WIDTH / 8U) * 7U); x += 8U) {
                uint8_t now = camera_frame_byte_at((y * CAMERA_FRAME_WIDTH) + x, phase);
                neighbor_delta += camera_abs_diff_u8(prev, now);
                neighbor_count++;
                prev = now;
            }
        }

        if(0U != neighbor_count) {
            neighbor_delta /= neighbor_count;
        }

        {
            uint32_t row_range = row_max - row_min;
            uint32_t col_range = col_max - col_min;
            uint32_t score = (col_range * 8U) + (neighbor_delta * 4U) + row_range;

            if((0U == best.valid) || (score > best.score)) {
                best.valid = 1U;
                best.best_phase = phase;
                best.row_range = row_range;
                best.col_range = col_range;
                best.neighbor_delta = neighbor_delta;
                best.score = score;
            }
        }
    }

    return best;
}

static uint8_t edgecare_camera_capture_attempt(const char *tag,
                                               uint32_t clock_polarity,
                                               uint32_t hsync_polarity,
                                               uint32_t vsync_polarity,
                                               uint8_t data_order,
                                               camera_frame_quality_t *quality)
{
    uint32_t timeout = CAMERA_CAPTURE_TIMEOUT;
    uint32_t checksum = 0U;
    uint32_t nonzero_words = 0U;
    uint32_t repeated_words = 0U;
    uint32_t i;
    uint32_t middle_index = CAMERA_CAPTURE_WORDS / 2U;
    uint32_t last_index = CAMERA_CAPTURE_WORDS - 4U;
    FlagStatus hs_before;
    FlagStatus vs_before;
    FlagStatus dma_done;
    FlagStatus dma_tae;
    FlagStatus dma_sde;
    FlagStatus dma_fee;
    FlagStatus dci_ef;
    FlagStatus dci_ovr;
    FlagStatus dci_vsync;
    FlagStatus dci_el;
    FlagStatus dci_fv;
    uint32_t dma_remaining;
    uint8_t data_order_readback = 0U;
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
    camera_dci_status_probe_t dci_probe;
#endif

    memset(g_camera_capture_buffer, 0, sizeof(g_camera_capture_buffer));
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)g_camera_capture_buffer, sizeof(g_camera_capture_buffer));

    (void)camera_sccb_write_reg16(0x4745U, data_order);
    delay_1ms(2U);
    (void)camera_sccb_read_reg16(0x4745U, &data_order_readback);

    camera_dci_dma_init(CAMERA_DCI_CAPTURE_MODE_DEFAULT, clock_polarity, hsync_polarity, vsync_polarity);

    hs_before = dci_flag_get(DCI_FLAG_HS);
    vs_before = dci_flag_get(DCI_FLAG_VS);
    dma_flag_clear(DMA1, DMA_CH7, DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_TAE | DMA_FLAG_SDE | DMA_FLAG_FEE);
    dci_interrupt_flag_clear(DCI_INT_FLAG_EF | DCI_INT_FLAG_OVR | DCI_INT_FLAG_VSYNC | DCI_INT_FLAG_EL);

    dma_channel_enable(DMA1, DMA_CH7);
    dci_enable();
    dci_capture_enable();
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
    camera_dci_status_probe_begin(&dci_probe);
    camera_dci_dma_summary_log(tag);
    camera_dci_status_probe_poll(&dci_probe);
#endif

    while(RESET == dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_FTF)) {
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
        if(0U == (timeout & CAMERA_DCI_STATUS_PROBE_POLL_MASK)) {
            camera_dci_status_probe_poll(&dci_probe);
        }
#endif
        if(0U == timeout--) {
            break;
        }
    }

    dci_capture_disable();
    dci_disable();
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
    camera_dci_status_probe_log(tag, &dci_probe);
#endif

    dma_done = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_FTF);
    dma_tae = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_TAE);
    dma_sde = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_SDE);
    dma_fee = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_FEE);
    dci_ef = dci_flag_get(DCI_FLAG_EF);
    dci_ovr = dci_flag_get(DCI_FLAG_OVR);
    dci_vsync = dci_flag_get(DCI_FLAG_VSYNC);
    dci_el = dci_flag_get(DCI_FLAG_EL);
    dci_fv = dci_flag_get(DCI_FLAG_FV);
    dma_remaining = dma_transfer_number_get(DMA1, DMA_CH7);

    dma_channel_disable(DMA1, DMA_CH7);

    SCB_InvalidateDCache_by_Addr((uint32_t *)g_camera_capture_buffer, sizeof(g_camera_capture_buffer));

    for(i = 0U; i < CAMERA_CAPTURE_WORDS; i++) {
        checksum ^= g_camera_capture_buffer[i];
        checksum = (checksum << 1U) | (checksum >> 31U);
        if(0U != g_camera_capture_buffer[i]) {
            nonzero_words++;
        }
        if((i > 0U) && (g_camera_capture_buffer[i] == g_camera_capture_buffer[i - 1U])) {
            repeated_words++;
        }
    }

    if(RESET != dma_done) {
        camera_frame_quality_t measured_quality = camera_estimate_frame_quality();
        if(NULL != quality) {
            *quality = measured_quality;
        }
    } else if(NULL != quality) {
        quality->valid = 0U;
        quality->best_phase = 0U;
        quality->row_range = 0U;
        quality->col_range = 0U;
        quality->neighbor_delta = 0U;
        quality->score = 0U;
    }

    printf("camera_capture[%s]: data_order=0x%02X/read0x%02X dma=%s words=%lu remain=%lu nonzero=%lu repeated=%lu checksum=0x%08lX quality=phase%u,row_range=%lu,col_range=%lu,neighbor_delta=%lu,score=%lu hs0=%u vs0=%u hs1=%u vs1=%u fv=%u ef=%u ovr=%u vsif=%u elif=%u dmaerr=%u%u%u first=%08lX,%08lX,%08lX,%08lX mid=%08lX,%08lX,%08lX,%08lX last=%08lX,%08lX,%08lX,%08lX\r\n",
           tag,
           data_order,
           data_order_readback,
           (RESET != dma_done) ? "done" : "timeout",
           (unsigned long)CAMERA_CAPTURE_WORDS,
           (unsigned long)dma_remaining,
           (unsigned long)nonzero_words,
           (unsigned long)repeated_words,
           (unsigned long)checksum,
           (NULL != quality) ? quality->best_phase : 0U,
           (NULL != quality) ? (unsigned long)quality->row_range : 0UL,
           (NULL != quality) ? (unsigned long)quality->col_range : 0UL,
           (NULL != quality) ? (unsigned long)quality->neighbor_delta : 0UL,
           (NULL != quality) ? (unsigned long)quality->score : 0UL,
           (SET == hs_before) ? 1U : 0U,
           (SET == vs_before) ? 1U : 0U,
           (SET == dci_flag_get(DCI_FLAG_HS)) ? 1U : 0U,
           (SET == dci_flag_get(DCI_FLAG_VS)) ? 1U : 0U,
           (SET == dci_fv) ? 1U : 0U,
           (SET == dci_ef) ? 1U : 0U,
           (SET == dci_ovr) ? 1U : 0U,
           (SET == dci_vsync) ? 1U : 0U,
           (SET == dci_el) ? 1U : 0U,
           (SET == dma_tae) ? 1U : 0U,
           (SET == dma_sde) ? 1U : 0U,
           (SET == dma_fee) ? 1U : 0U,
           (unsigned long)g_camera_capture_buffer[0],
           (unsigned long)g_camera_capture_buffer[1],
           (unsigned long)g_camera_capture_buffer[2],
           (unsigned long)g_camera_capture_buffer[3],
           (unsigned long)g_camera_capture_buffer[middle_index],
           (unsigned long)g_camera_capture_buffer[middle_index + 1U],
           (unsigned long)g_camera_capture_buffer[middle_index + 2U],
           (unsigned long)g_camera_capture_buffer[middle_index + 3U],
           (unsigned long)g_camera_capture_buffer[last_index],
           (unsigned long)g_camera_capture_buffer[last_index + 1U],
           (unsigned long)g_camera_capture_buffer[last_index + 2U],
           (unsigned long)g_camera_capture_buffer[last_index + 3U]);

    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TC)) {
    }

    return (RESET != dma_done) ? 1U : 0U;
}

static void camera_pattern_source_apply(const char *tag,
                                        uint8_t reg_503d,
                                        uint8_t reg_4741,
                                        uint8_t polarity_reg)
{
    uint8_t read_501f = 0U;
    uint8_t read_503d = 0U;
    uint8_t read_5584 = 0U;
    uint8_t read_4741 = 0U;
    uint8_t read_471d = 0U;
    uint8_t read_4740 = 0U;

    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
    if(0U != reg_503d) {
        (void)camera_sccb_write_reg16(0x5584U, 0x40U);
    } else {
        (void)camera_sccb_write_reg16(0x5584U, 0x10U);
    }
    (void)camera_sccb_write_reg16(0x503DU, reg_503d);
    (void)camera_sccb_write_reg16(0x4741U, reg_4741);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, polarity_reg);
    delay_1ms(50U);
    (void)camera_sccb_read_reg16(0x501FU, &read_501f);
    (void)camera_sccb_read_reg16(0x503DU, &read_503d);
    (void)camera_sccb_read_reg16(0x5584U, &read_5584);
    (void)camera_sccb_read_reg16(0x4741U, &read_4741);
    (void)camera_sccb_read_reg16(0x471DU, &read_471d);
    (void)camera_sccb_read_reg16(0x4740U, &read_4740);

    printf("camera_pattern_source_sweep[%s]: set_501f=0x00/read0x%02X set_503d=0x%02X/read0x%02X set_5584=0x%02X/read0x%02X set_4741=0x%02X/read0x%02X set_471d=0x00/read0x%02X set_4740=0x%02X/read0x%02X\r\n",
           tag,
           read_501f,
           reg_503d,
           read_503d,
           (0U != reg_503d) ? 0x40U : 0x10U,
           read_5584,
           reg_4741,
           read_4741,
           read_471d,
           polarity_reg,
           read_4740);
}

static void camera_pattern_source_capture(const char *tag,
                                          const char *raw_tag,
                                          const char *all_tag,
                                          const char *href_high_tag,
                                          const char *href_low_tag,
                                          uint8_t reg_503d,
                                          uint8_t reg_4741,
                                          uint8_t polarity_reg)
{
    camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};

    camera_pattern_source_apply(tag, reg_503d, reg_4741, polarity_reg);
    camera_raw_pclk_sample_probe_tag(raw_tag, all_tag, href_high_tag, href_low_tag);
    camera_isp_path_regs_log(tag);
    (void)edgecare_camera_capture_attempt(tag,
                                          DCI_CK_POLARITY_FALLING,
                                          DCI_HSYNC_POLARITY_LOW,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                          &quality);
}

static void camera_pattern_source_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_PATTERN_SOURCE_SWEEP
    printf("camera_pattern_source_sweep: enabled source=OV5640_503d_4741_polarity_ab data_order=0x%02X note=compare_4740_0x20_vs_0x22\r\n",
           (unsigned int)EDGECARE_CAMERA_DATA_ORDER_DEFAULT);

    printf("camera_pattern_source_sweep[isp_colorbar]: request 503d=0x80 4741=0x00 polarity=0x20/0x22\r\n");
    camera_pattern_source_capture("source_isp_colorbar_p20",
                                  "source_isp_colorbar_p20",
                                  "source_isp_colorbar_p20_all",
                                  "source_isp_colorbar_p20_href_high",
                                  "source_isp_colorbar_p20_href_low",
                                  0x80U,
                                  0x00U,
                                  0x20U);
    camera_pattern_source_capture("source_isp_colorbar_p22",
                                  "source_isp_colorbar_p22",
                                  "source_isp_colorbar_p22_all",
                                  "source_isp_colorbar_p22_href_high",
                                  "source_isp_colorbar_p22_href_low",
                                  0x80U,
                                  0x00U,
                                  0x22U);

    printf("camera_pattern_source_sweep[dvp_pattern]: request 503d=0x00 4741=0x05 polarity=0x20 bus_health_control\r\n");
    camera_pattern_source_capture("source_dvp_pattern_p20",
                                  "source_dvp_pattern_p20",
                                  "source_dvp_pattern_p20_all",
                                  "source_dvp_pattern_p20_href_high",
                                  "source_dvp_pattern_p20_href_low",
                                  0x00U,
                                  0x05U,
                                  0x20U);

    printf("camera_pattern_source_sweep[real_scene]: request 503d=0x00 4741=0x00 polarity=0x20/0x22\r\n");
    camera_pattern_source_capture("source_real_scene_p20",
                                  "source_real_scene_p20",
                                  "source_real_scene_p20_all",
                                  "source_real_scene_p20_href_high",
                                  "source_real_scene_p20_href_low",
                                  0x00U,
                                  0x00U,
                                  0x20U);
    camera_pattern_source_capture("source_real_scene_p22",
                                  "source_real_scene_p22",
                                  "source_real_scene_p22_all",
                                  "source_real_scene_p22_href_high",
                                  "source_real_scene_p22_href_low",
                                  0x00U,
                                  0x00U,
                                  0x22U);
#endif
}

static void camera_data_order_source_sweep_apply(const char *source_tag,
                                                 const char *raw_tag,
                                                 const char *all_tag,
                                                 const char *href_high_tag,
                                                 const char *href_low_tag,
                                                 uint8_t reg_503d,
                                                 uint8_t reg_4741,
                                                 uint8_t data_order)
{
    uint8_t read_501f = 0U;
    uint8_t read_503d = 0U;
    uint8_t read_5584 = 0U;
    uint8_t read_4741 = 0U;
    uint8_t read_4745 = 0U;

    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
    if(0U != reg_503d) {
        (void)camera_sccb_write_reg16(0x5584U, 0x40U);
    } else {
        (void)camera_sccb_write_reg16(0x5584U, 0x10U);
    }
    (void)camera_sccb_write_reg16(0x503DU, reg_503d);
    (void)camera_sccb_write_reg16(0x4741U, reg_4741);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
    (void)camera_sccb_write_reg16(0x4745U, data_order);
    delay_1ms(50U);

    (void)camera_sccb_read_reg16(0x501FU, &read_501f);
    (void)camera_sccb_read_reg16(0x503DU, &read_503d);
    (void)camera_sccb_read_reg16(0x5584U, &read_5584);
    (void)camera_sccb_read_reg16(0x4741U, &read_4741);
    (void)camera_sccb_read_reg16(0x4745U, &read_4745);
    printf("camera_data_order_source_sweep[%s]: order=0x%02X/read0x%02X set_501f=0x00/read0x%02X set_503d=0x%02X/read0x%02X set_5584=0x%02X/read0x%02X set_4741=0x%02X/read0x%02X\r\n",
           source_tag,
           data_order,
           read_4745,
           read_501f,
           reg_503d,
           read_503d,
           (0U != reg_503d) ? 0x40U : 0x10U,
           read_5584,
           reg_4741,
           read_4741);
    camera_raw_pclk_sample_probe_tag(raw_tag, all_tag, href_high_tag, href_low_tag);
}

static void camera_ov5640_data_order_source_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP
    static const uint8_t data_orders[] = {
        0x00U,
        0x01U,
        0x02U,
        0x03U,
        0x04U,
        0x05U,
        0x06U,
        0x07U
    };
    static const char *const isp_raw_tags[] = {
        "order_isp_colorbar_00",
        "order_isp_colorbar_01",
        "order_isp_colorbar_02",
        "order_isp_colorbar_03",
        "order_isp_colorbar_04",
        "order_isp_colorbar_05",
        "order_isp_colorbar_06",
        "order_isp_colorbar_07"
    };
    static const char *const isp_all_tags[] = {
        "order_isp_colorbar_00_all",
        "order_isp_colorbar_01_all",
        "order_isp_colorbar_02_all",
        "order_isp_colorbar_03_all",
        "order_isp_colorbar_04_all",
        "order_isp_colorbar_05_all",
        "order_isp_colorbar_06_all",
        "order_isp_colorbar_07_all"
    };
    static const char *const isp_high_tags[] = {
        "order_isp_colorbar_00_href_high",
        "order_isp_colorbar_01_href_high",
        "order_isp_colorbar_02_href_high",
        "order_isp_colorbar_03_href_high",
        "order_isp_colorbar_04_href_high",
        "order_isp_colorbar_05_href_high",
        "order_isp_colorbar_06_href_high",
        "order_isp_colorbar_07_href_high"
    };
    static const char *const isp_low_tags[] = {
        "order_isp_colorbar_00_href_low",
        "order_isp_colorbar_01_href_low",
        "order_isp_colorbar_02_href_low",
        "order_isp_colorbar_03_href_low",
        "order_isp_colorbar_04_href_low",
        "order_isp_colorbar_05_href_low",
        "order_isp_colorbar_06_href_low",
        "order_isp_colorbar_07_href_low"
    };
    static const char *const real_raw_tags[] = {
        "order_real_scene_00",
        "order_real_scene_01",
        "order_real_scene_02",
        "order_real_scene_03",
        "order_real_scene_04",
        "order_real_scene_05",
        "order_real_scene_06",
        "order_real_scene_07"
    };
    static const char *const real_all_tags[] = {
        "order_real_scene_00_all",
        "order_real_scene_01_all",
        "order_real_scene_02_all",
        "order_real_scene_03_all",
        "order_real_scene_04_all",
        "order_real_scene_05_all",
        "order_real_scene_06_all",
        "order_real_scene_07_all"
    };
    static const char *const real_high_tags[] = {
        "order_real_scene_00_href_high",
        "order_real_scene_01_href_high",
        "order_real_scene_02_href_high",
        "order_real_scene_03_href_high",
        "order_real_scene_04_href_high",
        "order_real_scene_05_href_high",
        "order_real_scene_06_href_high",
        "order_real_scene_07_href_high"
    };
    static const char *const real_low_tags[] = {
        "order_real_scene_00_href_low",
        "order_real_scene_01_href_low",
        "order_real_scene_02_href_low",
        "order_real_scene_03_href_low",
        "order_real_scene_04_href_low",
        "order_real_scene_05_href_low",
        "order_real_scene_06_href_low",
        "order_real_scene_07_href_low"
    };
    uint8_t saved_4745 = 0U;
    uint8_t saved_503d = 0U;
    uint8_t saved_5584 = 0U;
    uint8_t saved_4741 = 0U;
    uint32_t i;

    (void)camera_sccb_read_reg16(0x4745U, &saved_4745);
    (void)camera_sccb_read_reg16(0x503DU, &saved_503d);
    (void)camera_sccb_read_reg16(0x5584U, &saved_5584);
    (void)camera_sccb_read_reg16(0x4741U, &saved_4741);
    printf("camera_data_order_source_sweep: enabled orders=0x00..0x07 saved_4745=0x%02X saved_503d=0x%02X saved_5584=0x%02X saved_4741=0x%02X note=raw_pclk_only\r\n",
           saved_4745,
           saved_503d,
           saved_5584,
           saved_4741);

    printf("camera_data_order_source_sweep[isp_colorbar]: request 501f=0x00 503d=0x80 4741=0x00 orders=0x00..0x07\r\n");
    for(i = 0U; i < (sizeof(data_orders) / sizeof(data_orders[0])); i++) {
        camera_data_order_source_sweep_apply("isp_colorbar",
                                             isp_raw_tags[i],
                                             isp_all_tags[i],
                                             isp_high_tags[i],
                                             isp_low_tags[i],
                                             0x80U,
                                             0x00U,
                                             data_orders[i]);
    }

    printf("camera_data_order_source_sweep[real_scene]: request 501f=0x00 503d=0x00 4741=0x00 orders=0x00..0x07\r\n");
    for(i = 0U; i < (sizeof(data_orders) / sizeof(data_orders[0])); i++) {
        camera_data_order_source_sweep_apply("real_scene",
                                             real_raw_tags[i],
                                             real_all_tags[i],
                                             real_high_tags[i],
                                             real_low_tags[i],
                                             0x00U,
                                             0x00U,
                                             data_orders[i]);
    }

    (void)camera_sccb_write_reg16(0x503DU, saved_503d);
    (void)camera_sccb_write_reg16(0x5584U, saved_5584);
    (void)camera_sccb_write_reg16(0x4741U, saved_4741);
    (void)camera_sccb_write_reg16(0x4745U, saved_4745);
    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
    delay_1ms(50U);
    camera_isp_path_regs_log("data_order_source_sweep_restored");
#endif
}

static void camera_ov5640_raw_data_order_capture_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_RAW_DATA_ORDER_CAPTURE_SWEEP
    camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};

    printf("camera_raw_data_order_capture_sweep: enabled source=OV5640_501f_snr_raw timing=471d00_474022 dci=pclk_rising_hs_blank_high_vs_blank_high orders=0x00..0x07\r\n");
    camera_apply_raw_capture_path();
    (void)edgecare_camera_capture_attempt("raw_order_00",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x00U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_01",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x01U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_02",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x02U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_03",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x03U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_04",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x04U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_05",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x05U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_06",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x06U,
                                          &quality);
    (void)edgecare_camera_capture_attempt("raw_order_07",
                                          DCI_CK_POLARITY_RISING,
                                          DCI_HSYNC_POLARITY_HIGH,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          0x07U,
                                          &quality);
    (void)camera_sccb_write_reg16(0x4745U, EDGECARE_CAMERA_DATA_ORDER_DEFAULT);
    delay_1ms(20U);
    camera_isp_path_regs_log("raw_data_order_capture_sweep_restored");
#endif
}

#if EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP
static void camera_jpeg_to_yuv_byte_scale_log(const char *tag, uint8_t data_order);
#endif

static void camera_ov5640_jpeg_to_yuv_data_order_capture_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP
    static const uint8_t data_orders[] = {
        0x00U,
        0x01U,
        0x02U,
        0x03U,
        0x04U,
        0x05U,
        0x06U,
        0x07U
    };
    static const char *const tags[] = {
        "jpeg_yuv_order_00",
        "jpeg_yuv_order_01",
        "jpeg_yuv_order_02",
        "jpeg_yuv_order_03",
        "jpeg_yuv_order_04",
        "jpeg_yuv_order_05",
        "jpeg_yuv_order_06",
        "jpeg_yuv_order_07"
    };
    camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};
    uint32_t i;
    uint8_t capture_ok;

    printf("camera_jpeg_to_yuv_data_order_capture_sweep: enabled source=OV5640_JPEG_TO_YUV_REF timing=471d00_474020 dci=pclk_rising_hs_blank_low_vs_blank_high orders=0x00..0x07 note=test_byte_scale_vs_4745_mapping\r\n");
    camera_apply_jpeg_to_yuv_ref_capture_path();
    for(i = 0U; i < (sizeof(data_orders) / sizeof(data_orders[0])); i++) {
        capture_ok = edgecare_camera_capture_attempt(tags[i],
                                                     DCI_CK_POLARITY_RISING,
                                                     DCI_HSYNC_POLARITY_LOW,
                                                     DCI_VSYNC_POLARITY_HIGH,
                                                     data_orders[i],
                                                     &quality);
        if(0U != capture_ok) {
            camera_jpeg_to_yuv_byte_scale_log(tags[i], data_orders[i]);
        } else {
            printf("camera_byte_scale[%s]: order=0x%02X skipped=capture_timeout\r\n",
                   tags[i],
                   data_orders[i]);
        }
    }
    (void)camera_sccb_write_reg16(0x4745U, EDGECARE_CAMERA_DATA_ORDER_DEFAULT);
    delay_1ms(20U);
    camera_isp_path_regs_log("jpeg_to_yuv_data_order_capture_sweep_restored");
#endif
}

#if EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP
typedef struct {
    uint8_t raw_min;
    uint8_t raw_max;
    uint32_t raw_mean;
    uint8_t shifted_min;
    uint8_t shifted_max;
    uint32_t shifted_mean;
} camera_byte_phase_stats_t;

static uint8_t camera_byte_lshift2(uint8_t value)
{
    return (value > 63U) ? 255U : (uint8_t)(value << 2U);
}

static void camera_byte_phase_stats(uint32_t phase, camera_byte_phase_stats_t *stats)
{
    const uint8_t *bytes = (const uint8_t *)g_camera_capture_buffer;
    uint32_t offset;
    uint32_t count = 0U;
    uint32_t raw_sum = 0U;
    uint32_t shifted_sum = 0U;
    uint8_t raw_min = 0xFFU;
    uint8_t raw_max = 0x00U;
    uint8_t shifted_min = 0xFFU;
    uint8_t shifted_max = 0x00U;
    uint8_t raw;
    uint8_t shifted;

    for(offset = phase; offset < CAMERA_CAPTURE_BYTES; offset += 4U) {
        raw = bytes[offset];
        shifted = camera_byte_lshift2(raw);

        if(raw < raw_min) {
            raw_min = raw;
        }
        if(raw > raw_max) {
            raw_max = raw;
        }
        if(shifted < shifted_min) {
            shifted_min = shifted;
        }
        if(shifted > shifted_max) {
            shifted_max = shifted;
        }
        raw_sum += raw;
        shifted_sum += shifted;
        count++;
    }

    if(0U == count) {
        stats->raw_min = 0U;
        stats->raw_max = 0U;
        stats->raw_mean = 0U;
        stats->shifted_min = 0U;
        stats->shifted_max = 0U;
        stats->shifted_mean = 0U;
        return;
    }

    stats->raw_min = raw_min;
    stats->raw_max = raw_max;
    stats->raw_mean = raw_sum / count;
    stats->shifted_min = shifted_min;
    stats->shifted_max = shifted_max;
    stats->shifted_mean = shifted_sum / count;
}

static uint8_t camera_byte_scale_is_right_shift2_like(const camera_byte_phase_stats_t *phase0,
                                                      const camera_byte_phase_stats_t *phase1,
                                                      const camera_byte_phase_stats_t *phase2,
                                                      const camera_byte_phase_stats_t *phase3)
{
    uint8_t y_low_range = ((phase0->raw_max <= 64U) &&
                           (phase2->raw_max <= 64U) &&
                           (((phase0->raw_mean > phase2->raw_mean) ? phase0->raw_mean : phase2->raw_mean) >= 8U)) ? 1U : 0U;
    uint8_t chroma_near_32 = ((phase1->raw_mean >= 24U) &&
                              (phase1->raw_mean <= 40U) &&
                              (phase3->raw_mean >= 24U) &&
                              (phase3->raw_mean <= 40U) &&
                              (phase1->raw_max <= 64U) &&
                              (phase3->raw_max <= 64U)) ? 1U : 0U;
    uint8_t chroma_stable = (((uint32_t)phase1->raw_max - (uint32_t)phase1->raw_min <= 16U) &&
                             ((uint32_t)phase3->raw_max - (uint32_t)phase3->raw_min <= 16U)) ? 1U : 0U;

    return (0U != y_low_range && 0U != chroma_near_32 && 0U != chroma_stable) ? 1U : 0U;
}

static void camera_jpeg_to_yuv_byte_scale_log(const char *tag, uint8_t data_order)
{
    camera_byte_phase_stats_t phase0;
    camera_byte_phase_stats_t phase1;
    camera_byte_phase_stats_t phase2;
    camera_byte_phase_stats_t phase3;
    uint8_t right_shift2_like;

    camera_byte_phase_stats(0U, &phase0);
    camera_byte_phase_stats(1U, &phase1);
    camera_byte_phase_stats(2U, &phase2);
    camera_byte_phase_stats(3U, &phase3);
    right_shift2_like = camera_byte_scale_is_right_shift2_like(&phase0, &phase1, &phase2, &phase3);

    printf("camera_byte_scale[%s]: order=0x%02X hint=%s p0_raw=%u,%u,%lu p0_lshift2=%u,%u,%lu p1_raw=%u,%u,%lu p1_lshift2=%u,%u,%lu p2_raw=%u,%u,%lu p2_lshift2=%u,%u,%lu p3_raw=%u,%u,%lu p3_lshift2=%u,%u,%lu\r\n",
           tag,
           data_order,
           (0U != right_shift2_like) ? "right_shift2_like" : "not_right_shift2_like",
           phase0.raw_min,
           phase0.raw_max,
           (unsigned long)phase0.raw_mean,
           phase0.shifted_min,
           phase0.shifted_max,
           (unsigned long)phase0.shifted_mean,
           phase1.raw_min,
           phase1.raw_max,
           (unsigned long)phase1.raw_mean,
           phase1.shifted_min,
           phase1.shifted_max,
           (unsigned long)phase1.shifted_mean,
           phase2.raw_min,
           phase2.raw_max,
           (unsigned long)phase2.raw_mean,
           phase2.shifted_min,
           phase2.shifted_max,
           (unsigned long)phase2.shifted_mean,
           phase3.raw_min,
           phase3.raw_max,
           (unsigned long)phase3.raw_mean,
           phase3.shifted_min,
           phase3.shifted_max,
           (unsigned long)phase3.shifted_mean);
}
#endif

static void camera_output_mux_capture(const char *tag,
                                      const char *raw_tag,
                                      const char *all_tag,
                                      const char *href_high_tag,
                                      const char *href_low_tag,
                                      uint8_t reg_501f)
{
    camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};
    uint8_t read_501f = 0U;
    uint8_t read_503d = 0U;
    uint8_t read_4741 = 0U;
    uint8_t read_471d = 0U;
    uint8_t read_4740 = 0U;

    (void)camera_sccb_write_reg16(0x501FU, reg_501f);
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
    delay_1ms(80U);
    (void)camera_sccb_read_reg16(0x501FU, &read_501f);
    (void)camera_sccb_read_reg16(0x503DU, &read_503d);
    (void)camera_sccb_read_reg16(0x4741U, &read_4741);
    (void)camera_sccb_read_reg16(0x471DU, &read_471d);
    (void)camera_sccb_read_reg16(0x4740U, &read_4740);

    printf("camera_output_mux_sweep[%s]: set_501f=0x%02X/read0x%02X 503d=0x%02X 4741=0x%02X 471d=0x%02X 4740=0x%02X\r\n",
           tag,
           reg_501f,
           read_501f,
           read_503d,
           read_4741,
           read_471d,
           read_4740);
    camera_raw_pclk_sample_probe_tag(raw_tag, all_tag, href_high_tag, href_low_tag);
    camera_isp_path_regs_log(tag);
    (void)edgecare_camera_capture_attempt(tag,
                                          DCI_CK_POLARITY_FALLING,
                                          DCI_HSYNC_POLARITY_LOW,
                                          DCI_VSYNC_POLARITY_HIGH,
                                          EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                          &quality);
}

static void camera_ov5640_output_mux_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_OUTPUT_MUX_SWEEP
    uint8_t saved_501f = 0U;
    uint8_t saved_503d = 0U;
    uint8_t saved_4741 = 0U;

    (void)camera_sccb_read_reg16(0x501FU, &saved_501f);
    (void)camera_sccb_read_reg16(0x503DU, &saved_503d);
    (void)camera_sccb_read_reg16(0x4741U, &saved_4741);

    printf("camera_output_mux_sweep: enabled source=OV5640_datasheet_501f saved_501f=0x%02X saved_503d=0x%02X saved_4741=0x%02X note=real_scene_source\r\n",
           saved_501f,
           saved_503d,
           saved_4741);

    printf("camera_output_mux_sweep[mux_isp_yuv422]: request 501f=0x00\r\n");
    camera_output_mux_capture("mux_isp_yuv422",
                              "mux_isp_yuv422",
                              "mux_isp_yuv422_all",
                              "mux_isp_yuv422_href_high",
                              "mux_isp_yuv422_href_low",
                              0x00U);

    printf("camera_output_mux_sweep[mux_isp_raw_dpc]: request 501f=0x03\r\n");
    camera_output_mux_capture("mux_isp_raw_dpc",
                              "mux_isp_raw_dpc",
                              "mux_isp_raw_dpc_all",
                              "mux_isp_raw_dpc_href_high",
                              "mux_isp_raw_dpc_href_low",
                              0x03U);

    printf("camera_output_mux_sweep[mux_snr_raw]: request 501f=0x04\r\n");
    camera_output_mux_capture("mux_snr_raw",
                              "mux_snr_raw",
                              "mux_snr_raw_all",
                              "mux_snr_raw_href_high",
                              "mux_snr_raw_href_low",
                              0x04U);

    printf("camera_output_mux_sweep[mux_isp_raw_cip]: request 501f=0x05\r\n");
    camera_output_mux_capture("mux_isp_raw_cip",
                              "mux_isp_raw_cip",
                              "mux_isp_raw_cip_all",
                              "mux_isp_raw_cip_href_high",
                              "mux_isp_raw_cip_href_low",
                              0x05U);

    (void)camera_sccb_write_reg16(0x501FU, saved_501f);
    (void)camera_sccb_write_reg16(0x503DU, saved_503d);
    (void)camera_sccb_write_reg16(0x4741U, saved_4741);
    delay_1ms(50U);
    camera_isp_path_regs_log("output_mux_restored");
#endif
}

static void camera_ov5640_raw_dci_matrix_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_RAW_DCI_MATRIX
    static const camera_capture_variant_t variants[] = {
        {"raw_matrix_fall_hs_low_vs_low", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_LOW},
        {"raw_matrix_fall_hs_low_vs_high", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"raw_matrix_fall_hs_high_vs_low", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_LOW},
        {"raw_matrix_fall_hs_high_vs_high", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_HIGH},
        {"raw_matrix_rise_hs_low_vs_low", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_LOW},
        {"raw_matrix_rise_hs_low_vs_high", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"raw_matrix_rise_hs_high_vs_low", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_LOW},
        {"raw_matrix_rise_hs_high_vs_high", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_HIGH}
    };
    uint8_t saved_501f = 0U;
    uint8_t saved_503d = 0U;
    uint8_t saved_4741 = 0U;
    uint32_t i;

    (void)camera_sccb_read_reg16(0x501FU, &saved_501f);
    (void)camera_sccb_read_reg16(0x503DU, &saved_503d);
    (void)camera_sccb_read_reg16(0x4741U, &saved_4741);

    printf("camera_raw_dci_matrix: enabled source=OV5640_501f_snr_raw saved_501f=0x%02X saved_503d=0x%02X saved_4741=0x%02X variants=%lu\r\n",
           saved_501f,
           saved_503d,
           saved_4741,
           (unsigned long)(sizeof(variants) / sizeof(variants[0])));
    camera_apply_raw_capture_path();

    for(i = 0U; i < (sizeof(variants) / sizeof(variants[0])); i++) {
        camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};

        (void)edgecare_camera_capture_attempt(variants[i].tag,
                                              variants[i].clock_polarity,
                                              variants[i].hsync_polarity,
                                              variants[i].vsync_polarity,
                                              EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                              &quality);
    }

    (void)camera_sccb_write_reg16(0x501FU, saved_501f);
    (void)camera_sccb_write_reg16(0x503DU, saved_503d);
    (void)camera_sccb_write_reg16(0x4741U, saved_4741);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
    delay_1ms(50U);
    printf("camera_raw_dci_matrix: restored 501f=0x%02X 503d=0x%02X 4741=0x%02X\r\n",
           saved_501f,
           saved_503d,
           saved_4741);
#endif
}

static void camera_apply_raw_capture_path(void)
{
    (void)camera_sccb_write_reg16(0x501FU, 0x04U);
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#if EDGECARE_CAMERA_RELEASE_CLOCK_RESET_FOR_RAW
    (void)camera_sccb_write_reg16(0x3000U, 0x00U);
    (void)camera_sccb_write_reg16(0x3002U, 0x00U);
    (void)camera_sccb_write_reg16(0x3004U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3006U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3007U, 0xFFU);
#endif
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x22U);
    delay_1ms(80U);
#if EDGECARE_CAMERA_RELEASE_CLOCK_RESET_FOR_RAW
    camera_isp_path_regs_log("raw_clock_reset_release");
#endif
}

static void camera_apply_st_dvp_reference_path(void)
{
#if EDGECARE_ENABLE_OV5640_ST_DVP_REFERENCE_PATH
    uint32_t i;
    uint8_t read_3017 = 0U;
    uint8_t read_3018 = 0U;
    uint8_t read_300e = 0U;
    uint8_t read_302e = 0U;
    uint8_t read_3034 = 0U;
    uint8_t read_3035 = 0U;
    uint8_t read_3036 = 0U;
    uint8_t read_3037 = 0U;
    uint8_t read_3108 = 0U;
    uint8_t read_3808 = 0U;
    uint8_t read_3809 = 0U;
    uint8_t read_380a = 0U;
    uint8_t read_380b = 0U;
    uint8_t read_4300 = 0U;
    uint8_t read_4740 = 0U;
    uint8_t read_501f = 0U;

    printf("camera_st_dvp_reference: enabled source=STMicroelectronics_OV5640_DVP_QVGA_YUV422 regs=%lu note=photo_path_overlay\r\n",
           (unsigned long)(sizeof(g_ov5640_st_dvp_reference_regs) / sizeof(g_ov5640_st_dvp_reference_regs[0])));
    for(i = 0U; i < (sizeof(g_ov5640_st_dvp_reference_regs) / sizeof(g_ov5640_st_dvp_reference_regs[0])); i++) {
        (void)camera_sccb_write_reg16(g_ov5640_st_dvp_reference_regs[i].reg,
                                      g_ov5640_st_dvp_reference_regs[i].value);
        delay_1ms(1U);
    }

    (void)camera_sccb_read_reg16(0x3017U, &read_3017);
    (void)camera_sccb_read_reg16(0x3018U, &read_3018);
    (void)camera_sccb_read_reg16(0x300EU, &read_300e);
    (void)camera_sccb_read_reg16(0x302EU, &read_302e);
    (void)camera_sccb_read_reg16(0x3034U, &read_3034);
    (void)camera_sccb_read_reg16(0x3035U, &read_3035);
    (void)camera_sccb_read_reg16(0x3036U, &read_3036);
    (void)camera_sccb_read_reg16(0x3037U, &read_3037);
    (void)camera_sccb_read_reg16(0x3108U, &read_3108);
    (void)camera_sccb_read_reg16(0x3808U, &read_3808);
    (void)camera_sccb_read_reg16(0x3809U, &read_3809);
    (void)camera_sccb_read_reg16(0x380AU, &read_380a);
    (void)camera_sccb_read_reg16(0x380BU, &read_380b);
    (void)camera_sccb_read_reg16(0x4300U, &read_4300);
    (void)camera_sccb_read_reg16(0x4740U, &read_4740);
    (void)camera_sccb_read_reg16(0x501FU, &read_501f);
    printf("camera_st_dvp_reference_readback: 3017=0x%02X 3018=0x%02X 300e=0x%02X 302e=0x%02X pll=3034:%02X_3035:%02X_3036:%02X_3037:%02X 3108=0x%02X size=%ux%u 4300=0x%02X 4740=0x%02X 501f=0x%02X\r\n",
           read_3017,
           read_3018,
           read_300e,
           read_302e,
           read_3034,
           read_3035,
           read_3036,
           read_3037,
           read_3108,
           (uint16_t)(((uint16_t)read_3808 << 8U) | read_3809),
           (uint16_t)(((uint16_t)read_380a << 8U) | read_380b),
           read_4300,
           read_4740,
           read_501f);
#endif
}

static void camera_apply_dvp_pattern_capture_path(void)
{
    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x05U);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
    delay_1ms(80U);
}

static void camera_apply_rgb565_capture_path(void)
{
    uint8_t read_4300 = 0U;
    uint8_t read_501f = 0U;
    uint8_t read_503d = 0U;
    uint8_t read_4741 = 0U;
    uint8_t read_4740 = 0U;

#if EDGECARE_ENABLE_OV5640_ST_DVP_REFERENCE_PATH
    camera_apply_st_dvp_reference_path();
#endif
    /* OV5640_RGB565 follows ST's pixel-format path: 4300=0x6F, 501F=0x01. */
    (void)camera_sccb_write_reg16(0x4300U, 0x6FU);
    (void)camera_sccb_write_reg16(0x501FU, 0x01U);
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
    delay_1ms(120U);

    (void)camera_sccb_read_reg16(0x4300U, &read_4300);
    (void)camera_sccb_read_reg16(0x501FU, &read_501f);
    (void)camera_sccb_read_reg16(0x503DU, &read_503d);
    (void)camera_sccb_read_reg16(0x4741U, &read_4741);
    (void)camera_sccb_read_reg16(0x4740U, &read_4740);
    printf("camera_rgb565_path_readback: 4300=0x%02X 501f=0x%02X 503d=0x%02X 4741=0x%02X 4740=0x%02X note=OV5640_RGB565_real_scene\r\n",
           read_4300,
           read_501f,
           read_503d,
           read_4741,
           read_4740);
}

static void camera_apply_isp_yuv_capture_path(void)
{
    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
#if EDGECARE_CAMERA_RELEASE_CLOCK_RESET_FOR_RAW
    (void)camera_sccb_write_reg16(0x3000U, 0x00U);
    (void)camera_sccb_write_reg16(0x3002U, 0x00U);
    (void)camera_sccb_write_reg16(0x3004U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3006U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3007U, 0xFFU);
#endif
#if EDGECARE_ENABLE_CAMERA_TEST_PATTERN
#if EDGECARE_CAMERA_TEST_PATTERN_MODE == 1U
    (void)camera_sccb_write_reg16(0x503DU, 0x80U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#elif EDGECARE_CAMERA_TEST_PATTERN_MODE == 2U
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x05U);
#else
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#endif
#else
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
#endif
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
#if EDGECARE_ENABLE_OV5640_ST_DVP_REFERENCE_PATH
    camera_apply_st_dvp_reference_path();
#else
    (void)camera_sccb_write_reg16(0x4740U, 0x22U);
#endif
    delay_1ms(120U);
#if EDGECARE_CAMERA_RELEASE_CLOCK_RESET_FOR_RAW
    camera_isp_path_regs_log("isp_yuv_clock_reset_release");
#endif
}

static void camera_apply_jpeg_to_yuv_ref_capture_path(void)
{
#if EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP
    uint8_t read_3034 = 0U;
    uint8_t read_3035 = 0U;
    uint8_t read_3036 = 0U;
    uint8_t read_3037 = 0U;
#endif

    (void)camera_sccb_write_reg16(0x3002U, 0x1CU);
    (void)camera_sccb_write_reg16(0x3006U, 0xC3U);
    (void)camera_sccb_write_reg16(0x3821U, 0x07U);
    (void)camera_sccb_write_reg16(0x4300U, 0x30U);
    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
#if EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP
    printf("camera_jpeg_to_yuv_force_8bit_dvp: request 3034=0x18 note=single_variable_bitmode_ab\r\n");
    (void)camera_sccb_write_reg16(0x3034U, 0x18U);
#endif
#if EDGECARE_CAMERA_JPEG_TO_YUV_VFIFO_REF_CANDIDATE
    printf("camera_jpeg_to_yuv_vfifo_ref_candidate: request 460b=0x35 460c=0x22 3824=0x04 note=single_variable_vfifo_ab\r\n");
    (void)camera_sccb_write_reg16(0x460CU, 0x22U);
    (void)camera_sccb_write_reg16(0x3824U, 0x04U);
    (void)camera_sccb_write_reg16(0x460BU, 0x35U);
#elif EDGECARE_CAMERA_JPEG_TO_YUV_PCLKDIV08_CANDIDATE
    printf("camera_jpeg_to_yuv_pclkdiv08_candidate: request 460b=0x37 460c=0x20 3824=0x08 note=single_variable_pclkdiv_ab\r\n");
    (void)camera_sccb_write_reg16(0x460CU, 0x20U);
    (void)camera_sccb_write_reg16(0x3824U, 0x08U);
    (void)camera_sccb_write_reg16(0x460BU, 0x37U);
#else
    (void)camera_sccb_write_reg16(0x460CU, 0x20U);
    (void)camera_sccb_write_reg16(0x3824U, 0x04U);
    (void)camera_sccb_write_reg16(0x460BU, 0x37U);
#endif
    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
    (void)camera_sccb_write_reg16(0x471DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4740U, 0x20U);
#if EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE
    printf("camera_jpeg_to_yuv_manual_exposure: request 3503=0x07 exposure=0x%04X gain=0x%03X note=moderate_brightness_ab\r\n",
           EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES,
           EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN);
    (void)camera_sccb_write_reg16(0x3503U, 0x07U);
    (void)camera_sccb_write_reg16(0x3500U, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES >> 12U) & 0x0FU));
    (void)camera_sccb_write_reg16(0x3501U, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES >> 4U) & 0xFFU));
    (void)camera_sccb_write_reg16(0x3502U, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_EXPOSURE_LINES & 0x0FU) << 4U));
    (void)camera_sccb_write_reg16(0x350AU, (uint8_t)((EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN >> 8U) & 0x03U));
    (void)camera_sccb_write_reg16(0x350BU, (uint8_t)(EDGECARE_CAMERA_JPEG_TO_YUV_MANUAL_GAIN & 0xFFU));
#endif
    delay_1ms(120U);
#if EDGECARE_CAMERA_JPEG_TO_YUV_FORCE_8BIT_DVP
    (void)camera_sccb_read_reg16(0x3034U, &read_3034);
    (void)camera_sccb_read_reg16(0x3035U, &read_3035);
    (void)camera_sccb_read_reg16(0x3036U, &read_3036);
    (void)camera_sccb_read_reg16(0x3037U, &read_3037);
    printf("camera_jpeg_to_yuv_force_8bit_dvp_readback: 3034=0x%02X 3035=0x%02X 3036=0x%02X 3037=0x%02X\r\n",
           read_3034,
           read_3035,
           read_3036,
           read_3037);
#endif
    camera_isp_path_regs_log("normal_jpeg_to_yuv_ref");
}

static void camera_apply_jpeg_to_yuv_ref_tune_variant(const char *tag,
                                                      uint8_t reg_460b,
                                                      uint8_t reg_460c,
                                                      uint8_t reg_3824,
                                                      uint8_t reg_471d,
                                                      uint8_t reg_4740)
{
    camera_apply_jpeg_to_yuv_ref_capture_path();
    (void)camera_sccb_write_reg16(0x460CU, reg_460c);
    (void)camera_sccb_write_reg16(0x3824U, reg_3824);
    (void)camera_sccb_write_reg16(0x460BU, reg_460b);
    (void)camera_sccb_write_reg16(0x471DU, reg_471d);
    (void)camera_sccb_write_reg16(0x4740U, reg_4740);
    delay_1ms(120U);
    camera_isp_path_regs_log(tag);
}

static void camera_ov5640_jpeg_to_yuv_tune_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_TUNE_SWEEP
    static const camera_jpeg_to_yuv_tune_variant_t variants[] = {
        {"jpeg_tune_base_ref", 0x37U, 0x20U, 0x04U, 0x00U, 0x20U,
         DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"jpeg_tune_vfifo_init", 0x35U, 0x22U, 0x04U, 0x00U, 0x20U,
         DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"jpeg_tune_pclkdiv08", 0x37U, 0x20U, 0x08U, 0x00U, 0x20U,
         DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"jpeg_tune_pclkdiv02", 0x37U, 0x20U, 0x02U, 0x00U, 0x20U,
         DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"jpeg_tune_polarity22_rise_hh", 0x37U, 0x20U, 0x04U, 0x00U, 0x22U,
         DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_HIGH},
        {"jpeg_tune_href_gate24", 0x37U, 0x20U, 0x04U, 0x00U, 0x24U,
         DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH}
    };
    uint32_t i;

    printf("camera_jpeg_to_yuv_tune_sweep: enabled source=OV5640_JPEG_TO_YUV_REF variants=%lu note=compare_stripe_brightness_window\r\n",
           (unsigned long)(sizeof(variants) / sizeof(variants[0])));
    for(i = 0U; i < (sizeof(variants) / sizeof(variants[0])); i++) {
        camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};

        printf("camera_jpeg_to_yuv_tune_sweep[%s]: request 460b=0x%02X 460c=0x%02X 3824=0x%02X 471d=0x%02X 4740=0x%02X\r\n",
               variants[i].tag,
               variants[i].reg_460b,
               variants[i].reg_460c,
               variants[i].reg_3824,
               variants[i].reg_471d,
               variants[i].reg_4740);
        camera_apply_jpeg_to_yuv_ref_tune_variant(variants[i].tag,
                                                  variants[i].reg_460b,
                                                  variants[i].reg_460c,
                                                  variants[i].reg_3824,
                                                  variants[i].reg_471d,
                                                  variants[i].reg_4740);
        (void)edgecare_camera_capture_attempt(variants[i].tag,
                                              variants[i].clock_polarity,
                                              variants[i].hsync_polarity,
                                              variants[i].vsync_polarity,
                                              EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                              &quality);
    }
#endif
}

static void camera_ov5640_isp_path_sweep_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_ISP_PATH_SWEEP
    camera_isp_path_regs_t saved = {0U};

    camera_isp_path_regs_read(&saved);
    printf("camera_isp_path_sweep: enabled source=OV5640_sensor_ISP_clock_reset_diag note=restore_after_each_candidate\r\n");
    camera_isp_path_regs_log("isp_path_baseline");

    printf("camera_isp_path_sweep[clock_reset_release]: request 3000=0x00 3002=0x00 3004=0xFF 3006=0xFF 3007=0xFF real_scene\r\n");
    (void)camera_sccb_write_reg16(0x3000U, 0x00U);
    (void)camera_sccb_write_reg16(0x3002U, 0x00U);
    (void)camera_sccb_write_reg16(0x3004U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3006U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3007U, 0xFFU);
    delay_1ms(80U);
    camera_pattern_source_capture("isp_path_clock_reset_release",
                                  "isp_path_clock_reset_release",
                                  "isp_path_clock_reset_release_all",
                                  "isp_path_clock_reset_release_href_high",
                                  "isp_path_clock_reset_release_href_low",
                                  0x00U,
                                  0x00U,
                                  0x20U);
    camera_isp_path_regs_restore(&saved);
    delay_1ms(50U);

    printf("camera_isp_path_sweep[manual_bright]: request 3503=0x07 exposure=0x0FFF gain=0x03FF real_scene\r\n");
    (void)camera_sccb_write_reg16(0x3503U, 0x07U);
    (void)camera_sccb_write_reg16(0x3500U, 0x00U);
    (void)camera_sccb_write_reg16(0x3501U, 0xFFU);
    (void)camera_sccb_write_reg16(0x3502U, 0xF0U);
    (void)camera_sccb_write_reg16(0x350AU, 0x03U);
    (void)camera_sccb_write_reg16(0x350BU, 0xFFU);
    delay_1ms(120U);
    camera_pattern_source_capture("isp_path_manual_bright",
                                  "isp_path_manual_bright",
                                  "isp_path_manual_bright_all",
                                  "isp_path_manual_bright_href_high",
                                  "isp_path_manual_bright_href_low",
                                  0x00U,
                                  0x00U,
                                  0x20U);
    camera_isp_path_regs_restore(&saved);
    delay_1ms(50U);

    printf("camera_isp_path_sweep[isp_controls_full]: request 5000=0xFF 5001=0xFF 501f=0x00 isp_colorbar\r\n");
    (void)camera_sccb_write_reg16(0x5000U, 0xFFU);
    (void)camera_sccb_write_reg16(0x5001U, 0xFFU);
    (void)camera_sccb_write_reg16(0x501FU, 0x00U);
    delay_1ms(80U);
    camera_pattern_source_capture("isp_path_isp_controls_full_colorbar",
                                  "isp_path_isp_controls_full_colorbar",
                                  "isp_path_isp_controls_full_colorbar_all",
                                  "isp_path_isp_controls_full_colorbar_href_high",
                                  "isp_path_isp_controls_full_colorbar_href_low",
                                  0x80U,
                                  0x00U,
                                  0x20U);
    camera_isp_path_regs_restore(&saved);
    delay_1ms(50U);

    printf("camera_isp_path_sweep[jpeg_to_yuv_ref]: request app_note_13_1_7_2 real_scene\r\n");
    camera_apply_jpeg_to_yuv_ref_capture_path();
    camera_pattern_source_capture("isp_path_jpeg_to_yuv_ref",
                                  "isp_path_jpeg_to_yuv_ref",
                                  "isp_path_jpeg_to_yuv_ref_all",
                                  "isp_path_jpeg_to_yuv_ref_href_high",
                                  "isp_path_jpeg_to_yuv_ref_href_low",
                                  0x00U,
                                  0x00U,
                                  0x20U);
    camera_isp_path_regs_restore(&saved);
    delay_1ms(50U);

    printf("camera_isp_path_sweep[blanking_gate_href]: request 4740=0x24 real_scene\r\n");
    (void)camera_sccb_write_reg16(0x4740U, 0x24U);
    delay_1ms(80U);
    camera_pattern_source_capture("isp_path_blanking_gate_href",
                                  "isp_path_blanking_gate_href",
                                  "isp_path_blanking_gate_href_all",
                                  "isp_path_blanking_gate_href_href_high",
                                  "isp_path_blanking_gate_href_href_low",
                                  0x00U,
                                  0x00U,
                                  0x20U);
    camera_isp_path_regs_restore(&saved);
    delay_1ms(50U);

    (void)camera_sccb_write_reg16(0x503DU, 0x00U);
    (void)camera_sccb_write_reg16(0x4741U, 0x00U);
    camera_isp_path_regs_log("isp_path_restored_real_scene");
#endif
}

uint8_t bsp_camera_ov5640_capture_probe(void)
{
    camera_frame_quality_t normal_quality = {0U, 0U, 0U, 0U, 0U, 0U};

    printf("camera_capture_probe: start qvga=%ux%u bytes=%lu words=%lu timeout=%lu test_pattern=%u mode=%u slow_pclk=%u pclkdiv=0x%02X snapshot=%u sweep=%u data_order_default=0x%02X data_order_sweep=%u data_order_source_sweep=%u raw_order_capture_sweep=%u jpeg_yuv_order_capture_sweep=%u\r\n",
           (unsigned int)CAMERA_FRAME_WIDTH,
           (unsigned int)CAMERA_FRAME_HEIGHT,
           (unsigned long)CAMERA_CAPTURE_BYTES,
           (unsigned long)CAMERA_CAPTURE_WORDS,
           (unsigned long)CAMERA_CAPTURE_TIMEOUT,
           (unsigned int)EDGECARE_ENABLE_CAMERA_TEST_PATTERN,
           (unsigned int)EDGECARE_CAMERA_TEST_PATTERN_MODE,
           (unsigned int)EDGECARE_ENABLE_CAMERA_SLOW_PCLK,
           (unsigned int)EDGECARE_CAMERA_PCLK_DIV_REG,
           (unsigned int)EDGECARE_ENABLE_CAMERA_SNAPSHOT_CAPTURE,
           (unsigned int)EDGECARE_ENABLE_CAMERA_CAPTURE_SWEEP,
           (unsigned int)EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
           (unsigned int)EDGECARE_ENABLE_CAMERA_DATA_ORDER_SWEEP,
           (unsigned int)EDGECARE_ENABLE_CAMERA_DATA_ORDER_SOURCE_SWEEP,
           (unsigned int)EDGECARE_ENABLE_CAMERA_RAW_DATA_ORDER_CAPTURE_SWEEP,
           (unsigned int)EDGECARE_ENABLE_CAMERA_JPEG_TO_YUV_DATA_ORDER_CAPTURE_SWEEP);
    printf("camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PC11 D5=PB6 D6=PE5 D7=PB9\r\n");
    camera_log_dvp_registers("before_capture");
    camera_dvp_gpio_activity_probe();
#if EDGECARE_ENABLE_CAMERA_SYNC_TIMING_DIAGS
    camera_ov5640_sync_output_sweep();
    camera_ov5640_timing_sweep();
#endif
    camera_ov5640_dvp_mode_sweep();
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
    camera_dci_sync_matrix_probe();
#endif
    camera_log_dvp_registers("after_dvp_mode_sweep");
    camera_ov5640_data_pad_sweep();
    camera_raw_pclk_sample_probe();
#if EDGECARE_ENABLE_CAMERA_DCI_STATUS_PROBE
    camera_capture_mode_sweep_probe();
#endif
    camera_pattern_source_sweep_probe();
    camera_ov5640_data_order_source_sweep_probe();
    camera_ov5640_raw_data_order_capture_sweep_probe();
    camera_ov5640_jpeg_to_yuv_data_order_capture_sweep_probe();
    camera_ov5640_output_mux_sweep_probe();
    camera_ov5640_raw_dci_matrix_probe();
    camera_ov5640_isp_path_sweep_probe();
    camera_ov5640_jpeg_to_yuv_tune_sweep_probe();

#if EDGECARE_CAMERA_NORMAL_OUTPUT_DVP_PATTERN
    printf("camera_capture_normal: source=OV5640_DVP_PATTERN output_mux=0x00 timing=471d00_474020 dci=pclk_falling_hs_blank_low_vs_blank_high proof=not_real_scene\r\n");
    camera_apply_dvp_pattern_capture_path();
    return edgecare_camera_capture_attempt("normal_dvp_pattern",
                                           DCI_CK_POLARITY_FALLING,
                                           DCI_HSYNC_POLARITY_LOW,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &normal_quality);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_RGB565
    printf("camera_capture_normal: source=OV5640_RGB565 output_mux=0x01 timing=471d00_474020 dci=pclk_falling_hs_blank_low_vs_blank_high\r\n");
    camera_apply_rgb565_capture_path();
    return edgecare_camera_capture_attempt("normal_rgb565",
                                           DCI_CK_POLARITY_FALLING,
                                           DCI_HSYNC_POLARITY_LOW,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &normal_quality);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_ISP_YUV
    printf("camera_capture_normal: source=OV5640_ISP_YUV422 output_mux=0x00 timing=471d00_474022 dci=pclk_rising_hs_blank_high_vs_blank_high\r\n");
    camera_apply_isp_yuv_capture_path();
    return edgecare_camera_capture_attempt("normal_isp_yuv422",
                                           DCI_CK_POLARITY_RISING,
                                           DCI_HSYNC_POLARITY_HIGH,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &normal_quality);
#elif EDGECARE_CAMERA_NORMAL_OUTPUT_JPEG_TO_YUV_REF
    camera_apply_jpeg_to_yuv_ref_capture_path();
#if EDGECARE_ENABLE_CAMERA_WINDOW_READBACK
    camera_window_readback_log("normal_jpeg_to_yuv_ref");
#endif
#if EDGECARE_CAMERA_JPEG_TO_YUV_DCI_RISING
    printf("camera_capture_normal: source=OV5640_JPEG_TO_YUV_REF output_mux=0x00 timing=471d00_474020 dci=pclk_rising_hs_blank_low_vs_blank_high note=photo_proof_candidate\r\n");
    return edgecare_camera_capture_attempt("normal_jpeg_to_yuv_ref",
                                           DCI_CK_POLARITY_RISING,
                                           DCI_HSYNC_POLARITY_LOW,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &normal_quality);
#else
    printf("camera_capture_normal: source=OV5640_JPEG_TO_YUV_REF output_mux=0x00 timing=471d00_474020 dci=pclk_falling_hs_blank_low_vs_blank_high note=photo_proof_candidate\r\n");
    return edgecare_camera_capture_attempt("normal_jpeg_to_yuv_ref",
                                           DCI_CK_POLARITY_FALLING,
                                           DCI_HSYNC_POLARITY_LOW,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &normal_quality);
#endif
#else
    printf("camera_capture_normal: source=OV5640_SNR_RAW8 output_mux=0x04 timing=471d00_474022 dci=pclk_rising_hs_blank_high_vs_blank_high\r\n");
    camera_apply_raw_capture_path();
    return edgecare_camera_capture_attempt("normal_snr_raw8",
                                           DCI_CK_POLARITY_RISING,
                                           DCI_HSYNC_POLARITY_HIGH,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &normal_quality);
#endif
}


const uint8_t *bsp_camera_ov5640_frame(void)
{
    return (const uint8_t *)g_camera_capture_buffer;
}
