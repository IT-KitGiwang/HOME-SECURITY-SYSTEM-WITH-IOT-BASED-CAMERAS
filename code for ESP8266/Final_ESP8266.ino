// Blynk cấu hình
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6aWsElsZP"
#define BLYNK_TEMPLATE_NAME "esp32 live"
#define BLYNK_AUTH_TOKEN "jkFpybwtL1QHTrClJJo3QnI14s_CpGi8"

// Thư viện
#include <Servo.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// WiFi
char ssid[] = "tranquocgiang";
char pass[] = "12345678";

// Servo
Servo myservo; // SERVO CHÂN D5 - void setup() -> myservo.attach(14); 

// Chân kết nối
int camBienTu = 4;     // D2 - Cảm biến từ
int buzzerPin = 5;     // D1 - Còi buzzer
int camBienPIR = 12;   // D6 - Cảm biến chuyển động PIR
int buttonDoor = 2;    // D4 - Button nhấn cửa 
int ledSecurityMode = 13;// D7 - Led chế độ giám sát


// Các biến xử lý PIR
int cheDoGiamSat = 0; // biến để kiểm tra bật/tắt chế độ PIR từ blynk 
int lastState = LOW; // Biến trạng thái nhận diện PIR, khởi tạo = LOW (chưa nhận diện)

// Các biến để xử lí đóng / mở cửa
long doorOpenStartTime = 0; //Thời gian bắt đầu mở cửa của những lần gần nhất (Khởi tạo = 0)
bool doorOpened = false; // Biến kiểm tra cửa có mở hay ko, khởi tạo = false

// Giảm spam nhận diện PIR 
unsigned long lastStatus_PIR = 0; // Thời gian nhận diện gần nhất, khởi tạo = 0
const unsigned long printInterval_PIR = 100; // Thời gian giãn cách giữa các lần nhận diện - 100ms

// Giảm spam chuông cửa (button và còi buzz)
unsigned long lastNotifyTime_Buzz = 0; // Lần chuông kêu gần nhất, khởi tạo bằng 0
const unsigned long notifyCooldown_Buzz = 10000; // Thời gian chờ giữa các lần nhận giá trị button và gửi thông báo chuông cửa - 10 giây

// Giảm SPAM THÔNG BÁO BLYNK NOTICE KHI CỬA MỞ QUÁ 10S
unsigned long lastNotify_openDoor = 0; // thời gian cửa bị mở quá 10 giây gần nhất
const unsigned long notifyInterval_openDoor = 5000; // thời gian delay gửi thông báo đến BLYNK mỗi lần (5 giây)

void setup() {
  Serial.begin(9600);

  // Kết nối WiFi và Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Cấu hình chân
  myservo.attach(14);              // D5 - Servo
  myservo.write(180);               // đóng cửa ban đầu
  pinMode(camBienTu, INPUT_PULLUP); // D2 - Cảm biến từ
  pinMode(buzzerPin, OUTPUT);      // D1 - Buzzer
  pinMode(camBienPIR, INPUT);      // D6 - PIR
  pinMode(ledSecurityMode, OUTPUT); // TẮT LED BAN ĐẦU
  pinMode(buttonDoor, INPUT_PULLUP); // ĐỌC GIÁ TRỊ BUTTON
  // Đợi PIR ổn định
  Serial.println("Chờ cảm biến PIR ổn định..."); 
  delay(5000); // 5s làm nóng bề mặt lăng kính giúp nhận diện chính xác hơn
  Serial.println("CÁC CÀI ĐẶT ĐÃ XONG...");
}

// 👉 Xử lý nút mở/đóng cửa từ Blynk (V2)
BLYNK_WRITE(V2) {
  int buttonState = param.asInt();
  if (buttonState == 1) {
    closeDoor();
    Serial.println("🟢 Đóng cửa từ Blynk (V2)");
  } else {
    openDoor();
    Serial.println("🔴 Mở cửa từ Blynk (V2)");
  }
}
// 👉 Bật tắt chế độ giám sát từ BLynk (V1)
BLYNK_WRITE(V1) {
  int securityState = param.asInt();
  if (securityState == 1) {
    Serial.println("Bật chế độ giám sát");
    cheDoGiamSat = 1;
    digitalWrite(ledSecurityMode,HIGH);
  } else {
    Serial.println("tắt chế độ giám sát");
    cheDoGiamSat = 0;
    digitalWrite(ledSecurityMode,LOW);
  }
}

// 🚪 Mở cửa
void openDoor() {
  Blynk.virtualWrite(V4, "Cửa ngoài đang mở");
  for (int pos = 180; pos >= 0; pos--) {
    myservo.write(pos);
    delay(1);
  }
  
  doorOpened = true;
  doorOpenStartTime = millis();
}

// 🚪 Đóng cửa
void closeDoor() {
  Blynk.virtualWrite(V4, "Cửa ngoài đang đóng");
  for (int pos = 0; pos <= 180; pos++) {
    myservo.write(pos);
    delay(1);
  }
  doorOpened = false;
}

//  Xử lý an ninh cửa bằng cảm biến từ, gửi thông báo nếu cửa mở quá 10 giây
void SecurityOutDoor() {
  int currentState = digitalRead(camBienTu);

  if (currentState == HIGH) {
    if (!doorOpened) {
      doorOpenStartTime = millis();
      doorOpened = true;
      Serial.println("🚪 Cửa mở!");
    }
  } else {
    doorOpened = false;
  }
  // Nếu cửa mở quá 10 giây
  if (doorOpened && (millis() - doorOpenStartTime > 10000)) { // 10000 = 10 giây
    if (millis() - lastNotify_openDoor >= notifyInterval_openDoor) {
      digitalWrite(buzzerPin, HIGH);
      delay(500);
      digitalWrite(buzzerPin, LOW);
      Blynk.logEvent("doornotice", "Cửa mở quá 10 giây");
      Serial.println("🚨 Cửa đã mở quá 10 giây!");
      lastNotify_openDoor = millis(); // cập nhật thời gian gửi gần nhất
    }
  } else {
    digitalWrite(buzzerPin, LOW);
    lastNotify_openDoor = 0; // reset để lần sau gửi lại sau 5s
  }
}

// 👀 Xử lý cảm biến PIR trường hợp khi cả nhà đi vắng, dùng để nhận diện có trộm đột nhập.
void checkPIR(){
  int currentState = digitalRead(camBienPIR); // Đọc trạng thái cảm biến PIR

    // Kiểm tra nếu có sự thay đổi trạng thái và khoảng thời gian đủ lâu tránh spam
    if (cheDoGiamSat && currentState != lastState && (millis() - lastStatus_PIR) > printInterval_PIR) {
      // In trạng thái của cảm biến khi có sự thay đổi
      if (currentState == HIGH) {
        Blynk.logEvent("warning", "Cảnh báo có trộm đột nhập !");
        Serial.println("🔕 Có trộm đột nhập.");
      } else {
        Serial.println("🔕 Không còn chuyển động.");
      }
      // Cập nhật trạng thái và thời gian in lần cuối
      lastState = currentState;
      lastStatus_PIR = millis();
    }
}
// hàm gửi thông báo tới blynk khi có khách ấn chuông cửa
void guest(){ 
  bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(buttonDoor);

  // Phát hiện nhấn mới (nhấn từ HIGH sang LOW)
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    unsigned long currentTime = millis();
    if (currentTime - lastNotifyTime_Buzz >= notifyCooldown_Buzz) {
      digitalWrite(buzzerPin, HIGH);
      Blynk.logEvent("guest", "Có người nhấn chuông cổng !");
      Serial.println("Có người nhấn chuông cổng !");
      delay(500);
      digitalWrite(buzzerPin, LOW);
      lastNotifyTime_Buzz = currentTime; // cập nhật thời gian lần cuối đọc giá trị button và phát còi buzz
    }
  }
  lastButtonState = currentButtonState; // trạng thái gần nhất của button
}
void loop() {
  Blynk.run();
  SecurityOutDoor(); // Gọi kiểm tra cảm biến từ
  checkPIR();
  guest();

}
