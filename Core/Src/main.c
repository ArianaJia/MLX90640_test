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
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <stdint.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint16_t eeData[832];
uint16_t frameData[834];
float tempMap[768];
paramsMLX90640 mlx90640;
uint16_t i = 0;
uint16_t row = 0;
uint16_t col = 0;
float ta = 0.0;
float tr = 0.0;
int status = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static int MLX90640_InitSensor(void);
static void UART_SendString(const char *text);
static void UART_SendInt(int value);
static void MLX90640_PrintTemperature(float temperature);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int MLX90640_InitSensor(void)
{
  int status;

  status = MLX90640_DumpEE(MLX90640_ADDR, eeData);
  if (status != MLX90640_NO_ERROR)
  {
    return status;
  }

  status = MLX90640_ExtractParameters(eeData, &mlx90640);
  if (status != MLX90640_NO_ERROR)
  {
    return status;
  }

  status = MLX90640_SetChessMode(MLX90640_ADDR);
  if (status != MLX90640_NO_ERROR)
  {
    return status;
  }

  status = MLX90640_SetRefreshRate(MLX90640_ADDR, MLX90640_REF_8HZ);
  if (status != MLX90640_NO_ERROR)
  {
    return status;
  }

  return MLX90640_NO_ERROR;
}

static void UART_SendString(const char *text)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

static void UART_SendInt(int value)
{
  char buffer[12];
  uint32_t magnitude;
  uint32_t index = 0;
  uint32_t start = 0;
  char temp;

  if (value < 0)
  {
    buffer[index++] = '-';
    magnitude = (uint32_t)(-value);
    start = 1;
  }
  else
  {
    magnitude = (uint32_t)value;
  }

  do
  {
    buffer[index++] = (char)('0' + (magnitude % 10U));
    magnitude /= 10U;
  } while (magnitude > 0U);

  buffer[index] = '\0';

  for (uint32_t left = start, right = index - 1U; left < right; left++, right--)
  {
    temp = buffer[left];
    buffer[left] = buffer[right];
    buffer[right] = temp;
  }

  UART_SendString(buffer);
}

static void MLX90640_PrintTemperature(float temperature)
{
  int32_t centiDegrees;
  int32_t absValue;
  char buffer[16];
  uint32_t integerPart;
  uint32_t fractionPart;
  uint32_t index = 0;

  if (temperature >= 0.0f)
  {
    centiDegrees = (int32_t)(temperature * 100.0f + 0.5f);
  }
  else
  {
    centiDegrees = (int32_t)(temperature * 100.0f - 0.5f);
  }

  absValue = (centiDegrees < 0) ? -centiDegrees : centiDegrees;
  integerPart = (uint32_t)(absValue / 100);
  fractionPart = (uint32_t)(absValue % 100);

  if (centiDegrees < 0)
  {
    buffer[index++] = '-';
  }

  if (integerPart >= 100U)
  {
    buffer[index++] = (char)('0' + (integerPart / 100U) % 10U);
  }
  if (integerPart >= 10U)
  {
    buffer[index++] = (char)('0' + (integerPart / 10U) % 10U);
  }
  buffer[index++] = (char)('0' + (integerPart % 10U));
  buffer[index++] = '.';
  buffer[index++] = (char)('0' + (fractionPart / 10U));
  buffer[index++] = (char)('0' + (fractionPart % 10U));
  buffer[index++] = ' ';
  buffer[index] = '\0';

  UART_SendString(buffer);
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

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  if (MLX90640_InitSensor() != MLX90640_NO_ERROR)
  {
    UART_SendString("MLX90640 init failed\r\n");
    Error_Handler();
  }

  UART_SendString("MLX90640 initialized\r\n");
  HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin, GPIO_PIN_SET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
//  while (1)
  for(;;)
  {

	HAL_GPIO_TogglePin(LED_GPIO_Port,LED_Pin);
    status = MLX90640_GetFrameData(MLX90640_ADDR, frameData);
    if (status < 0)
    {
      UART_SendString("Frame read error: ");
      UART_SendInt(status);
      UART_SendString("\r\n");
      continue;
    }

    ta = MLX90640_GetTa(frameData, &mlx90640);
    tr = ta - MLX90640_TA_SHIFT;
    MLX90640_CalculateTo(frameData, &mlx90640, MLX90640_EMISSIVITY, tr, tempMap);
    MLX90640_BadPixelsCorrection(mlx90640.brokenPixels, tempMap, 1, &mlx90640);
    MLX90640_BadPixelsCorrection(mlx90640.outlierPixels, tempMap, 1, &mlx90640);

    UART_SendString("----- Temperature Matrix -----\r\n");
    for (row = 0; row < 24; row++)
    {
      for (col = 0; col < 32; col++)
      {
        i = (uint16_t)(row * 32U + col);
        MLX90640_PrintTemperature(tempMap[i]);
      }
      UART_SendString("\r\n");
    }
    UART_SendString("----- Temperature Matrix End -----\r\n");
    HAL_Delay(125);

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
