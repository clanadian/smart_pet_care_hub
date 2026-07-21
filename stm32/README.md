# Smart Pet Care Hub - STM32 Firmware

STM32F411 기반의 Smart Pet Care Hub 임베디드 펌웨어 프로젝트입니다. CMake 및 GCC ARM Toolchain 환경에서 빌드할 수 있도록 구성되어 있습니다.

---

## 1. 개발 환경 및 사양 (Hardware & Environment)

- **Target MCU:** STM32F411RE (ARM Cortex-M4)
- **Toolchain:** `arm-none-eabi-gcc`
- **Build System:** CMake (v3.22+)
- **HAL Driver:** STM32F4xx HAL Driver
- **주요 주변장치 (Peripherals):**
  - Character LCD (CLCD)
  - UART / I2C / GPIO / Timer (TIM)

---

## 2. 디렉터리 구조 (Directory Structure)

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

---

## 3. 빌드 및 컴파일 방법 (Build Instructions)

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