#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

// Pinos do hardware
#define PWM_GPIO 0     // Saída PWM no GPIO0
#define PROBE_GPIO 2   // Probe digital na GPIO2
#define JOY_X 27       // Joystick eixo X -> frequência
#define JOY_Y 26       // Joystick eixo Y -> duty cycle
#define BOTAO_A 10     // Incrementa duty
#define BOTAO_B 5      // Decrementa duty

// Variáveis de controle
uint32_t freq = 1000;       // Frequência inicial (Hz)
uint16_t duty = 32768;      // Duty inicial (~50%)
const uint16_t passo = 1000; // ajuste fino do duty em steps

// Variáveis para medição do sinal na GPIO2
volatile uint32_t last_edge_time = 0;
volatile uint32_t period = 0;
volatile uint32_t high_time = 0;

// Interrupção para capturar bordas do sinal
void probe_irq(uint gpio, uint32_t events) {
    uint32_t now = time_us_32();
    bool level = gpio_get(PROBE_GPIO);

    if (level) {
        // Borda de subida → início do ciclo
        period = now - last_edge_time;
        last_edge_time = now;
    } else {
        // Borda de descida → tempo em nível alto
        high_time = now - last_edge_time;
    }
}

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
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);

    // --- Configuração do probe na GPIO2 ---
    gpio_init(PROBE_GPIO);
    gpio_set_dir(PROBE_GPIO, GPIO_IN);
    gpio_pull_down(PROBE_GPIO);
    gpio_set_irq_enabled_with_callback(PROBE_GPIO,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true, &probe_irq);

    while (true) {
        // --- Leitura do joystick ---
        adc_select_input(1); // GPIO27
        uint16_t x_raw = adc_read();
        adc_select_input(0); // GPIO26
        uint16_t y_raw = adc_read();

        // Mapeia X (0–4095) -> 50 Hz a 10 kHz
        freq = 50 + (x_raw * (10000 - 50) / 4095);

        // Ajusta o divisor do PWM
        uint32_t clock = 125000000; // 125 MHz
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

        // --- Cálculo do duty e freq do probe ---
        float measured_freq = (period > 0) ? (1000000.0f / period) : 0;
        float measured_duty = (period > 0) ? (100.0f * high_time / period) : 0;

        // --- Debug no terminal ---
        float duty_percent = (duty * 100.0f) / wrap;
        printf("PWM: Freq %lu Hz | Duty %.1f %% | Probe: Freq %.1f Hz | Duty %.1f %%\n",
            freq, duty_percent, measured_freq, measured_duty);

        sleep_ms(200);
    }
}
