# Fibril Code Generation Specification (マイコン向け)

## 1. 概要

本ドキュメントは、Fibril IDLから**マイコン向けC++コード**を生成する際の仕様を定義します。マイコンの処理負荷とメモリ使用量を最小化することを最優先とし、複雑な処理はすべてMaster側で実行する設計方針を採用しています。

**設計原則**:

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
