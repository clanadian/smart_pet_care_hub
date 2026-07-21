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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "clcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* printf를 USART2(디버그용, NUCLEO 기본 USB)로 리다이렉트하기 위한 매크로 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */

#define ARR_CNT 5
#define CMD_SIZE 50
#define MAX_DEVICES 1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
/* 블루투스 수신 관련 */
uint8_t btchar;
char btData[50];
volatile unsigned char btFlag = 0;

/* 시스템 상태 */
volatile uint8_t emg_state = 0;      // 비상 상태 플래그
volatile int menuPage = 1;           // LCD 페이지 번호 (리모컨 제어용)
volatile uint8_t emg_btn_flag = 0;

/* 통신 및 센서 상태 모니터링 */
volatile uint32_t last_bt_recv_tick = 0;     // 마지막 BT 데이터 수신 시각
volatile uint32_t last_sensor_recv_tick = 0; // 마지막 센서 데이터 수신 시각
volatile uint8_t is_bt_connected = 0;        // 0: 끊김, 1: 정상
volatile uint8_t is_sensor_ok = 0;           // 0: 수신불량, 1: 정상

/* 내부 시계 */
int sys_hour = 0;
int sys_min = 0;
int sys_sec = 0;

volatile uint32_t last_scroll_tick = 0;

/* 센서 데이터 구조체 (현재 MAX_DEVICES=1이므로 sensor_node[0]만 사용) */
typedef struct {
    int water;
    int temp;
    int humi;
    int sound;
} SENSOR_NODE;

volatile SENSOR_NODE sensor_node[MAX_DEVICES] = {0};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */
void bluetooth_Event(void);
void emergency_Trigger(void);
void update_LCD(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART2_UART_Init();
  MX_USART6_UART_Init();

  /* USER CODE BEGIN 2 */
  // 블루투스(USART6) 수신 인터럽트 가동
  HAL_UART_Receive_IT(&huart6, &btchar, 1);

  // LCD 초기화 (I2C1)
  LCD_init(&hi2c1);
  LCD_writeStringXY(0,0, "System Ready");

  // 부팅 시 초록 LED 기본 점등
  HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET);

  // 부팅 완료 직후 라즈베리파이에 시간 요청 패킷 송신
  char time_req[] = "[STM32]@TIME_REQ\n";
  HAL_UART_Transmit(&huart6, (uint8_t*)time_req, strlen(time_req), 100);

  printf("STM32 Control Panel Start! Time Requested.\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_clock_tick = 0;     // 내부 시계용 타이머
  last_scroll_tick = HAL_GetTick(); // 부팅 직후 타이머 초기화

  while (1)
  {
      uint32_t current_tick = HAL_GetTick();

      // 1. 연결 상태 모니터링 (10초 이상 미수신 시 끊김 처리)
      if (current_tick - last_bt_recv_tick > 10000) is_bt_connected = 0;
      if (current_tick - last_sensor_recv_tick > 10000) is_sensor_ok = 0;

      // 2. 통신/센서 이상 시 3페이지(상태 화면)로 강제 고정
      if (emg_state == 0 && (is_bt_connected == 0 || is_sensor_ok == 0))
      {
          if (menuPage != 3) {
              menuPage = 3;
              update_LCD();
          }
          last_scroll_tick = current_tick; // 자동 스크롤 방지
      }

      // 3. 내부 시계 (1초마다 갱신)
      if (current_tick - last_clock_tick >= 1000)
      {
          sys_sec++;
          if (sys_sec >= 60) { sys_sec = 0; sys_min++; }
          if (sys_min >= 60) { sys_min = 0; sys_hour++; }
          if (sys_hour >= 24) { sys_hour = 0; }

          last_clock_tick = current_tick;

          if (emg_state == 0) {
              update_LCD();
          }
      }

      // 4. LCD 페이지 자동 스크롤 (3초마다, 정상 상태일 때만)
      if (current_tick - last_scroll_tick >= 3000)
      {
          last_scroll_tick = current_tick;

          if (emg_state == 0 && is_bt_connected == 1 && is_sensor_ok == 1)
          {
              menuPage++;
              if (menuPage > 2) menuPage = 1;
              update_LCD();
          }
      }

      // 5. 블루투스 수신 데이터 처리
      if (btFlag)
      {
          bluetooth_Event();
          btFlag = 0;
      }

      // 6. 평상시 LCD 갱신
      if (emg_state == 0)
      {
          update_LCD();
      }

      // 7. 비상 버튼 처리
      if (emg_btn_flag)
      {
          emergency_Trigger();
          emg_btn_flag = 0;
      }

      HAL_Delay(200); // 루프 안정화 딜레이
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
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
}

/**
  * @brief USART2 Initialization Function (디버그 PC용, 115200 bps)
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART6 Initialization Function (블루투스 통신용, 9600 bps)
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 9600;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, RGB_R_Pin|RGB_G_Pin|RGB_B_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : EMG_BUTTON_Pin */
  GPIO_InitStruct.Pin = EMG_BUTTON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(EMG_BUTTON_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : RGB_R_Pin RGB_G_Pin RGB_B_Pin */
  GPIO_InitStruct.Pin = RGB_R_Pin|RGB_G_Pin|RGB_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : IR_REMOTE_Pin */
  GPIO_InitStruct.Pin = IR_REMOTE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(IR_REMOTE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BUZZER_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BUZZER_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

/* USER CODE BEGIN 4 */
// [1] 블루투스 수신 인터럽트 처리 (1바이트씩 받아 문자열 완성)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART6) // 블루투스 모듈 (ZS-040)
    {
        static int i = 0;
        btData[i] = btchar;
        if ((btData[i] == '\n') || (btData[i] == '\r')) // 종료 문자
        {
            btData[i] = '\0';
            btFlag = 1;
            i = 0;
        }
        else
        {
            i++;
            if (i >= 50) i = 0; // 버퍼 오버플로우 방지
        }
        HAL_UART_Receive_IT(&huart6, &btchar, 1);
    }
}

// [2] 외부 버튼 & 리모컨 하드웨어 인터럽트 처리 (디바운스 적용)
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    uint32_t current_time = HAL_GetTick();

    if (GPIO_Pin == EMG_BUTTON_Pin)
    {
        static uint32_t last_btn_time = 0;

        if (current_time - last_btn_time > 500) // 500ms 디바운스
        {
            emg_btn_flag = 1;
            last_btn_time = current_time;
        }
    }
    else if (GPIO_Pin == IR_REMOTE_Pin)
    {
        static uint32_t last_ir_time = 0;
        if (current_time - last_ir_time > 200)
        {
            if (emg_state == 0)
            {
                menuPage++;
                if (menuPage > 2) menuPage = 1;
                printf("IR Triggered: Page %d\r\n", menuPage);

                last_scroll_tick = HAL_GetTick();
            }
            last_ir_time = current_time;
        }
    }
}

// [3] 블루투스 패킷 파싱 및 명령 실행 (예: [RASP]@LED@RED\n)
void bluetooth_Event()
{
    int i = 0;
    char *pToken;
    char *pArray[ARR_CNT] = {0};
    char recvBuf[CMD_SIZE] = {0};
    char *endptr;

    strcpy(recvBuf, btData);
    printf("BT Recv: %s\r\n", recvBuf);

    last_bt_recv_tick = HAL_GetTick();
    is_bt_connected = 1;

    // "[@]" 단위로 패킷 분리
    pToken = strtok(recvBuf, "[@]\n\r");
    while (pToken != NULL)
    {
        pArray[i] = pToken;
        if (++i >= ARR_CNT) break;
        pToken = strtok(NULL, "[@]\n\r");
    }

    if (pArray[1] != NULL)
    {
        // 센서 데이터 수신 (현재 장치 1개 → 인덱스 0에 저장)
        if (!strcmp(pArray[1], "SENSOR") && pArray[2] != NULL)
        {
            sensor_node[0].water = (int)strtol(pArray[2], &endptr, 10);
            sensor_node[0].temp  = (int)strtol(pArray[3], &endptr, 10);
            sensor_node[0].humi  = (int)strtol(pArray[4], &endptr, 10);
            sensor_node[0].sound = (int)strtol(pArray[5], &endptr, 10);

            last_sensor_recv_tick = HAL_GetTick();
            is_sensor_ok = 1;

            printf("Node[0] Updated: W:%d T:%d H:%d S:%d\r\n",
                   sensor_node[0].water, sensor_node[0].temp,
                   sensor_node[0].humi, sensor_node[0].sound);
        }
        // LED 제어
        else if (!strcmp(pArray[1], "LED") && pArray[2] != NULL)
        {
            HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, GPIO_PIN_RESET);

            if (!strcmp(pArray[2], "RED"))   HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_SET);
            if (!strcmp(pArray[2], "GREEN")) HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET);
            if (!strcmp(pArray[2], "BLUE"))  HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, GPIO_PIN_SET);
        }
        // 부저 제어
        else if (!strcmp(pArray[1], "BUZZER") && pArray[2] != NULL)
        {
            if (!strcmp(pArray[2], "ON")) HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
            if (!strcmp(pArray[2], "OFF")) HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        }
        // 비상 해제 (서버 제어)
        else if (!strcmp(pArray[1], "EMG") && pArray[2] != NULL && !strcmp(pArray[2], "OFF"))
        {
            emg_state = 0;
            HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET);
            printf("Emergency Cleared by Server\r\n");
        }
        // 시간 동기화
        else if (!strcmp(pArray[1], "TIME") && pArray[2] != NULL)
        {
            sscanf(pArray[2], "%d:%d:%d", &sys_hour, &sys_min, &sys_sec);
            printf("Time Synced: %02d:%02d:%02d\r\n", sys_hour, sys_min, sys_sec);
        }
    }
}

// [4] 비상 상황 토글 (물리 택트 스위치 전용)
void emergency_Trigger()
{
    // 이미 비상 상태(1)일 때 누르면 → 해제
    if (emg_state == 1)
    {
        emg_state = 0;

        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET);

        LCD_writeStringXY(0,0, "System Restored ");
        LCD_writeStringXY(1,0, "Returning...    ");

        char emg_packet[] = "[STM32]@EMG@UNLOCK\n";
        HAL_UART_Transmit(&huart6, (uint8_t*)emg_packet, strlen(emg_packet), 100);

        printf(">>> EMERGENCY CLEARED (Toggle OFF) <<<\r\n");
    }
    // 평상시 상태(0)일 때 누르면 → 비상 발동
    else
    {
        emg_state = 1;

        HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);

        LCD_writeStringXY(0,0, "[ EMERGENCY ! ] ");
        LCD_writeStringXY(1,0, " SYSTEM LOCKED  ");

        char emg_packet[] = "[STM32]@EMG@LOCK\n";
        HAL_UART_Transmit(&huart6, (uint8_t*)emg_packet, strlen(emg_packet), 100);

        printf("!!! EMERGENCY BUTTON PRESSED (Toggle ON) !!!\r\n");
    }
}

// [5] LCD 업데이트 (리모컨 페이지 전환용)
void update_LCD()
{
    char lcd_buf[32]; // 컴파일 경고(overflow) 방지용 여유 버퍼

    if (menuPage == 1) {
        sprintf(lcd_buf, "[ %02d:%02d:%02d ]    ", sys_hour, sys_min, sys_sec);
        LCD_writeStringXY(0, 0, lcd_buf);

        sprintf(lcd_buf, "T:%2dC H:%2d%%     ", sensor_node[0].temp, sensor_node[0].humi);
        LCD_writeStringXY(1, 0, lcd_buf);
    }
    else if (menuPage == 2) {
        sprintf(lcd_buf, "Water Lvl:%-3d   ", sensor_node[0].water);
        LCD_writeStringXY(0, 0, lcd_buf);

        if (sensor_node[0].sound == 1) {
            sprintf(lcd_buf, "Sound: DETECTED ");
        } else {
            sprintf(lcd_buf, "Sound: NORMAL   ");
        }
        LCD_writeStringXY(1, 0, lcd_buf);
    }
    else if (menuPage == 3) {
        // 통신(블루투스) 상태
        if (is_bt_connected) sprintf(lcd_buf, "Network: BT OK  ");
        else                 sprintf(lcd_buf, "Network: BT ERR!");
        LCD_writeStringXY(0, 0, lcd_buf);

        // 시스템 & 센서 상태
        if (emg_state == 1) {
            sprintf(lcd_buf, "System: LOCKED  ");
        }
        else if (is_sensor_ok == 0) {
            sprintf(lcd_buf, "System: SENS ERR");
        }
        else {
            sprintf(lcd_buf, "System: READY   ");
        }
        LCD_writeStringXY(1, 0, lcd_buf);
    }
}

// [6] printf 리타겟팅 (USART2로 PC에 디버그 메시지 전송)
PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}
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
  * where the assert_param error has occurred.
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