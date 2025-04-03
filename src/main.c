#include "stm32f0xx.h"
#include <stdlib.h>
#include <stdbool.h>

// define led pins (adjust according to your board)
#define LED_GREEN_PIN GPIO_PIN_8
#define LED_ORANGE_PIN GPIO_PIN_9
#define LED_RED_PIN GPIO_PIN_10
#define LED_BLUE_PIN GPIO_PIN_11
#define LED_PORT GPIOC


// define button pins (adjust according to your board)
#define BUTTON_GREEN_PIN GPIO_PIN_0
#define BUTTON_ORANGE_PIN GPIO_PIN_1
#define BUTTON_RED_PIN GPIO_PIN_2
#define BUTTON_BLUE_PIN GPIO_PIN_3
#define BUTTON_PORT GPIOA

//DAC pins
#define DAC_PINS GPIO_PIN_4
#define DAC_PORT GPIOA

// game constants
#define MAX_LEVEL 10
#define LED_ON_TIME 500   // led on duration in milliseconds
#define BETWEEN_TIME 200  // delay between sequence steps
#define DEBOUNCE_DELAY 50 // debounce time for button input

// game variables
uint8_t sequence[MAX_LEVEL]; // stores the full random pattern
uint8_t current_level = 0;   // current game level (starts from 0)
bool game_over = false;      // flag to check game over state
bool input_mode = false;     // true when waiting for player input
uint8_t current_input = 0;   // current position in input sequence

// function declarations
void GPIO_Configure(void);
void SysTick_Configure(void);
void PWM_Configure(void);
void generate_sequence(void);
void play_sequence(void);
void light_led(uint8_t led);
void set_led_brightness(uint8_t color, uint16_t brightness);
bool check_button(uint8_t button);
uint8_t get_button_press(void);
void delay_ms(uint32_t ms);
void setup_dac(void);
void audio_feedback(uint32_t i);

int main(void)
{
    // initialize gpio, systick timer, and pwm output
    GPIO_Configure();
    SysTick_Configure();
    PWM_Configure();
    setup_dac();

    // seed the random number generator using current systick value
    srand(SysTick->VAL);

    // generate the initial sequence of steps
    generate_sequence();

    // main game loop
    while (1)
    {
        if (!game_over)
        {
            // show the current pattern to the player
            play_sequence();

            // enable input mode and reset input index
            input_mode = true;
            current_input = 0;

            // loop while waiting for player to complete current sequence
            while (current_input < current_level && !game_over)
            {
                uint8_t button = get_button_press();
                if (button != 0)
                {
                    // briefly light the led corresponding to the button
                    light_led(button);

                    // check if the input matches the pattern
                    if (button != sequence[current_input])
                    {
                        // if incorrect, trigger game over sequence
                        game_over = true;
                        audio_feedback(1); //plays sound to indicate incorrect

                        // flash all leds multiple times
                        for (int i = 0; i < 5; i++)
                        {
                            HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN, GPIO_PIN_SET);
                            delay_ms(200);
                            HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN, GPIO_PIN_RESET);
                            delay_ms(200);
                        }
                    }
                    else
                    {
                        // input is correct, move to next step
                        audio_feedback(2); //plays sound to indicate correct
                        current_input++;
                    }
                }
            }

            input_mode = false;

            if (!game_over)
            {
                audio_feedback(2); //plays sound to indicate correct
                // flash leds quickly to indicate level success
                for (int i = 0; i < 3; i++)
                {
                    HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN, GPIO_PIN_SET);
                    delay_ms(100);
                    HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN, GPIO_PIN_RESET);
                    delay_ms(100);
                }

                // increase game level
                current_level++;

                // check for win condition
                if (current_level >= MAX_LEVEL)
                {
                    audio_feedback(2); //plays sound to indicate correct
                    // flash leds repeatedly to indicate victory
                    for (int i = 0; i < 10; i++)
                    {
                        HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN, GPIO_PIN_SET);
                        delay_ms(100);
                        HAL_GPIO_WritePin(LED_PORT, LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN, GPIO_PIN_RESET);
                        delay_ms(100);
                    }

                    // set game over flag
                    game_over = true;
                }
                else
                {
                    // add one more step to the sequence
                    sequence[current_level] = (rand() % 4) + 1;
                }
            }
        }
        else
        {
            // if game is over, wait for a button press to restart
            if (get_button_press() != 0)
            {
                game_over = false;
                current_level = 0;
                generate_sequence();
            }
        }
    }
}


void GPIO_Configure(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // enable clock for GPIOC and GPIOA
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // configure pc8-pc11 (leds) as output
    GPIO_InitStruct.Pin = LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    // configure pa0-pa3 (buttons) as input with pull-up resistors
    GPIO_InitStruct.Pin = BUTTON_GREEN_PIN | BUTTON_ORANGE_PIN | BUTTON_RED_PIN | BUTTON_BLUE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(BUTTON_PORT, &GPIO_InitStruct);
}

void SysTick_Configure(void)
{
    // configure systick to generate 1ms ticks
    HAL_SYSTICK_Config(SystemCoreClock / 1000);
}

void PWM_Configure(void)
{
    // configure timer 3 to output pwm signals on pc8-pc11

    // enable clocks and GPIOC peripheral clocks so we can configure them
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // configure led pins to alternate function mode for pwm so TIM3 can drive them
    GPIO_InitStruct.Pin = LED_GREEN_PIN | LED_ORANGE_PIN | LED_RED_PIN | LED_BLUE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM3;
    HAL_GPIO_Init(LED_PORT, &GPIO_InitStruct);

    // set up timer base for pwm
    TIM_HandleTypeDef htim3;
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 48 - 1; // set prescaler to 1 mhz
    htim3.Init.Period = 1000 - 1;  // set period for 1 khz pwm
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_PWM_Init(&htim3);

    // sets up each output channel to operate in PWM mode 1
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0; // start with leds off, 0% duty cycle
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    // links each LED pin to a timer channel
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3); // pc8 - green
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4); // pc9 - orange
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1); // pc6 - red (if used)
    HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2); // pc7 - blue (if used)

    // start all pwm channels
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
}

void set_led_brightness(uint8_t color, uint16_t brightness)
{
    // set duty cycle for the specified color channel
    // writes to the Compare/Capture Register (CCR) of TIM3.
    // controls the ON-time per PWM cycle for each LED.
    // for example, CCR3 = 1000 → 100% duty → full brightness for Green LED.

    switch (color)
    {
    case 1:
        TIM3->CCR3 = brightness;
        break; // green
    case 2:
        TIM3->CCR4 = brightness;
        break; // orange
    case 3:
        TIM3->CCR1 = brightness;
        break; // red
    case 4:
        TIM3->CCR2 = brightness;
        break; // blue
    default:
        break;
    }
}

void generate_sequence(void)
{
    // fill the sequence with random values from 1 to 4
    for (int i = 0; i < MAX_LEVEL; i++)
    {
        sequence[i] = (rand() % 4) + 1;
    }
}

void play_sequence(void)
{
    // show the sequence from the beginning up to current level
    for (int i = 0; i < current_level; i++)
    {
        light_led(sequence[i]);
        delay_ms(BETWEEN_TIME);
    }
}

void light_led(uint8_t led)
{
    // turn on led using pwm at full brightness
    set_led_brightness(led, 1000); // 1000 corresponds to full brightness

    // keep it on for a set time
    delay_ms(LED_ON_TIME);

    // turn off led by setting brightness to zero
    set_led_brightness(led, 0);
}

bool check_button(uint8_t button)
{
    // read the button state and return true if pressed
    GPIO_PinState state;
    switch (button)
    {
    case 1:
        state = HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_GREEN_PIN);
        return (state == GPIO_PIN_RESET);
    case 2:
        state = HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_ORANGE_PIN);
        return (state == GPIO_PIN_RESET);
    case 3:
        state = HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_RED_PIN);
        return (state == GPIO_PIN_RESET);
    case 4:
        state = HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_BLUE_PIN);
        return (state == GPIO_PIN_RESET);
    default:
        return false;
    }
}

uint8_t get_button_press(void)
{
    // wait until a button is pressed and released, then return its value
    while (1)
    {
        for (uint8_t i = 1; i <= 4; i++)
        {
            if (check_button(i))
            {
                delay_ms(DEBOUNCE_DELAY); // debounce delay
                if (check_button(i))
                {
                    while (check_button(i))
                        ; // wait for release
                    return i;
                }
            }
        }
        delay_ms(10); // prevent tight polling loop
    }
}

void delay_ms(uint32_t ms)
{
    // delay using hal systick timing
    uint32_t start = HAL_GetTick();
    while ((HAL_GetTick() - start) < ms)
        ;
}

void audio_feedback(uint32_t i){

    if(i == 1){ //incorrect
        //run tone for 50 cycles
        for(int k =0; k < 50; k++){
            DAC->DHR12R1 = 0xFFF; //set high
            DAC->SWTRIGR = DAC_SWTRIGR_SWTRIG1; //triggers conversion
            delay_ms(2);

            DAC->DHR12R1 = 0x000; //set low
            DAC->SWTRIGR = DAC_SWTRIGR_SWTRIG1;
            delay_ms(2);
        }

    }else if(i ==2){//correct
        for(int k =0; k < 20; k++){
            DAC->DHR12R1 = 0xFFF; //set high
            DAC->SWTRIGR = DAC_SWTRIGR_SWTRIG1;
            delay_ms(1);
            
            DAC->DHR12R1 = 0x000; //set low
            DAC->SWTRIGR = DAC_SWTRIGR_SWTRIG1;
            delay_ms(1);
        }

    }else{
        return;
    }

}

void setup_dac(void){
    GPIO_InitTypeDef GPIO_InitStruct;

    //enable port A
    __HAL_RCC_GPIOA_CLK_ENABLE();

    //enable port for analog mode
    GPIO_InitStruct.Pin = DAC_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DAC_PORT, &GPIO_InitStruct);

    //enable DAC
    RCC->APB1ENR |= RCC_APB1ENR_DACEN;
    //chanel 1

    DAC -> CR &= ~DAC_CR_TSEL1; //clear trigger
    DAC -> CR &= ~DAC_CR_BOFF1;
    DAC -> CR |= DAC_CR_TEN1; //enable trigger for channel 1
    DAC-> CR|= DAC_CR_TSEL1;//enable trigger
    DAC->CR |= DAC_CR_EN1; //enable DAC

}
