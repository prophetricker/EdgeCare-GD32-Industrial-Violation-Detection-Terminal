from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VW553 = ROOT / ".tmp" / "franklian_vw553_voice" / "MSDK" / "app"
UART = ROOT / ".tmp" / "franklian_vw553_voice" / "MSDK" / "plf" / "src" / "uart"


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def require_contains(text: str, needle: str, path: Path) -> None:
    assert needle in text, f"{path} missing {needle!r}"


def test_voice_uart_receiver_owns_uart2_pa6_pa7_protocol() -> None:
    source_path = VW553 / "voice_danger_uart.c"
    header_path = VW553 / "voice_danger_uart.h"
    uart_h_path = UART / "uart.h"

    source = read_text(source_path)
    header = read_text(header_path)
    uart_h = read_text(uart_h_path)

    require_contains(header, "int voice_danger_uart_start(void);", header_path)
    require_contains(source, "#define VOICE_DANGER_UART_PORT          UART2", source_path)
    require_contains(source, "#define VOICE_DANGER_UART_BAUDRATE      BAUDRATE_115200", source_path)
    require_contains(source, '#define VOICE_DANGER_UART_COMMAND       "DANGER"', source_path)
    require_contains(source, "uart_config(VOICE_DANGER_UART_PORT, VOICE_DANGER_UART_BAUDRATE", source_path)
    require_contains(source, "uart_irq_callback_register(VOICE_DANGER_UART_PORT", source_path)
    require_contains(source, "voice_prompt_player_trigger();", source_path)
    require_contains(source, 'printf("voice uart: DANGER command received', source_path)
    require_contains(uart_h, "#define UART2_TX_PIN                    GPIO_PIN_6", uart_h_path)
    require_contains(uart_h, "#define UART2_RX_PIN                    GPIO_PIN_7", uart_h_path)


def test_voice_app_starts_uart_receiver_for_h759_link() -> None:
    app_path = VW553 / "voice_app.c"
    cfg_path = VW553 / "app_cfg.h"
    cmake_path = VW553 / "CMakeLists.txt"

    app_c = read_text(app_path)
    cfg_h = read_text(cfg_path)
    cmake = read_text(cmake_path)

    require_contains(app_c, '#include "voice_danger_uart.h"', app_path)
    require_contains(app_c, "voice_danger_uart_start()", app_path)
    require_contains(cfg_h, "#undef CONFIG_BASECMD", cfg_path)
    require_contains(cfg_h, "#define CONFIG_VOICE_DANGER_UART", cfg_path)
    require_contains(cmake, "voice_danger_uart.c", cmake_path)
