#include "comms/backend_init.hpp"
#include "config_defaults.hpp"
#include "core/CommunicationBackend.hpp"
#include "core/KeyboardMode.hpp"
#include "core/Persistence.hpp"
#include "core/mode_selection.hpp"
#include "core/pinout.hpp"
#include "core/state.hpp"
#include "input/NunchukInput.hpp"
#include "reboot.hpp"
#include "stdlib.hpp"
#include "HAL/pico/include/input/GamecubeControllerInput.hpp"
#include "input/GpioButtonInput.hpp"

#include <config.pb.h>

Config config = default_config;

GpioButtonMapping button_mappings[] = {

    // comments refer to melee b0xx default layout for easier understanding
    { BTN_LF1, 2  }, //right
    { BTN_LF2, 3  }, //down
    { BTN_LF3, 4  }, //left
    { BTN_LF4, 5  }, // L-digital trigger

    { BTN_LT1, 6  }, // modx
    { BTN_LT2, 7  }, // mody

    { BTN_MB1, 0  }, // Start
    { BTN_MB2, 10 }, // Select
    { BTN_MB3, 11 }, //  Home

    { BTN_RT1, 14 }, // a
    { BTN_RT2, 15 }, //c-down
    { BTN_RT3, 13 }, //c-left
    { BTN_RT4, 12 }, //c-up
    { BTN_RT5, 16 }, //c-right

    { BTN_RF1, 26 }, // b
    { BTN_RF2, 21 }, // x
    { BTN_RF3, 19 }, // z
    { BTN_RF4, 17 }, //up

    { BTN_RF5, 27 }, // R-digital trigger
    { BTN_RF6, 22 }, // y
    { BTN_RF7, 20 }, // max-lightshield
    { BTN_RF8, 18 }, // mid-shield
};
const size_t button_count = sizeof(button_mappings) / sizeof(GpioButtonMapping);


const Pinout pinout = {
    .joybus_data = 28,
    .nes_data = -1,
    .nes_clock = -1,
    .nes_latch = -1,
    .mux = -1,
    .nunchuk_detect = -1,
    .nunchuk_sda = -1,
    .nunchuk_scl = -1,
};

CommunicationBackend **backends = nullptr;
size_t backend_count;
KeyboardMode *current_kb_mode = nullptr;
GamecubeControllerInput *gcc = nullptr;

void setup() {
    static InputState inputs;

    // Create GPIO input source and use it to read button states for checking button holds.
    static GpioButtonInput gpio_input(button_mappings, button_count);
    gpio_input.UpdateInputs(inputs);

    // Check bootsel button hold as early as possible for safety.
    if (inputs.rt2) {
        reboot_bootloader();
    }

    // Turn on LED to indicate firmware booted.
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 1);

    // Attempt to load config, or write default config to flash if failed to load config.
    if (!persistence.LoadConfig(config)) {
        persistence.SaveConfig(config);
    }

    // Create array of input sources to be used.
    static InputSource *input_sources[] = {&gpio_input};
    size_t input_source_count = sizeof(input_sources) / sizeof(InputSource *);

    backend_count =
        initialize_backends(backends, inputs, input_sources, input_source_count, config, pinout);

    setup_mode_activation_bindings(config.game_mode_configs, config.game_mode_configs_count);
}

void loop() {
    select_mode(backends, backend_count, config);

    for (size_t i = 0; i < backend_count; i++) {
        backends[i]->SendReport();
    }

    if (current_kb_mode != nullptr) {
        current_kb_mode->SendReport(backends[0]->GetInputs());
    }
}

/* Button inputs are read from the second core */

void setup1() {
    while (backends == nullptr) {
        tight_loop_contents();
    }

    gcc = new GamecubeControllerInput(9, 2500, pio1);
}

void loop1() {
    if (backends != nullptr) {
        gcc->UpdateInputs(backends[0]->GetInputs());
    }
}
