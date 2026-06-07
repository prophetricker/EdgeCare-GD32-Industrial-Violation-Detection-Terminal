/*!
    \file    main.c
    \brief   EdgeCare state-machine bring-up for GD32H759

    Current bring-up scope:
      - active-low alarm lamp on PA8
      - USART0 printf log on EVAL_COM
      - LD2410 digital OUT presence trigger on PF8
      - OV5640/OV2640 SCCB ID probe on PB10/PB11
      - DCI/DMA one-shot capture probe with timeout
*/

#include "gd32h7xx.h"
#include "gd32h759i_start.h"
#include "systick.h"
#include "board/board_config.h"
#include "bsp/bsp_alarm.h"
#include "bsp/bsp_radar_ld2410.h"
#include "model/edgecare_infer.h"
#include "platform/edgecare_log.h"
#include "vision/edgecare_preprocess.h"
#include <stdio.h>
#include <string.h>

#define DCI_DATA_ADDRESS       ((uint32_t)&DCI_DATA)

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

typedef struct {
    uint16_t reg;
    uint8_t value;
} camera_reg8_t;

static edgecare_context_t g_edgecare = {
    EDGECARE_STATE_IDLE,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U
};

static uint32_t g_camera_capture_buffer[CAMERA_CAPTURE_WORDS] __attribute__((aligned(32)));
static uint8_t g_model_input_gray[MODEL_INPUT_BYTES] __attribute__((aligned(32)));

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

static void cache_enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
}

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

    /* Default guess: RES high releases reset, PWON/PWDN low powers the sensor. */
    gpio_bit_reset(CAMERA_CTRL_GPIO_PORT, CAMERA_PWON_PIN);
    gpio_bit_reset(CAMERA_CTRL_GPIO_PORT, CAMERA_RES_PIN);
    delay_1ms(5U);
    gpio_bit_set(CAMERA_CTRL_GPIO_PORT, CAMERA_RES_PIN);
    delay_1ms(20U);

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

static uint8_t edgecare_camera_id_probe(void)
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
        printf("camera_id: OV5640 ID read timeout/no ack on PB10/PB11\r\n");
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

static uint8_t edgecare_ov5640_qvga_stream_init(void)
{
    uint32_t i;
    uint8_t stream_ctl = 0U;
    uint8_t width_h = 0U;
    uint8_t width_l = 0U;
    uint8_t height_h = 0U;
    uint8_t height_l = 0U;

    printf("camera_init: ov5640 qvga yuv probe regs=%lu source=OV5640 app note 13.1.1/13.1.2\r\n",
           (unsigned long)(sizeof(g_ov5640_qvga_yuv_probe_regs) / sizeof(g_ov5640_qvga_yuv_probe_regs[0])));

    if(!camera_sccb_write_reg16(0x3008U, 0x82U)) {
        printf("camera_init: soft_reset write failed\r\n");
        return 0U;
    }
    delay_1ms(10U);

    for(i = 0U; i < (sizeof(g_ov5640_qvga_yuv_probe_regs) / sizeof(g_ov5640_qvga_yuv_probe_regs[0])); i++) {
        if(!camera_sccb_write_reg16(g_ov5640_qvga_yuv_probe_regs[i].reg,
                                    g_ov5640_qvga_yuv_probe_regs[i].value)) {
            printf("camera_init: write_failed reg=0x%04lX value=0x%02X index=%lu\r\n",
                   (unsigned long)g_ov5640_qvga_yuv_probe_regs[i].reg,
                   g_ov5640_qvga_yuv_probe_regs[i].value,
                   (unsigned long)i);
            return 0U;
        }
        if(0x3008U == g_ov5640_qvga_yuv_probe_regs[i].reg) {
            delay_1ms(5U);
        }
    }

    delay_1ms(100U);

    (void)camera_sccb_read_reg16(0x3008U, &stream_ctl);
    (void)camera_sccb_read_reg16(0x3808U, &width_h);
    (void)camera_sccb_read_reg16(0x3809U, &width_l);
    (void)camera_sccb_read_reg16(0x380AU, &height_h);
    (void)camera_sccb_read_reg16(0x380BU, &height_l);
    printf("camera_init: readback 3008=0x%02X size=%ux%u\r\n",
           stream_ctl,
           (uint16_t)(((uint16_t)width_h << 8U) | width_l),
           (uint16_t)(((uint16_t)height_h << 8U) | height_l));

    return 1U;
}

static void camera_dvp_gpio_activity_probe(void)
{
    uint32_t burst;
    uint32_t i;
    uint32_t pclk_edges = 0U;
    uint32_t href_edges = 0U;
    uint32_t sync_edges = 0U;
    uint32_t href_high_samples = 0U;
    uint32_t sync_high_samples = 0U;
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

    for(burst = 0U; burst < 80U; burst++) {
        for(i = 0U; i < 50000U; i++) {
            pclk_now = gpio_input_bit_get(GPIOA, GPIO_PIN_6);
            href_now = gpio_input_bit_get(GPIOA, GPIO_PIN_4);
            sync_now = gpio_input_bit_get(GPIOB, GPIO_PIN_7);

            if(SET == href_now) {
                href_high_samples++;
            }
            if(SET == sync_now) {
                sync_high_samples++;
            }

            if(pclk_now != pclk_prev) {
                pclk_edges++;
                pclk_prev = pclk_now;
            }
            if(href_now != href_prev) {
                href_edges++;
                href_prev = href_now;
            }
            if(sync_now != sync_prev) {
                sync_edges++;
                sync_prev = sync_now;
            }
        }
    }

    printf("camera_dvp_gpio: bursts=80 samples_per_burst=50000 pclk_edges=%lu href_edges=%lu sync_edges=%lu href_high=%lu sync_high=%lu pclk=%u href=%u sync=%u\r\n",
           (unsigned long)pclk_edges,
           (unsigned long)href_edges,
           (unsigned long)sync_edges,
           (unsigned long)href_high_samples,
           (unsigned long)sync_high_samples,
           (SET == pclk_prev) ? 1U : 0U,
           (SET == href_prev) ? 1U : 0U,
           (SET == sync_prev) ? 1U : 0U);
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

    /* D0(PC6), D1(PC7), D2(PC8), D3(PG11), D4(PE4), D5(PB6), D6(PE5), D7(PE6). */
    gpio_af_set(GPIOC, GPIO_AF_13, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
    gpio_af_set(GPIOG, GPIO_AF_13, GPIO_PIN_11);
    gpio_af_set(GPIOE, GPIO_AF_13, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_af_set(GPIOB, GPIO_AF_13, GPIO_PIN_6);

    gpio_mode_set(GPIOC, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
    gpio_output_options_set(GPIOC, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ,
                            GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8);
    gpio_mode_set(GPIOG, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_11);
    gpio_output_options_set(GPIOG, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_11);
    gpio_mode_set(GPIOE, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ,
                            GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6);
    gpio_mode_set(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_6);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100_220MHZ, GPIO_PIN_6);
}

static void camera_dci_dma_init(uint32_t hsync_polarity, uint32_t vsync_polarity)
{
    dci_parameter_struct dci_struct;
    dma_single_data_parameter_struct dma_struct;

    camera_dci_gpio_init();

    dci_deinit();
    dci_struct.capture_mode = DCI_CAPTURE_MODE_CONTINUOUS;
    dci_struct.clock_polarity = DCI_CK_POLARITY_RISING;
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

static void edgecare_preprocess_gray96_probe(void)
{
    edgecare_preprocess_gray96_stats_t stats;
    edgecare_infer_result_t infer_result;

    edgecare_preprocess_gray96_from_yuyv((const uint8_t *)g_camera_capture_buffer,
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

static void edgecare_camera_capture_attempt(const char *tag, uint32_t hsync_polarity, uint32_t vsync_polarity)
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

    memset(g_camera_capture_buffer, 0, sizeof(g_camera_capture_buffer));
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)g_camera_capture_buffer, sizeof(g_camera_capture_buffer));

    camera_dci_dma_init(hsync_polarity, vsync_polarity);

    hs_before = dci_flag_get(DCI_FLAG_HS);
    vs_before = dci_flag_get(DCI_FLAG_VS);
    dma_flag_clear(DMA1, DMA_CH7, DMA_FLAG_FTF | DMA_FLAG_HTF | DMA_FLAG_TAE | DMA_FLAG_SDE | DMA_FLAG_FEE);
    dci_interrupt_flag_clear(DCI_INT_FLAG_EF | DCI_INT_FLAG_OVR | DCI_INT_FLAG_VSYNC | DCI_INT_FLAG_EL);

    dma_channel_enable(DMA1, DMA_CH7);
    dci_enable();
    dci_capture_enable();

    while(RESET == dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_FTF)) {
        if(0U == timeout--) {
            break;
        }
    }

    dci_capture_disable();
    dci_disable();

    dma_done = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_FTF);
    dma_tae = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_TAE);
    dma_sde = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_SDE);
    dma_fee = dma_flag_get(DMA1, DMA_CH7, DMA_FLAG_FEE);
    dci_ef = dci_flag_get(DCI_FLAG_EF);
    dci_ovr = dci_flag_get(DCI_FLAG_OVR);
    dci_vsync = dci_flag_get(DCI_FLAG_VSYNC);
    dci_el = dci_flag_get(DCI_FLAG_EL);
    dci_fv = dci_flag_get(DCI_FLAG_FV);

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

    printf("camera_capture[%s]: dma=%s words=%lu nonzero=%lu repeated=%lu checksum=0x%08lX hs0=%u vs0=%u hs1=%u vs1=%u fv=%u ef=%u ovr=%u vsif=%u elif=%u dmaerr=%u%u%u first=%08lX,%08lX,%08lX,%08lX mid=%08lX,%08lX,%08lX,%08lX last=%08lX,%08lX,%08lX,%08lX\r\n",
           tag,
           (RESET != dma_done) ? "done" : "timeout",
           (unsigned long)CAMERA_CAPTURE_WORDS,
           (unsigned long)nonzero_words,
           (unsigned long)repeated_words,
           (unsigned long)checksum,
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

    if(RESET != dma_done) {
        edgecare_preprocess_gray96_probe();
    }
}

static void edgecare_camera_capture_probe(void)
{
    printf("camera_capture_probe: start qvga=%ux%u bytes=%lu words=%lu timeout=%lu working_pol=hs_blank_low_vs_blank_high\r\n",
           (unsigned int)CAMERA_FRAME_WIDTH,
           (unsigned int)CAMERA_FRAME_HEIGHT,
           (unsigned long)CAMERA_CAPTURE_BYTES,
           (unsigned long)CAMERA_CAPTURE_WORDS,
           (unsigned long)CAMERA_CAPTURE_TIMEOUT);
    printf("camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PE4 D5=PB6 D6=PE5 D7=PE6\r\n");
    camera_dvp_gpio_activity_probe();

    edgecare_camera_capture_attempt("working_hs_blank_low_vs_blank_high",
                                    DCI_HSYNC_POLARITY_LOW,
                                    DCI_VSYNC_POLARITY_HIGH);
}

static void alarm_set(uint8_t active)
{
    g_edgecare.alarm_active = active ? 1U : 0U;
    bsp_alarm_set_active(g_edgecare.alarm_active);
}

static void edgecare_step(void)
{
    g_edgecare.radar_triggered = bsp_radar_ld2410_is_triggered();
    g_edgecare.infer_ms = 0U;

    if(g_edgecare.radar_triggered) {
        g_edgecare.state = EDGECARE_STATE_ALARM;
        g_edgecare.confidence_percent = 100U;
        alarm_set(1U);
    } else {
        g_edgecare.state = EDGECARE_STATE_IDLE;
        g_edgecare.confidence_percent = 0U;
        alarm_set(0U);
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

int main(void)
{
    uint8_t camera_detected;

    cache_enable();
    systick_config();
    gd_eval_com_init(EVAL_COM);
    bsp_alarm_init();
    bsp_radar_ld2410_init();

    printf("\r\n%s boot: EdgeCare GD32H759 terminal bring-up\r\n", EDGECARE_DEVICE_ID);
    printf("log_format: [ts_ms] state=... radar=... infer_ms=... conf=... alarm=... seq=...\r\n");
    printf("radar_input: LD2410 OUT active-high on PF8, alarm active-low on PA8\r\n");
    printf("camera_sccb: SCL=PB10 SDA=PB11 RES=PD0 PWON=PD1\r\n");
    printf("camera_capture_probe: enabled; expect one camera_capture line before periodic state logs\r\n");
    camera_detected = edgecare_camera_id_probe();
    if(camera_detected) {
        (void)edgecare_ov5640_qvga_stream_init();
    } else {
        printf("camera_init: skipped because OV5640 ID was not confirmed\r\n");
    }
    edgecare_camera_capture_probe();

    while(1) {
        edgecare_step();
        delay_1ms(EDGECARE_LOG_PERIOD_MS);
        g_edgecare.ts_ms += EDGECARE_LOG_PERIOD_MS;
    }
}

#ifdef GD_ECLIPSE_GCC
int __io_putchar(int ch)
{
    usart_data_transmit(EVAL_COM, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TBE)) {
    }
    return ch;
}
#else
int fputc(int ch, FILE *f)
{
    (void)f;
    usart_data_transmit(EVAL_COM, (uint8_t)ch);
    while(RESET == usart_flag_get(EVAL_COM, USART_FLAG_TBE)) {
    }
    return ch;
}
#endif
