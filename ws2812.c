#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "ws2812.pio.h"

// Definições de constantes utilizadas no programa
#define IS_RGBW false          // Define se a matriz é RGB ou RGBW
#define NUM_PIXELS 25         // Quantidade de nuúmeros de LEDs na matriz
#define NUMBERS 10           // Quantidade de números que aparecerão na matriz
#define WS2812_PIN 7        // GPIO7 responsável pela comunicação com a matriz de LEDs
#define TEMPO 200          // Tempo de espera em ms, faz com que o LED Vermelho pisque 5 vezes por segundo

// Pinos para controle do LED e botões
const uint ledRed_pin = 13;     // Red => GPIO13
const uint ledBlue_pin = 12;   // Blue => GPIO12
const uint ledGreen_pin = 11; // Green => GPIO11
const uint button_A = 5;     // Botão A => GPIO5
const uint button_B = 6;    // Botão B => GPIO6

// Variáveis globais para controle do LED e cor
uint8_t displayed_number = 0;      // Índice do LED a ser controlado (0 a 24)
uint8_t selected_r = 0;           // Intensidade do vermelho (0 a 255)
uint8_t selected_g = 0;          // Intensidade do verde (0 a 255)
uint8_t selected_b = 255;       // Intensidade do azul (0 a 255)


// Buffer para armazenar as cores de todos os LEDs
bool led_buffer[NUMBERS][NUM_PIXELS] = {
    // Número 0
    0, 1, 1, 1, 0, 
    0, 1, 0, 1, 0, 
    0, 1, 0, 1, 0, 
    0, 1, 0, 1, 0, 
    0, 1, 1, 1, 0,

    // Número 1
    0, 1, 1, 1, 0,
    0, 0, 1, 0, 0,
    0, 0, 1, 0, 0,
    0, 1, 1, 0, 0,
    0, 0, 1, 0, 0,

    // Número 2
    0, 1, 1, 1, 0,
    0, 1, 0, 0, 0,
    0, 0, 1, 0, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,

    // Número 3
    0, 1, 1, 1, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,

    // Número 4
    0, 1, 0, 0, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 0, 1, 0,

    // Número 5
    0, 1, 1, 1, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 1, 0, 0, 0,
    0, 1, 1, 1, 0,

    // Número 6
    0, 1, 1, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 1, 0, 0, 0,
    0, 1, 1, 1, 0,

    // Número 7
    0, 0, 0, 1, 0,
    0, 1, 0, 0, 0,
    0, 0, 1, 0, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,

    // Número 8
    0, 1, 1, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 1, 1, 0,

    // Número 9
    0, 1, 1, 1, 0,
    0, 0, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 1, 0, 1, 0,
    0, 1, 1, 1, 0
};

// Variáveis globais para controle do tempo
static volatile uint32_t last_time = 0; // Armazena o tempo do último evento (em microssegundos)

// Prototipação das funções
static void gpio_irq_handler(uint gpio, uint32_t events);
static inline void put_pixel(uint32_t pixel_grb);
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);
static inline void put_pixel(uint32_t pixel_grb);
void set_led_pattern(uint8_t r, uint8_t g, uint8_t b, int displayed_number);

int main() {
    stdio_init_all();

    // Inicializa o LED
    gpio_init(ledRed_pin);                     // Inicializa o pino do LED
    gpio_set_dir(ledRed_pin, GPIO_OUT);       // Configura o pino como saída
    gpio_init(ledBlue_pin);                  // Inicializa o pino do LED
    gpio_set_dir(ledBlue_pin, GPIO_OUT);    // Configura o pino como saída
    gpio_init(ledGreen_pin);               // Inicializa o pino do LED
    gpio_set_dir(ledGreen_pin, GPIO_OUT); // Configura o pino como saída

    gpio_init(button_A);                // Inicializa o botão
    gpio_set_dir(button_A, GPIO_IN);   // Configura o pino como entrada
    gpio_pull_up(button_A);           // Habilita o pull-up interno

    gpio_init(button_B);                // Inicializa o botão
    gpio_set_dir(button_B, GPIO_IN);   // Configura o pino como entrada
    gpio_pull_up(button_B);           // Habilita o pull-up interno

    PIO pio = pio0;
    int sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, IS_RGBW);

    // Configuração da interrupção com callback
    gpio_set_irq_enabled_with_callback(button_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    set_led_pattern(selected_r, selected_g, selected_b, displayed_number); // Define a cor inicial

    while (1) {

        gpio_put(ledRed_pin, true);    // Liga o LED
        sleep_ms(TEMPO);              // Aguarda 200 ms
        gpio_put(ledRed_pin, false); // Desliga o LED
    }

    return 0;
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

// Função de interrupção com debouncing
void gpio_irq_handler(uint gpio, uint32_t events)
{
    // Obtém o tempo atual em microssegundos
    uint32_t current_time = to_us_since_boot(get_absolute_time());
    // Verifica se passou tempo suficiente desde o último evento
    if (current_time - last_time > 200000) // 200 ms de debouncing
    {
        last_time = current_time; // Atualiza o tempo do último evento

        switch (gpio)
        {
        case 5:
            printf("Botão A pressionado\n");
            displayed_number = (displayed_number + 1) % NUMBERS; // Incrementa o índice do LED
            printf("Número mudado para %d\n", displayed_number);
            set_led_pattern(selected_r, selected_g, selected_b, displayed_number);
            break;
        case 6:
            printf("Botão B pressionado\n");
            displayed_number = (displayed_number - 1 + NUMBERS) % NUMBERS; // Decrementa o índice do LED e evita valores negativos
            printf("Número mudado para %d\n", displayed_number);
            set_led_pattern(selected_r, selected_g, selected_b, displayed_number);
            break;
        default:
            break;
        }        
    }
}

void set_led_pattern(uint8_t r, uint8_t g, uint8_t b, int displayed_number)
{
    // Define a cor com base nos parâmetros fornecidos
    uint32_t color = urgb_u32(r, g, b);

    // Define todos os LEDs com a cor especificada
    for (int i = 0; i < NUM_PIXELS; i++)
    {
        if (led_buffer[displayed_number][i])
        {
            put_pixel(color); // Liga o LED com um no buffer
        }
        else
        {
            put_pixel(0);  // Desliga os LEDs com zero no buffer
        }
    }
}