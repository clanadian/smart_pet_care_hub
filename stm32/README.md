# STM32 F411RE - 실시간 모니터링 및 비상 알림

Smart Pet Care Hub의 실시간 상태 모니터링과 비상 알림(SOS)을 담당하는 STM32F411RE 펌웨어입니다. Bluetooth(USART6)로 Raspberry Pi 4 서버와 통신하며, LCD에 시스템 상태를 실시간으로 표시합니다.

---

## 1. 담당 역할

- LCD에 시간, 온습도, 수위, 소리, 통신/시스템 상태를 페이지 단위로 표시 (3페이지 자동 순환)
- 물리 버튼으로 비상 상황(SOS)을 토글하고 Bluetooth로 서버에 알림 전송
- Bluetooth 통신(BT) 및 센서 데이터 수신 상태를 10초 단위로 모니터링, 끊기면 즉시 상태 페이지로 전환
- 리모컨(IR) 입력으로 LCD 페이지 수동 전환
- RGB LED / Buzzer로 평상시·경고·비상 상태를 시각·청각적으로 표시

## 2. 사용 부품

| 부품 | 용도 |
|---|---|
| LCD (I2C) | 실시간 상태 모니터링 |
| RGB LED | 시스템 상태 표시 (정상/경고/비상) |
| Buzzer | 비상 경보 알림 |
| Push Button | 비상 알림 전송 (SOS) |
| IR 리모컨 | LCD 페이지 수동 전환 |

## 3. 개발 환경 및 사양 (Hardware & Environment)

- **Target MCU:** STM32F411RE (ARM Cortex-M4)
- **Toolchain:** `arm-none-eabi-gcc`
- **Build System:** CMake (v3.22+)
- **HAL Driver:** STM32F4xx HAL Driver
- **주요 주변장치 (Peripherals):**
  - Character LCD (I2C1)
  - UART2 (디버그, 115200bps) / UART6 (Bluetooth, 9600bps)
  - GPIO / Timer (TIM) / EXTI

## 4. 디렉터리 구조 (Directory Structure)

```text
stm32/
├── Core/
│   ├── Inc/               # 헤더 파일 (main.h, clcd.h, stm32f4xx_hal_conf.h 등)
│   └── Src/               # 소스 파일 (main.c, clcd.c, syscalls.c 등)
├── Drivers/
│   ├── CMSIS/             # ARM CMSIS 라이브러리
│   └── STM32F4xx_HAL_Driver/ # STM32F4 HAL 드라이버
├── cmake/                 # CMake 툴체인 및 CubeMX 연동 설정
│   └── gcc-arm-none-eabi.cmake
├── CMakeLists.txt         # 메인 CMake 빌드 스크립트
├── CMakePresets.json      # CMake 프리셋 설정
├── STM32F411XX_FLASH.ld   # 링커 스크립트
├── startup_stm32f411xe.s  # 어셈블리 스타트업 코드
└── smart_pet_hub.ioc      # STM32CubeMX 프로젝트 파일
```

## 5. 통신 프로토콜

라즈베리파이(`[RASP]`)와 `@` 구분자로 패킷을 주고받습니다. STM32 → 서버는 비상 상태 알림 위주, 서버 → STM32는 상태값 갱신 위주입니다.

**STM32 → 서버**
```
[STM32]@TIME_REQ            # 부팅 직후 시간 동기화 요청
[STM32]@EMG@LOCK            # 비상 발동 알림
[STM32]@EMG@UNLOCK          # 비상 해제 알림
```

**서버 → STM32**
```
[RASP]@SENSOR@수위@온도@습도@소리감지여부
[RASP]@LED@RED/GREEN/BLUE
[RASP]@BUZZER@ON/OFF
[RASP]@EMG@OFF              # 서버 측 비상 해제
[RASP]@TIME@시:분:초         # 시간 동기화
```

## 6. 상태 판단 로직

- **정상 상태**: BT/센서 데이터 정상 수신 → 3초마다 1·2페이지 자동 순환 표시
- **통신 이상**: BT 또는 센서 데이터가 10초 이상 미수신 → 3페이지(상태 화면) 강제 고정, 자동 스크롤 중지
- **비상 상태**: 버튼 입력 시 즉시 LCD·LED·Buzzer 경고 전환, 서버로 `EMG@LOCK` 전송. 버튼을 한 번 더 누르거나 서버에서 `EMG@OFF` 수신 시 해제

## 7. 빌드 및 컴파일 방법 (Build Instructions)

### 필수 도구

- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/gnu-rm) (`arm-none-eabi-gcc`)
- [CMake](https://cmake.org/)
- [Ninja](https://ninja-build.org/) 또는 `Make`

### 빌드 절차

```bash
# 1. stm32 디렉터리로 이동
cd stm32

# 2. 빌드 출력 디렉터리 생성 및 이동
mkdir build
cd build

# 3. CMake 구성 (Toolchain 지정)
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/gcc-arm-none-eabi.cmake ..

# 4. 프로젝트 빌드
make -j4
# 또는 Ninja 사용 시
# ninja
```

빌드가 완료되면 `build/` 디렉터리 내에 `.elf`, `.bin`, `.hex` 실행 파일이 생성됩니다.