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

static uint32_t g_camera_capture_buffer[CAMERA_CAPTURE_WORDS] __attribute__((aligned(32)));
static void camera_log_dvp_registers(const char *stage);

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

static void camera_log_dvp_registers(const char *stage)
{
    uint8_t reg_300e = 0U;
    uint8_t reg_3017 = 0U;
    uint8_t reg_3018 = 0U;
    uint8_t reg_4300 = 0U;
    uint8_t reg_501f = 0U;
    uint8_t reg_460b = 0U;
    uint8_t reg_460c = 0U;
    uint8_t reg_3824 = 0U;
    uint8_t reg_471b = 0U;
    uint8_t reg_471d = 0U;
    uint8_t reg_4730 = 0U;
    uint8_t reg_4740 = 0U;
    uint8_t reg_4741 = 0U;
    uint8_t reg_4745 = 0U;
    uint8_t reg_503d = 0U;

    (void)camera_sccb_read_reg16(0x300EU, &reg_300e);
    (void)camera_sccb_read_reg16(0x3017U, &reg_3017);
    (void)camera_sccb_read_reg16(0x3018U, &reg_3018);
    (void)camera_sccb_read_reg16(0x4300U, &reg_4300);
    (void)camera_sccb_read_reg16(0x501FU, &reg_501f);
    (void)camera_sccb_read_reg16(0x460BU, &reg_460b);
    (void)camera_sccb_read_reg16(0x460CU, &reg_460c);
    (void)camera_sccb_read_reg16(0x3824U, &reg_3824);
    (void)camera_sccb_read_reg16(0x471BU, &reg_471b);
    (void)camera_sccb_read_reg16(0x471DU, &reg_471d);
    (void)camera_sccb_read_reg16(0x4730U, &reg_4730);
    (void)camera_sccb_read_reg16(0x4740U, &reg_4740);
    (void)camera_sccb_read_reg16(0x4741U, &reg_4741);
    (void)camera_sccb_read_reg16(0x4745U, &reg_4745);
    (void)camera_sccb_read_reg16(0x503DU, &reg_503d);

    printf("camera_dvp_regs[%s]: 300e=0x%02X 3017=0x%02X 3018=0x%02X 4300=0x%02X 501f=0x%02X 460b=0x%02X 460c=0x%02X 3824=0x%02X\r\n",
           stage,
           reg_300e,
           reg_3017,
           reg_3018,
           reg_4300,
           reg_501f,
           reg_460b,
           reg_460c,
           reg_3824);
    printf("camera_dvp_regs[%s]: 471b=0x%02X 471d=0x%02X 4730=0x%02X 4740=0x%02X 4741=0x%02X 4745=0x%02X 503d=0x%02X\r\n",
           stage,
           reg_471b,
           reg_471d,
           reg_4730,
           reg_4740,
           reg_4741,
           reg_4745,
           reg_503d);
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

uint8_t bsp_camera_ov5640_qvga_stream_init(void)
{
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

static void camera_dci_dma_init(uint32_t clock_polarity, uint32_t hsync_polarity, uint32_t vsync_polarity)
{
    dci_parameter_struct dci_struct;
    dma_single_data_parameter_struct dma_struct;

    camera_dci_gpio_init();

    dci_deinit();
#if EDGECARE_ENABLE_CAMERA_SNAPSHOT_CAPTURE
    dci_struct.capture_mode = DCI_CAPTURE_MODE_SNAPSHOT;
#else
    dci_struct.capture_mode = DCI_CAPTURE_MODE_CONTINUOUS;
#endif
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

    memset(g_camera_capture_buffer, 0, sizeof(g_camera_capture_buffer));
    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)g_camera_capture_buffer, sizeof(g_camera_capture_buffer));

    (void)camera_sccb_write_reg16(0x4745U, data_order);
    delay_1ms(2U);
    (void)camera_sccb_read_reg16(0x4745U, &data_order_readback);

    camera_dci_dma_init(clock_polarity, hsync_polarity, vsync_polarity);

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

uint8_t bsp_camera_ov5640_capture_probe(void)
{
#if EDGECARE_ENABLE_CAMERA_DATA_ORDER_SWEEP
    static const camera_capture_variant_t variants[] = {
        {"pclk_falling_hs_blank_low_vs_blank_high", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"pclk_rising_hs_blank_low_vs_blank_high", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH}
    };
    static const uint8_t data_orders[] = {
        EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
        0x00U,
        0x01U,
        0x03U,
        0x04U,
        0x05U,
        0x06U,
        0x07U
    };
#else
    static const camera_capture_variant_t variants[] = {
        {"pclk_falling_hs_blank_low_vs_blank_low", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_LOW},
        {"pclk_falling_hs_blank_low_vs_blank_high", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"pclk_falling_hs_blank_high_vs_blank_low", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_LOW},
        {"pclk_falling_hs_blank_high_vs_blank_high", DCI_CK_POLARITY_FALLING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_HIGH},
        {"pclk_rising_hs_blank_low_vs_blank_low", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_LOW},
        {"pclk_rising_hs_blank_low_vs_blank_high", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_LOW, DCI_VSYNC_POLARITY_HIGH},
        {"pclk_rising_hs_blank_high_vs_blank_low", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_LOW},
        {"pclk_rising_hs_blank_high_vs_blank_high", DCI_CK_POLARITY_RISING, DCI_HSYNC_POLARITY_HIGH, DCI_VSYNC_POLARITY_HIGH}
    };
    static const uint8_t data_orders[] = {
        EDGECARE_CAMERA_DATA_ORDER_DEFAULT
    };
#endif
    uint8_t best_valid = 0U;
    uint32_t best_index = 0U;
    uint8_t best_data_order = EDGECARE_CAMERA_DATA_ORDER_DEFAULT;
    camera_frame_quality_t best_quality = {0U, 0U, 0U, 0U, 0U, 0U};

    printf("camera_capture_probe: start qvga=%ux%u bytes=%lu words=%lu timeout=%lu test_pattern=%u mode=%u slow_pclk=%u pclkdiv=0x%02X snapshot=%u sweep=%u data_order_default=0x%02X data_order_sweep=%u\r\n",
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
           (unsigned int)EDGECARE_ENABLE_CAMERA_DATA_ORDER_SWEEP);
    printf("camera_dvp: PCLK=PA6 HREF=PA4 SYNC=PB7 D0=PC6 D1=PC7 D2=PC8 D3=PG11 D4=PE4 D5=PB6 D6=PE5 D7=PE6\r\n");
    camera_log_dvp_registers("before_capture");
    camera_dvp_gpio_activity_probe();

#if EDGECARE_ENABLE_CAMERA_CAPTURE_SWEEP
    {
        uint32_t order_index;

        for(order_index = 0U; order_index < (sizeof(data_orders) / sizeof(data_orders[0])); order_index++) {
            uint32_t i;

            for(i = 0U; i < (sizeof(variants) / sizeof(variants[0])); i++) {
                camera_frame_quality_t quality = {0U, 0U, 0U, 0U, 0U, 0U};

                if(edgecare_camera_capture_attempt(variants[i].tag,
                                                   variants[i].clock_polarity,
                                                   variants[i].hsync_polarity,
                                                   variants[i].vsync_polarity,
                                                   data_orders[order_index],
                                                   &quality)) {
                    if((0U == best_valid) || (quality.score > best_quality.score)) {
                        best_valid = 1U;
                        best_index = i;
                        best_data_order = data_orders[order_index];
                        best_quality = quality;
                    }
                }
            }
        }
    }

    if(best_valid) {
        camera_frame_quality_t final_quality = {0U, 0U, 0U, 0U, 0U, 0U};

        printf("camera_capture_best: tag=%s data_order=0x%02X phase=%u row_range=%lu col_range=%lu neighbor_delta=%lu score=%lu recapture=1\r\n",
               variants[best_index].tag,
               best_data_order,
               best_quality.best_phase,
               (unsigned long)best_quality.row_range,
               (unsigned long)best_quality.col_range,
               (unsigned long)best_quality.neighbor_delta,
               (unsigned long)best_quality.score);

        return edgecare_camera_capture_attempt("selected_best_for_dump",
                                               variants[best_index].clock_polarity,
                                               variants[best_index].hsync_polarity,
                                               variants[best_index].vsync_polarity,
                                               best_data_order,
                                               &final_quality);
    }

    printf("camera_capture_best: none; all capture variants timed out\r\n");
    return 0U;
#else
    return edgecare_camera_capture_attempt("working_pclk_rising_hs_blank_low_vs_blank_high",
                                           DCI_CK_POLARITY_RISING,
                                           DCI_HSYNC_POLARITY_LOW,
                                           DCI_VSYNC_POLARITY_HIGH,
                                           EDGECARE_CAMERA_DATA_ORDER_DEFAULT,
                                           &best_quality);
#endif
}


const uint8_t *bsp_camera_ov5640_frame(void)
{
    return (const uint8_t *)g_camera_capture_buffer;
}
