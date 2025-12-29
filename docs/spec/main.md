# fibril_can_transport Specification (v2.0)

## 1. 概要

**fibril_can_transport** は、ROS 2ノード（Master）とCAN-FD接続された組み込みデバイス（Device）間の通信を抽象化するミドルウェアプロトコルです。
ProtobufライクなDSL（定義ファイル）を用いることで、ファームウェアコードの自動生成、通信フレームの最適化、ROS 2インターフェースの動的構築を一気通貫で行います。

## 2. システム構成

- 基本的にスペックが高いMaster（PC等）
- マイコンからなるDevice群（メモリ少・リアルタイム性）

## 3. 通信プロトコル (Hybrid ID Architecture)

通信の性質に応じて、CAN IDのフォーマットと優先度を使い分ける**ハイブリッド方式**を採用します。

| Path | CAN ID Format | 用途 | 特徴 |
| --- | --- | --- | --- |
| **Fast Path** | **Standard (11-bit)** | Realtime Control (Sub)<br>Realtime Notify (Pub) | **動的マッピング**。ID空間の競合を避けるため、Masterが実行時にIDを割り当てる。オーバーヘッドが最小。 |
| **Slow Path** | **Extended (29-bit)** | Discovery<br>Configuration<br>Parameters (Param) | **固定割り当て**。デバイスIDや機能IDを含む構造化されたIDを使用。確実性を重視。 |

### 3.1.1. Extended ID (29-bit) のビットフィールド

```text
[28:24] 機能種別 (5-bit) - 最大32種類
  - 0x00: Discovery (デバイス列挙)
  - 0x01: Definition Exchange (ノード定義の交換)
  - 0x02: Configuration (IDマップ配布)
  - 0x03: Service (汎用サービス / パラメータ)
  - 0x04: Heartbeat (生存報告)
  - 0x05-0x1F: (予約)
[23:19] DeviceID (5-bit) - 最大32デバイス
  - 0x1F: ブロードキャスト用予約
[18:14] NodeID (5-bit) - 1デバイス最大32ノード
  - 0x1F: デバイス全体を指す場合に使用
[13:0]  サブ機能/シーケンス (14-bit)
  - 機能ごとに用途が異なる（リクエストID、フレーム番号など）
```

### 3.1. 通信モード

- **Command (Fire-and-Forget):** `Master -> Device`。レスポンス不要の高速指令。Standard IDを使用。
- **Notify (Stream):** `Device -> Master`。周期または変化時のデータ送信。Standard IDを使用。
- **Service (Req/Res):** 双方向。設定変更やパラメータ取得。Extended IDを使用。

---

## 4. DSL仕様 (Fibril IDL)

インターフェース定義言語（`.fibril`）により、デバイスの機能とROS上での振る舞いを記述します。

### 4.1. キーワードと役割

- **`sub`**: **Deviceが購読**。ROS側はSubscriberとなり、Deviceへ指令（Command）を送る。
- **`pub`**: **Deviceが発行**。ROS側はPublisherとなり、Deviceからの通知（Notify）を配信する。
- **`param`**: **設定値**。Deviceが保持し、ROSパラメータとしてアクセス可能（Service）。

### 4.2. 主な属性

- `ros`: ROSトピック/パラメータ名（デフォルトは変数名）。
- `ros_type`: マッピングするROSメッセージ型（例: `geometry_msgs/msg/Twist`）。
- `ros_map`: ROSメッセージ内のフィールドパスとの対応付け。
- `unit`: 単位（ドキュメント・可視化用）。

### 4.3. 記述例

```protobuf
syntax = "fibril v2";
package robot.mobility;

// 構造体定義 (FW側のメモリレイアウト)
#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    // ROSの linear.x -> FWの v
    #[ros_map(linear.x), unit("m/s")]
    float v;
    // ROSの angular.z -> FWの w
    #[ros_map(angular.z), unit("rad/s")]
    float w;
}

node MobileBase {
    // --- Command (Sub) ---
    // ROS: /cmd_vel (geometry_msgs/Twist)
    #[ros("/cmd_vel")]
    sub Twist2D target_vel;

    // --- Notify (Pub) ---
    // ROS: ~/voltage (std_msgs/Float32) - Float32 msgはプリミティブ扱い
    #[ros("~/voltage")]
    pub float battery_voltage;

    // --- Parameter (Param) ---
    // ROS: ~/max_velocity
    param float max_velocity = 1.0;
}

```

---

## 5. 複数ノード管理 (Multi-Node Management)

### 5.1. デバイス側のノード管理

1つのデバイスは、複数の `.fibril` ファイルから生成されたノードを搭載できます。各ノードは **NodeID** によって識別されます。

#### 5.1.1. 静的登録と自動採番

デバイスのファームウェアでは、以下のように明示的にノードを登録します：

```cpp
// 各DSLから生成されたノードインスタンス
GamePad node_gamepad;
LEDController node_led;
MobileBase node_base;

// main関数内で登録（この順序でNodeIDが自動採番される）
int main() {
    FibrilDevice device;
    device.register_node(&node_gamepad);  // NodeID = 0
    device.register_node(&node_led);      // NodeID = 1
    device.register_node(&node_base);     // NodeID = 2

    device.init();
    while(1) {
        device.process();
    }
}
```

**重要な点:**

- NodeIDは `register_node()` の**呼び出し順序**によって決定される（0から始まる連番）
- 登録順序はユーザーの責任で制御する
- DSL内にNodeIDは記述されない（同じDSLから複数インスタンスを作成可能）

#### 5.1.2. Definition Binary (definition.bin)

各 `.fibril` ファイルから、`fibril_gen` ツールによって以下が生成されます：

- `node_xxx.hpp`: C++コード（ノードクラス定義）
- `node_xxx_def.bin`: ノード定義バイナリ（埋め込みデータ）

**definition.binの役割:**

- Masterがノードの構造を理解するためのメタデータ
- チェックサム計算の元データ（`definition.bin` 全体のハッシュがノード定義のIDとなる）

### 5.2. チェックサムベースの定義キャッシュ

Masterは、過去に取得した定義を**チェックサム**をキーにしてキャッシュします。

- **チェックサム**: `definition.bin` 全体のSHA256ハッシュ（またはCRC32など）
- **衝突時の扱い**: 同じチェックサム = 同じノード定義なので、通信内容は同一。複数のNodeIDが同じチェックサムを持つことは問題なし。

## 6. データ最適化と動的構成

### 6.1. Auto-Addressing

DSL内の各フィールドには、定義順に自動的に連番の内部アドレスが割り当てられます。ユーザーが手動で `0x10` などを管理する必要はありません。

### 6.2. Auto-Layout & Packing (Virtual Frame)

MasterはDeviceの定義に基づき、複数のデータを1つのCANフレーム（最大64byte）にまとめる**パッキング**を自動計算します。

- **アライメント:** データ型に応じた適切なアライメント（padding）を自動挿入。
- **Sync-Group:** 同期が必要なデータ（例：左右の車輪指令）は同一フレームにパッキングされるようスケジューリング可能。
  - 複数Nodeがある場合、同じpub/subを優先してまとめにいく。

### 6.3. ROS Type Mapping (Duck Typing)

ROSの標準メッセージ型と、FWの軽量な型を自動変換します。

- **Device:** 必要なデータだけを持つ軽量な構造体（例: `float x, y`）。
- **Master:** `ros_map` 属性に従い、ROSメッセージ（例: `Pose`）の該当フィールドへ値を注入/抽出。

---

## 7. ライフサイクル (Startup Sequence)

起動時、Masterは以下のフェーズを順に実行します。すべてExtended IDを使用します。

### 7.1. Discovery (デバイス列挙)

**目的:** バス上の全デバイスを発見する。

1. **Master → Broadcast (DeviceID=0x1F):**
   - Extended ID: `[機能=0x00][DevID=0x1F][NodeID=0x1F][Sub=0x0000]`
   - Payload: 空（Discovery Request）

2. **各Device → Master:**
   - Extended ID: `[機能=0x00][DevID=自分のID][NodeID=0x1F][Sub=0x0000]`
   - Payload: `[ノード数 (1byte)]`

**結果:** Masterは存在する全DeviceIDとそれぞれのノード数を把握。

### 7.2. Definition Exchange (ノード定義の交換)

**目的:** 各デバイスの各ノードの定義を取得する（チェックサムベース）。

#### 7.2.1. チェックサムリスト取得

MasterはDeviceごとに以下を実行：

1. **Master → Device:**
   - Extended ID: `[機能=0x01][DevID=X][NodeID=0x1F][Sub=0x0000]`
   - Payload: 空（ノード定義リスト要求）

2. **Device → Master（複数フレーム）:**
   各ノードについて以下を送信：
   - Extended ID: `[機能=0x01][DevID=X][NodeID=0x1F][Sub=NodeIndex]`
   - Payload: `[NodeID (1byte)][Checksum (4byte)]`

**例:** DeviceID=1が3ノードを持つ場合

```text
Frame 1: ExtID=[0x01][0x01][0x1F][0x0000], Data=[0x00][0xAABBCCDD]  // Node 0
Frame 2: ExtID=[0x01][0x01][0x1F][0x0001], Data=[0x01][0x11223344]  // Node 1
Frame 3: ExtID=[0x01][0x01][0x1F][0x0002], Data=[0x02][0xAABBCCDD]  // Node 2 (同じチェックサム=同じ定義)
```

#### 7.2.2. 未知の定義バイナリ取得

Masterは未知のチェックサムについて、定義バイナリを要求：

1. **Master → Device:**
   - Extended ID: `[機能=0x01][DevID=X][NodeID=Y][Sub=0x1000]`
   - Payload: 空（定義バイナリ要求）

2. **Device → Master（複数フレーム）:**
   - Extended ID: `[機能=0x01][DevID=X][NodeID=Y][Sub=フレーム番号]`
   - Payload: definition.binの断片（最大64byte/フレーム）

3. **Master:**
   - 受信したバイナリを結合し、チェックサムを検証
   - 定義を解析してキャッシュに保存

**型の検証:**

- Masterは取得した定義から型情報を抽出
- ルーティング設定時に、Port間の型の構造的一致を確認
- 不一致の場合はエラーを報告（Device側では検証しない）

### 7.3. Layout Calculation (パッキング計算)

**目的:** 各デバイス・各ノードのデータをCANフレームにどう配置するか決定。

Masterは以下を計算：

- **Standard IDの割り当て:** 各Portまたはデータグループに一意なIDを発行
- **フレーム内レイアウト:** 複数のデータを1フレームにパッキングする場合のOffset
- **ルーティング:** device-to-device接続の場合、送信側と受信側のマッピング

### 7.4. Configuration (設定配布)

**目的:** 計算したレイアウト情報をデバイスへ送信。

MasterはDeviceごとに以下を送信：

1. **Master → Device:**
   - Extended ID: `[機能=0x02][DevID=X][NodeID=Y][Sub=シーケンス番号]`
   - Payload: IDマップ情報

**IDマップ情報の例（受信設定）:**

```text
[NodeID (1byte)][Address (1byte)][Standard ID (2byte)][Offset (1byte)][Length (1byte)]
→ "NodeID YのAddress Zのデータは、Standard ID 0x100のOffset 4から8byteで受信せよ"
```

**IDマップ情報の例（送信設定）:**

```text
[NodeID (1byte)][Address (1byte)][Standard ID (2byte)][Offset (1byte)]
→ "NodeID YのAddress Zのデータは、Standard ID 0x200のOffset 0へ送信せよ"
```

複数のPort/Addressがある場合、複数のフレームで送信。

### 7.5. Activation (通信開始)

設定完了後、通常動作に移行：

- **Fast Path (Standard ID):** リアルタイム制御データの送受信
- **Slow Path (Extended ID):** パラメータアクセスおよびサービスコール（機能=0x03）、Heartbeat（機能=0x04）

### 7.6. Heartbeat & Reliability

信頼性を担保するため、以下のメカニズムを導入します。

#### 7.6.1. Heartbeat (Device -> Master)

各デバイスは、自身の状態を定期的にブロードキャストします。

- **Extended ID:** `[機能=0x04][DevID=X][NodeID=0x1F][Sub=0]`
- **Payload:** `[Status (1byte)][Session ID (4bytes)]`
  - `Status`: 0=Init, 1=Active, 2=Error
  - `Session ID`: 起動時にランダム生成される32bit整数

**役割:**
1. **生存確認:** Masterは一定期間Heartbeatが途絶えたデバイスを「ロスト」扱いとします。
2. **リセット検知:** 既知のDeviceIDから異なるSession IDが送られてきた場合、デバイスが再起動したと判断し、初期化シーケンス（Definition Exchange -> Config）を再実行します。
3. **ID衝突検知:** 異なる物理デバイスが同じDeviceID（DIPスイッチ設定ミス等）を持つ場合、異なるSession IDが交互に観測されることでConflictを検知できます。Masterはエラーログを出力し、安全のためそのDeviceIDを無効化します。

#### 7.6.2. Reconnection policy

Masterは、Heartbeatの途絶やSession IDの不整合を検知した場合、即座にそのデバイスへのルーティングを停止し、Discoveryフェーズから再試行します。

#### 7.6.3. Dynamic Reconfiguration

`routing.yaml` の変更などにより構成を変える場合、MasterはいつでもConfigurationフェーズ（機能=0x02）を実行可能です。デバイスは新しいIDマップを受け取り、即座に適用します。

---

## 8. 開発フロー

1. **Define:** ユーザーが `.fibril` ファイルを作成。
2. **Generate:** `fibril_gen` ツールを実行。
    - `device_code.hpp` (C++): MCU用ドライバコード。
    - `definition.bin`: MCUに埋め込むメタデータ。
3. **Build FW:** 生成コードをインクルードしてFWをコンビルド。
4. **Run Master:** PCとMCUを接続し、Masterノードを起動。
    - 設定ファイル不要（Zero-Config）で、DSL通りのトピックが即座に出現。

---

## Appendix A: DeviceIDとNodeID

- **DeviceID**: 各デバイスのハードウェアに依存しており、この開発では具体的な方法は議論しません。
  - 一般にはDIPスイッチなどが考えられ、生成コードのAPIを通じて設定します。
- **NodeID**: デバイス内でのノード識別子。`register_node()` の呼び出し順で自動採番されます。
- DeviceID/NodeIDをそのままROSトピック名に含めることも可能ですが、ユーザーが上書きできます。
  - これはYAML等の設定ファイルによって与えられます。

## Appendix B: definition.bin フォーマット

各ノードの定義バイナリ（`definition.bin`）は以下の構造を持ちます：

### B.1. ヘッダ部

```text
Offset | Size | Field              | Description
-------|------|--------------------|---------------------------------
0x00   | 4    | Magic Number       | "FBRL" (0x4642524C)
0x04   | 2    | Version            | プロトコルバージョン (0x0200 = v2.0)
0x06   | 1    | Node Name Length   | ノード名の長さ (N bytes)
0x07   | N    | Node Name          | ノード名文字列 (例: "MobileBase")
0x07+N | 1    | Port Count         | Portの数 (P個)
...    | ...  | Port Entries       | 各Portの定義（以下参照）
...    | ...  | Metadata           | ノード全体のメタデータ（ros_map, descriptionなど）
```

### B.2. Port Entry (× Port Count)

各Portについて以下の情報が続きます：

```text
Offset | Size | Field              | Description
-------|------|--------------------|---------------------------------
0x00   | 1    | Port Name Length   | Port名の長さ (M bytes)
0x01   | M    | Port Name          | Port名文字列 (例: "target_vel")
0x01+M | 1    | Direction          | 0=sub, 1=pub, 2=param
0x02+M | 2    | Type Info Length   | 型情報部のサイズ (T bytes)
0x04+M | T    | Type Info          | 型定義（構造体情報）
...    | ...  | Metadata           | ros_map, unit, default値など
```

### B.3. 型情報 (Type Info)

構造体の場合：

```text
Offset | Size | Field              | Description
-------|------|--------------------|---------------------------------
0x00   | 1    | Type Kind          | 0=primitive, 1=struct
0x01   | 1    | Struct Name Length | 構造体名の長さ (S bytes)
0x02   | S    | Struct Name        | 構造体名 (例: "Twist2D")
0x02+S | 1    | Field Count        | フィールド数 (F個)
```

各フィールド：

```text
Offset | Size | Field              | Description
-------|------|--------------------|---------------------------------
0x00   | 1    | Field Name Length  | フィールド名の長さ (L bytes)
0x01   | L    | Field Name         | フィールド名 (例: "v")
0x01+L | 1    | Primitive Type     | 型ID (0=float, 1=int32, 2=uint32, ...)
0x02+L | 2    | Array Size         | 配列長 (0=非配列, >0=固定長配列)
```

366: プリミティブ型の場合：
367: 
368: ```text
369: Offset | Size | Field              | Description
370: -------|------|--------------------|---------------------------------
371: 0x00   | 1    | Type Kind          | 0=primitive
372: 0x01   | 1    | Primitive Type     | 型ID (下表参照)
373: ```
374:
375: **Primitive Type IDs:**
376:
377: | ID | Type | Size |
378: |---|---|---|
379: | 0x00 | bool | 1 |
380: | 0x01 | int8 | 1 |
381: | 0x02 | uint8 | 1 |
382: | 0x03 | int16 | 2 |
383: | 0x04 | uint16 | 2 |
384: | 0x05 | int32 | 4 |
385: | 0x06 | uint32 | 4 |
386: | 0x07 | int64 | 8 |
387: | 0x08 | uint64 | 8 |
388: | 0x09 | float | 4 |
389: | 0x0A | double | 8 |
390: | 0x0B-0xFF | (Reserved) | - |

### B.4. メタデータ (Metadata)

可変長のKey-Value形式：

```text
Offset | Size | Field              | Description
-------|------|--------------------|---------------------------------
0x00   | 1    | Metadata Count     | メタデータ項目数 (K個)
```

各メタデータ項目：

```text
Offset | Size | Field              | Description
-------|------|--------------------|---------------------------------
0x00   | 1    | Key Length         | キーの長さ
0x01   | K    | Key                | キー文字列 (例: "ros_map", "unit")
0x01+K | 2    | Value Length       | 値の長さ
0x03+K | V    | Value              | 値（文字列または数値）
```

### B.5. チェックサム計算

- definition.bin全体（ヘッダから最後のメタデータまで）のCRC32またはSHA256ハッシュ
- ファイル末尾には含まれず、Masterで別途計算される
