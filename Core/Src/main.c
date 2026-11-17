#include "main.h"
#include "scheduler.h"
#include "lab3_config.h"
#include "input_reading.h"
#include "input_processing.h"
#include "output_display.h"
#include <stdio.h>
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
UART_HandleTypeDef huart1;
/* Store the first 8 callback timestamps */
#define DEMO_TIMESTAMP_SIZE 8u

volatile uint32_t demo_timestamps[DEMO_TIMESTAMP_SIZE] = {0};

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);
/* ===== LAB 4: Task monitoring ===== */

#define LAB4_TASK_COUNT   5u
#define LAB4_LOG_SIZE     32u

// Count how many times each task executes
volatile uint32_t lab4_task_count[LAB4_TASK_COUNT] = {0};

// Store the latest execution time in milliseconds
volatile uint32_t lab4_last_run_ms[LAB4_TASK_COUNT] = {0};

// Store IDs returned by SCH_Add_Task()
uint32_t lab4_task_ids[LAB4_TASK_COUNT] = {0};

// Keep a history of task executions for debugging
volatile uint8_t lab4_log_task[LAB4_LOG_SIZE] = {0};
volatile uint32_t lab4_log_time[LAB4_LOG_SIZE] = {0};
volatile uint32_t lab4_log_total = 0;

/* ===== LAB 4: Print timestamp via UART ===== */

static void Lab4_PrintTimestamp(const char *task_name,
                                uint32_t timestamp)
{
    char buffer[64];

    // Convert task name and timestamp to text
    int len = snprintf(
        buffer,
        sizeof(buffer),
        "%s: %lu ms\r\n",
        task_name,
        (unsigned long)timestamp
    );

    // Send text to Proteus Virtual Terminal
    if (len > 0 && len < (int)sizeof(buffer))
    {
        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)buffer,
            (uint16_t)len,
            5u
        );
    }
}

/* ===== LAB 4: Record task execution ===== */

static void Lab4_Record_Task(uint8_t index)
{
    // Read current scheduler time
    uint32_t now = SCH_GetTimeMs();
    // Print timestamp every 10ms
    // Increase execution counter
    lab4_task_count[index]++;

    // Save most recent execution timestamp
    lab4_last_run_ms[index] = now;

    // Calculate position in circular log buffer
    uint32_t pos = lab4_log_total % LAB4_LOG_SIZE;

    // Store task number (1 to 5) and timestamp
    lab4_log_task[pos] = index + 1u;
    lab4_log_time[pos] = now;

    // Increase number of recorded events
    lab4_log_total++;

    // Names of the five periodic tasks
    static const char *task_names[5] = {
        "T500",
        "T1000",
        "T1500",
        "T2000",
        "T2500"
    };

    // Print the timestamp of this task
    Lab4_PrintTimestamp(task_names[index], now);

}

/* ===== LAB 4: Five periodic tasks ===== */

// Task 1: runs every 500ms
static void Task_500ms(void)
{
    Lab4_Record_Task(0);
}

// Task 2: runs every 1000ms
static void Task_1000ms(void)
{
    Lab4_Record_Task(1);
}

// Task 3: runs every 1500ms
static void Task_1500ms(void)
{
    Lab4_Record_Task(2);
}

// Task 4: runs every 2000ms
static void Task_2000ms(void)
{
    Lab4_Record_Task(3);
}

// Task 5: runs every 2500ms
static void Task_2500ms(void)
{
    Lab4_Record_Task(4);
}


/* ===== LAB 4: One-shot and Delete tests ===== */

// One-shot test
volatile uint32_t lab4_oneshot_count = 0;
volatile uint32_t lab4_oneshot_time = 0;

// Delete test
volatile uint32_t lab4_delete_count = 0;
volatile uint32_t lab4_delete_time = 0;
volatile uint32_t lab4_cancel_result = 0;
volatile uint32_t lab4_cancel_time = 0;

static uint32_t lab4_delete_id = 0;

// Runs once at approximately 1200ms
static void Task_OneShot(void)
{
    lab4_oneshot_count++;
    lab4_oneshot_time = SCH_GetTimeMs();
}

// Runs every 700ms until deleted
static void Task_To_Delete(void)
{
    lab4_delete_count++;
    lab4_delete_time = SCH_GetTimeMs();
}

// Deletes the periodic task at approximately 2300ms
static void Task_Cancel_Delete(void)
{
    lab4_cancel_result = SCH_Delete_Task(lab4_delete_id);
    lab4_cancel_time = SCH_GetTimeMs();
}


/* ===== LAB 4: 10ms Callback Test ===== */

volatile uint32_t lab4_10ms_count = 0;
volatile uint32_t lab4_10ms_last = 0;
volatile uint32_t lab4_10ms_min_delta = 0xFFFFFFFFu;
volatile uint32_t lab4_10ms_max_delta = 0;

// Executed every 10ms by scheduler
static void Task_10ms_Test(void)
{
    uint32_t now = SCH_GetTimeMs();
    Lab4_PrintTimestamp("T10", now);

    // Store the first 8 callback timestamps
    // Keep them unchanged for demonstration
    if (lab4_10ms_count < DEMO_TIMESTAMP_SIZE)
    {
        demo_timestamps[lab4_10ms_count] = now;
    }

    // Calculate interval between callbacks
    if (lab4_10ms_count > 0)
    {
        uint32_t delta = now - lab4_10ms_last;

        if (delta < lab4_10ms_min_delta)
            lab4_10ms_min_delta = delta;

        if (delta > lab4_10ms_max_delta)
            lab4_10ms_max_delta = delta;
    }

    lab4_10ms_last = now;
    lab4_10ms_count++;
}

int main(void)
{
    /* ===== 1. Initialize STM32 hardware ===== */

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();



    // Initialize TIM2 and TIM3
    MX_TIM2_Init();
    MX_TIM3_Init();

    /* ===== 2. Initialize Lab 3 ===== */

    // Initialize seven-segment display
    output_display_init();

    // Initialize input buttons
    buttons_init();

    // Initialize Lab 3 application
    lab3_app_init();
    // Configure PA9 as UART TX after Lab 3 GPIO setup
    MX_USART1_UART_Init();

    // Test USART1 before scheduler starts
    const char message[] = "LAB4 UART OK\r\n";

    HAL_UART_Transmit(
        &huart1,
        (uint8_t *)message,
        sizeof(message) - 1u,
        100u
    );

    /* ===== 3. Initialize Lab 4 scheduler ===== */

    // Clear scheduler data and reset tick to zero
    SCH_Init();

    /* ===== 4. Register five periodic tasks ===== */

    // Task 1: 500 ms
    lab4_task_ids[0] = SCH_Add_Task(
        Task_500ms,
        SCH_MS_TO_TICKS(500),
        SCH_MS_TO_TICKS(500)
    );

    // Task 2: 1000 ms
    lab4_task_ids[1] = SCH_Add_Task(
        Task_1000ms,
        SCH_MS_TO_TICKS(1000),
        SCH_MS_TO_TICKS(1000)
    );

    // Task 3: 1500 ms
    lab4_task_ids[2] = SCH_Add_Task(
        Task_1500ms,
        SCH_MS_TO_TICKS(1500),
        SCH_MS_TO_TICKS(1500)
    );

    // Task 4: 2000 ms
    lab4_task_ids[3] = SCH_Add_Task(
        Task_2000ms,
        SCH_MS_TO_TICKS(2000),
        SCH_MS_TO_TICKS(2000)
    );

    // Task 5: 2500 ms
    lab4_task_ids[4] = SCH_Add_Task(
        Task_2500ms,
        SCH_MS_TO_TICKS(2500),
        SCH_MS_TO_TICKS(2500)
    );

    // Periodic task: runs every 700ms
    lab4_delete_id = SCH_Add_Task(
        Task_To_Delete,
        SCH_MS_TO_TICKS(700),
        SCH_MS_TO_TICKS(700)
    );

    // One-shot task: runs once after 1200ms
    uint32_t oneshot_id = SCH_Add_Task(
        Task_OneShot,
        SCH_MS_TO_TICKS(1200),
        0
    );

    // One-shot task: deletes the 700ms task after 2300ms
    uint32_t cancel_id = SCH_Add_Task(
        Task_Cancel_Delete,
        SCH_MS_TO_TICKS(2300),
        0
    );

    // Check successful registration
    if (lab4_delete_id == SCH_INVALID_TASK_ID ||
        oneshot_id == SCH_INVALID_TASK_ID ||
        cancel_id == SCH_INVALID_TASK_ID)
    {
        Error_Handler();
    }

    // Register 10ms periodic callback
    uint32_t task_10ms_id = SCH_Add_Task(
        Task_10ms_Test,
        SCH_MS_TO_TICKS(10),
        SCH_MS_TO_TICKS(10)
    );

    if (task_10ms_id == SCH_INVALID_TASK_ID)
    {
        Error_Handler();
    }

    /* ===== 5. Check task registration ===== */

    for (uint32_t i = 0; i < LAB4_TASK_COUNT; i++)
    {
        // Task ID 0 means registration failed
        if (lab4_task_ids[i] == SCH_INVALID_TASK_ID)
        {
            Error_Handler();
        }
    }

    /* ===== 6. Start hardware timers ===== */

    // TIM3: scan Lab 3 seven-segment displays
    if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
    {
        Error_Handler();
    }

    // TIM2: update scheduler every 10 ms
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    /* ===== 7. Main super loop ===== */

    while (1)
    {
        // Keep Lab 3 button processing
        fsm_for_input_processing();

        // Run tasks that have reached their deadlines
        SCH_Dispatch_Tasks();
    }
}


/* Use HSI 8MHz so PSC=7999 => 1kHz TIM2 count, ARR=tick_ms-1. */
static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_OFF;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef io = {0};
    const uint16_t seg_pins = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 |
                              GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8;
    const uint16_t digit_pins = GPIO_PIN_9 | GPIO_PIN_10 |
                                GPIO_PIN_11 | GPIO_PIN_12;
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOA, (uint16_t)0x0FFFu, GPIO_PIN_RESET);
    /* Common Anode: segments and PNP digit drivers are active LOW. */
    HAL_GPIO_WritePin(GPIOB, seg_pins | digit_pins, GPIO_PIN_SET);
    io.Pin = (uint16_t)0x0FFFu;
    io.Mode = GPIO_MODE_OUTPUT_PP;
    io.Pull = GPIO_NOPULL;
    io.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &io);
    io.Pin = seg_pins | digit_pins;
    HAL_GPIO_Init(GPIOB, &io);
    io.Pin = GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    io.Mode = GPIO_MODE_INPUT;
    io.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &io);
}

/* ===== LAB 4: USART1 for Virtual Terminal ===== */

static void MX_USART1_UART_Init(void)
{
    GPIO_InitTypeDef io = {0};

    // Enable GPIOA and USART1 clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    // Configure PA9 as USART1 TX
    io.Pin = GPIO_PIN_9;
    io.Mode = GPIO_MODE_AF_PP;
    io.Pull = GPIO_NOPULL;
    io.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &io);

    // USART1 configuration
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;

    // Initialize USART1
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
}

static void MX_TIM2_Init(void) {
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 7999u;         /* HSI 8MHz -> 1kHz */
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = LAB3_TIM2_TICK_MS;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(TIM2_IRQn, 2u, 0u);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

static void MX_TIM3_Init(void) {
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 7u;            /* HSI 8MHz -> 1MHz */
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 999u;            /* 1ms digit-scan tick */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim3) != HAL_OK) Error_Handler();
    HAL_NVIC_SetPriority(TIM3_IRQn, 3u, 0u);
    HAL_NVIC_EnableIRQ(TIM3_IRQn);
}

void Error_Handler(void) {
    __disable_irq();
    while (1) { }
}
