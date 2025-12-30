# Fibril Code Generation Specification (マイコン向け)

## 1. 概要

本ドキュメントは、Fibril IDLから**マイコン向けC++コード**を生成する際の仕様を定義します。マイコンの処理負荷とメモリ使用量を最小化することを最優先とし、複雑な処理はすべてMaster側で実行する設計方針を採用しています。

### 1.1. システム全体における位置づけ

本仕様は、Fibril CAN Transport システムの一部を構成します。システム全体のフローについては、以下のドキュメントを参照してください：

- **[main.md](./main.md)**: システム全体のアーキテクチャ、起動フロー、通信フェーズ
- **[dsl.md](./dsl.md)**: Fibril IDL（インターフェース定義言語）の構文仕様
- **[transport.md](./transport.md)**: FTP (Fibril Transport Protocol) の詳細仕様

特に、本ドキュメントで説明するマイコンコードは、`main.md`で定義されている以下のフェーズで動作します：

1. **Discovery** (機能=0x00): Masterがデバイスを検出
2. **Definition Exchange** (機能=0x01): デバイスがノード定義をMasterに送信
3. **Configuration** (機能=0x02): **本ドキュメントの主要部分** - MasterがIDマップをデバイスに配布
4. **Activation**: 通信開始
5. **Runtime**: Fast Path (Standard ID) でpub/sub、Slow Path (Extended ID) でservice通信

### 1.2. 設計原則

1. **Zero-Copy受信**: 受信したCANフレームを直接メモリに配置
2. **Direct Memory Access**: アドレス計算や再配置処理を不要にする
3. **Master-Driven Complexity**: 複雑なパッキング計算はMaster側で実行
4. **Static Layout**: 実行時の動的処理を排除し、コンパイル時に確定

---

## 2. コード生成の単位

### 2.1. ノードごとの生成

**重要**: コード生成は**ノードごと**に行われます。1つの`.fibril`ファイルから1つのノードクラスが生成されます。

```bash
# 各.fibrilファイルから独立したノードクラスヘッダを生成
fibril_gen motor.fibril       → motor_node.hpp
fibril_gen sensor.fibril      → sensor_node.hpp
fibril_gen controller.fibril  → controller_node.hpp
```

### 2.2. 利用者側での組み合わせ

利用者は任意の数のノードを組み合わせて使用できます：

```cpp
#include "motor_node.hpp"
#include "sensor_node.hpp"

int main() {
    // 同じクラスを複数インスタンス化可能
    MotorNode motor1;  // NodeID=0
    MotorNode motor2;  // NodeID=1
    SensorNode sensor; // NodeID=2
    
    // FibrilDeviceに登録（この順序でNodeIDが決定）
    FibrilDevice device;
    device.register_node(&motor1);
    device.register_node(&motor2);
    device.register_node(&sensor);
    
    device.init();
    while(1) { device.process(); }
}
```

**特徴**:

- 各ノードは**独立したアドレス空間**（0x00始まり）を持つ
- 同じノードクラスを複数回インスタンス化可能
- NodeIDは`register_node()`の呼び出し順で自動採番

---

## 3. メモリレイアウトと直接配置方式

### 3.1. 基本コンセプト

CANフレームのペイロードとマイコンのメモリレイアウトを完全に一致させ、`memcpy`のみで受信完了とします。

### 3.2. 生成されるデータ構造

```cpp
class MotorNode {
public:
    // すべてのpub/subデータを連続したメモリ領域に配置
    struct __attribute__((packed)) {
        // Sub ports (受信データ)
        float cmd_velocity;     // Address 0x00
        float cmd_torque;       // Address 0x04
        
        // Pub ports (送信データ)
        float current_velocity; // Address 0x08
        float current_torque;   // Address 0x0C
    } data;
};
```

**重要**: `__attribute__((packed))`により、アライメントパディングを排除し、Master側のパッキング計算と完全一致させます。

### 3.3. 受信処理

```cpp
void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
    // IDマップから該当エントリを検索
    for (uint8_t i = 0; i < rx_mapping_count; i++) {
        if (rx_map[i].can_id == can_id) {
            // 直接メモリへコピー
            uint8_t* dest = (uint8_t*)&data + rx_map[i].address;
            memcpy(dest, payload + rx_map[i].offset, rx_map[i].length);
            
            // 完了判定（最大ID方式）
            if (rx_map[i].is_completion) {
                on_data_received(rx_map[i].address);
            }
            return;
        }
    }
}
```

---

## 4. 複数フレーム対応（最大ID完了判定方式）

### 4.1. 設計方針

64byteを超えるデータは複数のCANフレームに分割されますが、以下の方針で単純化：

1. **直接書き込み**: 各フレームを受信次第、対象メモリアドレスに即座に書き込む
2. **順序保証**: CANは優先度順で送信されるため、通常は順序が保たれる
3. **最大ID完了判定**: 最大IDを持つフレームの受信で完了とみなす

### 4.2. 実装例

**DSL定義**:

```protobuf
struct LargeCommand {
    float data[20];  // 80 bytes → 2フレームに分割
}

node Motor {
    sub LargeCommand cmd;
}
```

**生成コード**:

```cpp
class Motor {
    struct __attribute__((packed)) {
        LargeCommand cmd;  // Address 0x00-0x4F (80 bytes)
    } data;
    
    struct IDMapping {
        uint16_t can_id;
        uint16_t address;
        uint8_t offset;
        uint8_t length;
        bool is_completion;  // 最大IDフラグ
    };
    
    IDMapping rx_map[2];  // 2フレーム分
};
```

**Masterからの設定**:

```
# フレーム1: 最初の64byte (IsCompletion=0)
NodeID=0, Address=0x00, CANID=0x100, Offset=0, Length=64, IsCompletion=0, Direction=RX

# フレーム2: 残りの16byte (IsCompletion=1) ← 最大ID、これで完了
NodeID=0, Address=0x40, CANID=0x101, Offset=0, Length=16, IsCompletion=1, Direction=RX
```

### 4.3. 順序保証の戦略

Master側で以下を保証：

1. **連続IDの割り当て**: 分割フレームには連続したStandard IDを割り当て
2. **優先度の統一**: 同じデータの分割フレームは同じ優先度
3. **送信間隔の最小化**: バースト送信で順序を保証

---

## 5. IDマッピング構造

### 5.1. IDMapping構造体

```cpp
struct IDMapping {
    uint16_t can_id;        // 2 bytes - 受信/送信するCAN ID
    uint16_t address;       // 2 bytes - data構造体内のオフセット
    uint8_t offset;         // 1 byte - CANフレーム内のオフセット
    uint8_t length;         // 1 byte - データサイズ
    bool is_completion;     // 1 byte - 完了判定フラグ
    // [padding 1 byte]
};  // Total: 8 bytes
```

### 5.2. Configuration Protocol

MasterからのIDマップ設定フォーマット：

```binary
[NodeID (1)] [Address (2)] [StandardID (2)] [Offset (1)] [Length (1)] [IsCompletion (1)] [Direction (1)]
```

- **Direction**: 0=RX, 1=TX
- **IsCompletion**: 1=このフレーム受信で完了、0=まだ続きがある

---

## 6. リソースサイジング

### 6.1. MAX_RX_MAPPINGS / MAX_TX_MAPPINGSの計算

コード生成時に自動計算されます：

```cpp
MAX_RX_MAPPINGS = Σ(各subポートの分割フレーム数)
MAX_TX_MAPPINGS = Σ(各pubポートの分割フレーム数)

// 分割フレーム数 = ceil(データサイズ / 64)
```

**例**:

```protobuf
node MotorController {
    sub LargeCommand cmd;        // 160 bytes → 3フレーム
    sub float emergency_stop;    // 4 bytes → 1フレーム
    pub float current_position;  // 4 bytes → 1フレーム
    pub float current_velocity;  // 4 bytes → 1フレーム
}
```

生成される定数：

```cpp
static constexpr uint8_t MAX_RX_MAPPINGS = 4;  // 3 + 1
static constexpr uint8_t MAX_TX_MAPPINGS = 2;  // 1 + 1
```

### 6.2. メモリ使用量の見積もり

```
ノード全体のメモリ = データ構造 + IDマッピング配列

データ構造 = Σ(各ポートのサイズ)
IDマッピング = (MAX_RX_MAPPINGS + MAX_TX_MAPPINGS) × 8 bytes
```

**上記例の場合**:

- データ構造: 172 bytes (160 + 4 + 4 + 4)
- IDマッピング: 48 bytes ((4 + 2) × 8)
- **合計: 220 bytes**

### 6.3. 生成コードへの埋め込み

```cpp
// motor_controller_node.hpp (自動生成)
class MotorController {
public:
    static constexpr uint8_t MAX_RX_MAPPINGS = 4;
    static constexpr uint8_t MAX_TX_MAPPINGS = 2;
    
    // Estimated memory usage:
    //   - Data structure: 172 bytes
    //   - ID mappings: 48 bytes
    //   - Total: 220 bytes
    
    IDMapping rx_map[MAX_RX_MAPPINGS];
    IDMapping tx_map[MAX_TX_MAPPINGS];
    
    uint8_t rx_mapping_count = 0;  // 実際に設定された数
    uint8_t tx_mapping_count = 0;
};
```

---

## 7. クロスノードパッキング

### 7.1. コンセプト

複数ノードのpub/subデータを1つのCANフレームにまとめることで、バス効率を向上させます。

### 7.2. 実装例

**DSL定義**:

```protobuf
node Motor1 {
    pub float position;  // 4 bytes
}

node Motor2 {
    pub float position;  // 4 bytes
}

node Motor3 {
    pub float position;  // 4 bytes
}
```

**従来方式**: 3フレーム必要
**パッキング方式**: 1フレームで完結

```
CAN ID 0x200: [Motor1.position(4)] [Motor2.position(4)] [Motor3.position(4)]
              └─ Offset 0          └─ Offset 4          └─ Offset 8
```

**Masterからの設定**:

```
NodeID=0, Address=0x00, CANID=0x200, Offset=0, Length=4, IsCompletion=1, Direction=TX
NodeID=1, Address=0x00, CANID=0x200, Offset=4, Length=4, IsCompletion=1, Direction=TX
NodeID=2, Address=0x00, CANID=0x200, Offset=8, Length=4, IsCompletion=1, Direction=TX
```

### 7.3. マイコン側の実装

各ノードは独立したアドレス空間を持つため、特別な処理は不要：

```cpp
// 送信側（デバイス共通処理）
void FibrilDevice::publish_periodic() {
    // 同じCAN IDに複数ノードのデータを集約
    uint8_t payload[64];
    uint16_t current_id = 0;
    uint8_t payload_len = 0;
    
    for (auto& node : nodes) {
        for (auto& mapping : node->tx_map) {
            if (mapping.can_id != current_id && payload_len > 0) {
                // 前のIDのフレームを送信
                can_send(current_id, payload, payload_len);
                payload_len = 0;
            }
            
            current_id = mapping.can_id;
            uint8_t* src = (uint8_t*)&node->data + mapping.address;
            memcpy(&payload[mapping.offset], src, mapping.length);
            payload_len = max(payload_len, mapping.offset + mapping.length);
        }
    }
    
    // 最後のフレームを送信
    if (payload_len > 0) {
        can_send(current_id, payload, payload_len);
    }
}
```

---

## 8. 生成されるノードクラスの完全な例

```cpp
// motor_node.hpp (fibril_genによる自動生成)

#ifndef MOTOR_NODE_HPP
#define MOTOR_NODE_HPP

#include <stdint.h>
#include <string.h>

// リソースサイジング（自動計算）
#ifndef MOTOR_MAX_RX_MAPPINGS
#define MOTOR_MAX_RX_MAPPINGS 2
#endif

#ifndef MOTOR_MAX_TX_MAPPINGS
#define MOTOR_MAX_TX_MAPPINGS 2
#endif

class MotorNode {
public:
    // データ構造（__attribute__((packed))でアライメント無効化）
    struct __attribute__((packed)) {
        // Sub ports (RX)
        float cmd_velocity;     // Address 0x00 (4 bytes)
        float cmd_torque;       // Address 0x04 (4 bytes)
        
        // Pub ports (TX)
        float current_velocity; // Address 0x08 (4 bytes)
        float current_torque;   // Address 0x0C (4 bytes)
    } data;
    
    // IDマッピング構造体
    struct IDMapping {
        uint16_t can_id;
        uint16_t address;
        uint8_t offset;
        uint8_t length;
        bool is_completion;
    };
    
    // IDマッピング配列
    IDMapping rx_map[MOTOR_MAX_RX_MAPPINGS];
    IDMapping tx_map[MOTOR_MAX_TX_MAPPINGS];
    
    uint8_t rx_mapping_count = 0;
    uint8_t tx_mapping_count = 0;
    
    // CAN受信処理
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
        for (uint8_t i = 0; i < rx_mapping_count; i++) {
            if (rx_map[i].can_id == can_id) {
                uint8_t* dest = (uint8_t*)&data + rx_map[i].address;
                memcpy(dest, payload + rx_map[i].offset, rx_map[i].length);
                
                if (rx_map[i].is_completion) {
                    on_cmd_received(rx_map[i].address);
                }
                return;
            }
        }
    }
    
    // Configuration受信（Masterからのマッピング設定）
    void configure_rx_mapping(uint16_t address, uint16_t can_id, 
                             uint8_t offset, uint8_t length, bool is_completion) {
        if (rx_mapping_count < MOTOR_MAX_RX_MAPPINGS) {
            rx_map[rx_mapping_count].address = address;
            rx_map[rx_mapping_count].can_id = can_id;
            rx_map[rx_mapping_count].offset = offset;
            rx_map[rx_mapping_count].length = length;
            rx_map[rx_mapping_count].is_completion = is_completion;
            rx_mapping_count++;
        }
    }
    
    void configure_tx_mapping(uint16_t address, uint16_t can_id,
                             uint8_t offset, uint8_t length, bool is_completion) {
        if (tx_mapping_count < MOTOR_MAX_TX_MAPPINGS) {
            tx_map[tx_mapping_count].address = address;
            tx_map[tx_mapping_count].can_id = can_id;
            tx_map[tx_mapping_count].offset = offset;
            tx_map[tx_mapping_count].length = length;
            tx_map[tx_mapping_count].is_completion = is_completion;
            tx_mapping_count++;
        }
    }
    
    // ユーザーオーバーライド可能なコールバック
    virtual void on_cmd_received(uint16_t address) {
        // アドレスに応じて処理を分岐
        if (address == 0x00) {
            // cmd_velocityが更新された
        } else if (address == 0x04) {
            // cmd_torqueが更新された
        }
    }
};

#endif // MOTOR_NODE_HPP
```

---

## 9. 最適化ガイドライン

### 9.1. データサイズの推奨事項

- **推奨**: 各ポートのデータは64byte以内に収める
- **理由**: 複数フレームに分割されるとIDマッピングが増加し、メモリとCPU負荷が増える

```protobuf
// ❌ 避けるべき
struct HugeData {
    float data[100];  // 400 bytes → 7フレーム、7マッピング必要
}

// ✅ 推奨
struct ReasonableData {
    float position[10];  // 40 bytes → 1フレーム
    float velocity[10];  // 40 bytes → 1フレーム
}
```

### 9.2. メモリ使用量の最小化

1. **不要なポートを削除**: 使用しないpub/subは定義から除外
2. **プリミティブ型の活用**: 構造体よりプリミティブ型を優先
3. **固定配列のサイズ調整**: 必要最小限のサイズに設定

### 9.3. CPU負荷の最小化

- **直接メモリアクセス**: すべての受信は1回の`memcpy`で完了
- **線形探索の最適化**: IDマッピング数を少なく保つ（通常2-4個）
- **早期リターン**: マッチしたら即座にループを抜ける

---

## 10. コード生成ツール (fibril_gen) の動作

### 10.1. 入力

- `.fibril`ファイル（DSL定義）

### 10.2. 処理フロー

1. **DSL解析**: 構文解析とセマンティック解析
2. **データサイズ計算**: 各ポートのバイトサイズを計算
3. **フレーム分割判定**: 64byte基準で分割数を決定
4. **リソースサイジング**: `MAX_RX_MAPPINGS`等を算出
5. **コード生成**: C++ヘッダファイルを出力
6. **定義バイナリ生成**: `definition.bin`を出力（Master側で使用）

### 10.3. 出力

- `<node_name>_node.hpp`: ノードクラスヘッダ
- `<node_name>_def.bin`: ノード定義バイナリ（Masterへ送信用）

---

## 12. Service と Parameter の実装

### 12.1. 概要

サービス（Service）とパラメータ（Parameter）は、pub/subと異なり**双方向通信**を行います。

- **Service**: Request/Response型の汎用サービス（例: モーター有効化、オドメトリリセット）
- **Parameter**: 設定値の読み書き（例: 最大速度、制御周波数）

**通信方式**:

- **通信路**: Extended ID (Slow Path)
- **Feature ID**: 0x03 (Service Protocol)
- **分割転送**: ISO-TP風のFragmentationをサポート

### 12.2. DSL定義

#### 12.2.1. ROS Serviceとして公開

```protobuf
// Request/Response型の定義
struct EnableMotorRequest {
    #[ros_map(data)]
    bool enable;
}

struct EnableMotorResponse {
    #[ros_map(success)]
    bool success;
}

node MobileBase {
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(EnableMotorRequest) -> EnableMotorResponse;
    
    #[ros_service(std_srvs/srv/Trigger)]
    service reset_odometry() -> void;  // Requestなし
}
```

#### 12.2.2. ROS Parameterとして公開

```protobuf
node MobileBase {
    #[ros_param("~/max_velocity", 1.0)]
    service set_max_velocity(float value) -> void;
    
    #[ros_param("~/control_frequency", 50)]
    service set_control_freq(uint16 freq) -> void;
}
```

**制約**:

- Parameter: Request型はプリミティブ型のみ、Response型は必ず`void`
- デフォルト値が必須

### 12.3. Service ID の割り当て

各サービスには、ノード内で一意な**Service ID**が割り当てられます。

**割り当てルール**:

1. DSL定義順に0から連番で自動割り当て
2. コード生成時に`enum`として定義

**例**:

```cpp
// 自動生成される定数
class MobileBase {
    enum ServiceID : uint8_t {
        SERVICE_ENABLE_MOTOR = 0,
        SERVICE_RESET_ODOMETRY = 1,
        SERVICE_SET_MAX_VELOCITY = 2,
        SERVICE_SET_CONTROL_FREQ = 3,
        SERVICE_COUNT = 4
    };
};
```

### 12.4. Request/Response のメモリレイアウト

#### 12.4.1. Request型の格納

サービスごとに専用の構造体を生成：

```cpp
struct EnableMotorRequest {
    bool enable;  // 1 byte
};

struct SetMaxVelocityRequest {
    float value;  // 4 bytes
};
```

#### 12.4.2. Response型の格納

同様に専用構造体を生成：

```cpp
struct EnableMotorResponse {
    bool success;  // 1 byte
};

// voidの場合は構造体なし
```

#### 12.4.3. 独立したRequest/Response変数

**重要**: 複数のサービスを同時に実行可能にするため、各サービスごとに独立した変数を定義します。

```cpp
class MobileBase {
    // 各サービス専用のRequest/Response変数
    EnableMotorRequest enable_motor_req;
    EnableMotorResponse enable_motor_res;
    
    SetMaxVelocityRequest set_max_velocity_req;
    // set_max_velocityはResponseなし（void）
    
    // reset_odometryはRequest/Responseともになし（void -> void）
};
```

**メモリ使用量**: `Σ(各Request型のサイズ)` + `Σ(各Response型のサイズ)`

**例**:

```
EnableMotorRequest: 1 byte
EnableMotorResponse: 1 byte
SetMaxVelocityRequest: 4 bytes
合計: 6 bytes
```

**メリット**:

- 複数のサービスを同時に処理可能
- 非同期応答が可能（後述）
- デバッグが容易（各変数が独立）

### 12.5. Service Protocol (Extended ID)

#### 12.5.1. CAN ID構造

```text
[28:24] Feature = 0x03 (Service)
[23:19] DeviceID
[18:14] NodeID
[13:0]  Transaction ID (Request/Responseの対応付け)
```

#### 12.5.2. Payload Format

**Request (Master → Device)**:

```binary
[FTP Header (1)] [Service ID (1)] [Request Data...]
```

- **FTP Header**: Single Frame (0x00 | Length) または First Frame (0x40 | LenHigh)
- **Service ID**: 0-255の範囲でノード内で一意
- **Request Data**: Request型のシリアライズデータ

**Response (Device → Master)**:

```binary
[FTP Header (1)] [Result Code (1)] [Response Data...]
```

- **Result Code**: 実行結果（0x00=OK, 0x01=Unknown Service, etc.）
- **Response Data**: Response型のシリアライズデータ

#### 12.5.3. Result Codes

```cpp
enum ResultCode : uint8_t {
    OK = 0x00,               // 成功
    UNKNOWN_SERVICE = 0x01,  // Service IDが見つからない
    INVALID_ARGUMENT = 0x02, // Request Dataの解析失敗
    INTERNAL_ERROR = 0x03,   // サービス実行中のエラー
    BUSY = 0xFF              // 処理中（リトライ推奨）
};
```

### 12.6. 生成されるコード

#### 12.6.1. Service Handler の生成

```cpp
class MobileBase {
public:
    // Service ID定義
    enum ServiceID : uint8_t {
        SERVICE_ENABLE_MOTOR = 0,
        SERVICE_RESET_ODOMETRY = 1,
        SERVICE_COUNT = 2
    };
    
    // Request/Response型
    struct EnableMotorRequest { bool enable; };
    struct EnableMotorResponse { bool success; };
    
    // Service受信処理
    void on_service_request(uint16_t transaction_id, uint8_t service_id, 
                           uint8_t* request_data, uint8_t request_len) {
        switch (service_id) {
            case SERVICE_ENABLE_MOTOR: {
                // Requestデシリアライズ
                EnableMotorRequest req;
                memcpy(&req, request_data, sizeof(EnableMotorRequest));
                
                // ユーザーハンドラ呼び出し
                EnableMotorResponse res = handle_enable_motor(req);
                
                // Response送信
                send_service_response(transaction_id, OK, 
                                     (uint8_t*)&res, sizeof(res));
                break;
            }
            
            case SERVICE_RESET_ODOMETRY: {
                // Requestなし
                handle_reset_odometry();
                
                // Responseなし（voidの場合）
                send_service_response(transaction_id, OK, nullptr, 0);
                break;
            }
            
            default:
                send_service_response(transaction_id, UNKNOWN_SERVICE, nullptr, 0);
                break;
        }
    }
    
    // ユーザーオーバーライド可能なハンドラ
    virtual EnableMotorResponse handle_enable_motor(const EnableMotorRequest& req) {
        // デフォルト実装
        EnableMotorResponse res;
        res.success = false;
        return res;
    }
    
    virtual void handle_reset_odometry() {
        // デフォルト実装（何もしない）
    }
    
private:
    void send_service_response(uint16_t transaction_id, uint8_t result_code,
                               uint8_t* response_data, uint8_t response_len);
};
```

### 12.7. Parameter の実装

#### 12.7.1. Parameter用のメモリ領域

パラメータは通常の`data`構造体とは別に保存：

```cpp
class MobileBase {
    // Pub/Subデータ
    struct __attribute__((packed)) {
        float cmd_velocity;
        // ...
    } data;
    
    // Parameter専用領域
    struct {
        float max_velocity;       // デフォルト: 1.0
        uint16_t control_frequency; // デフォルト: 50
    } params;
};
```

#### 12.7.2. Parameter Service の生成

```cpp
void on_service_request(uint16_t transaction_id, uint8_t service_id, 
                       uint8_t* request_data, uint8_t request_len) {
    switch (service_id) {
        case SERVICE_SET_MAX_VELOCITY: {
            float value;
            memcpy(&value, request_data, sizeof(float));
            
            // パラメータ更新
            params.max_velocity = value;
            
            // コールバック（オプション）
            on_param_changed_max_velocity(value);
            
            // voidなのでResponseなし
            send_service_response(transaction_id, OK, nullptr, 0);
            break;
        }
        
        // 同様に他のパラメータも処理
    }
}

// ユーザーオーバーライド可能
virtual void on_param_changed_max_velocity(float new_value) {
    // パラメータ変更時の処理
}
```

#### 12.7.3. Parameter初期化

```cpp
MobileBase() {
    // デフォルト値で初期化
    params.max_velocity = 1.0f;
    params.control_frequency = 50;
}
```

### 12.8. Service数の制約

#### 12.8.1. MAX_SERVICES の決定

```cpp
static constexpr uint8_t MAX_SERVICES = サービス定義数;
```

**例**:

```protobuf
node Example {
    service A(...);
    service B(...);
    service C(...);
}
// → MAX_SERVICES = 3
```

Service IDは8bitなので、最大255個まで定義可能です。

#### 12.8.2. メモリ使用量

```
Service用メモリ = Σ(各Request型) + Σ(各Response型) + Parameter領域 + 非同期コンテキスト
```

**例**:

```
EnableMotorRequest: 1 byte
EnableMotorResponse: 1 byte
SetMaxVelocityRequest: 4 bytes
Parameter領域: 4 bytes (float max_velocity)
非同期コンテキスト: 3 bytes (AsyncServiceContext × 1)
合計: 13 bytes
```

**注意**: 共用体を使用しないため、複数のサービスがある場合はメモリ使用量が増加しますが、同時実行と非同期応答が可能になります。

### 12.9. 非同期応答のサポート

#### 12.9.1. 非同期応答の必要性

長時間かかる処理（モーター初期化、センサーキャリブレーションなど）では、即座にResponseを返すことができません。このような場合、非同期応答機能を使用します。

#### 12.9.2. Transaction IDの保存

Service要求を受信した際、Transaction IDを保存しておき、後でResponse送信に使用します：

```cpp
class MobileBase {
    // 非同期処理用のトランザクション管理
    struct AsyncServiceContext {
        uint16_t transaction_id;
        uint8_t service_id;
        bool is_pending;  // 処理中フラグ
    };
    
    // サービスごとにコンテキストを保持
    AsyncServiceContext enable_motor_ctx;
    AsyncServiceContext reset_odometry_ctx;
};
```

#### 12.9.3. 非同期応答の実装パターン

**パターン1: 即座にACKを返し、後で完了通知**

```cpp
void on_service_request(uint16_t transaction_id, uint8_t service_id,
                       uint8_t* request_data, uint8_t request_len) {
    switch (service_id) {
        case SERVICE_ENABLE_MOTOR: {
            EnableMotorRequest req;
            memcpy(&req, request_data, sizeof(req));
            
            // Transaction IDを保存
            enable_motor_ctx.transaction_id = transaction_id;
            enable_motor_ctx.service_id = service_id;
            enable_motor_ctx.is_pending = true;
            
            // Request情報を保存
            enable_motor_req = req;
            
            // 即座にACKを返す（処理開始を通知）
            EnableMotorResponse ack;
            ack.success = true;  // 仮の成功
            send_service_response(transaction_id, OK, (uint8_t*)&ack, sizeof(ack));
            
            // 実際の処理はバックグラウンドで実行
            start_enable_motor_async(req.enable);
            break;
        }
    }
}

// メインループやタイマーコールバックで処理完了をチェック
void process() {
    if (enable_motor_ctx.is_pending && is_motor_ready()) {
        // 処理完了、最終結果を送信
        enable_motor_res.success = get_motor_status();
        
        // 別のTransaction IDで完了通知（オプション）
        // または、Pub/Subで状態を通知
        
        enable_motor_ctx.is_pending = false;
    }
}
```

**パターン2: Response送信を遅延**

```cpp
void on_service_request(uint16_t transaction_id, uint8_t service_id,
                       uint8_t* request_data, uint8_t request_len) {
    switch (service_id) {
        case SERVICE_RESET_ODOMETRY: {
            // Transaction IDを保存（Responseは送らない）
            reset_odometry_ctx.transaction_id = transaction_id;
            reset_odometry_ctx.service_id = service_id;
            reset_odometry_ctx.is_pending = true;
            
            // 非同期処理を開始
            start_reset_odometry_async();
            // ここではResponseを返さない
            break;
        }
    }
}

// 処理完了時にResponse送信
void on_reset_odometry_complete() {
    if (reset_odometry_ctx.is_pending) {
        send_service_response(reset_odometry_ctx.transaction_id, OK, nullptr, 0);
        reset_odometry_ctx.is_pending = false;
    }
}
```

#### 12.9.4. タイムアウト管理

Master側でタイムアウトを設定：

- デフォルト: 1秒
- 長時間処理用サービス: ユーザー定義（例: 10秒）

Device側での推奨事項：

1. **短時間処理（<100ms）**: 同期的にResponseを返す
2. **中時間処理（100ms-1s）**: 即座にACKを返し、完了をPubで通知
3. **長時間処理（>1s）**: 遅延Responseを使用し、Masterのタイムアウトを長めに設定

#### 12.9.5. 生成コードでの対応

**非同期コンテキストの自動生成**:

```cpp
class MobileBase {
public:
    // 非同期対応が必要なサービスのコンテキスト
    struct AsyncServiceContext {
        uint16_t transaction_id = 0;
        bool is_pending = false;
    };
    
    // サービスごとのコンテキスト（voidでないResponseを持つサービスのみ）
    AsyncServiceContext enable_motor_async;
    
    // 非同期Response送信API
    void send_async_response_enable_motor(const EnableMotorResponse& res, uint8_t result_code = OK) {
        if (enable_motor_async.is_pending) {
            send_service_response(enable_motor_async.transaction_id, result_code,
                                 (uint8_t*)&res, sizeof(res));
            enable_motor_async.is_pending = false;
        }
    }
};
```

**ユーザーコードでの使用例**:

```cpp
// Serviceハンドラで非同期処理を開始
EnableMotorResponse handle_enable_motor(const EnableMotorRequest& req) {
    // Transaction IDを保存（自動的に行われる）
    // enable_motor_async.is_pending = true;
    
    // 非同期処理を開始
    motor_enable_requested = req.enable;
    
    // 仮のResponseを返す（または例外を投げて遅延Responseを示す）
    EnableMotorResponse res;
    res.success = true;  // 仮
    return res;
}

// メインループで処理完了をチェック
void loop() {
    if (enable_motor_async.is_pending && motor_init_complete()) {
        EnableMotorResponse final_res;
        final_res.success = motor_is_enabled();
        send_async_response_enable_motor(final_res);
    }
}
```

### 12.10. タイムアウト処理

#### 12.10.1. Master側のタイムアウト

Master側でタイムアウトを管理：

- デフォルト: 1秒
- タイムアウト時は`BUSY`とみなしてリトライ

#### 12.10.2. Device側の処理時間

デバイス側では即座にResponse送信を推奨：

- 長時間かかる処理は非同期で実行
- Requestを受けたら即座に`OK`を返し、処理は後で実行

```cpp
EnableMotorResponse handle_enable_motor(const EnableMotorRequest& req) {
    // フラグをセットして非同期処理
    enable_motor_requested = true;
    enable_motor_value = req.enable;
    
    // 即座にResponse返却
    EnableMotorResponse res;
    res.success = true;
    return res;
}
```

### 12.11. 完全な実装例（Response送信遅延パターン）

```cpp
// mobile_base_node.hpp (自動生成)
class MobileBase {
public:
    // Service ID定義
    enum ServiceID : uint8_t {
        SERVICE_ENABLE_MOTOR = 0,
        SERVICE_RESET_ODOMETRY = 1,
        SERVICE_SET_MAX_VELOCITY = 2,
        SERVICE_COUNT = 3
    };
    
    // Result Code定義
    enum ResultCode : uint8_t {
        OK = 0x00,
        UNKNOWN_SERVICE = 0x01,
        INVALID_ARGUMENT = 0x02,
        INTERNAL_ERROR = 0x03,
        BUSY = 0xFF
    };
    
    // Request/Response型
    struct EnableMotorRequest { bool enable; };
    struct EnableMotorResponse { bool success; };
    struct SetMaxVelocityRequest { float value; };
    
    // Pub/Subデータ
    struct __attribute__((packed)) {
        float cmd_velocity;
        float current_velocity;
    } data;
    
    // Parameter領域
    struct {
        float max_velocity = 1.0f;
    } params;
    
    // 各サービスのRequest/Response変数（独立）
    EnableMotorRequest enable_motor_req;
    EnableMotorResponse enable_motor_res;
    SetMaxVelocityRequest set_max_velocity_req;
    // reset_odometry, set_max_velocityはResponseなし
    
    // 非同期コンテキスト
    struct AsyncServiceContext {
        uint16_t transaction_id = 0;
        bool is_pending = false;
    };
    
    AsyncServiceContext enable_motor_async;
    AsyncServiceContext reset_odometry_async;
    AsyncServiceContext set_max_velocity_async;
    
    // Service受信処理（Response送信を遅延）
    void on_service_request(uint16_t transaction_id, uint8_t service_id,
                           uint8_t* request_data, uint8_t request_len) {
        switch (service_id) {
            case SERVICE_ENABLE_MOTOR: {
                if (request_len != sizeof(EnableMotorRequest)) {
                    // エラーの場合のみ即座にResponse
                    send_service_response(transaction_id, INVALID_ARGUMENT, nullptr, 0);
                    return;
                }
                memcpy(&enable_motor_req, request_data, sizeof(enable_motor_req));
                
                // Transaction IDを保存（Responseは送らない）
                enable_motor_async.transaction_id = transaction_id;
                enable_motor_async.is_pending = true;
                
                // ハンドラ呼び出し（非同期処理開始）
                handle_enable_motor(enable_motor_req);
                
                // ここではResponseを送らない
                // 処理完了後にsend_async_response_enable_motor()で送信
                break;
            }
            
            case SERVICE_RESET_ODOMETRY: {
                // Transaction IDを保存
                reset_odometry_async.transaction_id = transaction_id;
                reset_odometry_async.is_pending = true;
                
                // 非同期処理開始
                handle_reset_odometry();
                
                // Responseは後で送信
                break;
            }
            
            case SERVICE_SET_MAX_VELOCITY: {
                if (request_len != sizeof(SetMaxVelocityRequest)) {
                    send_service_response(transaction_id, INVALID_ARGUMENT, nullptr, 0);
                    return;
                }
                memcpy(&set_max_velocity_req, request_data, sizeof(set_max_velocity_req));
                
                // Transaction IDを保存
                set_max_velocity_async.transaction_id = transaction_id;
                set_max_velocity_async.is_pending = true;
                
                // パラメータ更新（この例では即座に完了）
                params.max_velocity = set_max_velocity_req.value;
                on_param_changed_max_velocity(set_max_velocity_req.value);
                
                // Responseは後で送信（即座に完了する場合でも統一）
                break;
            }
            
            default:
                send_service_response(transaction_id, UNKNOWN_SERVICE, nullptr, 0);
                break;
        }
    }
    
    // 非同期Response送信API（自動生成）
    void send_async_response_enable_motor(const EnableMotorResponse& res, 
                                          uint8_t result_code = OK) {
        if (enable_motor_async.is_pending) {
            send_service_response(enable_motor_async.transaction_id, result_code,
                                 (uint8_t*)&res, sizeof(res));
            enable_motor_async.is_pending = false;
        }
    }
    
    void send_async_response_reset_odometry(uint8_t result_code = OK) {
        if (reset_odometry_async.is_pending) {
            send_service_response(reset_odometry_async.transaction_id, result_code, 
                                 nullptr, 0);
            reset_odometry_async.is_pending = false;
        }
    }
    
    void send_async_response_set_max_velocity(uint8_t result_code = OK) {
        if (set_max_velocity_async.is_pending) {
            send_service_response(set_max_velocity_async.transaction_id, result_code,
                                 nullptr, 0);
            set_max_velocity_async.is_pending = false;
        }
    }
    
    // ユーザーオーバーライド可能なハンドラ
    // 注意: このパターンでは戻り値は使用されない（非同期応答用）
    virtual void handle_enable_motor(const EnableMotorRequest& req) {
        // デフォルト実装
        // 非同期処理を開始し、完了時にsend_async_response_enable_motor()を呼ぶ
    }
    
    virtual void handle_reset_odometry() {
        // デフォルト実装
        // 非同期処理を開始し、完了時にsend_async_response_reset_odometry()を呼ぶ
    }
    
    virtual void on_param_changed_max_velocity(float new_value) {
        // パラメータ変更通知
        // 即座に完了する場合、ここでsend_async_response_set_max_velocity()を呼んでもよい
    }
    
private:
    void send_service_response(uint16_t transaction_id, uint8_t result_code,
                               uint8_t* response_data, uint8_t response_len);
};
```

**ユーザー実装例**:

```cpp
class MyMobileBase : public MobileBase {
private:
    bool motor_init_in_progress = false;
    
public:
    // 非同期処理を開始するだけ
    void handle_enable_motor(const EnableMotorRequest& req) override {
        if (req.enable) {
            motor_init_in_progress = true;
            // モーター初期化を開始（時間がかかる）
            start_motor_initialization();
        } else {
            // 即座に完了する場合
            enable_motor_res.success = false;
            send_async_response_enable_motor(enable_motor_res);
        }
    }
    
    void handle_reset_odometry() override {
        // オドメトリリセット処理
        reset_odometry_values();
        
        // 即座に完了
        send_async_response_reset_odometry();
    }
    
    void on_param_changed_max_velocity(float new_value) override {
        // パラメータ適用（即座に完了）
        apply_max_velocity(new_value);
        
        // Response送信
        send_async_response_set_max_velocity();
    }
    
    // メインループで処理完了をチェック
    void loop() {
        // 他の処理...
        
        // モーター初期化完了チェック
        if (motor_init_in_progress && is_motor_initialized()) {
            motor_init_in_progress = false;
            
            // Response送信
            enable_motor_res.success = true;
            send_async_response_enable_motor(enable_motor_res);
        }
    }
};
```

**メモリ使用量の内訳**:

```
Pub/Subデータ: 8 bytes (cmd_velocity + current_velocity)
Parameter: 4 bytes (max_velocity)
Service Request: 5 bytes (EnableMotorRequest(1) + SetMaxVelocityRequest(4))
Service Response: 1 byte (EnableMotorResponse(1))
Async Context: 9 bytes (AsyncServiceContext × 3)
合計: 27 bytes
```

**このパターンの特徴**:

- ✅ **完全な非同期**: すべてのサービスが非同期応答可能
- ✅ **統一されたAPI**: 全サービスで同じ実装パターン
- ✅ **柔軟な処理時間**: 即座に完了する場合も、長時間かかる場合も同じコードで対応
- ⚠️ **メモリ増加**: すべてのサービスに非同期コンテキストが必要（3 bytes × サービス数）
- ⚠️ **応答忘れのリスク**: ユーザーが明示的にResponse送信を呼ぶ必要がある

---

## 13. FibrilDevice の実装

### 13.1. 概要

`FibrilDevice`クラスは、生成された個々のノードクラスを統合し、CAN通信とノードのライフサイクルを管理します。

**主な責務**:

1. ノードの登録とNodeID自動割り当て
2. CAN受信フレームの適切なノードへのディスパッチ
3. Configuration受信とIDマップ設定
4. 定周期pub送信の管理
5. Service要求の処理とResponse送信

### 13.2. クラス定義

```cpp
class FibrilDevice {
public:
    // 最大ノード数（コンパイル時定数）
    static constexpr uint8_t MAX_NODES = 32;
    
    FibrilDevice();
    
    // ノード登録（呼び出し順でNodeID決定）
    template<typename NodeT>
    bool register_node(NodeT* node);
    
    // 初期化
    void init();
    
    // 定期処理（メインループから呼ぶ）
    void process();
    
    // CAN受信ハンドラ（CANドライバから呼ばれる）
    void on_can_receive(uint32_t can_id, bool is_extended, 
                       uint8_t* payload, uint8_t len);
    
private:
    // ノード抽象基底クラス
    class INode {
    public:
        virtual ~INode() = default;
        virtual void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) = 0;
        virtual void on_service_request(uint16_t transaction_id, uint8_t service_id,
                                       uint8_t* request_data, uint8_t request_len) = 0;
        virtual void configure_rx_mapping(uint16_t address, uint16_t can_id,
                                         uint8_t offset, uint8_t length, bool is_completion) = 0;
        virtual void configure_tx_mapping(uint16_t address, uint16_t can_id,
                                         uint8_t offset, uint8_t length, bool is_completion) = 0;
    };
    
    // 登録されたノード
    INode* nodes_[MAX_NODES];
    uint8_t node_count_ = 0;
    
    // デバイスID（Discoveryで取得）
    uint8_t device_id_ = 0;
    
    // CAN送信関数（ユーザーが実装）
    void (*can_send_fn_)(uint32_t can_id, bool is_extended, uint8_t* data, uint8_t len) = nullptr;
    
    // Configuration処理
    void handle_configuration(uint8_t node_id, uint8_t* payload, uint8_t len);
    
    // Fast Path (Standard ID) のディスパッチ
    void dispatch_standard_id(uint16_t can_id, uint8_t* payload, uint8_t len);
    
    // Slow Path (Extended ID) のディスパッチ
    void dispatch_extended_id(uint32_t can_id, uint8_t* payload, uint8_t len);
};
```

### 13.3. ノード登録とNodeID割り当て

```cpp
template<typename NodeT>
bool FibrilDevice::register_node(NodeT* node) {
    if (node_count_ >= MAX_NODES) {
        return false;  // 最大数超過
    }
    
    // ノードを登録（NodeID = 呼び出し順）
    nodes_[node_count_] = static_cast<INode*>(node);
    node_count_++;
    
    return true;
}
```

**NodeID決定ルール**:

- 最初に登録されたノード: NodeID = 0
- 2番目に登録されたノード: NodeID = 1
- 以降同様

### 13.4. CAN受信とディスパッチ

```cpp
void FibrilDevice::on_can_receive(uint32_t can_id, bool is_extended,
                                  uint8_t* payload, uint8_t len) {
    if (is_extended) {
        // Slow Path: Extended ID
        dispatch_extended_id(can_id, payload, len);
    } else {
        // Fast Path: Standard ID
        dispatch_standard_id(can_id & 0x7FF, payload, len);
    }
}

void FibrilDevice::dispatch_standard_id(uint16_t can_id, uint8_t* payload, uint8_t len) {
    // すべてのノードに対してIDマッピングを試行
    for (uint8_t i = 0; i < node_count_; i++) {
        nodes_[i]->on_can_receive(can_id, payload, len);
        // 各ノードは自分のIDマップに一致する場合のみ処理
    }
}

void FibrilDevice::dispatch_extended_id(uint32_t can_id, uint8_t* payload, uint8_t len) {
    // Extended IDビットフィールドを抽出
    uint8_t feature = (can_id >> 24) & 0x1F;
    uint8_t dev_id = (can_id >> 19) & 0x1F;
    uint8_t node_id = (can_id >> 14) & 0x1F;
    uint16_t sub = can_id & 0x3FFF;
    
    // 自デバイス宛でなければ無視
    if (dev_id != device_id_ && dev_id != 0x1F) {
        return;
    }
    
    switch (feature) {
        case 0x02:  // Configuration
            if (node_id < node_count_) {
                handle_configuration(node_id, payload, len);
            }
            break;
            
        case 0x03:  // Service
            if (node_id < node_count_) {
                // FTP Header解析（簡略版）
                uint8_t ftp_header = payload[0];
                uint8_t service_id = payload[1];
                uint8_t* request_data = &payload[2];
                uint8_t request_len = len - 2;
                
                nodes_[node_id]->on_service_request(sub, service_id, 
                                                    request_data, request_len);
            }
            break;
            
        default:
            // 他の機能は未実装
            break;
    }
}
```

### 13.5. Configuration処理

```cpp
void FibrilDevice::handle_configuration(uint8_t node_id, uint8_t* payload, uint8_t len) {
    // IDマップエントリをパース（9 bytes/entry）
    for (uint8_t offset = 0; offset + 9 <= len; offset += 9) {
        uint8_t  target_node_id = payload[offset + 0];
        uint16_t address        = (payload[offset + 1] << 8) | payload[offset + 2];
        uint16_t standard_id    = (payload[offset + 3] << 8) | payload[offset + 4];
        uint8_t  frame_offset   = payload[offset + 5];
        uint8_t  length         = payload[offset + 6];
        bool     is_completion  = payload[offset + 7];
        uint8_t  direction      = payload[offset + 8];
        
        // 対象ノードが範囲内か確認
        if (target_node_id >= node_count_) {
            continue;
        }
        
        // 該当ノードにマッピングを設定
        if (direction == 0) {  // RX
            nodes_[target_node_id]->configure_rx_mapping(address, standard_id,
                                                          frame_offset, length, is_completion);
        } else {  // TX
            nodes_[target_node_id]->configure_tx_mapping(address, standard_id,
                                                          frame_offset, length, is_completion);
        }
    }
}
```

### 13.6. 定周期送信

```cpp
void FibrilDevice::process() {
    // すべてのノードの送信データを集約
    // 同じCAN IDにマップされたデータをパッキング
    
    uint8_t payload[64];
    uint16_t current_id = 0;
    uint8_t payload_len = 0;
    
    for (uint8_t node_idx = 0; node_idx < node_count_; node_idx++) {
        INode* node = nodes_[node_idx];
        
        // 各ノードの送信マッピングを走査
        for (auto& mapping : node->get_tx_mappings()) {
            if (mapping.can_id != current_id && payload_len > 0) {
                // 前のIDのフレームを送信
                can_send_fn_(current_id, false, payload, payload_len);
                payload_len = 0;
            }
            
            current_id = mapping.can_id;
            
            // ノードのdataからコピー
            uint8_t* src = node->get_data_ptr() + mapping.address;
            memcpy(&payload[mapping.offset], src, mapping.length);
            payload_len = max(payload_len, mapping.offset + mapping.length);
        }
    }
    
    // 最後のフレームを送信
    if (payload_len > 0) {
        can_send_fn_(current_id, false, payload, payload_len);
    }
}
```

### 13.7. 完全な使用例

```cpp
#include "fibril_device.hpp"
#include "motor_node.hpp"
#include "sensor_node.hpp"

// CAN送信コールバック（ユーザー実装）
void can_send_callback(uint32_t can_id, bool is_extended, uint8_t* data, uint8_t len) {
    if (is_extended) {
        CAN_SendExtended(can_id, data, len);
    } else {
        CAN_SendStandard(can_id & 0x7FF, data, len);
    }
}

int main() {
    // ノードインスタンス
    MotorNode motor1;
    MotorNode motor2;
    SensorNode sensor;
    
    // デバイスインスタンス
    FibrilDevice device;
    
    // CAN送信コールバック設定
    device.set_can_send_callback(can_send_callback);
    
    // ノード登録（この順序でNodeID決定）
    device.register_node(&motor1);  // NodeID = 0
    device.register_node(&motor2);  // NodeID = 1
    device.register_node(&sensor);  // NodeID = 2
    
    // 初期化
    device.init();
    
    // メインループ
    while (1) {
        // 定周期処理（pub送信など）
        device.process();
        
        // CAN受信（割り込みまたはポーリング）
        if (CAN_HasMessage()) {
            uint32_t can_id;
            bool is_extended;
            uint8_t data[64];
            uint8_t len;
            
            CAN_Receive(&can_id, &is_extended, data, &len);
            device.on_can_receive(can_id, is_extended, data, len);
        }
        
        // 他の処理...
        delay_ms(10);
    }
    
    return 0;
}
```

### 13.8. ノード基底クラスの実装

生成される各ノードクラスは`INode`インターフェースを実装します：

```cpp
// motor_node.hpp (自動生成)
class MotorNode : public FibrilDevice::INode {
public:
    // データ構造
    struct __attribute__((packed)) {
        float cmd_velocity;
        float current_velocity;
    } data;
    
    // IDマッピング
    IDMapping rx_map[MAX_RX_MAPPINGS];
    IDMapping tx_map[MAX_TX_MAPPINGS];
    uint8_t rx_mapping_count = 0;
    uint8_t tx_mapping_count = 0;
    
    // INodeインターフェース実装
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) override {
        // セクション3.3の実装
    }
    
    void on_service_request(uint16_t transaction_id, uint8_t service_id,
                           uint8_t* request_data, uint8_t request_len) override {
        // セクション12.11の実装
    }
    
    void configure_rx_mapping(uint16_t address, uint16_t can_id,
                             uint8_t offset, uint8_t length, bool is_completion) override {
        if (rx_mapping_count < MAX_RX_MAPPINGS) {
            rx_map[rx_mapping_count].address = address;
            rx_map[rx_mapping_count].can_id = can_id;
            rx_map[rx_mapping_count].offset = offset;
            rx_map[rx_mapping_count].length = length;
            rx_map[rx_mapping_count].is_completion = is_completion;
            rx_mapping_count++;
        }
    }
    
    void configure_tx_mapping(uint16_t address, uint16_t can_id,
                             uint8_t offset, uint8_t length, bool is_completion) override {
        // 同様の実装
    }
    
    uint8_t* get_data_ptr() override {
        return (uint8_t*)&data;
    }
    
    const std::vector<IDMapping>& get_tx_mappings() const override {
        return std::vector<IDMapping>(tx_map, tx_map + tx_mapping_count);
    }
};
```

---

## 11. まとめ

本仕様により、以下を達成します：

| 項目 | 達成内容 |
|------|---------|
| **マイコン負荷** | 最小（1回のmemcpyのみ） |
| **メモリ使用量** | 最小（バッファ不要） |
| **柔軟性** | ノードごとの独立した生成で自由な組み合わせ |
| **拡張性** | 単一/複数フレームを統一的に扱う |
| **開発効率** | 自動計算で手動設定不要 |
| **保守性** | シンプルな実装で理解しやすい |

この仕様に基づいてコード生成ツール`fibril_gen`を実装することで、効率的で保守性の高いマイコンコードを自動生成できます。
