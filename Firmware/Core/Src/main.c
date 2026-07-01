/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "FreeRTOS.h"
#include "cmsis_os2.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ili9488.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_events.h"
#include "tuner.h"
#include "screen.h"
#include "cs4270.h"
#include "AudioPath.h"
#include "task.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320
#define BUFF_SIZE (DISPLAY_WIDTH * 10)
#define LVGL_BUF_PIXELS  (DISPLAY_WIDTH * 10)
#define RGB666_BUF_BYTES (LVGL_BUF_PIXELS * 3)

#define AUDIO_BUFFER_SIZE 256
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;

I2C_HandleTypeDef hi2c2;

SAI_HandleTypeDef hsai_BlockA2;
SAI_HandleTypeDef hsai_BlockB2;
DMA_HandleTypeDef hdma_sai2_b;
DMA_HandleTypeDef hdma_sai2_a;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim8;

/* Definitions for InputTask */
osThreadId_t InputTaskHandle;
const osThreadAttr_t InputTask_attributes = {
  .name = "InputTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for lvglTick */
osThreadId_t lvglTickHandle;
const osThreadAttr_t lvglTick_attributes = {
  .name = "lvglTick",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for lvglTimer */
osThreadId_t lvglTimerHandle;
const osThreadAttr_t lvglTimer_attributes = {
  .name = "lvglTimer",
  .stack_size = 4096 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for audioTask */
osThreadId_t audioTaskHandle;
const osThreadAttr_t audioTask_attributes = {
  .name = "audioTask",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for lvgl_mutex */
osMutexId_t lvgl_mutexHandle;
const osMutexAttr_t lvgl_mutex_attributes = {
  .name = "lvgl_mutex"
};
/* Definitions for sem_flush */
osSemaphoreId_t sem_flushHandle;
const osSemaphoreAttr_t sem_flush_attributes = {
  .name = "sem_flush"
};
/* Definitions for sem_audio */
osSemaphoreId_t sem_audioHandle;
const osSemaphoreAttr_t sem_audio_attributes = {
  .name = "sem_audio"
};
/* USER CODE BEGIN PV */
// UI Variables
static lv_color_t buf_1[LVGL_BUF_PIXELS] __attribute__((section(".displaybuffer")));
static lv_color_t buf_2[LVGL_BUF_PIXELS] __attribute__((section(".displaybuffer")));
static uint8_t    rgb666_buf[RGB666_BUF_BYTES] __attribute__((section(".displaybuffer")));

extern lv_group_t *encoder_group;
volatile static int32_t last_encoder_value = 0;
lv_indev_t *encoder_indev = NULL;

// Audio Variables prob refactor
static int32_t adcBuf[AUDIO_BUFFER_SIZE] 		__attribute__ ((section(".audiobuffer")));
static int32_t dacBuf[AUDIO_BUFFER_SIZE] 		__attribute__ ((section(".audiobuffer")));

static volatile int32_t *inBufPtr 		__attribute__ ((section(".audiobuffer")));
static volatile int32_t *outBufPtr 		__attribute__ ((section(".audiobuffer")));

volatile uint8_t processHalfFlag		__attribute__ ((section(".audiobuffer")));

extern AudioChannel_t Channel1;
extern AudioChannel_t Channel2;

volatile float volume = 1.0f;
volatile uint8_t paramSide = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM4_Init(void);
static void MX_SAI2_Init(void);
static void MX_I2C2_Init(void);
static void MX_ADC3_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM8_Init(void);
void StartInputTask(void *argument);
void LVGLTick(void *argument);
void LVGLTimer(void *argument);
void StartAudioTask(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void my_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    uint32_t n_px = lv_area_get_size(area);
    uint16_t *src = (uint16_t *)px_map;
    uint8_t  *dst = rgb666_buf;

    for (uint32_t i = 0; i < n_px; i++)
    {
        uint16_t c = *src++;
        *dst++ = ((c >> 11) & 0x1F) << 3;
        *dst++ = ((c >>  5) & 0x3F) << 2;
        *dst++ = ((c >>  0) & 0x1F) << 3;
    }

    uint32_t  remaining = n_px * 3;
    uint8_t  *ptr       = rgb666_buf;

    setAddrWindow(area->x1, area->y1, area->x2, area->y2);
    DC_DATA();
    CS_A();

    // Blocking for all but final chunk
    while (remaining > 65535)
    {
        HAL_SPI_Transmit(&hspi1, ptr, 65535, HAL_MAX_DELAY);
        ptr       += 65535;
        remaining -= 65535;
    }

    // Final chunk async — callback releases semaphore

    HAL_SPI_Transmit_DMA(&hspi1, ptr, (uint16_t)remaining);
    osSemaphoreAcquire(sem_flushHandle, osWaitForever);

    lv_display_flush_ready(disp);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &hspi1)
    {
        CS_D();
        osSemaphoreRelease(sem_flushHandle);
    }
}

static void encoder_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
   // Button
   data->state = HAL_GPIO_ReadPin(NavEncoder_SW_GPIO_Port, NavEncoder_SW_Pin) == GPIO_PIN_RESET
                 ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
   // Rotation with wraparound
   int32_t current_val = (int32_t)__HAL_TIM_GET_COUNTER(&htim4);
   int32_t diff = current_val - last_encoder_value;
   if (diff > 32767)  diff -= 65536;
   if (diff < -32767) diff += 65536;
   last_encoder_value = current_val;
   data->enc_diff = (int16_t)diff;
}

void HAL_SAI_RxHalfCpltCallback(SAI_HandleTypeDef *hsai) {

	inBufPtr  = &(adcBuf[0]);
	outBufPtr = &(dacBuf[0]);

	osSemaphoreRelease(sem_audioHandle);

}

void HAL_SAI_RxCpltCallback(SAI_HandleTypeDef *hsai) {

	inBufPtr  = &(adcBuf[AUDIO_BUFFER_SIZE/2]);
	outBufPtr = &(dacBuf[AUDIO_BUFFER_SIZE/2]);

	osSemaphoreRelease(sem_audioHandle);

}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if(GPIO_Pin == Encoder_3W_Pin)
    {
    	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    	paramSide ^= 1;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();
  MX_SAI2_Init();
  MX_I2C2_Init();
  MX_ADC3_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM8_Init();
  /* USER CODE BEGIN 2 */
  ILI9488_Init();
  lv_init();

  lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  lv_display_set_flush_cb(disp, my_flush_cb);
  lv_display_set_buffers(disp, buf_1, buf_2, sizeof(buf_1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
  encoder_indev = lv_indev_create();
  lv_indev_set_type(encoder_indev, LV_INDEV_TYPE_ENCODER);
  lv_indev_set_read_cb(encoder_indev, encoder_read_cb);

  encoder_group = lv_group_create();
  lv_group_set_default(encoder_group);
  lv_indev_set_group(encoder_indev, encoder_group);

  ui_init();
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of lvgl_mutex */
  lvgl_mutexHandle = osMutexNew(&lvgl_mutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* Create the semaphores(s) */
  /* creation of sem_flush */
  sem_flushHandle = osSemaphoreNew(1, 0, &sem_flush_attributes);

  /* creation of sem_audio */
  sem_audioHandle = osSemaphoreNew(1, 0, &sem_audio_attributes);

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of InputTask */
  InputTaskHandle = osThreadNew(StartInputTask, NULL, &InputTask_attributes);

  /* creation of lvglTick */
  lvglTickHandle = osThreadNew(LVGLTick, NULL, &lvglTick_attributes);

  /* creation of lvglTimer */
  lvglTimerHandle = osThreadNew(LVGLTimer, NULL, &lvglTimer_attributes);

  /* creation of audioTask */
  audioTaskHandle = osThreadNew(StartAudioTask, NULL, &audioTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 75;
  RCC_OscInitStruct.PLL.PLLP = 4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 4;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 3072;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SAI2|RCC_PERIPHCLK_CKPER;
  PeriphClkInitStruct.PLL3.PLL3M = 2;
  PeriphClkInitStruct.PLL3.PLL3N = 48;
  PeriphClkInitStruct.PLL3.PLL3P = 24;
  PeriphClkInitStruct.PLL3.PLL3Q = 2;
  PeriphClkInitStruct.PLL3.PLL3R = 2;
  PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
  PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
  PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
  PeriphClkInitStruct.CkperClockSelection = RCC_CLKPSOURCE_HSI;
  PeriphClkInitStruct.Sai23ClockSelection = RCC_SAI23CLKSOURCE_PLL3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC3_Init(void)
{

  /* USER CODE BEGIN ADC3_Init 0 */

  /* USER CODE END ADC3_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC3_Init 1 */

  /* USER CODE END ADC3_Init 1 */

  /** Common config
  */
  hadc3.Instance = ADC3;
  hadc3.Init.Resolution = ADC_RESOLUTION_16B;
  hadc3.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc3.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc3.Init.LowPowerAutoWait = DISABLE;
  hadc3.Init.ContinuousConvMode = DISABLE;
  hadc3.Init.NbrOfConversion = 1;
  hadc3.Init.DiscontinuousConvMode = DISABLE;
  hadc3.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc3.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc3.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc3.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc3.Init.OversamplingMode = DISABLE;
  hadc3.Init.Oversampling.Ratio = 1;
  if (HAL_ADC_Init(&hadc3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;
  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC3_Init 2 */

  /* USER CODE END ADC3_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x10707DBC;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SAI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SAI2_Init(void)
{

  /* USER CODE BEGIN SAI2_Init 0 */

  /* USER CODE END SAI2_Init 0 */

  /* USER CODE BEGIN SAI2_Init 1 */

  /* USER CODE END SAI2_Init 1 */
  hsai_BlockA2.Instance = SAI2_Block_A;
  hsai_BlockA2.Init.AudioMode = SAI_MODESLAVE_TX;
  hsai_BlockA2.Init.Synchro = SAI_SYNCHRONOUS;
  hsai_BlockA2.Init.OutputDrive = SAI_OUTPUTDRIVE_DISABLE;
  hsai_BlockA2.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockA2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
  hsai_BlockA2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockA2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockA2.Init.CompandingMode = SAI_NOCOMPANDING;
  hsai_BlockA2.Init.TriState = SAI_OUTPUT_NOTRELEASED;
  if (HAL_SAI_InitProtocol(&hsai_BlockA2, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_24BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
  hsai_BlockB2.Instance = SAI2_Block_B;
  hsai_BlockB2.Init.AudioMode = SAI_MODEMASTER_RX;
  hsai_BlockB2.Init.Synchro = SAI_ASYNCHRONOUS;
  hsai_BlockB2.Init.OutputDrive = SAI_OUTPUTDRIVE_ENABLE;
  hsai_BlockB2.Init.NoDivider = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockB2.Init.MckOverSampling = SAI_MCK_OVERSAMPLING_DISABLE;
  hsai_BlockB2.Init.FIFOThreshold = SAI_FIFOTHRESHOLD_1QF;
  hsai_BlockB2.Init.AudioFrequency = SAI_AUDIO_FREQUENCY_48K;
  hsai_BlockB2.Init.SynchroExt = SAI_SYNCEXT_DISABLE;
  hsai_BlockB2.Init.MonoStereoMode = SAI_STEREOMODE;
  hsai_BlockB2.Init.CompandingMode = SAI_NOCOMPANDING;
  if (HAL_SAI_InitProtocol(&hsai_BlockB2, SAI_I2S_STANDARD, SAI_PROTOCOL_DATASIZE_24BIT, 2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SAI2_Init 2 */

  /* USER CODE END SAI2_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x0;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 0;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim4, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief TIM8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM8_Init(void)
{

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 0;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 65535;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI1;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 0;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim8, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA1_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(Codec_RST_GPIO_Port, Codec_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, TFT_CS_Pin|TFT_RST_Pin|TFT_DC_Pin|TFT_BL_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Encoder_3W_Pin */
  GPIO_InitStruct.Pin = Encoder_3W_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(Encoder_3W_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : Codec_RST_Pin */
  GPIO_InitStruct.Pin = Codec_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(Codec_RST_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_CS_Pin TFT_RST_Pin TFT_DC_Pin */
  GPIO_InitStruct.Pin = TFT_CS_Pin|TFT_RST_Pin|TFT_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : TFT_BL_Pin */
  GPIO_InitStruct.Pin = TFT_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TFT_BL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : NavEncoder_SW_Pin */
  GPIO_InitStruct.Pin = NavEncoder_SW_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(NavEncoder_SW_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(Encoder_3W_EXTI_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(Encoder_3W_EXTI_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartInputTask */
/**
  * @brief  Function implementing the InputTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartInputTask */
void StartInputTask(void *argument)
{
  /* USER CODE BEGIN 5 */
	HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
	HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL);
	HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
	HAL_TIM_Encoder_Start(&htim8, TIM_CHANNEL_ALL);

	static uint32_t lastEnc1 = 0, lastEnc2 = 0, lastEnc3 = 0;
  /* Infinite loop */
  for(;;)
  {
      osDelay(pdMS_TO_TICKS(200));

      // Volume Pot
      uint32_t raw = 0;
      HAL_ADC_Start(&hadc3);
      if (HAL_ADC_PollForConversion(&hadc3, 10) == HAL_OK) {
          raw = HAL_ADC_GetValue(&hadc3);
      }
      HAL_ADC_Stop(&hadc3);
      int32_t volume_percent = (((65535 - raw) * 100) >> 16) + 1;
      volume = (65535 - raw) / 65535.0f; // for 0-1 audio scaling

      // Parameter Encoders
      uint32_t enc1 = __HAL_TIM_GET_COUNTER(&htim8);
      uint32_t enc2 = __HAL_TIM_GET_COUNTER(&htim3);
      uint32_t enc3 = __HAL_TIM_GET_COUNTER(&htim2);

      int32_t delta1 = (int16_t)(enc1 - lastEnc1);
      int32_t delta2 = (int16_t)(enc2 - lastEnc2);
      int32_t delta3 = (int16_t)(enc3 - lastEnc3);

      lastEnc1 = enc1; lastEnc2 = enc2; lastEnc3 = enc3;

      if (g_display_mgr.activeSlotIdx > 2) offset = 3;
      AudioChannel_t* pChannel = (g_display_mgr.activeSlotIdx < 5) ? &Channel1 : &Channel2;
	  int internal_slot_idx = (g_display_mgr.activeSlotIdx < 5) ? g_display_mgr.activeSlotIdx : g_display_mgr.activeSlotIdx - 5;

	  // keep this small so other lvgl tasks dont get blocked for a long time
	  osMutexAcquire(lvgl_mutexHandle, osWaitForever);

	  lv_arc_set_value(ui_VolumeIndicator, volume_percent);

	  if (lv_scr_act() == ui_Pedal) {
		  if (paramSide == 0) {
			lv_arc_set_value(ui_Parameter,  lv_arc_get_value(ui_Parameter)  + delta1);
			lv_arc_set_value(ui_Parameter1, lv_arc_get_value(ui_Parameter1) + delta2);
			lv_arc_set_value(ui_Parameter2, lv_arc_get_value(ui_Parameter2) + delta3);

			lv_label_set_text_fmt(ui_ParamValue, "%ld", lv_arc_get_value(ui_Parameter));
			lv_label_set_text_fmt(ui_ParamValue1, "%ld", lv_arc_get_value(ui_Parameter1));
			lv_label_set_text_fmt(ui_ParamValue2, "%ld", lv_arc_get_value(ui_Parameter2));

			pChannel->chain[internal_slot_idx].params[0] = lv_arc_get_value(ui_Parameter);
			pChannel->chain[internal_slot_idx].params[1] = lv_arc_get_value(ui_Parameter1);
			pChannel->chain[internal_slot_idx].params[2] = lv_arc_get_value(ui_Parameter2);

		} else {
			lv_arc_set_value(ui_Parameter3, lv_arc_get_value(ui_Parameter3) + delta1);
			lv_arc_set_value(ui_Parameter4, lv_arc_get_value(ui_Parameter4) + delta2);
			lv_arc_set_value(ui_Parameter5, lv_arc_get_value(ui_Parameter5) + delta3);

			lv_label_set_text_fmt(ui_ParamValue3, "%ld", lv_arc_get_value(ui_Parameter3));
			lv_label_set_text_fmt(ui_ParamValue4, "%ld", lv_arc_get_value(ui_Parameter4));
			lv_label_set_text_fmt(ui_ParamValue5, "%ld", lv_arc_get_value(ui_Parameter5));

			pChannel->chain[internal_slot_idx].params[3] = lv_arc_get_value(ui_Parameter3);
			pChannel->chain[internal_slot_idx].params[4] = lv_arc_get_value(ui_Parameter4);
			pChannel->chain[internal_slot_idx].params[5] = lv_arc_get_value(ui_Parameter5);
		}
	}

    //syncChannelFromDisplay(&Channel1);
    //syncChannelFromDisplay(&Channel2);

    osMutexRelease(lvgl_mutexHandle);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_LVGLTick */
/**
* @brief Function implementing the lvglTick thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LVGLTick */
void LVGLTick(void *argument)
{
  /* USER CODE BEGIN LVGLTick */
  /* Infinite loop */
  for(;;)
  {
      lv_tick_inc(5);
      osDelay(5);
  }
  /* USER CODE END LVGLTick */
}

/* USER CODE BEGIN Header_LVGLTimer */
/**
* @brief Function implementing the lvglTimer thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LVGLTimer */
void LVGLTimer(void *argument)
{
  /* USER CODE BEGIN LVGLTimer */
  /* Infinite loop */
  for(;;)
  {
	  osMutexAcquire(lvgl_mutexHandle, osWaitForever);
	  lv_timer_handler();
	  osMutexRelease(lvgl_mutexHandle);
      osDelay(10);
  }
  /* USER CODE END LVGLTimer */
}

/* USER CODE BEGIN Header_StartAudioTask */
/**
* @brief Function implementing the audioTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartAudioTask */
void StartAudioTask(void *argument)
{
  /* USER CODE BEGIN StartAudioTask */
	CS4270_Init();
	audioPathInit(&Channel1, &Channel2);
	HAL_SAI_Transmit_DMA(&hsai_BlockA2, (uint8_t *) dacBuf, AUDIO_BUFFER_SIZE);
	HAL_SAI_Receive_DMA(&hsai_BlockB2, (uint8_t *) adcBuf, AUDIO_BUFFER_SIZE);
  /* Infinite loop */
  for(;;)
  {
	  osSemaphoreAcquire(sem_audioHandle, osWaitForever);

  	static float leftIn		= 0.0f;
  	static float rightIn	= 0.0f;

  	static float leftOut	= 0.0f;
  	static float rightOut	= 0.0f;

  	for (uint16_t sampleIndex = 0; sampleIndex < (AUDIO_BUFFER_SIZE/2) - 1; sampleIndex += 2) {

  		syncChannelFromDisplay(&Channel1);
  		syncChannelFromDisplay(&Channel2);

  		// Extract 24-bits via bit mask
  		inBufPtr[sampleIndex]		&= 0xFFFFFF;
  		inBufPtr[sampleIndex + 1]	&= 0xFFFFFF;

  		if (inBufPtr[sampleIndex] & 0x800000) {
  			inBufPtr[sampleIndex] |= ~0xFFFFFF;
  		}

  		if (inBufPtr[sampleIndex + 1] & 0x800000) {
  			inBufPtr[sampleIndex + 1] |= ~0xFFFFFF;
  		}

  		// Normalise
  		leftIn  = (float) inBufPtr[sampleIndex]     / (float) (0x7FFFFF);
  		rightIn = (float) inBufPtr[sampleIndex + 1] / (float) (0x7FFFFF);

  		// flipped left and right because I made the wire shorter and it doesnt reach the port so its easier and less wasteful to just switch the assignments in software
  		leftOut  = effectsRouter(&Channel2, rightIn) * volume;
  		rightOut = effectsRouter(&Channel1, leftIn) * volume;

  		if (leftOut < -1.0f) {
  			leftOut = -1.0f;

  		} else if (leftOut > 1.0f) {
  			leftOut =  1.0f;
  		}

  		if (rightOut < -1.0f) {
  			rightOut = -1.0f;
  		} else if (rightOut > 1.0f) {
  			rightOut =  1.0f;
  		}

  		// Scale to 24-bit signed integer and set output buffer
  		outBufPtr[sampleIndex]	   = (int32_t) (leftOut  * 0x7FFFFF);
  		outBufPtr[sampleIndex + 1] = (int32_t) (rightOut * 0x7FFFFF);
  	}
  }
  /* USER CODE END StartAudioTask */
}

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.BaseAddress = 0x30000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER4;
  MPU_InitStruct.BaseAddress = 0x38000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
