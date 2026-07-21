# 🐾 Smart Pet Care Hub

STM32 마이크로컨트롤러 기반의 임베디드 스마트 펫 케어 시스템 프로젝트입니다.
반려동물의 거주 환경을 실시간으로 모니터링하고, 급식·급수를 자동으로 제어합니다.

## 📌 주요 기능 (Key Features)

- **환경 모니터링**: 온습도 센서를 이용한 반려동물 거주 환경 실시간 모니터링
- **자동 급식 / 급수 제어**: 모터(PWM) 및 타이머 기반 정량 · 정시 제어
- **상태 표시 및 알림**: Display, LED, Buzzer를 통한 시스템 동작 상태 출력
- **통신 기능**: UART 시리얼 통신을 통한 데이터 모니터링 및 제어 명령 수신

## 🛠️ 기술 스택 (Tech Stack)

**Hardware**
- MCU: STM32
- Peripherals: GPIO, PWM, UART, I2C, Timers

**Software & Tools**
- Language: C / C++
- IDE / Toolchain: VS Code / STM32CubeIDE / GCC ARM Embedded
- Build System: CMake / Makefile
- Version Control: Git / GitHub

## 📂 프로젝트 구조 (Directory Structure)

```
smart_pet_care_hub/
├── Core/
│   ├── Inc/           # 헤더 파일 (.h)
│   └── Src/           # 소스 파일 (.c)
├── Drivers/           # STM32 HAL / CMSIS 드라이버
├── CMakeLists.txt     # CMake 빌드 설정
└── README.md
```

## 🚀 시작하기 (Getting Started)

### 1. 저장소 클론

```bash
git clone https://github.com/clanadian/smart_pet_care_hub.git
cd smart_pet_care_hub
```

### 2. 빌드 및 플래싱

**VS Code / STM32CubeIDE 사용 시**
프로젝트를 로드한 후 Build & Run을 실행합니다.

**CLI 환경**

```bash
mkdir build && cd build
cmake ..
make
```