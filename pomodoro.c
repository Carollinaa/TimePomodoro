#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"

// Biblioteca gerada pelo arquivo .pio durante compilação.
#include "ws2818b.pio.h"
#include "inc/ssd1306.h"

// Definição do número de LEDs e pinos.
#define LED_COUNT 25
#define LED_PIN 7
#define BTN_A 05
#define BTN_B 06
#define BUZZER_PIN 21
#define SCREEN_WIDTH 128 // Largura do display SSD1306
#define CHAR_WIDTH 6     // Largura média de um caractere na fonte padrão

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;

volatile bool inicializado = false;
int ciclos = 4;
int ciclo_atual = 0;
int tempo_restante = 0;

struct repeating_timer timer;
struct repeating_timer timer2;

// Protótipos das funções (declaração antes da implementação)
void display_text(uint8_t *ssd, char *text[]);
void desenhar_sprite(int matriz[5][5][3]);
uint8_t ssd[ssd1306_buffer_length];

// Definição de pixel GRB
struct pixel_t
{
  uint8_t G, R, B; // Três valores de 8-bits compõem um pixel.
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Mudança de nome de "struct pixel_t" para "npLED_t" por clareza.

// Declaração do buffer de pixels que formam a matriz.
npLED_t leds[LED_COUNT];

// Variáveis para uso da máquina PIO.
PIO np_pio;
uint sm;

// Inicializa a máquina PIO para controle da matriz de LEDs.
void npInit(uint pin)
{
  // Cria programa PIO.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Toma posse de uma máquina PIO.
  sm = pio_claim_unused_sm(np_pio, false);
  if (sm < 0)
  {
    np_pio = pio1;
    sm = pio_claim_unused_sm(np_pio, true); // Se nenhuma máquina estiver livre, panic!
  }
  // Inicia programa na máquina PIO obtida.
  ws2818b_program_init(np_pio, sm, offset, pin, 800000.f);

  // Limpa buffer de pixels.
  for (uint i = 0; i < LED_COUNT; ++i)
  {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}
// Atribui uma cor RGB a um LED.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b)
{
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

// Limpa o buffer de pixels.
void npClear()
{
  for (uint i = 0; i < LED_COUNT; ++i)
    npSetLED(i, 0, 0, 0);
}

// Escreve os dados do buffer nos LEDs.
void npWrite()
{
  // Escreve cada dado de 8-bits dos pixels em sequência no buffer da máquina PIO.
  for (uint i = 0; i < LED_COUNT; ++i)
  {
    pio_sm_put_blocking(np_pio, sm, leds[i].G);
    pio_sm_put_blocking(np_pio, sm, leds[i].R);
    pio_sm_put_blocking(np_pio, sm, leds[i].B);
  }
  // sleep_us(100); // Espera 100us, sinal de RESET do datasheet.
}

// Modificado do github: https://github.com/BitDogLab/BitDogLab-C/tree/main/neopixel_pio
// Função para converter a posição do matriz para uma posição do vetor.
int getIndex(int x, int y)
{
  // Se a linha for par (0, 2, 4), percorremos da esquerda para a direita.
  // Se a linha for ímpar (1, 3), percorremos da direita para a esquerda.
  if (y % 2 == 0)
  {
    return 24 - (y * 5 + x); // Linha par (esquerda para direita).
  }
  else
  {
    return 24 - (y * 5 + (4 - x)); // Linha ímpar (direita para esquerda).
  }
}

// matriz de led do tempo de foco
int matriz_foco[5][5][3] =
    {
        {{0, 0, 0}, {0, 100, 0}, {0, 100, 0}, {0, 100, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 100, 0}, {0, 100, 0}, {0, 100, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {0, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}}

};
// matriz de led do tempo de pausa curta
int matriz_pausa_curta[5][5][3] =
    {
        {{0, 0, 0}, {100, 100, 0}, {100, 100, 0}, {100, 100, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 100, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 100, 0}, {100, 100, 0}, {100, 100, 0}, {0, 0, 0}}

};
// matriz de led do tempo de pausa longa
int matriz_pausa_longa[5][5][3] =
    {
        {{0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}},
        {{0, 0, 0}, {100, 0, 0}, {100, 0, 0}, {100, 0, 0}, {0, 0, 0}}

};

void beep(int frequency)
{
  uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN); // Obtém o slice do PWM
  uint wrap_value = 125000000 / frequency;            // Define o valor de wrap com base na frequência

  pwm_set_wrap(slice_num, wrap_value);            // Define o ciclo de contagem máxima
  pwm_set_gpio_level(BUZZER_PIN, wrap_value / 2); // Define o ciclo de trabalho (50%)
  pwm_set_enabled(slice_num, true);               // Liga o PWM
}

bool stop_beep(struct repeating_timer *t)
{
  uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN); // Obtém o slice do PWM
  pwm_set_enabled(slice_num, false);                  // Desliga o PWM
  return false;
}

bool foco = true;
char text_fase[20];

bool start(struct repeating_timer *t)
{
  if (!inicializado)
  {
    foco = true;
    ciclos = 4;
    ciclo_atual = 0;
    tempo_restante = 0;
    return false;
  }
  if (tempo_restante <= 0)
  {
    
    if (foco)
    {
      strcpy(text_fase, "Tempo de Foco");
      desenhar_sprite(matriz_foco);
      tempo_restante = tempo_em_minutos(25);
      foco = false;
      ciclo_atual++;
    }
    else
    {
      foco = true;
      if (ciclo_atual < ciclos)
      {
        strcpy(text_fase, "Pausa Curta");
        desenhar_sprite(matriz_pausa_curta);
        tempo_restante = tempo_em_minutos(5);
      }
      else
      {
        strcpy(text_fase, "Pausa Longa");
        desenhar_sprite(matriz_pausa_longa);
        tempo_restante = tempo_em_minutos(15);
        ciclo_atual = 0;
      }
    }
  }

  // Converte segundos para MM:SS
  int minutos = tempo_restante / 60;
  int segundos = tempo_restante % 60;

  char tempo[10];
  sprintf(tempo, "%02d:%02d", minutos, segundos); // Formata o tempo

  // Texto para exibição
  char *text[] = {
      "   Bem-vindos",
      " ",
      "  Ao Pomodoro",
      " ",
      " ",
      text_fase,
      " ",
      tempo};
  display_text(ssd, text);

  tempo_restante--; // Decrementa 1 segundo
  return true;      // Mantém o timer rodando
}

// converte milissegundos em segundods
int tempo_em_minutos(int minutos)
{
  return minutos * 60;
}
// inicializa os inputs
void init_input(int input)
{
  gpio_init(input);
  gpio_set_dir(input, GPIO_IN);
  gpio_pull_up(input);
}
// inicializa os outputs
void init_output(int output)
{
  gpio_init(output);
  gpio_set_dir(output, GPIO_OUT);
}

// Desenha os Sprites
void desenhar_sprite(int matriz[5][5][3])
{
  beep(2000);
  add_repeating_timer_ms(1000, stop_beep, NULL, &timer2);
  for (int linha = 0; linha < 5; linha++)
  {
    for (int coluna = 0; coluna < 5; coluna++)
    {
      int posicao = getIndex(linha, coluna);
      npSetLED(posicao, matriz[coluna][linha][0], matriz[coluna][linha][1], matriz[coluna][linha][2]);
    }
  }

  npWrite(); // Escreve os dados nos LEDs.
}

void desligar()
{
  inicializado = false;
  npClear();
  npWrite();

  struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
  };

  calculate_render_area_buffer_length(&frame_area);
  // Zera o display
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);
}

zerar_display(struct render_area frame_area)
{

  calculate_render_area_buffer_length(&frame_area);
  // Zera o display
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);
}

void display_text(uint8_t *ssd, char *text[])
{

  struct render_area frame_area = {
    start_column : 0,
    end_column : ssd1306_width - 1,
    start_page : 0,
    end_page : ssd1306_n_pages - 1
  };

  calculate_render_area_buffer_length(&frame_area);
  // Zera o display
  memset(ssd, 0, ssd1306_buffer_length);
  render_on_display(ssd, &frame_area);

  int y = 0;
  for (uint i = 0; i < (count_of(text) * 8); i++)
  {
    // Verifica se há espaço na tela para mais uma linha de texto
    if (y >= ssd1306_height)
    {
      break; // Se exceder a altura do display, interrompe
    }

    // Desenha o texto na posição correta
    ssd1306_draw_string(ssd, 5, y, text[i]);
    y += 8; // Move para a próxima linha (8 pixels de altura)
  }

  render_on_display(ssd, &frame_area); // Atualiza a tela
}

int main()
{

  init_input(BTN_A);
  init_input(BTN_B);

  // Inicialização do i2c
  // i2c_init(i2c1, ssd1306_i2c_clock * 1000);
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);

  // Processo de inicialização completo do OLED SSD1306
  // ssd1306_init();

  // Inicializa o GPIO como saída PWM
  gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);

  // Inicializa entradas e saídas.
  stdio_init_all();

  // Inicializa matriz de LEDs NeoPixel.
  npInit(LED_PIN);
  npClear();

  npWrite(); // Escreve os dados nos LEDs.

  while (true)
  {

    if (!gpio_get(BTN_A) && !inicializado)
    {
      inicializado = true;
      add_repeating_timer_ms(-1000, start, NULL, &timer);
      sleep_ms(50);
    }
    if (!gpio_get(BTN_B) && inicializado)
    {
      desligar();
      sleep_ms(50);
    }
  }
}
