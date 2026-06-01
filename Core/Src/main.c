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
// printf 함수를 USART2(PC 디버그용 - NUCLEO 기본 USB)으로 연결하기 위한 매크로
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
// 블루투스 수신용 변수
uint8_t btchar;
char btData[50];
volatile unsigned char btFlag = 0;

// 시스템 상태 변수
volatile uint8_t emg_state = 0; // 비상 상태 플래그
volatile int menuPage = 1;      // LCD 페이지 번호 (리모컨 제어용)
volatile uint8_t emg_btn_flag = 0;  



// 내부 시계용 변수
int sys_hour = 0;
int sys_min = 0;
int sys_sec = 0;

volatile uint32_t last_scroll_tick = 0;

// 🌟 [추가됨] 센서 데이터를 담을 구조체 정의
typedef struct {
    int water;
    int temp;
    int humi;
    int sound;
} SENSOR_NODE;

// 🌟 [추가됨] 구조체 배열 선언 (모든 값 0으로 초기화됨)
// 현재는 MAX_DEVICES가 1이므로 sensor_node[0] 만 사용합니다.
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
  // 1. 블루투스(USART6) 수신 인터럽트 가동
  HAL_UART_Receive_IT(&huart6, &btchar, 1);

  // 2. LCD 초기화 (I2C1 사용)
  LCD_init(&hi2c1); 
  LCD_writeStringXY(0,0, "System Ready");
  
  // 💡 부팅 시 초록색 LED 기본 점등
  HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET); 
  
  // 🌟 [여기에 추가] 부팅/이닛 완료 직후 라즈베리 파이에게 시간 요청 패킷 송신
  char time_req[] = "[STM32]@TIME_REQ\n";
  HAL_UART_Transmit(&huart6, (uint8_t*)time_req, strlen(time_req), 100);
  
  printf("STM32 Control Panel Start! Time Requested.\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t last_clock_tick = 0; // 시계 카운트용 타이머
  last_scroll_tick = HAL_GetTick(); // 부팅 직후 타이머 초기화

  while (1)
  {
      uint32_t current_tick = HAL_GetTick();

      // 🌟 1. [내부 시계 로직] 1000ms(1초)마다 실행
      if (current_tick - last_clock_tick >= 1000)
      {
          sys_sec++;
          if (sys_sec >= 60) { sys_sec = 0; sys_min++; }
          if (sys_min >= 60) { sys_min = 0; sys_hour++; }
          if (sys_hour >= 24) { sys_hour = 0; }
          
          last_clock_tick = current_tick; // 타이머 갱신
          
          if(emg_state == 0) {
              update_LCD(); 
          }
      }

      // 🌟 2. [추가된 로직] 3000ms(3초)마다 LCD 페이지 자동 스크롤
      if (current_tick - last_scroll_tick >= 3000)
      {
          last_scroll_tick = current_tick; // 타이머 갱신
          
          if(emg_state == 0) // 비상 상황이 아닐 때만 스크롤
          {
              menuPage++;
              if(menuPage > 3) menuPage = 1; // 1~3페이지 반복
              update_LCD(); // 페이지가 넘어갔으니 즉각 화면 갱신
          }
      }

      // 3. 블루투스 데이터가 들어왔을 때 처리
      if(btFlag)
      {
          bluetooth_Event();
          btFlag = 0;
      }
      
      // 4. 평상시 상태일 때 LCD 화면 업데이트
      if(emg_state == 0)
      {
          update_LCD();
      }

      // 5. 비상 버튼 처리
      if(emg_btn_flag)
      {
          emergency_Trigger();
          emg_btn_flag = 0;
      }

      HAL_Delay(200); // 루프 안정화를 위한 딜레이
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
  * @brief USART2 Initialization Function (Debug PC용 - 115200 bps)
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
  * @brief USART6 Initialization Function (Bluetooth 통신용 - 9600 bps)
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
// ----------------------------------------------------
// [1] 블루투스 수신 인터럽트 처리 (1바이트씩 받아 문자열 완성)
// ----------------------------------------------------
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART6) // 블루투스 모듈 (ZS-040)
    {
        static int i=0;
        btData[i] = btchar;
        if((btData[i] == '\n') || (btData[i] == '\r')) // 종료 문자 인식
        {
            btData[i] = '\0';
            btFlag = 1;
            i = 0;
        }
        else
        {
            i++;
            if(i >= 50) i = 0; // 버퍼 오버플로우 방지
        }
        // 다음 문자 수신 대기
        HAL_UART_Receive_IT(&huart6, &btchar, 1);
    }
}

// ----------------------------------------------------
// [2] 외부 물리버튼 & 리모컨 하드웨어 인터럽트 처리 (디바운스 적용)
// ----------------------------------------------------
    void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
    {
        uint32_t current_time = HAL_GetTick();

        if (GPIO_Pin == EMG_BUTTON_Pin)
        {
            static uint32_t last_btn_time = 0;

            if (current_time - last_btn_time > 500)  // 500ms 디바운스
            {
                // GPIO_PIN_RESET 조건 제거 - falling edge이므로 이미 눌린 것
                emg_btn_flag = 1;  // ← 플래그만 세우기
                last_btn_time = current_time;
            }
        }
        else if (GPIO_Pin == IR_REMOTE_Pin)
        {
            static uint32_t last_ir_time = 0;
            if (current_time - last_ir_time > 200)
            {
                if(emg_state == 0)
                {
                    menuPage++;
                    if(menuPage > 3) menuPage = 1;
                    printf("IR Triggered: Page %d\r\n", menuPage);

                    last_scroll_tick = HAL_GetTick();
                }
                last_ir_time = current_time;
            }
        }
    }

// ----------------------------------------------------
// [3] 교수님 로직 기반: 블루투스 패킷 파싱 및 명령 실행
//     예: [RASP]@LED@RED\n
// ----------------------------------------------------
void bluetooth_Event()
{
    int i=0;
    char * pToken;
    char * pArray[ARR_CNT]={0};
    char recvBuf[CMD_SIZE]={0};
    char *endptr;
    
    strcpy(recvBuf, btData);
    printf("BT Recv: %s\r\n", recvBuf);

    // "[@]" 단위로 패킷 쪼개기
    pToken = strtok(recvBuf,"[@]\n\r");
    while(pToken != NULL)
    {
        pArray[i] = pToken;
        if(++i >= ARR_CNT) break;
        pToken = strtok(NULL,"[@]\n\r");
    }

    if(pArray[1] != NULL) 
    {
        // 💡 1. 센서 데이터 수신 (구조체 배열에 저장)
        if(!strcmp(pArray[1], "SENSOR") && pArray[2] != NULL)
        {
            // 현재는 장치가 1개라고 가정하므로 인덱스 0에 무조건 저장합니다.
            // (추후 pArray[0]에 있는 ID를 비교해서 인덱스를 분기하면 여러 장치 대응 가능!)
            sensor_node[0].water = (int)strtol(pArray[2], &endptr, 10);
            sensor_node[0].temp  = (int)strtol(pArray[3], &endptr, 10);
            sensor_node[0].humi  = (int)strtol(pArray[4], &endptr, 10);
            sensor_node[0].sound = (int)strtol(pArray[5], &endptr, 10);
            
            printf("Node[0] Updated: W:%d T:%d H:%d S:%d\r\n", 
                   sensor_node[0].water, sensor_node[0].temp, 
                   sensor_node[0].humi, sensor_node[0].sound);
        }
        // 2. LED 제어 명령
        else if(!strcmp(pArray[1], "LED") && pArray[2] != NULL)
        {
            HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, GPIO_PIN_RESET);

            if(!strcmp(pArray[2], "RED"))   HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_SET);
            if(!strcmp(pArray[2], "GREEN")) HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET);
            if(!strcmp(pArray[2], "BLUE"))  HAL_GPIO_WritePin(RGB_B_GPIO_Port, RGB_B_Pin, GPIO_PIN_SET);
        }
        // 3. 부저 제어 명령
        else if(!strcmp(pArray[1], "BUZZER") && pArray[2] != NULL)
        {
            if(!strcmp(pArray[2], "ON")) HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
            if(!strcmp(pArray[2], "OFF")) HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        }
        // 4. 비상 해제 명령 (서버에서 제어)
        else if(!strcmp(pArray[1], "EMG") && pArray[2] != NULL && !strcmp(pArray[2], "OFF"))
        {
            emg_state = 0; 
            HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET); 
            printf("Emergency Cleared by Server\r\n");
        }
        // 5. 시간 동기화
        else if(!strcmp(pArray[1], "TIME") && pArray[2] != NULL)
        {
            sscanf(pArray[2], "%d:%d:%d", &sys_hour, &sys_min, &sys_sec);
            printf("Time Synced: %02d:%02d:%02d\r\n", sys_hour, sys_min, sys_sec);
        }
    }
}

// ----------------------------------------------------
// [4] 비상 상황 토글 (물리 택트 스위치 전용)
// ----------------------------------------------------
void emergency_Trigger()
{
    // 🟢 1. 이미 비상 상태(1)일 때 누르면 -> 비상 해제! (OFF)
    if (emg_state == 1) 
    {
        emg_state = 0; // 상태를 정상으로 되돌림
        
        // 부저 끄고, 빨간불 끄기
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_RESET);
        
        // 💡 초록색 LED 다시 켜기! (평상시 상태)
        HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_SET);
        
        // LCD 복구
        LCD_writeStringXY(0,0, "System Restored ");
        LCD_writeStringXY(1,0, "Returning...    ");
        
        // 서버로 "락 풀림" 패킷 전송
        char emg_packet[] = "[STM32]@EMG@UNLOCK\n";
        HAL_UART_Transmit(&huart6, (uint8_t*)emg_packet, strlen(emg_packet), 100);
        
        printf(">>> EMERGENCY CLEARED (Toggle OFF) <<<\r\n");
    }
    
    // 🚨 2. 평상시 상태(0)일 때 누르면 -> 비상 발동! (ON)
    else 
    {
        emg_state = 1; // 상태를 비상으로 바꿈
        
        // 💡 빨간불 켜고, 초록불 끄기, 부저 켜기
        HAL_GPIO_WritePin(RGB_R_GPIO_Port, RGB_R_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(RGB_G_GPIO_Port, RGB_G_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET); 

        // LCD 경고
        LCD_writeStringXY(0,0, "[ EMERGENCY ! ] ");
        LCD_writeStringXY(1,0, " SYSTEM LOCKED  ");

        // 서버로 비상 락 패킷 송신
        char emg_packet[] = "[STM32]@EMG@LOCK\n";
        HAL_UART_Transmit(&huart6, (uint8_t*)emg_packet, strlen(emg_packet), 100);
        
        printf("!!! EMERGENCY BUTTON PRESSED (Toggle ON) !!!\r\n");
    }
}
// ----------------------------------------------------
// [5] LCD 디스플레이 업데이트 (리모컨 페이지 전환용)
// ----------------------------------------------------
void update_LCD()
{
    // 💡 버퍼는 넉넉하게 32로 잡아서 컴파일 경고(Overflow)를 원천 차단!
    char lcd_buf[32]; 

    if (menuPage == 1) {
        // [ 12:34:56 ] 뒤에 공백 4칸을 직접 넣어서 총 16칸을 딱 맞춤
        sprintf(lcd_buf, "[ %02d:%02d:%02d ]    ", sys_hour, sys_min, sys_sec);
        LCD_writeStringXY(0, 0, lcd_buf);
        
        // T:25C H:60% 뒤에 공백 5칸을 직접 넣어서 총 16칸 딱 맞춤 (%2d로 숫자 2자리 고정)
        sprintf(lcd_buf, "T:%2dC H:%2d%%     ", sensor_node[0].temp, sensor_node[0].humi);
        LCD_writeStringXY(1, 0, lcd_buf);
    }
    else if (menuPage == 2) {
        // Water Lvl:30 뒤에 공백을 넣어서 16칸 딱 맞춤 (%-3d는 숫자를 왼쪽 정렬로 3칸 차지하게 함)
        sprintf(lcd_buf, "Water Lvl:%-3d   ", sensor_node[0].water);
        LCD_writeStringXY(0, 0, lcd_buf);
        
        if(sensor_node[0].sound == 1) {
            sprintf(lcd_buf, "Sound: DETECTED "); // 딱 16글자
        } else {
            sprintf(lcd_buf, "Sound: NORMAL   "); // 딱 16글자 (뒤에 공백 3칸)
        }
        LCD_writeStringXY(1, 0, lcd_buf);
    }
    else if (menuPage == 3) {
        sprintf(lcd_buf, "Network: BT OK  "); // 딱 16글자 (뒤에 공백 2칸)
        LCD_writeStringXY(0, 0, lcd_buf);
        
        if(emg_state == 1) {
            sprintf(lcd_buf, "System: LOCKED  "); // 딱 16글자 (뒤에 공백 2칸)
        } else {
            sprintf(lcd_buf, "System: READY   "); // 딱 16글자 (뒤에 공백 3칸)
        }
        LCD_writeStringXY(1, 0, lcd_buf);
    }
}

// ----------------------------------------------------
// [6] printf 리타겟팅 (USART2로 PC에 디버그 메세지 전송)
// ----------------------------------------------------
PUTCHAR_PROTOTYPE
{
    // NUCLEO 보드의 ST-LINK 가상 COM 포트인 USART2로 문자열을 보냅니다.
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