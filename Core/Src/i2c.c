/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    i2c.c
  * @brief   This file provides code for the configuration
  *          of the I2C instances.
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
#include "i2c.h"

/* USER CODE BEGIN 0 */
static HAL_StatusTypeDef tca9548aLastHalStatus = HAL_OK;
static uint32_t tca9548aLastErrorCode = HAL_I2C_ERROR_NONE;
static HAL_StatusTypeDef mlx90640LastHalStatus = HAL_OK;
static uint32_t mlx90640LastErrorCode = HAL_I2C_ERROR_NONE;
static uint8_t i2c1StartupRecoveryDone = 0U;
static uint8_t tca9548aConsecutiveBusyCount = 0U;
static const uint8_t mlx90640SensorChannels[MLX90640_SENSOR_NUM] = {
  0U,
  1U,
  2U,
  3U
};

static void I2C1_RecoverBus(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  HAL_I2C_DeInit(&hi2c1);
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(1);

  for (uint8_t pulse = 0U; pulse < 9U; pulse++)
  {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
  }

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
  HAL_Delay(1);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(1);

  MX_I2C1_Init();
}

static uint8_t TCA9548A_IsBusyOrTimeout(void)
{
  return ((tca9548aLastHalStatus == HAL_BUSY) ||
          ((tca9548aLastErrorCode & HAL_I2C_ERROR_TIMEOUT) != 0U)) ? 1U : 0U;
}

static HAL_StatusTypeDef TCA9548A_WriteChannelMask(uint8_t channelMask)
{
  tca9548aLastHalStatus = HAL_I2C_Master_Transmit(
      &hi2c1, TCA9548A_ADDR, &channelMask, 1, MLX90640_I2C_TIMEOUT_MS);
  tca9548aLastErrorCode = hi2c1.ErrorCode;

  return tca9548aLastHalStatus;
}

static int MLX90640_HAL_StatusToError(HAL_StatusTypeDef status)
{
  if (status == HAL_OK)
  {
    return 0;
  }

  if ((status == HAL_ERROR) && ((hi2c1.ErrorCode & HAL_I2C_ERROR_AF) != 0U))
  {
    return -MLX90640_I2C_NACK_ERROR;
  }

  return -MLX90640_I2C_WRITE_ERROR;
}

/* USER CODE END 0 */

I2C_HandleTypeDef hi2c1;

/* I2C1 init function */
void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

void HAL_I2C_MspInit(I2C_HandleTypeDef* i2cHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspInit 0 */

  /* USER CODE END I2C1_MspInit 0 */

    __HAL_RCC_GPIOB_CLK_ENABLE();
    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* I2C1 clock enable */
    __HAL_RCC_I2C1_CLK_ENABLE();
  /* USER CODE BEGIN I2C1_MspInit 1 */

  /* USER CODE END I2C1_MspInit 1 */
  }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef* i2cHandle)
{

  if(i2cHandle->Instance==I2C1)
  {
  /* USER CODE BEGIN I2C1_MspDeInit 0 */

  /* USER CODE END I2C1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_I2C1_CLK_DISABLE();

    /**I2C1 GPIO Configuration
    PB6     ------> I2C1_SCL
    PB7     ------> I2C1_SDA
    */
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6);

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_7);

  /* USER CODE BEGIN I2C1_MspDeInit 1 */

  /* USER CODE END I2C1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */
void MLX90640_I2CInit(void)
{
  MX_I2C1_Init();
}

int MLX90640_I2CGeneralReset(void)
{
  uint8_t command = 0x06;

  return MLX90640_HAL_StatusToError(
      HAL_I2C_Master_Transmit(&hi2c1, 0x00, &command, 1, MLX90640_I2C_TIMEOUT_MS));
}

int TCA9548A_SelectChannel(uint8_t channel)
{
  if (channel > 7U)
  {
    tca9548aLastHalStatus = HAL_ERROR;
    tca9548aLastErrorCode = HAL_I2C_ERROR_NONE;
    return -MLX90640_I2C_WRITE_ERROR;
  }

  uint8_t channelMask = (uint8_t)(1U << channel);

  if (i2c1StartupRecoveryDone == 0U)
  {
    uint8_t disableAllChannels = 0U;

    I2C1_RecoverBus();
    (void)TCA9548A_WriteChannelMask(disableAllChannels);
    HAL_Delay(1);
    i2c1StartupRecoveryDone = 1U;
  }

  (void)TCA9548A_WriteChannelMask(channelMask);

  if (TCA9548A_IsBusyOrTimeout() != 0U)
  {
    tca9548aConsecutiveBusyCount++;

    if (tca9548aConsecutiveBusyCount >= 3U)
    {
      uint8_t disableAllChannels = 0U;

      I2C1_RecoverBus();
      (void)TCA9548A_WriteChannelMask(disableAllChannels);
      HAL_Delay(1);
      (void)TCA9548A_WriteChannelMask(channelMask);

      tca9548aConsecutiveBusyCount = (TCA9548A_IsBusyOrTimeout() != 0U) ? 1U : 0U;
    }
  }
  else
  {
    tca9548aConsecutiveBusyCount = 0U;
  }

  int status = MLX90640_HAL_StatusToError(tca9548aLastHalStatus);

  if (status == MLX90640_NO_ERROR)
  {
    HAL_Delay(1);
  }

  return status;
}

int MLX90640_SelectSensor(uint8_t sensorIndex)
{
  if (sensorIndex >= MLX90640_SENSOR_NUM)
  {
    return -MLX90640_I2C_WRITE_ERROR;
  }

  return TCA9548A_SelectChannel(mlx90640SensorChannels[sensorIndex]);
}

uint8_t MLX90640_GetSensorChannel(uint8_t sensorIndex)
{
  if (sensorIndex >= MLX90640_SENSOR_NUM)
  {
    return 0xFFU;
  }

  return mlx90640SensorChannels[sensorIndex];
}

HAL_StatusTypeDef TCA9548A_GetLastHalStatus(void)
{
  return tca9548aLastHalStatus;
}

uint32_t TCA9548A_GetLastErrorCode(void)
{
  return tca9548aLastErrorCode;
}

HAL_StatusTypeDef MLX90640_GetLastHalStatus(void)
{
  return mlx90640LastHalStatus;
}

uint32_t MLX90640_GetLastErrorCode(void)
{
  return mlx90640LastErrorCode;
}

int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data)
{
  uint16_t wordsRemaining = nMemAddressRead;
  uint16_t currentAddress = startAddress;
  uint16_t dataIndex = 0;
  uint8_t rawBuffer[MLX90640_I2C_READ_CHUNK_WORDS * 2];

  while (wordsRemaining > 0)
  {
    uint16_t wordsToRead = (wordsRemaining > MLX90640_I2C_READ_CHUNK_WORDS) ?
        MLX90640_I2C_READ_CHUNK_WORDS : wordsRemaining;
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        &hi2c1,
        slaveAddr,
        currentAddress,
        I2C_MEMADD_SIZE_16BIT,
        rawBuffer,
        (uint16_t)(wordsToRead * 2U),
        MLX90640_I2C_TIMEOUT_MS);
    mlx90640LastHalStatus = status;
    mlx90640LastErrorCode = hi2c1.ErrorCode;

    if (status != HAL_OK)
    {
      return MLX90640_HAL_StatusToError(status);
    }

    for (uint16_t i = 0; i < wordsToRead; ++i)
    {
      data[dataIndex + i] = ((uint16_t)rawBuffer[2U * i] << 8) | rawBuffer[2U * i + 1U];
    }

    dataIndex = (uint16_t)(dataIndex + wordsToRead);
    currentAddress = (uint16_t)(currentAddress + wordsToRead);
    wordsRemaining = (uint16_t)(wordsRemaining - wordsToRead);
  }

  return 0;
}

int MLX90640_I2CProbe(uint8_t slaveAddr)
{
  mlx90640LastHalStatus = HAL_I2C_IsDeviceReady(&hi2c1, slaveAddr, 1, MLX90640_I2C_TIMEOUT_MS);
  mlx90640LastErrorCode = hi2c1.ErrorCode;

  return MLX90640_HAL_StatusToError(mlx90640LastHalStatus);
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
  uint8_t rawBuffer[2];
  HAL_StatusTypeDef status;

  rawBuffer[0] = (uint8_t)(data >> 8);
  rawBuffer[1] = (uint8_t)(data & 0xFF);

  status = HAL_I2C_Mem_Write(
      &hi2c1,
      slaveAddr,
      writeAddress,
      I2C_MEMADD_SIZE_16BIT,
      rawBuffer,
      2,
      MLX90640_I2C_TIMEOUT_MS);
  mlx90640LastHalStatus = status;
  mlx90640LastErrorCode = hi2c1.ErrorCode;

  return MLX90640_HAL_StatusToError(status);
}

void MLX90640_I2CFreqSet(int freq)
{
  if ((freq <= 0) || (hi2c1.Init.ClockSpeed == (uint32_t)freq))
  {
    return;
  }

  hi2c1.Init.ClockSpeed = (uint32_t)freq;
  HAL_I2C_DeInit(&hi2c1);
  HAL_I2C_Init(&hi2c1);
}

/* USER CODE END 1 */
