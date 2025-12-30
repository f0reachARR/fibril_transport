# マイコン最適化仕様: Packingとアドレス設計

## 1. 概要

本ドキュメントでは、マイコン側の処理負荷を最小化することを目的とした、PackingとAddressingの仕様案を提示します。

## 2. 設計方針

### 2.1. 基本原則

マイコン処理を削減するための主要な原則：

1. **Zero-Copy受信**: 受信したCANフレームのペイロードを、マイコンのメモリ上に直接配置
2. **Direct Memory Access**: アドレス計算や再配置処理を不要にする
3. **Master-Driven Complexity**: 複雑な処理はすべてMaster側で実行
4. **Static Layout**: 実行時の動的な処理を排除し、コンパイル時に確定

---

## 3. 提案仕様

### 提案A: メモリマップ直接配置方式（最も単純）

#### 3.1. 概要

**コンセプト**: CANフレームのペイロードとマイコンのメモリレイアウトを完全に一致させる

#### 3.2. 動作原理

```cpp
// DSL定義
struct MotorCommand {
    float velocity;     // 4 bytes
    float torque;       // 4 bytes
    uint16 mode;        // 2 bytes
    // Total: 10 bytes
}

node Motor {
    sub MotorCommand cmd;
    pub float current_velocity;
    pub float current_torque;
}
```

**マイコン側の生成コード**:

```cpp
class Motor {
    // すべてのpub/subデータを連続したメモリ領域に配置
    struct __attribute__((packed)) {
        // Sub ports (受信データ)
        MotorCommand cmd;           // Address 0x00-0x09
        
        // Pub ports (送信データ)
        float current_velocity;     // Address 0x0A-0x0D
        float current_torque;       // Address 0x0E-0x11
    } data;
    
    // CANフレーム受信時のコールバック（Master側で設定されたマッピングに基づく）
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
        // アドレス0x00のcmdを受信する場合
        memcpy(&data.cmd, payload, len);  // そのままコピー
    }
    
    // 送信時
    void publish_velocity() {
        uint8_t* ptr = (uint8_t*)&data.current_velocity;
        can_send(assigned_id, ptr, 4);  // そのまま送信
    }
};
```

**Master側の処理**:

```yaml
# Configuration送信内容
- NodeID: 0
  Address: 0x00      # data.cmd の開始アドレス
  StandardID: 0x100
  Offset: 0          # CANフレーム内のオフセット
  Length: 10         # MotorCommandのサイズ
  Direction: RX      # 受信
  
- NodeID: 0
  Address: 0x0A      # data.current_velocity の開始アドレス
  StandardID: 0x200
  Offset: 0
  Length: 4
  Direction: TX      # 送信
```

#### 3.3. メリット

- ✅ **Zero-Copy**: `memcpy`のみで受信完了（アライメント処理不要）
- ✅ **最小CPU負荷**: アドレス計算なし、再配置なし
- ✅ **予測可能な性能**: 固定時間で処理完了
- ✅ **デバッグ容易**: メモリダンプで状態確認が簡単

#### 3.4. デメリット

- ⚠️ Masterが複雑なパッキング計算を担当
- ⚠️ アライメント制約がある場合、パディングが増える可能性

---

### 提案B: 最大ID完了判定方式（複数フレーム対応）

#### 3.1. 概要

**コンセプト**: 複数フレームに分割されたデータを**直接メモリに書き込み**、**最大IDのフレームを受信した時点で完了**とする

#### 3.2. 設計原則

従来のバッファベース方式（全フレーム受信を待つ）ではなく、以下の原則で簡素化：

1. **直接書き込み**: 各フレームを受信次第、対象メモリアドレスに即座に書き込む
2. **順序保証**: CANは基本的に送信順で到着するため、最後のフレーム（最大ID）が到着すれば全体が揃っている
3. **最大ID完了判定**: 最大IDを持つフレームの受信で完了とみなし、コールバックを呼ぶ

#### 3.2. 動作原理

```cpp
class Motor {
    // ユーザーアクセス用データ（構造体定義通り）
    struct __attribute__((packed)) {
        MotorCommand cmd;           // 64byteを超える場合に複数フレームで受信
        float current_velocity;
        float current_torque;
    } data;
    
    // IDマッピング情報（Masterから設定）
    struct IDMapping {
        uint16_t can_id;
        uint16_t address;       // data構造体内のオフセット
        uint8_t offset;         // CANフレーム内のオフセット
        uint8_t length;
        bool is_completion;     // 最大IDフラグ（これを受信したら完了）
    };
    
    IDMapping rx_map[MAX_RX_MAPPINGS];
    
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
        // IDマップから該当エントリを検索
        for (uint8_t i = 0; i < rx_mapping_count; i++) {
            if (rx_map[i].can_id == can_id) {
                // 直接メモリへコピー（提案Aと同じ）
                uint8_t* dest = (uint8_t*)&data + rx_map[i].address;
                memcpy(dest, payload + rx_map[i].offset, rx_map[i].length);
                
                // 最大IDフレームなら完了判定
                if (rx_map[i].is_completion) {
                    on_cmd_received();  // コールバック呼び出し
                }
                return;
            }
        }
    }
};
```

**Master側の処理**:

```yaml
# 例: MotorCommandが70byteで、2フレームに分割される場合
# CAN ID 0x100: 最初の64byte
# CAN ID 0x101: 残りの6byte（最大ID）

- NodeID: 0
  Address: 0x00      # data.cmd の開始アドレス
  StandardID: 0x100
  Offset: 0          # CANフレーム内オフセット
  Length: 64
  IsCompletion: false  # まだ完了ではない

- NodeID: 0
  Address: 0x40      # data.cmd + 64byte (0x00 + 64)
  StandardID: 0x101
  Offset: 0
  Length: 6
  IsCompletion: true   # 最大ID → これで完了
```

#### 3.3. メリット

- ✅ **提案Aと同等の単純さ**: 直接メモリ書き込み、バッファ不要
- ✅ **最小メモリ**: 追加バッファが不要
- ✅ **最小CPU負荷**: 完了判定が1つのフラグチェックのみ
- ✅ **低レイテンシ**: 最後のフレーム受信と同時にコールバック発火
- ✅ **64byte超のデータに対応**: 単一フレームと同じ実装で大きなデータも扱える

#### 3.4. 前提条件

- ⚠️ **順序保証**: CANは優先度順で送信されるため、通常は順序が保たれる（同じ優先度なら送信順）
- ⚠️ **パケットロス**: 最大IDより前のフレームが欠落しても検出できない（高信頼性が必要ならCRCを追加）

#### 3.5. 順序保証の戦略

CANフレームの順序を保証するため、以下の戦略を適用：

1. **連続IDの割り当て**: 分割フレームには連続したStandard IDを割り当て（例: 0x100, 0x101, 0x102）
2. **優先度の統一**: 同じデータの分割フレームは同じ優先度（Standard IDの上位ビット）
3. **送信間隔の最小化**: Master側で分割フレームをバースト送信

これにより、99%以上のケースで順序が保たれます。

---

### 提案C: 統合方式（推奨）

#### 3.1. 概要

**コンセプト**: 提案Aと提案Bを統合し、サイズに関わらず**同じ実装**で対応

#### 3.2. 統合の理由

提案Bが最大ID完了判定を採用したことで、提案Aと提案Bの実装が実質的に同一になりました：

- **単一フレーム（≤64byte）**: 1つのIDマッピングで完了
- **複数フレーム（>64byte）**: 複数のIDマッピング、最大IDで完了判定

どちらも**直接メモリ書き込み + IsCompletionフラグ**で実現できるため、コード生成時にサイズで分岐する必要がありません。

#### 3.3. 統合実装

```cpp
class Motor {
    // すべてのデータを同じ構造体に配置
    struct __attribute__((packed)) {
        MotorCommand cmd;           // 70byte（複数フレーム）
        float current_velocity;     // 4byte（単一フレーム）
        float current_torque;       // 4byte（単一フレーム）
    } data;
    
    // すべて同じIDマッピング構造
    struct IDMapping {
        uint16_t can_id;
        uint16_t address;
        uint8_t offset;
        uint8_t length;
        bool is_completion;     // 単一なら常にtrue、複数なら最大IDのみtrue
    };
    
    IDMapping rx_map[MAX_RX_MAPPINGS];
    
    // 統一された受信処理
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
        for (uint8_t i = 0; i < rx_mapping_count; i++) {
            if (rx_map[i].can_id == can_id) {
                uint8_t* dest = (uint8_t*)&data + rx_map[i].address;
                memcpy(dest, payload + rx_map[i].offset, rx_map[i].length);
                
                if (rx_map[i].is_completion) {
                    on_data_received(rx_map[i].address);  // アドレスに応じたコールバック
                }
                return;
            }
        }
    }
};
```

#### 3.4. メリット

- ✅ **実装の単純化**: 単一/複数フレームで同じコード、同じデータ構造
- ✅ **最小CPU負荷**: 提案Aと同じ処理（直接メモリ書き込み）
- ✅ **最小メモリ**: バッファ不要（提案Aと同等）
- ✅ **柔軟性**: データサイズに関わらず同じ仕組みで対応
- ✅ **コード生成の簡素化**: 分岐処理が不要

---

## 4. アドレス設計の詳細

### 4.1. Auto-Addressing（自動アドレス割り当て）

DSL定義順に、各フィールドに連続したアドレスを割り当てます。

```protobuf
node Motor {
    sub float cmd_velocity;      // Address 0x00-0x03
    sub float cmd_torque;        // Address 0x04-0x07
    pub float current_velocity;  // Address 0x08-0x0B
    pub float current_torque;    // Address 0x0C-0x0F
}
```

**生成されるメモリマップ**:

```cpp
struct __attribute__((packed)) MotorData {
    // Sub section (RX)
    float cmd_velocity;      // 0x00
    float cmd_torque;        // 0x04
    
    // Pub section (TX)
    float current_velocity;  // 0x08
    float current_torque;    // 0x0C
};
```

### 4.2. アライメント処理

**方針**: Masterがアライメントを考慮してパッキング

- **Option 1: Packed構造体** - `__attribute__((packed))` でアライメントを無効化（提案A/C）
- **Option 2: Natural Alignment** - 型サイズに応じて自動パディング挿入

```cpp
// Option 2の例
struct MotorData {
    float cmd_velocity;      // 0x00-0x03 (4-byte aligned)
    uint8_t mode;            // 0x04
    // [padding 3 bytes]     // 0x05-0x07
    float cmd_torque;        // 0x08-0x0B (4-byte aligned)
};
```

Masterは、この構造を解析してCANフレームの配置を決定します。

### 4.3. Struct Packing（構造体のグループ化）

pub/subで同じ構造体を使用する場合、メモリ上で連続配置：

```protobuf
struct Twist2D {
    float v;
    float w;
}

node Base {
    sub Twist2D cmd;         // Address 0x00-0x07
    pub Twist2D odom;        // Address 0x08-0x0F
}
```

---

## 5. Configuration Protocol の詳細

### 5.1. IDマップ送信フォーマット（統一方式）

```binary
[NodeID (1)] [Address (2)] [StandardID (2)] [Offset (1)] [Length (1)] [IsCompletion (1)] [Direction (1)]
```

- **NodeID**: 対象ノードID
- **Address**: マイコン側のメモリアドレス（構造体内オフセット）
- **StandardID**: 使用するCAN ID
- **Offset**: CANフレーム内のデータ開始位置
- **Length**: データサイズ
- **IsCompletion**: 完了判定フラグ（1=このフレーム受信で完了、0=まだ続きがある）
- **Direction**: 0=RX, 1=TX

#### 5.1.1. 単一フレームの例

```
# 単一フレームの場合、IsCompletion=1
NodeID=0, Address=0x08, CANID=0x200, Offset=0, Length=4, IsCompletion=1, Direction=RX
```

#### 5.1.2. 複数フレームの例

```
# 70byteのデータを2フレームに分割
# フレーム1: 最初の64byte (IsCompletion=0)
NodeID=0, Address=0x00, CANID=0x100, Offset=0, Length=64, IsCompletion=0, Direction=RX

# フレーム2: 残りの6byte (IsCompletion=1) ← 最大ID、これで完了
NodeID=0, Address=0x40, CANID=0x101, Offset=0, Length=6, IsCompletion=1, Direction=RX
```

マイコンは `IsCompletion=1` のフレームを受信した時点で、対応するコールバックを呼び出します。

#### 5.1.3. クロスノードパッキングの例

**重要**: 同じStandardIDに複数のNodeIDが紐づくことで、**クロスノードパッキング**を実現します。

**例: 3つのノードから1つのCANフレームを構築**

```
# 同じCAN ID 0x100に、異なるNodeIDのデータをマッピング
# すべてIsCompletion=1（それぞれ単一フレームなので）
NodeID=0, Address=0x04, CANID=0x100, Offset=0, Length=4, IsCompletion=1, Direction=TX
NodeID=1, Address=0x04, CANID=0x100, Offset=4, Length=4, IsCompletion=1, Direction=TX
NodeID=2, Address=0x04, CANID=0x100, Offset=8, Length=4, IsCompletion=1, Direction=TX

→ CAN ID 0x100フレーム: [Node0データ][Node1データ][Node2データ]
```

マイコン側では、各ノードが独立してメモリアドレスを持ちますが、送信時にMasterが指定したOffsetに従って1つのCANフレームに集約されます。

### 5.2. ノードごとのコード生成

**重要な設計方針**: コード生成は**ノードごと**に行われ、利用者は任意の数のノードを組み合わせて使用できます。

#### 5.2.1. 生成単位

```bash
# 各.fibrilファイルから、1つのノードクラスを生成
fibril_gen motor.fibril → motor_node.hpp
fibril_gen sensor.fibril → sensor_node.hpp
fibril_gen controller.fibril → controller_node.hpp
```

#### 5.2.2. 利用者側での組み合わせ

```cpp
#include "motor_node.hpp"
#include "sensor_node.hpp"
#include "controller_node.hpp"

int main() {
    // 任意の数のノードをインスタンス化
    MotorNode motor1;  // NodeID=0
    MotorNode motor2;  // NodeID=1 (同じクラスを2回使用可能)
    SensorNode sensor; // NodeID=2
    ControllerNode ctrl; // NodeID=3
    
    // FibrilDeviceに登録（この順序でNodeIDが決定）
    FibrilDevice device;
    device.register_node(&motor1);   // NodeID = 0
    device.register_node(&motor2);   // NodeID = 1
    device.register_node(&sensor);   // NodeID = 2
    device.register_node(&ctrl);     // NodeID = 3
    
    device.init();
    while(1) {
        device.process();
    }
}
```

#### 5.2.3. 生成コードの構造

各ノードクラスは独立して動作し、以下の特徴を持ちます：

```cpp
// motor_node.hpp (生成例)
class MotorNode {
public:
    // データ構造（独立したアドレス空間）
    struct __attribute__((packed)) {
        float cmd;      // Address 0x00 (このノード内)
        float pos;      // Address 0x04
    } data;
    
    // IDマッピング（Masterから設定される）
    struct IDMapping {
        uint16_t can_id;
        uint16_t address;
        uint8_t offset;
        uint8_t length;
        bool is_completion;
    };
    
    IDMapping rx_map[MAX_RX_MAPPINGS];
    IDMapping tx_map[MAX_TX_MAPPINGS];
    
    // CAN受信処理（FibrilDeviceから呼ばれる）
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len);
    
    // ユーザーコールバック（オーバーライド可能）
    virtual void on_cmd_received() {}
};
```

**重要**: 各ノードは0x00から始まる**独立したアドレス空間**を持ちます。NodeIDによって区別されるため、同じAddressが複数ノードで使われても問題ありません。

### 5.3. リソースサイジング（配列サイズの決定）

#### 5.3.1. MAX_RX_MAPPINGS / MAX_TX_MAPPINGSの計算

各ノードで必要なIDマッピング配列のサイズは、コード生成時に自動計算されます。

**基本的な計算式**:

```cpp
// 受信マッピング数
MAX_RX_MAPPINGS = Σ(各subポートの分割フレーム数)

// 送信マッピング数  
MAX_TX_MAPPINGS = Σ(各pubポートの分割フレーム数)
```

**分割フレーム数の計算**:

```python
def calc_frame_count(data_size, max_frame_size=64):
    """1つのポートに必要なフレーム数を計算"""
    return (data_size + max_frame_size - 1) // max_frame_size

# 例:
# 4byte (float) → 1フレーム
# 70byte (大きい構造体) → 2フレーム (64 + 6)
# 200byte → 4フレーム (64 + 64 + 64 + 8)
```

#### 5.3.2. 具体例

**例1: シンプルなセンサノード**

```protobuf
node BatterySensor {
    pub float voltage;   // 4 bytes → 1フレーム
    pub float current;   // 4 bytes → 1フレーム
}
```

**生成されるコード**:

```cpp
class BatterySensor {
    // 受信なし
    static constexpr uint8_t MAX_RX_MAPPINGS = 0;
    
    // 送信: voltage(1) + current(1) = 2
    static constexpr uint8_t MAX_TX_MAPPINGS = 2;
    
    IDMapping tx_map[MAX_TX_MAPPINGS];  // 8 bytes (4 bytes × 2)
};
```

**例2: 複雑なモーターコントローラ**

```protobuf
struct LargeCommand {
    float position[20];  // 80 bytes → 2フレーム
    float velocity[20];  // 80 bytes → 2フレーム
    // Total: 160 bytes → 3フレーム (64 + 64 + 32)
}

node MotorController {
    sub LargeCommand cmd;        // 160 bytes → 3フレーム
    sub float emergency_stop;    // 4 bytes → 1フレーム
    
    pub float current_position;  // 4 bytes → 1フレーム
    pub float current_velocity;  // 4 bytes → 1フレーム
}
```

**生成されるコード**:

```cpp
class MotorController {
    // 受信: cmd(3) + emergency_stop(1) = 4
    static constexpr uint8_t MAX_RX_MAPPINGS = 4;
    
    // 送信: current_position(1) + current_velocity(1) = 2
    static constexpr uint8_t MAX_TX_MAPPINGS = 2;
    
    IDMapping rx_map[MAX_RX_MAPPINGS];  // 16 bytes (4 bytes × 4)
    IDMapping tx_map[MAX_TX_MAPPINGS];  // 8 bytes (4 bytes × 2)
};
```

#### 5.3.3. メモリ使用量の見積もり

**IDMapping構造体のサイズ**:

```cpp
struct IDMapping {
    uint16_t can_id;        // 2 bytes
    uint16_t address;       // 2 bytes
    uint8_t offset;         // 1 byte
    uint8_t length;         // 1 byte
    bool is_completion;     // 1 byte
    // [padding 1 byte]     // アライメント用
};  // Total: 8 bytes
```

**ノード全体のメモリ使用量**:

```
Total = データ構造 + IDマッピング配列

データ構造 = Σ(各ポートのサイズ)
IDマッピング = (MAX_RX_MAPPINGS + MAX_TX_MAPPINGS) × 8 bytes
```

**例: MotorControllerの場合**

```
データ構造:
  - LargeCommand cmd: 160 bytes
  - float emergency_stop: 4 bytes
  - float current_position: 4 bytes
  - float current_velocity: 4 bytes
  Total: 172 bytes

IDマッピング:
  - rx_map[4]: 32 bytes
  - tx_map[2]: 16 bytes
  Total: 48 bytes

合計: 220 bytes / ノード
```

#### 5.3.4. 最適化戦略

**戦略1: データサイズの制限**

64byteを超えるデータは複数フレームになるため、IDマッピングが増加します。可能な限り64byte以内に収めることを推奨：

```protobuf
// ❌ 非推奨: 大きすぎる
struct HugeData {
    float data[100];  // 400 bytes → 7フレーム
}

// ✅ 推奨: 分割して定義
struct ReasonableData {
    float position[10];  // 40 bytes → 1フレーム
    float velocity[10];  // 40 bytes → 1フレーム
}
```

**戦略2: 動的配列の使用（上級）**

メモリが非常に限られている場合、最大値の代わりに動的配列を使用：

```cpp
// コード生成オプションで有効化
class MotorController {
    std::vector<IDMapping> rx_map;  // 動的サイズ
    std::vector<IDMapping> tx_map;
};
```

ただし、組み込みでは`std::vector`が使えない場合があるため、通常は固定配列を推奨します。

**戦略3: 共用体の活用（将来的な拡張）**

受信と送信が同時に発生しない場合、共用体で省メモリ化：

```cpp
union {
    IDMapping rx_map[MAX_RX_MAPPINGS];
    IDMapping tx_map[MAX_TX_MAPPINGS];
} mapping_storage;
```

#### 5.3.5. コード生成時の自動決定

`fibril_gen`ツールは以下の処理を自動実行：

1. **DSL解析**: 各ポートのデータサイズを計算
2. **フレーム分割判定**: 64byte基準で分割数を決定
3. **定数生成**: `MAX_RX_MAPPINGS`と`MAX_TX_MAPPINGS`を算出
4. **ヘッダ出力**: コンパイル時定数として埋め込み

**生成例**:

```cpp
// motor_controller_node.hpp (自動生成)
class MotorController {
public:
    // コード生成ツールが自動算出した定数
    static constexpr uint8_t MAX_RX_MAPPINGS = 4;  // cmd(3) + emergency(1)
    static constexpr uint8_t MAX_TX_MAPPINGS = 2;  // position(1) + velocity(1)
    
    // メモリ使用量のドキュメント（コメントとして出力）
    // Estimated memory usage:
    //   - Data structure: 172 bytes
    //   - ID mappings: 48 bytes (6 mappings × 8 bytes)
    //   - Total: 220 bytes
    
    IDMapping rx_map[MAX_RX_MAPPINGS];
    IDMapping tx_map[MAX_TX_MAPPINGS];
};
```

#### 5.3.6. ユーザー定義のオーバーライド（オプション）

特定のケースでメモリを節約したい場合、ユーザーが手動で定義することも可能：

```cpp
// ユーザー定義ヘッダ
#define MOTOR_CONTROLLER_MAX_RX_MAPPINGS 2  // 実際には4だが、一部のポートを無効化

#include "motor_controller_node.hpp"
```

生成コードは、定義済みの場合はそれを使用：

```cpp
#ifndef MOTOR_CONTROLLER_MAX_RX_MAPPINGS
#define MOTOR_CONTROLLER_MAX_RX_MAPPINGS 4  // デフォルト値
#endif
```

#### 5.3.7. 実行時の設定数のトラッキング

実際に設定されたマッピング数は実行時に追跡：

```cpp
class MotorController {
    IDMapping rx_map[MAX_RX_MAPPINGS];
    uint8_t rx_mapping_count = 0;  // 実際に設定された数
    
    IDMapping tx_map[MAX_TX_MAPPINGS];
    uint8_t tx_mapping_count = 0;
    
    void configure_rx_mapping(IDMapping& mapping) {
        if (rx_mapping_count < MAX_RX_MAPPINGS) {
            rx_map[rx_mapping_count++] = mapping;
        } else {
            // エラー処理
        }
    }
};
```

これにより、Master側のパッキング最適化で実際のマッピング数が減る場合でも、無駄なメモリアクセスを回避できます。

---

## 6. 性能比較

| 方式 | 受信処理時間 | メモリ使用量 | 複数フレーム対応 | 推奨ケース |
|------|------------|------------|----------------|-----------|
| 提案A | **最速** (1 memcpy) | **最小** (Zero buffer) | ❌ 単一フレームのみ | ≤64byte専用 |
| 提案B | **最速** (1 memcpy + フラグチェック) | **最小** (Zero buffer) | ✅ 最大ID完了判定 | \>64byteに対応 |
| 提案C | **最速** (統合実装) | **最小** (Zero buffer) | ✅ サイズ自動対応 | **推奨: すべてのケース** |

**結論**: 提案Cは提案AとBを統合したもので、実装は実質的に同一です。データサイズに関わらず、同じコードで最高のパフォーマンスを実現します。

---

## 7. コード生成例（提案C）

### 7.1. DSL入力

```protobuf
syntax = "fibril v2";
package robot;

struct Twist2D {
    float v;
    float w;
}

node MobileBase {
    sub Twist2D cmd;           // 8 bytes -> 直接配置
    pub float voltage;          // 4 bytes -> 直接配置
}
```

### 7.2. 生成されるC++コード

```cpp
class MobileBase {
public:
    // --- Direct Memory Layout ---
    struct __attribute__((packed)) {
        // RX Section
        Twist2D cmd;            // Address 0x00-0x07
        
        // TX Section
        float voltage;          // Address 0x08-0x0B
    } data;
    
    // --- IDマッピングテーブル（Masterから設定される） ---
    struct IDMapping {
        uint16_t can_id;
        uint16_t address;       // data構造体内のオフセット
        uint8_t length;
        bool is_tx;
    };
    
    IDMapping id_map[MAX_MAPPINGS];
    uint8_t mapping_count = 0;
    
    // --- CAN受信ハンドラ ---
    void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
        // IDマップから該当エントリを検索
        for (uint8_t i = 0; i < mapping_count; i++) {
            if (id_map[i].can_id == can_id && !id_map[i].is_tx) {
                // 直接メモリへコピー
                uint8_t* dest = (uint8_t*)&data + id_map[i].address;
                memcpy(dest, payload, len);
                
                // コールバック（オプション）
                if (id_map[i].address == 0x00) {  // cmd受信
                    on_cmd_received();
                }
                return;
            }
        }
    }
    
    // --- 送信API ---
    void publish_voltage() {
        uint8_t* src = (uint8_t*)&data + 0x08;  // voltage address
        
        // IDマップから送信IDを取得
        for (uint8_t i = 0; i < mapping_count; i++) {
            if (id_map[i].address == 0x08 && id_map[i].is_tx) {
                can_send(id_map[i].can_id, src, 4);
                return;
            }
        }
    }
    
    // --- Configuration受信（Masterからのマッピング設定） ---
    void configure_mapping(uint8_t* config_data, uint8_t len) {
        // [Address(2)] [CANID(2)] [Length(1)] [Direction(1)]
        uint8_t idx = 0;
        while (idx < len) {
            IDMapping map;
            map.address = *(uint16_t*)&config_data[idx]; idx += 2;
            map.can_id = *(uint16_t*)&config_data[idx]; idx += 2;
            map.length = config_data[idx++];
            map.is_tx = config_data[idx++];
            
            id_map[mapping_count++] = map;
        }
    }
    
    // --- ユーザーコールバック（オプション） ---
    virtual void on_cmd_received() {
        // ユーザー実装: data.cmdが更新されたときの処理
    }
};
```

---

## 8. 推奨仕様（統合方式）の採用理由

| 項目 | 理由 |
|------|------|
| **マイコン負荷最小** | 直接メモリ書き込み（1回のmemcpy）のみ |
| **最小メモリ** | バッファ不要、追加メモリゼロ |
| **柔軟性** | 単一/複数フレームを同じ実装で対応 |
| **実装容易** | コード生成が機械的かつ単純 |
| **拡張性** | ノードごとのコード生成で柔軟な組み合わせ |
| **完了判定の単純さ** | IsCompletionフラグのみで完了判定 |

---

## 9. 次のステップ

1. **提案Cの詳細設計**: Configuration Protocolの完全定義
2. **コード生成器の実装**: `fibril_gen`への組み込み
3. **パフォーマンステスト**: 実機での処理時間測定
4. **ドキュメント統合**: `codegen.md`への反映

---

## Appendix A: メモリレイアウト例

### A.1. 単一ノード、複数Port

```protobuf
node Sensor {
    pub float temperature;   // 4 bytes
    pub float humidity;      // 4 bytes
    pub uint16 pressure;     // 2 bytes
}
```

**生成メモリマップ**:

```
Offset | Size | Field
-------|------|-------------
0x00   | 4    | temperature
0x04   | 4    | humidity
0x08   | 2    | pressure
Total: 10 bytes
```

**CANフレーム配置（Masterが計算）**:

```
CAN ID 0x200: [temperature(4)] [humidity(4)] [pressure(2)]
              └─ Offset 0     └─ Offset 4   └─ Offset 8
```

マイコン側の設定:

```
StandardID: 0x200
Address: 0x00
Offset: 0
Length: 10
Direction: TX
```

---

## Appendix B: 複数ノードの場合

### B.1. 独立したアドレス空間（基本）

```protobuf
node Motor1 {
    sub float cmd;  // Address 0x00
    pub float pos;  // Address 0x04
}

node Motor2 {
    sub float cmd;  // Address 0x00 (Motor2のアドレス空間)
    pub float pos;  // Address 0x04
}
```

各ノードは独立したアドレス空間を持ちます。NodeIDで区別されるため、同じAddressでも問題ありません。

**Configuration（個別CANフレーム）**:

```
NodeID=0, Address=0x00, CANID=0x100 (Motor1.cmd)
NodeID=0, Address=0x04, CANID=0x200 (Motor1.pos)
NodeID=1, Address=0x00, CANID=0x101 (Motor2.cmd)
NodeID=1, Address=0x04, CANID=0x201 (Motor2.pos)
```

### B.2. クロスノードパッキング（効率化）

**ユースケース**: 複数ノードのpub/subデータを1つのCANフレームにまとめることで、以下のメリットが得られます：

1. **CANフレーム数の削減**: バス効率の向上
2. **同期送信**: 同じタイミングで送られることが保証される
3. **タイムスタンプの一致**: 同じ周期で更新されるデータをグループ化

**例: 複数モーターへの同時指令**

```protobuf
node Motor1 {
    sub float cmd;  // 4 bytes
    pub float pos;  // 4 bytes
}

node Motor2 {
    sub float cmd;  // 4 bytes
    pub float pos;  // 4 bytes
}

node Motor3 {
    sub float cmd;  // 4 bytes
    pub float pos;  // 4 bytes
}
```

**従来方式（個別フレーム）**: 6フレーム必要

- Motor1.cmd (CANID=0x100)
- Motor2.cmd (CANID=0x101)
- Motor3.cmd (CANID=0x102)
- Motor1.pos (CANID=0x200)
- Motor2.pos (CANID=0x201)
- Motor3.pos (CANID=0x202)

**クロスノードパッキング方式**: 2フレームで完結

```
# Sub (Command) をまとめる
CAN ID 0x100: [Motor1.cmd(4)] [Motor2.cmd(4)] [Motor3.cmd(4)]
              └─ Offset 0    └─ Offset 4    └─ Offset 8

# Pub (Position) をまとめる
CAN ID 0x200: [Motor1.pos(4)] [Motor2.pos(4)] [Motor3.pos(4)]
              └─ Offset 0    └─ Offset 4    └─ Offset 8
```

**Configuration（クロスノードパッキング）**:

```
# 受信設定（1つのCANフレームから3つのノードへ分配）
NodeID=0, Address=0x00, CANID=0x100, Offset=0, Length=4, Direction=RX
NodeID=1, Address=0x00, CANID=0x100, Offset=4, Length=4, Direction=RX
NodeID=2, Address=0x00, CANID=0x100, Offset=8, Length=4, Direction=RX

# 送信設定（3つのノードから1つのCANフレームへ集約）
NodeID=0, Address=0x04, CANID=0x200, Offset=0, Length=4, Direction=TX
NodeID=1, Address=0x04, CANID=0x200, Offset=4, Length=4, Direction=TX
NodeID=2, Address=0x04, CANID=0x200, Offset=8, Length=4, Direction=TX
```

### B.3. マイコン側の実装（クロスノードパッキング対応）

```cpp
// 各ノードのインスタンス
Motor1 node_motor1;
Motor2 node_motor2;
Motor3 node_motor3;

// CAN受信時（1フレームで3ノードを更新）
void on_can_receive(uint16_t can_id, uint8_t* payload, uint8_t len) {
    if (can_id == 0x100) {  // 3モーター指令
        // IDマップに基づいて各ノードへ分配
        memcpy(&node_motor1.data.cmd, &payload[0], 4);  // Offset 0
        memcpy(&node_motor2.data.cmd, &payload[4], 4);  // Offset 4
        memcpy(&node_motor3.data.cmd, &payload[8], 4);  // Offset 8
        
        // 各ノードのコールバック呼び出し
        node_motor1.on_cmd_received();
        node_motor2.on_cmd_received();
        node_motor3.on_cmd_received();
    }
}

// CAN送信時（3ノードから1フレームを構築）
void publish_positions() {
    uint8_t payload[12];
    
    // 各ノードから位置データを集約
    memcpy(&payload[0], &node_motor1.data.pos, 4);  // Offset 0
    memcpy(&payload[4], &node_motor2.data.pos, 4);  // Offset 4
    memcpy(&payload[8], &node_motor3.data.pos, 4);  // Offset 8
    
    // 1フレームで送信
    can_send(0x200, payload, 12);
}
```

### B.4. 自動パッキング戦略（Master側）

Masterは以下の基準でクロスノードパッキングを自動判定します：

1. **同じ送信周期**: `#[periodic]`で同じ周期が設定されているpubデータ
2. **同じ型・同じサイズ**: 合計が64byte以内に収まる
3. **同じデバイス**: 同一DeviceID内のノード間でのみ適用

**自動パッキングの例**:

```protobuf
node Sensor1 {
    #[periodic]  // 100Hz想定
    pub float temperature;
}

node Sensor2 {
    #[periodic]  // 100Hz想定
    pub float humidity;
}

node Sensor3 {
    #[periodic]  // 100Hz想定
    pub float pressure;
}
```

Master側で周期が同じことを検出し、自動的に1つのCANフレームにパッキング：

```
CAN ID 0x300: [temperature(4)] [humidity(4)] [pressure(4)]
```

### B.5. メリットと注意点

**メリット**:

- ✅ **バス効率**: CANフレーム数を最大1/64に削減可能（64byteフル活用）
- ✅ **原子性**: 複数データが同時に更新されることが保証
- ✅ **レイテンシ削減**: フレーム送信回数が減るため、通信遅延が短縮
- ✅ **マイコン処理は変わらず**: 各ノードは独立したアドレス空間を維持

**注意点**:

- ⚠️ **周期の一致**: 異なる周期のデータをまとめると、無駄な送信が発生
- ⚠️ **部分更新不可**: 1つのノードだけ更新したい場合でも全体を送る必要がある
- ⚠️ **デバッグの複雑化**: 1フレームに複数ノードのデータが含まれるため、トレースが難しくなる可能性
