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
#include "dma.h"
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
typedef struct
{
  uint16_t EeData[832];
  uint16_t FrameData[834];
  float TempMap[768];
  paramsMLX90640 Params;
} MLX90640_RuntimeData_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_LINE_BUFFER_SIZE 512U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static MLX90640_RuntimeData_t mlx90640Runtime;
uint16_t i = 0;
uint16_t row = 0;
uint16_t col = 0;
float ta = 0.0;
float tr = 0.0;
int status = 0;
volatile uint8_t uartTxDone = 1;
static char uartLineBuffer[UART_LINE_BUFFER_SIZE];
static uint8_t mlx90640SensorReady[MLX90640_SENSOR_NUM];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static int MLX90640_InitSensor(void);
static int MLX90640_InitSensorOnChannel(uint8_t sensorIndex);
static int MLX90640_LoadParametersOnChannel(uint8_t sensorIndex);
static void UART_SendString(const char *text);
static void UART_SendInt(int value);
static void UART_SendHexByte(uint8_t value);
static void UART_SendHexWord(uint32_t value);
static void UART_SendInitError(uint8_t sensorIndex, const char *step, int error);
static uint16_t UART_AppendTemperature(char *buffer, uint16_t offset, uint16_t size, float temperature);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static int MLX90640_InitSensor(void)
{
  uint8_t initializedCount = 0U;

  for (uint8_t sensorIndex = 0U; sensorIndex < MLX90640_SENSOR_NUM; sensorIndex++)
  {
    int status = MLX90640_InitSensorOnChannel(sensorIndex);
    if (status != MLX90640_NO_ERROR)
    {
      mlx90640SensorReady[sensorIndex] = 0U;
      continue;
    }

    mlx90640SensorReady[sensorIndex] = 1U;
    initializedCount++;
    UART_SendString("MLX90640 init ok, sensor=");
    UART_SendInt(sensorIndex);
    UART_SendString(", channel=");
    UART_SendHexByte(MLX90640_GetSensorChannel(sensorIndex));
    UART_SendString("\r\n");
  }

  if (initializedCount == 0U)
  {
    return -MLX90640_I2C_WRITE_ERROR;
  }

  return MLX90640_NO_ERROR;
}

static int MLX90640_InitSensorOnChannel(uint8_t sensorIndex)
{
  int status;

  status = MLX90640_LoadParametersOnChannel(sensorIndex);
  if (status != MLX90640_NO_ERROR)
  {
    return status;
  }

  status = MLX90640_SetChessMode(MLX90640_ADDR);
  if (status != MLX90640_NO_ERROR)
  {
    UART_SendInitError(sensorIndex, "set chess mode", status);
    return status;
  }

  status = MLX90640_SetRefreshRate(MLX90640_ADDR, MLX90640_REF_8HZ);
  if (status != MLX90640_NO_ERROR)
  {
    UART_SendInitError(sensorIndex, "set refresh rate", status);
    return status;
  }

  return MLX90640_NO_ERROR;
}

static int MLX90640_LoadParametersOnChannel(uint8_t sensorIndex)
{
  int status;

  status = MLX90640_SelectSensor(sensorIndex);
  if (status != MLX90640_NO_ERROR)
  {
    UART_SendInitError(sensorIndex, "select TCA9548A channel", status);
    return status;
  }

  status = MLX90640_I2CProbe(MLX90640_ADDR);
  UART_SendString("MLX90640 probe, sensor=");
  UART_SendInt(sensorIndex);
  UART_SendString(", channel=");
  UART_SendHexByte(MLX90640_GetSensorChannel(sensorIndex));
  UART_SendString(", status=");
  UART_SendInt(status);
  UART_SendString(", hal=");
  UART_SendInt((int)MLX90640_GetLastHalStatus());
  UART_SendString(", err=");
  UART_SendHexWord(MLX90640_GetLastErrorCode());
  UART_SendString("\r\n");
  if (status != MLX90640_NO_ERROR)
  {
    return status;
  }

  status = MLX90640_DumpEE(MLX90640_ADDR, mlx90640Runtime.EeData);
  if (status != MLX90640_NO_ERROR)
  {
    UART_SendInitError(sensorIndex, "dump EEPROM", status);
    return status;
  }

  status = MLX90640_ExtractParameters(mlx90640Runtime.EeData, &mlx90640Runtime.Params);
  if (status != MLX90640_NO_ERROR)
  {
    UART_SendInitError(sensorIndex, "extract parameters", status);
  }

  return status;
}

static void UART_SendString(const char *text)
{
	uint16_t len = (uint16_t)strlen(text);

	    while (!uartTxDone)
	    {
	    }

	    uartTxDone = 0;
	    if (HAL_UART_Transmit_DMA(&huart1, (uint8_t *)text, len) != HAL_OK)
	    {
	        uartTxDone = 1;
	        return;
	    }

	    while (!uartTxDone)
	    {
	    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        uartTxDone = 1;
    }
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

static void UART_SendHexByte(uint8_t value)
{
  static const char hexDigits[] = "0123456789ABCDEF";
  char buffer[5];

  buffer[0] = '0';
  buffer[1] = 'x';
  buffer[2] = hexDigits[(value >> 4) & 0x0FU];
  buffer[3] = hexDigits[value & 0x0FU];
  buffer[4] = '\0';

  UART_SendString(buffer);
}

static void UART_SendHexWord(uint32_t value)
{
  static const char hexDigits[] = "0123456789ABCDEF";
  char buffer[11];

  buffer[0] = '0';
  buffer[1] = 'x';
  for (uint8_t index = 0U; index < 8U; index++)
  {
    uint8_t shift = (uint8_t)(28U - (index * 4U));
    buffer[2U + index] = hexDigits[(value >> shift) & 0x0FU];
  }
  buffer[10] = '\0';

  UART_SendString(buffer);
}

static void UART_SendInitError(uint8_t sensorIndex, const char *step, int error)
{
  UART_SendString("MLX90640 init error, sensor=");
  UART_SendInt(sensorIndex);
  UART_SendString(", channel=");
  UART_SendHexByte(MLX90640_GetSensorChannel(sensorIndex));
  UART_SendString(", step=");
  UART_SendString(step);
  UART_SendString(", status=");
  UART_SendInt(error);
  UART_SendString(", hal=");
  UART_SendInt((int)TCA9548A_GetLastHalStatus());
  UART_SendString(", err=");
  UART_SendHexWord(TCA9548A_GetLastErrorCode());
  UART_SendString("\r\n");
}

static uint16_t UART_AppendTemperature(char *buffer, uint16_t offset, uint16_t size, float temperature)
{
  int32_t centiDegrees;
  int32_t absValue;
  uint32_t integerPart;
  uint32_t fractionPart;
  char temp[16];
  uint32_t index = 0;

  if (offset >= size)
  {
    return offset;
  }

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
    temp[index++] = '-';
  }

  if (integerPart >= 100U)
  {
    temp[index++] = (char)('0' + (integerPart / 100U) % 10U);
  }
  if (integerPart >= 10U)
  {
    temp[index++] = (char)('0' + (integerPart / 10U) % 10U);
  }
  temp[index++] = (char)('0' + (integerPart % 10U));
  temp[index++] = '.';
  temp[index++] = (char)('0' + (fractionPart / 10U));
  temp[index++] = (char)('0' + (fractionPart % 10U));
  temp[index++] = ' ';

  for (uint32_t copyIndex = 0; (copyIndex < index) && (offset < (size - 1U)); copyIndex++)
  {
    buffer[offset++] = temp[copyIndex];
  }

  buffer[offset] = '\0';
  return offset;
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
  MX_DMA_Init();
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
    uint8_t subPageMask = 0U;
    uint8_t frameAttempts = 0U;

	HAL_GPIO_TogglePin(LED_GPIO_Port,LED_Pin);
    for (uint8_t sensorIndex = 0U; sensorIndex < MLX90640_SENSOR_NUM; sensorIndex++)
    {
      if (mlx90640SensorReady[sensorIndex] == 0U)
      {
        continue;
      }

      subPageMask = 0U;
      frameAttempts = 0U;

      status = MLX90640_LoadParametersOnChannel(sensorIndex);
      if (status != MLX90640_NO_ERROR)
      {
        mlx90640SensorReady[sensorIndex] = 0U;
        continue;
      }

      while ((subPageMask != 0x03U) && (frameAttempts < 4U))
      {
        status = MLX90640_GetFrameData(MLX90640_ADDR, mlx90640Runtime.FrameData);
        if (status < 0)
        {
          UART_SendString("Frame read error: ");
          UART_SendInt(status);
          UART_SendString("\r\n");
          break;
        }

        subPageMask |= (uint8_t)(1U << (mlx90640Runtime.FrameData[833] & 1U));
        ta = MLX90640_GetTa(mlx90640Runtime.FrameData, &mlx90640Runtime.Params);
        tr = ta - MLX90640_TA_SHIFT;
        MLX90640_CalculateTo(
            mlx90640Runtime.FrameData,
            &mlx90640Runtime.Params,
            MLX90640_EMISSIVITY,
            tr,
            mlx90640Runtime.TempMap);
        frameAttempts++;
      }

      if ((status < 0) || (subPageMask != 0x03U))
      {
        continue;
      }

      MLX90640_BadPixelsCorrection(
          mlx90640Runtime.Params.brokenPixels,
          mlx90640Runtime.TempMap,
          1,
          &mlx90640Runtime.Params);
      MLX90640_BadPixelsCorrection(
          mlx90640Runtime.Params.outlierPixels,
          mlx90640Runtime.TempMap,
          1,
          &mlx90640Runtime.Params);

      UART_SendString("----- Sensor ");
      UART_SendInt(sensorIndex);
      UART_SendString(" Temperature Matrix -----\r\n");
      for (row = 0; row < 24; row++)
      {
        uint16_t lineOffset = 0;

        for (col = 0; col < 32; col++)
        {
          i = (uint16_t)(row * 32U + col);
          lineOffset = UART_AppendTemperature(
              uartLineBuffer,
              lineOffset,
              UART_LINE_BUFFER_SIZE,
              mlx90640Runtime.TempMap[i]);
        }

        if (lineOffset < (UART_LINE_BUFFER_SIZE - 2U))
        {
          uartLineBuffer[lineOffset++] = '\r';
          uartLineBuffer[lineOffset++] = '\n';
        }
        uartLineBuffer[lineOffset] = '\0';
        UART_SendString(uartLineBuffer);
      }
      UART_SendString("----- Sensor ");
      UART_SendInt(sensorIndex);
      UART_SendString(" Temperature Matrix End -----\r\n");
    }
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
