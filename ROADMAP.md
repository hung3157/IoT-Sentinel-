# IoT Sentinel

**IoT Sentinel** là hệ thống cầm tay sử dụng ESP32 để quan sát hoạt động Wi-Fi 2.4 GHz xung quanh, theo dõi các thiết bị phát hiện được, xử lý tín hiệu RSSI, ước lượng độ gần tương đối và đưa ra cảnh báo đối với các thiết bị hoặc hoạt động không dây chưa xác định.

Dự án tập trung vào:

- Lập trình nhúng trên ESP32
- Thu thập và quan sát tín hiệu Wi-Fi
- Quan sát khung IEEE 802.11
- Xử lý tín hiệu RSSI
- Theo dõi thiết bị
- Phân tích Wi-Fi channel
- Giao diện người dùng nhúng
- Mở rộng kiến thức Cấu kiện điện tử ET2041E
- Đo đạc và kiểm chứng thực nghiệm

> **Lưu ý:** IoT Sentinel không phải radar phản xạ sóng theo nghĩa truyền thống. Hệ thống không thể xác định chính xác một thiết bị là camera, microphone hay thiết bị giám sát chỉ dựa trên địa chỉ MAC hoặc RSSI. Khoảng cách suy ra từ RSSI chỉ được coi là **khoảng cách ước lượng**.

---

# 1. Thành viên nhóm

## Thành viên A — Wi-Fi / Firmware

**Hoàng Nguyễn Thành Hưng**  
**MSSV:** 202514818

Nhiệm vụ chính:

- Kiến trúc firmware ESP32
- Wi-Fi scanning
- Wi-Fi promiscuous mode
- Quan sát khung IEEE 802.11
- Channel hopping
- Xử lý MAC/BSSID
- Quản lý thiết bị
- Tích hợp hệ thống

## Thành viên B — Xử lý tín hiệu / Phần cứng

**Mai Thái Huy**  
**MSSV:** 202514824

Nhiệm vụ chính:

- Xử lý tín hiệu RSSI
- Moving Average Filter
- Ước lượng khoảng cách
- Hiệu chuẩn RSSI
- Đo đạc thực nghiệm
- OLED và giao diện
- Phần cứng cảnh báo
- Phần mở rộng ET2041E

---

# 2. Mục tiêu dự án

Pipeline xử lý chính của hệ thống:

~~~text
Môi trường Wi-Fi 2.4 GHz
            ↓
       Bộ thu ESP32
            ↓
   Tầng quan sát vô tuyến
            ↓
 MAC / BSSID / RSSI / Channel
            ↓
      Device Manager
            ↓
        Lọc RSSI
            ↓
   Ước lượng khoảng cách
            ↓
    Phân loại thiết bị
            ↓
      Đánh giá rủi ro
            ↓
       OLED / Cảnh báo
~~~

Ở phiên bản nâng cao:

~~~text
Khung IEEE 802.11
        ↓
 Promiscuous Mode
        ↓
  Channel Hopping
        ↓
MAC + RSSI + Frame Metadata
        ↓
  Radio Observation
        ↓
   Device Tracking
        ↓
  Activity Analysis
        ↓
    Risk Evaluation
~~~

---

# 3. Nguyên tắc thiết kế cốt lõi

Dự án sử dụng kiến trúc **module hóa**.

Các module xử lý phía trên không phụ thuộc trực tiếp vào cách dữ liệu Wi-Fi được thu thập.

Ở giai đoạn đầu, ESP32 sử dụng:

~~~cpp
WiFi.scanNetworks();
~~~

Sau này, tầng thu thập dữ liệu sẽ được nâng cấp sang:

~~~text
Promiscuous Mode
+
Channel Hopping
~~~

Cả hai phương pháp đều phải chuyển dữ liệu về một định dạng trung gian chung.

Ví dụ:

~~~cpp
struct RadioObservation {
    uint8_t mac[6];

    int rssi;
    int channel;

    unsigned long timestamp;

    bool hasSSID;
    String ssid;
};
~~~

Pipeline khi đó trở thành:

~~~text
WiFi.scanNetworks()
        ↓
RadioObservation
        ↓

        hoặc

Promiscuous Sniffer
        ↓
RadioObservation
        ↓

Device Manager
        ↓
RSSI Filter
        ↓
Distance Estimator
        ↓
Risk Engine
        ↓
User Interface
~~~

Nhờ kiến trúc này, khi chuyển từ Wi-Fi Scanner sang Promiscuous Sniffer, các module như Device Manager, RSSI Filter, Distance Estimator, Risk Engine và OLED vẫn có thể được tái sử dụng.

---

# 4. Lộ trình phát triển

Dự án được chia thành **6 phase kỹ thuật chính**.

Mỗi phase phải tạo ra một phiên bản chạy được và có thể kiểm thử trước khi chuyển sang phase tiếp theo.

---

# PHASE 1 — Prototype thu thập dữ liệu Wi-Fi

## Mục tiêu

Xây dựng tầng thu thập dữ liệu Wi-Fi đơn giản nhất để:

- Kiểm tra hoạt động Wi-Fi của ESP32
- Thu được SSID
- Thu được BSSID
- Thu được RSSI
- Thu được channel
- Thêm timestamp
- Tạo dữ liệu `RadioObservation`
- Scan Wi-Fi định kỳ

Ở phase này sử dụng:

~~~cpp
WiFi.scanNetworks();
~~~

Đây là **prototype của tầng acquisition**, không phải phương pháp phát hiện cuối cùng.

## Dữ liệu cần thu thập

~~~text
SSID
BSSID
RSSI
Channel
Timestamp
~~~

Ví dụ:

~~~text
=== WIFI SCAN ===

1. HomeWiFi

BSSID : AA:BB:CC:DD:EE:FF
RSSI  : -47 dBm
CH    : 6
~~~

## Thành viên A — Hưng

### Công việc

- Tạo project PlatformIO
- Cấu hình ESP32
- Khởi tạo Wi-Fi ở Station Mode
- Triển khai `WiFi.scanNetworks()`
- Đọc SSID
- Đọc BSSID
- Đọc RSSI
- Đọc channel
- Thêm timestamp
- Tạo cấu trúc `RadioObservation`
- Chuyển kết quả scan sang `RadioObservation`
- Triển khai scan định kỳ

Cấu trúc file đề xuất:

~~~text
src/
├── main.cpp
└── wifi_scanner.cpp

include/
├── radio_observation.h
└── wifi_scanner.h
~~~

## Thành viên B — Huy

### Công việc

- Kiểm tra kết quả Wi-Fi scan
- Quan sát dao động RSSI
- So sánh RSSI tại nhiều vị trí
- Ghi lại các Wi-Fi channel quan sát được
- Thu thập dữ liệu kiểm thử
- Chuẩn bị thí nghiệm hiệu chuẩn RSSI

## Output

~~~text
ESP32
  ↓
Wi-Fi Scan
  ↓
RadioObservation
  ↓
Serial Monitor
~~~

## Điều kiện hoàn thành

Phase 1 hoàn thành khi:

- ESP32 có thể scan Wi-Fi lặp lại ổn định
- Firmware không crash trong quá trình test thông thường
- Đọc được BSSID
- Đọc được RSSI
- Đọc được channel
- Có timestamp
- Dữ liệu được chuyển thành `RadioObservation`
- Có log dữ liệu kiểm thử

---

# PHASE 2 — Theo dõi thiết bị và xử lý tín hiệu RSSI

## Mục tiêu

Chuyển các observation riêng lẻ thành các thiết bị có trạng thái được theo dõi theo thời gian.

Pipeline:

~~~text
RadioObservation
        ↓
Device Manager
        ↓
RSSI History
        ↓
Moving Average
        ↓
Filtered RSSI
        ↓
Distance Estimate
~~~

## Cấu trúc dữ liệu thiết bị

~~~cpp
struct Device {
    uint8_t mac[6];

    String ssid;
    bool hasSSID;

    int rawRSSI;
    float filteredRSSI;

    int channel;

    unsigned long firstSeen;
    unsigned long lastSeen;
    unsigned long seenCount;

    bool known;
};
~~~

MAC được sử dụng làm định danh chính.

SSID chỉ là metadata bổ sung.

## Lọc RSSI

Bộ lọc ban đầu:

~~~text
Moving Average
N = 8 mẫu
~~~

Công thức:

~~~text
y[n] = (1/N) Σ x[n-k]
~~~

Nên triển khai bằng:

~~~text
Circular Buffer
+
Running Sum
~~~

## Ước lượng khoảng cách

Sử dụng mô hình Log-Distance Path Loss:

~~~text
d = 10 ^ ((RSSI_1m - RSSI_filtered) / (10 × n))
~~~

Trong đó:

- `d`: khoảng cách ước lượng
- `RSSI_1m`: RSSI thực nghiệm tại khoảng cách 1 mét
- `RSSI_filtered`: RSSI sau khi lọc
- `n`: hệ số suy hao môi trường

Khoảng cách phải được hiển thị dưới dạng:

~~~text
DIST EST
~~~

không được coi là khoảng cách chính xác tuyệt đối.

## Thành viên A — Hưng

### Device Manager

- Thiết kế bảng thiết bị
- Tìm thiết bị bằng MAC/BSSID
- Thêm thiết bị mới
- Cập nhật thiết bị đã tồn tại
- Quản lý `firstSeen`
- Quản lý `lastSeen`
- Quản lý `seenCount`
- Triển khai device timeout
- Kết nối `RadioObservation` với Device Manager

## Thành viên B — Huy

### Xử lý tín hiệu

- Triển khai Moving Average Filter
- Triển khai circular buffer
- Triển khai Distance Estimator
- Hiệu chuẩn RSSI
- Đo RSSI tại nhiều khoảng cách
- Xác định `RSSI_1m`
- Ước lượng hệ số `n`

Khoảng cách thí nghiệm đề xuất:

~~~text
1 m
2 m
3 m
5 m
~~~

## Cấu trúc file đề xuất

~~~text
src/
├── device_manager.cpp
├── rssi_filter.cpp
└── distance_estimator.cpp

include/
├── device_manager.h
├── rssi_filter.h
└── distance_estimator.h
~~~

## Output dự kiến

~~~text
DEVICE

MAC:
AA:BB:CC:DD:EE:FF

SSID:
HomeWiFi

RSSI RAW:
-52 dBm

RSSI AVG:
-49.6 dBm

DIST EST:
~1.7 m

SEEN:
42
~~~

## Điều kiện hoàn thành

Phase 2 hoàn thành khi:

- Không tạo nhiều bản ghi dư thừa cho cùng một MAC
- Device Manager hoạt động
- `seenCount` hoạt động
- `lastSeen` hoạt động
- Device timeout hoạt động
- RSSI Filtering hoạt động
- RSSI sau lọc ổn định hơn RSSI raw
- Có Distance Estimate
- Đã có dữ liệu calibration thực nghiệm

---

# PHASE 3 — Phân loại thiết bị, đánh giá rủi ro và giao diện

## Mục tiêu

Biến core firmware thành một thiết bị cầm tay có thể sử dụng độc lập.

## Phân loại thiết bị

Phân loại ban đầu:

~~~text
KNOWN
UNKNOWN
~~~

Không tự động kết luận thiết bị là:

~~~text
CAMERA
MICROPHONE
SPY DEVICE
~~~

chỉ dựa trên MAC hoặc RSSI.

## Risk Engine

Có thể sử dụng Risk Score dạng heuristic.

Các yếu tố:

~~~text
Unknown Device
Strong RSSI
Persistent Presence
Increasing RSSI
High Activity
~~~

Ví dụ:

~~~text
UNKNOWN             +30
STRONG RSSI         +25
PERSISTENT          +20
APPROACHING         +15
HIGH ACTIVITY       +10
~~~

Phân mức:

~~~text
0–30    LOW
31–60   MEDIUM
61–100  HIGH
~~~

Risk Score chỉ là **heuristic đánh giá**, không phải bằng chứng chắc chắn về nguy cơ bảo mật.

## Phần cứng

~~~text
ESP32 DevKit V1
SSD1306 OLED
NEXT Button
MODE Button
Alert LED
Breadboard
~~~

## OLED — List View

~~~text
IOT SENTINEL

> Unknown-01
  RSSI -48

  HomeWiFi
  RSSI -67
~~~

## OLED — Detail View

~~~text
UNKNOWN-01

RSSI RAW -52
RSSI AVG -49

DIST EST ~1.5m
CH 6

SEEN 42

RISK HIGH
~~~

## Thành viên A — Hưng

- Triển khai classification logic
- Triển khai Risk Engine
- Kết nối Device Manager với UI
- Xử lý button state
- Triển khai LIST Mode
- Triển khai DETAIL Mode
- Đảm bảo UI không block Wi-Fi processing

## Thành viên B — Huy

- Kết nối OLED
- Kết nối nút NEXT
- Kết nối nút MODE
- Kết nối LED cảnh báo
- Thiết kế giao diện
- Kiểm thử OLED
- Kiểm thử button
- Kiểm thử alert
- Làm gọn wiring

## Điều kiện hoàn thành

~~~text
Khởi động
   ↓
Thu thập dữ liệu Wi-Fi
   ↓
Theo dõi thiết bị
   ↓
Lọc RSSI
   ↓
Ước lượng khoảng cách
   ↓
Đánh giá rủi ro
   ↓
Hiển thị OLED
   ↓
Cảnh báo
~~~

Thiết bị phải có khả năng hoạt động mà không phụ thuộc vào Serial Monitor.

---

# PHASE 4 — Promiscuous Mode và Channel Hopping

## Mục tiêu

Nâng cấp tầng acquisition từ:

~~~text
WiFi.scanNetworks()
~~~

sang:

~~~text
Promiscuous Mode
+
Channel Hopping
~~~

để quan sát hoạt động IEEE 802.11 phong phú hơn và không chỉ phụ thuộc vào Access Point scan.

## Nguyên tắc kiến trúc

Phase này chỉ thay thế:

~~~text
Wi-Fi Scanner
~~~

bằng:

~~~text
Wi-Fi Sniffer
~~~

Các module sau phải tiếp tục được tái sử dụng:

~~~text
Device Manager
RSSI Filter
Distance Estimator
Risk Engine
OLED
~~~

## Pipeline

~~~text
IEEE 802.11 Frame
        ↓
Promiscuous Callback
        ↓
Trích xuất Metadata
        ↓
MAC
RSSI
Channel
Frame Type
Timestamp
        ↓
RadioObservation
        ↓
Device Manager
~~~

## Callback Design

Callback phải được giữ nhẹ:

~~~text
Nhận Packet
      ↓
Đọc Metadata
      ↓
Copy dữ liệu cần thiết
      ↓
Queue / Buffer
      ↓
Xử lý bên ngoài Callback
~~~

Không nên thực hiện trực tiếp trong callback:

~~~text
OLED Rendering
RSSI Filtering phức tạp
Distance Calculation
Risk Calculation
Logging nặng
~~~

## Channel Hopping

~~~text
CH1
 ↓
CH2
 ↓
CH3
 ↓
...
 ↓
CH13
 ↓
CH1
~~~

Mỗi channel có một khoảng thời gian quan sát gọi là **dwell time**.

Cần đánh giá sự đánh đổi giữa:

~~~text
Dwell Time
    vs
Detection Latency
~~~

## Thành viên A — Hưng

### Sniffer Firmware

- Bật promiscuous mode
- Cấu hình packet callback
- Trích xuất RSSI
- Trích xuất MAC
- Nhận dạng frame type
- Ghi nhận channel
- Ghi nhận timestamp
- Chuyển packet metadata thành `RadioObservation`
- Triển khai Channel Hopping

## Thành viên B — Huy

### Traffic Analysis

- Đếm packet activity
- Quản lý packet count
- Phân tích observation trùng lặp
- Phân tích RSSI
- Phân tích hoạt động giữa các channel
- Thử nghiệm nhiều dwell time
- So sánh Scanner Mode với Sniffer Mode

## Điều kiện hoàn thành

- Promiscuous Mode hoạt động ổn định
- Thu được MAC
- Thu được RSSI
- Thu được frame metadata cần thiết
- Thu được channel
- Channel Hopping hoạt động
- Packet count hoạt động
- Dữ liệu được chuyển sang `RadioObservation`
- Device Manager cũ tiếp tục hoạt động
- Firmware không crash trong traffic thông thường

---

# PHASE 5 — Phân tích hoạt động thiết bị nâng cao

## Mục tiêu

Sử dụng dữ liệu thu được từ promiscuous mode để xây dựng hồ sơ thiết bị chi tiết hơn.

Thông tin có thể sử dụng:

~~~text
MAC
RSSI
Filtered RSSI
Packet Count
Frame Type
Activity Frequency
Channel
First Seen
Last Seen
Persistence
RSSI Trend
~~~

Ví dụ:

~~~text
DEVICE

MAC:
AA:BB:CC:DD:EE:FF

RSSI:
-46 dBm

CHANNEL:
6

PACKETS:
842

LAST SEEN:
0.2 s ago

TREND:
APPROACHING
~~~

## RSSI Trend

Có thể bắt đầu từ:

~~~text
RSSI hiện tại - RSSI trước
~~~

Các trạng thái:

~~~text
APPROACHING
STABLE
MOVING AWAY
~~~

Tuy nhiên phiên bản hoàn chỉnh nên sử dụng nhiều mẫu RSSI để xác định trend thay vì chỉ dùng hai mẫu liên tiếp.

## Thành viên A — Hưng

- Tích hợp packet metadata vào Device Manager
- Phân loại metadata cần thiết
- Cải thiện state management
- Tích hợp packet activity vào Risk Engine
- Tối ưu RAM
- Tối ưu firmware
- Kiểm tra độ ổn định hệ thống

## Thành viên B — Huy

- Phân tích RSSI trend
- Đánh giá RSSI variance
- Phân tích channel activity
- Thử nghiệm các activity threshold
- Kiểm thử Risk Score
- Thu thập dataset thực nghiệm

## Điều kiện hoàn thành

Hệ thống có thể kết hợp:

~~~text
Identity
+
RSSI
+
Persistence
+
Activity
+
Channel
~~~

thành một **Device Profile**.

---

# PHASE 6 — Mở rộng Cấu kiện điện tử ET2041E

## Mục tiêu

Sau khi hệ thống IoT Sentinel chính hoạt động ổn định, bổ sung các mạch bán dẫn nhằm thể hiện trực tiếp kiến thức của học phần ET2041E.

Phần mở rộng phải hỗ trợ sản phẩm, không được làm mất trọng tâm chính của hệ thống.

## Linh kiện đề xuất

~~~text
2 × BJT 2N2222
1 × MOSFET 2N7000
2 × Diode 1N4148
1 × Potentiometer
LED
Resistors
~~~

## BJT Q1 — Alert Driver

~~~text
ESP32 GPIO
    ↓
Base Resistor
    ↓
2N2222
    ↓
Alert LED
~~~

Mạch thể hiện:

~~~text
Cutoff
Saturation
~~~

đồng thời thực hiện chức năng cảnh báo thực tế.

## BJT Q2 — Characterization Circuit

Sử dụng potentiometer để thay đổi base bias.

Đo:

~~~text
VB
VC
VE
~~~

Tính:

~~~text
VBE = VB - VE

VCE = VC - VE
~~~

Có thể ước lượng:

~~~text
IC
~~~

từ collector resistor.

Phân tích các vùng:

~~~text
Cutoff Region
Active Region
Saturation Region
~~~

Có thể mở rộng sang:

~~~text
DC Load Line
Operating Point
Q-Point
~~~

## MOSFET — 2N7000

Sử dụng 2N7000 làm switching stage.

So sánh:

~~~text
BJT
→ điều khiển theo dòng

MOSFET
→ điều khiển theo điện áp
~~~

## Diode — 1N4148

Có thể khảo sát:

~~~text
Protection
Clamping
Forward Voltage
~~~

## Thành viên A — Hưng

### Firmware / Measurement Integration

- Đọc ADC nếu cần
- Xử lý giá trị điện áp
- Tính các đại lượng suy ra
- Xác định transistor operating region
- Hiển thị thông số bán dẫn
- Tích hợp Diagnostic Mode vào firmware

## Thành viên B — Huy

### Hardware / Experimental Work

- Lắp mạch BJT
- Lắp mạch MOSFET
- Lắp mạch diode
- Kiểm tra pinout
- Đo bằng multimeter
- So sánh lý thuyết với thực nghiệm
- Khảo sát Q-point / Load Line nếu phù hợp

---

# 5. Kiến trúc hệ thống hoàn chỉnh

~~~text
                 IOT SENTINEL
                      │
                      ▼
              Wi-Fi 2.4 GHz
                      │
                      ▼
             Acquisition Layer
            ┌─────────┴─────────┐
            │                   │
            ▼                   ▼
      Wi-Fi Scanner       Wi-Fi Sniffer
       Prototype            Final Mode
            │                   │
            └─────────┬─────────┘
                      ▼
              RadioObservation
                      │
                      ▼
                Device Manager
                      │
        ┌─────────────┴─────────────┐
        ▼                           ▼
    RSSI Filter                Activity Data
        │                           │
        ▼                           │
 Distance Estimate                  │
        │                           │
        └─────────────┬─────────────┘
                      ▼
                Risk Engine
                      │
            ┌─────────┴─────────┐
            ▼                   ▼
           OLED               Alert
                                │
                                ▼
                         Semiconductor
                          Driver Stage
~~~

---

# 6. Mức độ ưu tiên

## P0 — Core System

Bắt buộc hoàn thành trước:

~~~text
Wi-Fi Acquisition
RadioObservation
MAC/BSSID Processing
Device Manager
RSSI History
RSSI Filtering
Distance Estimation
Stable Firmware
~~~

## P1 — Chức năng chính

~~~text
KNOWN / UNKNOWN
Risk Evaluation
OLED
Buttons
Alert
Promiscuous Mode
Channel Hopping
Packet Activity
~~~

## P2 — Phần mở rộng

~~~text
RSSI Trend Analysis
Advanced Activity Analysis
Semiconductor Diagnostics
BJT Characterization
MOSFET Experiment
Diode Experiment
Enclosure
Advanced UI
~~~

Các tính năng P2 không được làm chậm việc hoàn thành P0.

---

# 7. Cấu trúc Repository đề xuất

~~~text
iot-sentinel/
│
├── README.md
├── platformio.ini
│
├── include/
│   ├── radio_observation.h
│   ├── wifi_scanner.h
│   ├── wifi_sniffer.h
│   ├── channel_hopper.h
│   ├── device_manager.h
│   ├── rssi_filter.h
│   ├── distance_estimator.h
│   ├── risk_engine.h
│   └── oled_ui.h
│
├── src/
│   ├── main.cpp
│   ├── wifi_scanner.cpp
│   ├── wifi_sniffer.cpp
│   ├── channel_hopper.cpp
│   ├── device_manager.cpp
│   ├── rssi_filter.cpp
│   ├── distance_estimator.cpp
│   ├── risk_engine.cpp
│   └── oled_ui.cpp
│
├── test/
│
├── docs/
│   ├── architecture/
│   ├── hardware/
│   ├── measurements/
│   └── images/
│
└── data/
    ├── calibration/
    └── experiments/
~~~

---

# 8. Quy trình Git

Không phát triển trực tiếp trên branch `main`.

Các branch đề xuất:

~~~text
main
│
├── feature/wifi-scanner
├── feature/device-manager
├── feature/rssi-filter
├── feature/distance-estimator
├── feature/oled-ui
├── feature/wifi-sniffer
├── feature/channel-hopping
└── feature/et2041e-hardware
~~~

Workflow:

~~~text
Tạo Feature Branch
        ↓
Triển khai Feature
        ↓
Kiểm thử
        ↓
Commit
        ↓
Push
        ↓
Review chéo
        ↓
Merge vào Main
~~~

Ví dụ commit message:

~~~text
feat: add wifi scanning
feat: add radio observation structure
feat: add device tracking
feat: add RSSI moving average
feat: add distance estimator
feat: add promiscuous receiver
feat: add channel hopping
fix: prevent duplicate device entries
test: add RSSI calibration data
docs: update project architecture
~~~

---

# 9. Chiến lược phát triển

Luôn duy trì:

~~~text
MỘT PHIÊN BẢN ĐANG HOẠT ĐỘNG
             +
MỘT TÍNH NĂNG ĐANG PHÁT TRIỂN
~~~

Tránh:

~~~text
NHIỀU TÍNH NĂNG DỞ DANG
          +
KHÔNG CÓ BẢN ỔN ĐỊNH
~~~

Thứ tự phát triển:

~~~text
Make it work
      ↓
Make it measurable
      ↓
Make it modular
      ↓
Make it reliable
      ↓
Make it advanced
      ↓
Make it presentable
~~~

Tương ứng:

~~~text
Làm cho chạy được
      ↓
Làm cho đo được
      ↓
Module hóa
      ↓
Làm cho ổn định
      ↓
Nâng cấp tính năng
      ↓
Hoàn thiện sản phẩm
~~~

---

# 10. Các mốc phát triển

## M1 — Prototype thu thập dữ liệu

~~~text
ESP32
  ↓
WiFi.scanNetworks()
  ↓
RadioObservation
~~~

## M2 — Theo dõi và xử lý tín hiệu

~~~text
RadioObservation
       ↓
Device Manager
       ↓
RSSI Filter
       ↓
DIST EST
~~~

## M3 — Prototype cầm tay

~~~text
Device Analysis
      ↓
Risk Engine
      ↓
OLED + Buttons + Alert
~~~

## M4 — Passive Wi-Fi Monitoring

~~~text
Promiscuous Mode
       +
Channel Hopping
       ↓
RadioObservation
~~~

Pipeline đã xây dựng ở các phase trước phải tiếp tục được tái sử dụng.

## M5 — Advanced Device Analysis

~~~text
MAC
+
RSSI
+
Packet Activity
+
Persistence
+
Channel
↓
Device Profile
~~~

## M6 — ET2041E Extension

~~~text
IoT Sentinel
      +
Diode
      +
BJT
      +
MOSFET
      +
Experimental Measurements
~~~

---

# 11. Trạng thái triển khai

Firmware đã tích hợp các đầu việc phần mềm đến hết Phase 5:

- [x] Phase 1 — Wi-Fi scan định kỳ, `RadioObservation`, timestamp và xử lý
  kết quả bất đồng bộ.
- [x] Phase 2 — Device Manager, loại trùng theo MAC, timeout, RSSI moving
  average và distance estimate.
- [x] Phase 3 — KNOWN/UNKNOWN, heuristic Risk Engine, OLED, buttons và alert
  LED.
- [x] Phase 4 — Passive promiscuous metadata receiver và channel hopping;
  scanner vẫn có thể được chọn làm fallback lúc biên dịch.
- [x] Phase 5 — Packet/frame counters, activity rate, RSSI trend và device
  profile.

## Kiểm thử phần mềm

- [x] Firmware build bằng PlatformIO cho `esp32dev`.
- [x] Host tests cho RSSI filter, distance estimator, device tracking và
  risk scoring (`test/native/test_core.cpp`).

## Kiểm thử cần phần cứng / dữ liệu thực nghiệm

- [ ] Nạp và chạy firmware trên ESP32; xác minh OLED, nút, LED, promiscuous
  callback và channel hopping với wiring thực tế.
- [ ] Đo RSSI ở các khoảng cách đề xuất và hiệu chuẩn `RSSI_1m` cùng hệ số
  suy hao môi trường.
- [ ] Thu dataset và đối chiếu RSSI trend, activity thresholds, risk score.

Các mục cuối yêu cầu board, linh kiện, mạch và phép đo vật lý; chúng không
được đánh dấu hoàn thành chỉ dựa trên build hoặc mô phỏng phần mềm. Xem
`docs/hardware.md` trước khi lắp mạch. Khoảng cách RSSI và risk score chỉ là
ước lượng heuristic, không phải kết luận chắc chắn về thiết bị hay nguy cơ.

Phạm vi triển khai hiện tại dừng ở Phase 5. Phase 6 chưa được thực hiện.

## Trạng thái tại thời điểm khởi tạo roadmap (lưu trữ)

Hiện tại nhóm đang ở:

~~~text
PHASE 1
Prototype thu thập dữ liệu Wi-Fi
~~~

## Hưng — Thành viên A

~~~text
[ ] Triển khai WiFi.scanNetworks()
[ ] Đọc SSID
[ ] Đọc BSSID
[ ] Đọc RSSI
[ ] Đọc channel
[ ] Thêm timestamp
[ ] Tạo RadioObservation
[ ] Chuyển kết quả scan thành RadioObservation
[ ] Triển khai scan định kỳ
~~~

## Huy — Thành viên B

~~~text
[ ] Kiểm thử Wi-Fi scanning
[ ] Ghi nhận dao động RSSI
[ ] So sánh RSSI tại nhiều vị trí
[ ] Ghi lại các channel quan sát được
[ ] Thu thập dữ liệu test ban đầu
[ ] Chuẩn bị thí nghiệm hiệu chuẩn RSSI
~~~

> Chỉ chuyển sang **Phase 2** sau khi **Phase 1 hoạt động ổn định**.
