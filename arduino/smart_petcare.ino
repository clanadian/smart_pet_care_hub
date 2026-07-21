/*
  Smart Dog Feeder : Arduino Uno All-in-One
  http://www.kccistc.net/
  작성일 : 2026.05.29
  작성자 : AIoT 임베디드
*/

//#define DEBUG_WIFI
#define DEBUG
#define AP_SSID     "KCCI601"
#define AP_PASS     "@kcci601@"
#define SERVER_NAME "10.10.16.82"
#define SERVER_PORT 5000
#define LOGID       "KKM_ARD"
#define PASSWD      "PASSWD"
#define RECV_ID     "KKM_AND"

// ── 핀 정의 ──────────────────────────────────────────
#define WIFIRX      6
#define WIFITX      7
#define SOUND_PIN   2
#define SERVO_PIN   10
#define BUZZ_PIN    4
#define RGB_R       5
#define DHT_PIN     9
#define RGB_G       12
#define RGB_B       13
#define WATER_PIN   A0

// ── 상수 ─────────────────────────────────────────────
#define CMD_SIZE        60
#define ARR_CNT         5
#define WATER_LOW_THR   30
#define TEMP_HIGH_THR   35
#define DHTTYPE         DHT11
#define SOUND_THRESHOLD  9

// 360도 서보
#define SERVO_STOP      90
#define SERVO_CW        45
#define SERVO_CCW       135
#define SERVO_90DEG_MS  209

#include "WiFiEsp.h"
#include "SoftwareSerial.h"
#include <Servo.h>
#include <DHT.h>

// ── 전역 변수 ─────────────────────────────────────────
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient  client;
Servo          servo;
DHT            dht(DHT_PIN, DHTTYPE);

char sendBuf[CMD_SIZE];

unsigned long last_sec_ms = 0;
unsigned long secCount    = 0;
bool timerIsrFlag         = false;

int     sensorTime = 5;
uint8_t water = 0;
uint8_t temp  = 0;
uint8_t humi  = 0;
uint8_t sound = 0;

// 비상 상태 플래그
bool emg_state = false;

// =====================================================
// RGB LED
// =====================================================
#define RGB_BOOTING() { digitalWrite(RGB_R,1); digitalWrite(RGB_G,0); digitalWrite(RGB_B,0); }
#define RGB_READY()   { digitalWrite(RGB_R,0); digitalWrite(RGB_G,1); digitalWrite(RGB_B,0); }
#define RGB_FEEDING() { digitalWrite(RGB_R,0); digitalWrite(RGB_G,0); digitalWrite(RGB_B,1); }
#define RGB_ALERT()   { digitalWrite(RGB_R,1); digitalWrite(RGB_G,0); digitalWrite(RGB_B,0); }

void rgb_set(bool r, bool g, bool b) {
    digitalWrite(RGB_R, r);
    digitalWrite(RGB_G, g);
    digitalWrite(RGB_B, b);
}

// =====================================================
// 부저
// =====================================================
void buzz(int times) {
    for (int i = 0; i < times; i++) {
        digitalWrite(BUZZ_PIN, HIGH); delay(200);
        digitalWrite(BUZZ_PIN, LOW);  delay(200);
    }
}

void buzz_on() {
    digitalWrite(BUZZ_PIN, HIGH);
}

void buzz_off() {
    digitalWrite(BUZZ_PIN, LOW);
}

// =====================================================
// 급식 (360도 서보 시간 제어)
// =====================================================
void feed() {
#ifdef DEBUG
    Serial.println("급식 시작");
#endif

    RGB_FEEDING();

    servo.attach(SERVO_PIN);
    servo.write(SERVO_CW);
    delay(SERVO_90DEG_MS);
    servo.write(SERVO_STOP);
    delay(200);
    servo.detach();

    buzz(1);
    RGB_READY();

    if (client.connected()) {
        sprintf(sendBuf, "[%s]FEED_DONE\n", RECV_ID);
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
    }

#ifdef DEBUG
    Serial.println("급식 완료");
#endif
}

// =====================================================
// 센서 읽기
// =====================================================
uint8_t get_water_level() {
    int raw = analogRead(WATER_PIN);
    return (uint8_t)map(raw, 0, 1023, 0, 100);
}

uint8_t get_temperature() {
    float t = dht.readTemperature();
    if (isnan(t)) {
#ifdef DEBUG
        Serial.println("DHT11 온도 읽기 실패");
#endif
        return temp;
    }
    return (uint8_t)t;
}

uint8_t get_humidity() {
    float h = dht.readHumidity();
    if (isnan(h)) {
#ifdef DEBUG
        Serial.println("DHT11 습도 읽기 실패");
#endif
        return humi;
    }
    return (uint8_t)h;
}

uint8_t get_sound() {
    int count = 0;
    for (int i = 0; i < 20; i++) {
        if (digitalRead(SOUND_PIN)) count++;
        delay(10);
    }
    return (count >= 20) ? 1 : 0;
}

// =====================================================
// WiFi / 서버 연결
// =====================================================
void wifi_Init() {
    do {
        WiFi.init(&wifiSerial);
        if (WiFi.status() == WL_NO_SHIELD) {
#ifdef DEBUG_WIFI
            Serial.println("WiFi shield not present");
#endif
        } else break;
    } while (1);

    while (WiFi.begin(AP_SSID, AP_PASS) != WL_CONNECTED) {
#ifdef DEBUG_WIFI
        Serial.print("Attempting to connect to WPA SSID: ");
        Serial.println(AP_SSID);
#endif
    }
#ifdef DEBUG_WIFI
    Serial.println("You're connected to the network");
#endif
}

int server_Connect() {
    client.stop();
    delay(500);

    if (client.connect(SERVER_NAME, SERVER_PORT)) {
#ifdef DEBUG_WIFI
        Serial.println("Connected to server");
#endif
        client.print("[" LOGID ":" PASSWD "]");
        delay(500);
        return 1;
    } else {
#ifdef DEBUG_WIFI
        Serial.println("server connection failure");
#endif
        return 0;
    }
}

void wifi_Setup() {
    wifiSerial.begin(38400);
    wifi_Init();
    server_Connect();
}

// =====================================================
// 소켓 수신 이벤트
// =====================================================
void socketEvent() {
    int i = 0;
    char* pToken;
    char* pArray[ARR_CNT] = { 0 };
    char recvBuf[CMD_SIZE] = { 0 };

    client.readBytesUntil('\n', recvBuf, CMD_SIZE);
    client.flush();

#ifdef DEBUG
    Serial.print("recv : ");
    Serial.println(recvBuf);
#endif

    pToken = strtok(recvBuf, "[@]");
    while (pToken != NULL) {
        pArray[i] = pToken;
        if (++i >= ARR_CNT) break;
        pToken = strtok(NULL, "[@]");
    }

    if (!pArray[0] || !pArray[1]) return;

    if (!strncmp(pArray[1], " New connected", 4)) {
        Serial.write('\n');
        return;
    }
    else if (!strncmp(pArray[1], " Alr", 4)) {
        Serial.write('\n');
        client.stop();
        delay(500);
        server_Connect();
        return;
    }
    else if (!strcmp(pArray[1], "FEED")) {
        feed();
        return;
    }
    else if (!strcmp(pArray[1], "GETSENSOR")) {
        if (pArray[2]) sensorTime = atoi(pArray[2]);
        sprintf(sendBuf, "[%s]GETSENSOR@%d\n",
                RECV_ID, sensorTime);
    }
    else if (!strcmp(pArray[1], "GETSTATE")) {
        if (pArray[2] && !strcmp(pArray[2], "DEV")) {
            sprintf(sendBuf,
                    "[%s]WATER@%d@TEMP@%d@HUMI@%d@SOUND@%d\n",
                    RECV_ID, water, temp, humi, sound);
        }
    }
    else if (!strcmp(pArray[1], "RGB")) {
        if (!pArray[2]) return;
        if      (!strcmp(pArray[2], "RED"))   rgb_set(1,0,0);
        else if (!strcmp(pArray[2], "GREEN")) rgb_set(0,1,0);
        else if (!strcmp(pArray[2], "BLUE"))  rgb_set(0,0,1);
        else if (!strcmp(pArray[2], "OFF"))   rgb_set(0,0,0);
        sprintf(sendBuf, "[%s]RGB@%s\n", RECV_ID, pArray[2]);
    }
    else if (!strcmp(pArray[1], "BUZZ")) {
        if (!pArray[2]) return;
        if      (!strcmp(pArray[2], "ON"))  buzz_on();   // 계속 울림
        else if (!strcmp(pArray[2], "OFF")) buzz_off();  // 즉시 정지
        sprintf(sendBuf, "[%s]BUZZ@%s\n", RECV_ID, pArray[2]);
    }
    else if (!strcmp(pArray[1], "SERVO")) {
        if (!pArray[2]) return;
        if (!strcmp(pArray[2], "CW")) {
            servo.attach(SERVO_PIN);
            servo.write(SERVO_CW);
        }
        else if (!strcmp(pArray[2], "STOP")) {
            servo.write(SERVO_STOP);
            delay(200);
            servo.detach();
        }
        sprintf(sendBuf, "[%s]SERVO@%s\n", RECV_ID, pArray[2]);
    }
    else {
        return;
    }

    if (client.connected()) {
        client.write(sendBuf, strlen(sendBuf));
        client.flush();
    }

#ifdef DEBUG
    Serial.print("send : ");
    Serial.println(sendBuf);
#endif
}

// =====================================================
// setup / loop
// =====================================================
void setup() {
    Serial.begin(115200);

    pinMode(SOUND_PIN, INPUT);
    pinMode(BUZZ_PIN,  OUTPUT);
    pinMode(RGB_R,     OUTPUT);
    pinMode(RGB_G,     OUTPUT);
    pinMode(RGB_B,     OUTPUT);

    servo.attach(SERVO_PIN);
    servo.write(SERVO_STOP);
    delay(500);
    servo.detach();

    dht.begin();

    RGB_BOOTING();
    wifi_Setup();

    RGB_READY();

#ifdef DEBUG
    Serial.println("== Smart Dog Feeder Ready ==");
#endif

    last_sec_ms = millis();
}

void loop() {
    // if → while 로 변경 (버퍼에 쌓인 패킷 전부 처리)
    while (client.available()) {
        socketEvent();
    }

    unsigned long now = millis();
    if (now - last_sec_ms >= 1000) {
        last_sec_ms = now;
        secCount++;
        timerIsrFlag = true;
    }

    if (timerIsrFlag) {
        timerIsrFlag = false;

        if (!(secCount % 5)) {
            if (!client.connected()) {
                RGB_BOOTING();
#ifdef DEBUG
                Serial.println("재연결 시도...");
#endif
                server_Connect();
                RGB_READY();
            }
            water = get_water_level();
            temp  = get_temperature();
            humi  = get_humidity();
            sound = get_sound();

#ifdef DEBUG
            Serial.print("water : "); Serial.print(water);
            Serial.print(", temp : "); Serial.print(temp);
            Serial.print(", humi : "); Serial.print(humi);
            Serial.print(", sound : "); Serial.println(sound);
#endif
        }

        if (sensorTime != 0 && !(secCount % sensorTime)) {
            if (!client.connected()) return;

            sprintf(sendBuf, "[%s]SENSOR@%d@%d@%d@%d\n",
                    RECV_ID, water, temp, humi, sound);
#ifdef DEBUG
            Serial.print("sendBuf: ");
            Serial.println(sendBuf);
#endif
            client.write(sendBuf, strlen(sendBuf));
            client.flush();

            bool alert = false;

            if (water < WATER_LOW_THR) {
                alert = true;
                RGB_ALERT();
                buzz(2);
                sprintf(sendBuf, "[ALLMSG]WATER_LOW@%d\n", water);
                client.write(sendBuf, strlen(sendBuf));
                client.flush();
#ifdef DEBUG
                Serial.println("ALERT: WATER_LOW");
#endif
            }
            if (temp > TEMP_HIGH_THR) {
                alert = true;
                RGB_ALERT();
                sprintf(sendBuf, "[ALLMSG]TEMP_HIGH@%d\n", temp);
                client.write(sendBuf, strlen(sendBuf));
                client.flush();
#ifdef DEBUG
                Serial.println("ALERT: TEMP_HIGH");
#endif
            }
            if (!alert) {
                RGB_READY();
            }
        }
    }
}