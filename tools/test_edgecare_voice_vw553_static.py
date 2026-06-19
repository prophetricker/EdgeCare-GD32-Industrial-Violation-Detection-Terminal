from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PROJECT = (
    ROOT
    / "EdgeCare_GD32_Industrial_Violation_Terminal"
    / "GD32H759I_START_Demo_Suites"
    / "Projects"
    / "01_EdgeCare_Industrial_Violation_Terminal"
)


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, path: Path) -> None:
    assert needle in text, f"{path} missing {needle!r}"


def test_voice_uart_uses_independent_usart1_pa2_pa3() -> None:
    board_path = PROJECT / "board" / "board_config.h"
    board_h = read_text(board_path)

    require_contains(board_h, "#define VOICE_VW553_USART        USART1", board_path)
    require_contains(board_h, "#define VOICE_VW553_TX_PIN       GPIO_PIN_2", board_path)
    require_contains(board_h, "#define VOICE_VW553_RX_PIN       GPIO_PIN_3", board_path)
    require_contains(board_h, "#define VOICE_VW553_GPIO_AF      GPIO_AF_7", board_path)
    require_contains(board_h, '#define VOICE_VW553_DANGER_COMMAND "DANGER\\n"', board_path)


def test_voice_bsp_initializes_and_sends_danger_command() -> None:
    header_path = PROJECT / "bsp" / "bsp_voice_vw553.h"
    source_path = PROJECT / "bsp" / "bsp_voice_vw553.c"
    header = read_text(header_path)
    source = read_text(source_path)

    require_contains(header, "void bsp_voice_vw553_init(void);", header_path)
    require_contains(header, "void bsp_voice_vw553_send_danger(void);", header_path)
    require_contains(source, "rcu_periph_clock_enable(VOICE_VW553_USART_RCU);", source_path)
    require_contains(source, "gpio_af_set(VOICE_VW553_GPIO_PORT", source_path)
    require_contains(source, "usart_baudrate_set(VOICE_VW553_USART, VOICE_VW553_BAUDRATE);", source_path)
    require_contains(source, "VOICE_VW553_DANGER_COMMAND", source_path)


def test_app_triggers_voice_only_on_alarm_rising_edge() -> None:
    app_path = PROJECT / "app" / "edgecare_app.c"
    app_c = read_text(app_path)

    require_contains(app_c, '#include "../bsp/bsp_voice_vw553.h"', app_path)
    require_contains(app_c, "uint8_t voice_alarm_latched;", app_path)
    require_contains(app_c, "uint32_t voice_last_trigger_ms;", app_path)
    require_contains(app_c, "bsp_voice_vw553_init();", app_path)
    require_contains(app_c, "static void edgecare_voice_on_alarm_update(uint8_t alarm_active)", app_path)
    require_contains(app_c, "bsp_voice_vw553_send_danger();", app_path)
    require_contains(app_c, "voice_probe: event=danger_cmd", app_path)
    require_contains(app_c, "edgecare_voice_on_alarm_update(vision_alarm);", app_path)
    require_contains(app_c, "edgecare_voice_on_alarm_update(0U);", app_path)


def test_periodic_status_log_shape_is_unchanged() -> None:
    log_path = PROJECT / "platform" / "edgecare_log.c"
    log_c = read_text(log_path)

    require_contains(
        log_c,
        'printf("[%lu] state=%s radar=%u infer_ms=%lu conf=%u.%02u alarm=%u seq=%lu\\r\\n"',
        log_path,
    )
