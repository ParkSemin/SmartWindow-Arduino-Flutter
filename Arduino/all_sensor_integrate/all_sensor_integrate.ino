/*
- 우적 센서, 대기질 센서, 모터, 블루투스 통합 버전

- 창문 열리는 조건 우선순위
  1. 실내 대기질이 안좋을 때
  2. 우적센서에 빗물 감지도 안되고, 실외 미세먼지도 좋을 때

- 창문 닫히는 조건 우선순위
  1. 우적센서에 빗물이 감지될 때
  2. 실외 미세먼지가 많을 때

<기타 내용>
- 블루투스로 열고 닫기를 하면 환기모드 자동 Off

<Message Code>
- 1 : 아두이노와 연결되었음을 의미
- 10 : 창문을 열었음을 의미
  - 11 : 실내 대기질이 나빠서 창문을 열었음을 의미
  - 12 : 화재가 발생해서 창문을 열었음을 의미
- 20 : 창문을 닫았음을 의미
  - 21 : 비가 와서 창문을 닫았음을 의미
  - 22 : 미세먼지가 나빠서 창문을 닫았음을 의미
- 30 : 자동 모드가 켜졌음을 의미
- 31 : 자동 모드가 꺼졌음을 의미
*/

#include <SoftwareSerial.h>  // 블루투스 시리얼 통신 라이브러리 추가
#include <Stepper.h> // 스텝 모터 라이브러리
#include "SparkFun_ENS160.h" // 대기질 센서 라이브러리
#include <Wire.h> // 대기질 센서 라이브러리

#define BT_TXD 8
#define BT_RXD 9

// 창문 열려있으면 true, 닫혀있으면 false
bool is_open = false;

// 환기 모드 트리거
bool auto_mode = true;

// 비오면 true, 안오면 false
bool is_rain = false;

// 미세먼지 나쁘면 true, 좋으면 false
bool is_pollute_out = false;

// 실내 대기질 나쁘면 true, 좋으면 false
bool is_pollute_in = false;

// 실내 대기질이 5단계라면 화재가 발생한 것
bool is_fire = false;

// 대기질 센서
SparkFun_ENS160 myENS;
int ensStatus; 

// 빗물 센서
int Raindrops_pin = A0;

SoftwareSerial BTSerial(BT_TXD, BT_RXD);

int stepsmotor = 1300; // 모터 돌아가는 각도
Stepper myStepper(stepsmotor, 2, 4, 3, 5); // 모터 보드에 연결한 핀 번호

void setup() {
  Wire.begin();
  Serial.begin(9600);
  BTSerial.begin(9600);
  
  myStepper.setSpeed(17); // 모터 돌아가는 속도 설정

  if( !myENS.begin() )
  {
      Serial.println("Did not begin.");
      while(1);
  }

  if( myENS.setOperatingMode(SFE_ENS160_RESET) )
    Serial.println("Ready.");

  // 우적 센서
  pinMode(A0, INPUT);

  delay(100);
  myENS.setOperatingMode(SFE_ENS160_STANDARD);
  ensStatus = myENS.getFlags();
  Serial.print("Gas Sensor Status Flag: ");
  Serial.println(ensStatus);
}

void loop() {
  // 모터 작동 코드
  if (BTSerial.available()) { // 블루투스로 개폐 명령을 받으면 환기 모드(auto_mode)를 비활성화
    String data = BTSerial.readString();

    if (data == "OK+CONN") {
      Serial.println("연결됨");
      send_message(1);
    } 
    else if (data == "OK+LOST") {
      Serial.println("연결해제됨");
    } 
    else if (data == "OPEN") {
      auto_mode = false;
      open_window(10);
      Serial.println("창문을 열었습니다");
    } 
    else if (data == "CLOSE") {
      auto_mode = false;
      close_window(20);
      Serial.println("창문을 닫았습니다");
    } 
    else if (data == "ON") {
      auto_mode = true;
      Serial.println("자동모드 ON");
      send_message(30);
    } 
    else if (data == "OFF") {
      auto_mode = false;
      Serial.println("자동모드 OFF");
      send_message(31);
    }
  }

  // 환기 모드가 활성화 된 상태여야지만 센서를 작동시킴
  if (auto_mode == true) {
    Serial.println("Auto ON");
    // 우적 센서
    if(analogRead(A0) < 190){ // 빗물 X , 190은 임의적으로 정한 수치임
      is_rain = false;
    }
    else { // 빗물 O
      is_rain = true;
    }

    // 대기질 센서
    if( myENS.checkDataStatus() )
    {
      int v = myENS.getAQI();

      if (v >= 3) { // 실내 대기질이 오염된 정도를 3 이상으로 임의로 설정하였음
        is_pollute_in = true;
        
        // 실내 대기질 오염 정도가 5단계라면 화재가 발생한 것으로 여김
        // is_fire = (v == 5) ? true : false;
        if (v == 5) {
          is_fire = true;
        } else {
          is_fire = false;
        }
      } else {
        is_pollute_in = false;
        is_fire = false;
      }
    }

    if (is_open == true) { // 창문이 열려있다면 이 부분이 실행[창문을 닫을 조건을 확인]
      // 비가 오거나 실외 미세먼지가 많다면 창문을 닫음
      if (is_rain == true) {
        close_window(21); // 21 : 비 내리는 중
      } 
      else if (is_pollute_out == true) {
        close_window(22); // 22 : 미세먼지 나쁨
      }
    } 
    else { // 창문이 닫혀있다면 이 부분이 실행[창문을 열 조건을 확인]
      // 실내에 불이 났다면 비가 오든 미세먼지가 안좋든 무조건 창문을 열음
      if (is_fire == true) {
        Serial.println("실내 화재 발생");
        open_window(12);
      } else {
        // 불이 나지 않았다면 1. 밖에 비가 안오고, 2. 실외 미세먼지가 좋고, 3. 실내 대기질이 나쁘면 창문을 열음
        if (is_rain == false and is_pollute_out == false and is_pollute_in == true) {
          open_window(11);
        }
      }
    }
  }
  
  delay(1000); // 1초 간격으로 반복
}

// 스마트폰으로 메시지 전송
void send_message(int code) {
  BTSerial.write(code);
  BTSerial.write(is_open);
  BTSerial.write(auto_mode);
}

// 창문 여는 함수
void open_window(int code) {
  Serial.println("***OPEN***");
  is_open = true;
  send_message(code);
  myStepper.step(-stepsmotor);
}

// 창문 닫는 함수
void close_window(int code) {
  Serial.println("***CLOSE***");
  is_open = false;
  send_message(code);
  myStepper.step(stepsmotor);
}