#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"



// Pinos do hardware
#define PWM_GPIO 0     // Saída PWM no GPIO0
#define JOY_X 27       // Joystick eixo X -> frequência
#define JOY_Y 26       // Joystick eixo Y -> duty cycle
#define BOTAO_A 10     // Incrementa duty
#define BOTAO_B 5      // Decrementa duty

// Variáveis de controle
uint32_t freq = 1000;    // Frequência inicial (Hz)
uint16_t duty = 32768;   // Duty inicial (~50%)
const uint16_t passo = 1000; // ajuste fino do duty em steps

int main() {
    stdio_init_all();

    // --- Configuração do ADC (joystick) ---
    adc_init();
    adc_gpio_init(JOY_X);
    adc_gpio_init(JOY_Y);

    // --- Configuração dos botões ---
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);

    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);

    // --- Configuração do PWM na GPIO0 ---
    gpio_set_function(PWM_GPIO, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(PWM_GPIO);

    // Frequência inicial
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);

    while (true) {
        // --- Leitura do joystick ---
        adc_select_input(1); // GPIO27
        uint16_t x_raw = adc_read();
        adc_select_input(0); // GPIO26
        uint16_t y_raw = adc_read();

        // Mapeia X (0–4095) -> 50 Hz a 10 kHz
        freq = 50 + (x_raw * (10000 - 50) / 4095);

        // Ajusta o divisor do PWM para obter freq desejada
        uint32_t clock = 125000000; // 125 MHz clock base
        uint32_t wrap = clock / freq - 1;
        if (wrap < 1) wrap = 1;
        pwm_set_wrap(slice_num, wrap);

        // Mapeia Y (0–4095) -> duty (0 a wrap)
        duty = (y_raw * wrap) / 4095;

        // --- Ajuste fino via botões ---
        if (!gpio_get(BOTAO_A)) {
            if (duty + passo < wrap) duty += passo;
        }
        if (!gpio_get(BOTAO_B)) {
            if (duty > passo) duty -= passo;
        }

        // Aplica duty
        pwm_set_gpio_level(PWM_GPIO, duty);

        // --- Debug no terminal serial ---
        float duty_percent = (duty * 100.0) / wrap;
        printf("Freq: %lu Hz | Duty: %.1f %%\n", freq, duty_percent);

        sleep_ms(200);
    }
}