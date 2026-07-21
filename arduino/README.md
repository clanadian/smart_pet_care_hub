# Arduino UNO - 센서 노드 및 급식 제어

Smart Pet Care Hub의 센서 데이터 수집 및 급식 제어를 담당하는 Arduino UNO 스케치입니다. Wi-Fi(ESP 모듈)를 통해 Raspberry Pi 4 서버와 통신합니다.

## 1. 담당 역할

- 온도·습도(DHT11), 수위, 소리 센서 데이터를 5초 주기로 수집
- 수위 부족 및 고온 상태를 자체 판단하여 즉시 경보(LED/부저) 발생
- 서보모터 기반 정량 급식 실행 및 완료 신호 전송
- Wi-Fi를 통해 Raspberry Pi 4로 센서 데이터 전송 및 원격 명령 수신

## 2. 사용 부품

| 부품 | 용도 |
|---|---|
| Servo Motor | 사료 배출 제어 |
| DHT11 | 온·습도 측정 |
| Sound Sensor | 소리 감지 |
| Water Level Sensor | 수위 측정 |
| RGB LED | 시스템 상태 표시 |
| Buzzer | 비상 알림 |

## 3. 파일 구성

아두이노 IDE 스케치 규칙에 따라 폴더명과 `.ino` 파일명이 동일해야 합니다.

```
arduino/
└── (스케치 폴더명)/
    └── (스케치 폴더명).ino
```

## 4. 통신 프로토콜

`[XXX_AND]` = Raspberry Pi 서버, `[XXX_ARD]` = Arduino 자신을 가리키는 식별자입니다.

**센서 데이터 전송 (Arduino → 서버)**
```
[XXX_AND]SENSOR@수위@온도@습도@소리감지여부
예) [XXX_AND]SENSOR@57@26@39@1
```

**명령 수신 (서버 → Arduino)**
```
[XXX_ARD]FEED          # 급식 실행
[XXX_ARD]BUZZ@ON       # 부저 ON/OFF
[XXX_ARD]RGB@GREEN     # LED 색상 변경 (GREEN/RED 등)
```

**완료 응답**
```
[XXX_AND]FEED_DONE     # 급식 완료 알림
```

## 5. 트러블슈팅

**Timer1 라이브러리 충돌**

| | |
|---|---|
| 문제 | 서보모터 미동작 |
| 원인 | `Servo.h`와 `TimerOne.h`가 동일한 Timer1 자원을 두고 충돌 |
| 해결 | `TimerOne.h` 제거, `millis()` 기반 소프트웨어 타이머로 대체 |
| 결과 | 서보모터 정상 동작 확인 |

```cpp
unsigned long now = millis();
if (now - last_sec_ms >= 1000) {
    last_sec_ms = now;   // 기준점 갱신
    secCount++;           // 초 카운트 증가
    timerIsrFlag = true;  // 1초 경과 플래그
}
```

**Sound 센서 민감도 문제**

| | |
|---|---|
| 문제 | 조용한 환경에서도 소리 감지가 지속적으로 발생 |
| 원인 | 1회 샘플링 방식이라 순간 노이즈에도 반응 |
| 해결 | 20회 다중 샘플링 후 전부 감지된 경우에만 유효 처리 |
| 결과 | 오검출 감소, 감지 신뢰성 향상 |

```cpp
uint8_t get_sound() {
    int count = 0;
    for (int i = 0; i < 20; i++) {   // 20회 샘플링
        if (digitalRead(SOUND_PIN)) count++;
        delay(10);                    // 10ms 간격, 총 200ms
    }
    return (count >= 20) ? 1 : 0;    // 20회 모두 감지 시만 1
}
```

## 6. 한계

- 서보모터를 시간 기반으로 제어하다 보니 사료 배출량에 편차가 있음 → 스텝모터 + 로드셀 기반 정량 제어로 개선 가능
- 급식이 수동 트리거 방식이라 정해진 시간에 자동으로 급식할 수 없음 → 서버 측 Cron 스케줄링과 연동 필요