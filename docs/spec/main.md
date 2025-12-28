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

## 5. データ最適化と動的構成

### 5.1. Auto-Addressing

DSL内の各フィールドには、定義順に自動的に連番の内部アドレスが割り当てられます。ユーザーが手動で `0x10` などを管理する必要はありません。

### 5.2. Auto-Layout & Packing (Virtual Frame)

MasterはDeviceの定義に基づき、複数のデータを1つのCANフレーム（最大64byte）にまとめる**パッキング**を自動計算します。

- **アライメント:** データ型に応じた適切なアライメント（padding）を自動挿入。
- **Sync-Group:** 同期が必要なデータ（例：左右の車輪指令）は同一フレームにパッキングされるようスケジューリング可能。
  - 複数Nodeがある場合、同じpub/subを優先してまとめにいく。

### 5.3. ROS Type Mapping (Duck Typing)

ROSの標準メッセージ型と、FWの軽量な型を自動変換します。

- **Device:** 必要なデータだけを持つ軽量な構造体（例: `float x, y`）。
- **Master:** `ros_map` 属性に従い、ROSメッセージ（例: `Pose`）の該当フィールドへ値を注入/抽出。

---

## 6. ライフサイクル (Startup Sequence)

1. **Enumeration:** Masterがバス上のDeviceをスキャン（Ext ID）。
    - 各Deviceは固有の `DeviceID` を持ち、同じNode定義を持つ複数インスタンスを識別可能。
    - Device中のノードは異種同種に関わらず `NodeID` を持ち、同一Device内での区別に使用。
    - `DeviceID` を一覧取得したあと、それぞれに `NodeID` 一覧を問い合わせ、次に進む。
2. **Definition Exchange:** MasterがDeviceのもつ各Nodeから「チェックサム」を取得、必要ならさらに「定義バイナリ」を取得。
    - スキーマチェックサムにより、定義の一致を確認。
    - Masterは次回以降の起動を高速化するため、定義をキャッシュ可能。
3. **Layout Calculation:** MasterがDSL情報を解析し、パッキングルールとStandard IDの割り当てマップを作成。
4. **Configuration:** MasterがDeviceへ「IDマップ情報」を送信（Ext ID）。
    - 例: 「NodeID 0について、Address 0と1のデータは、今後 Standard ID `0x100` の Offset 0, 4 で受信せよ」
5. **Activation:** 通信開始。以降、Fast Path（Std ID）での高速制御が行われる。

---

## 7. 開発フロー

1. **Define:** ユーザーが `.fibril` ファイルを作成。
2. **Generate:** `fibril_gen` ツールを実行。
    - `device_code.hpp` (C++): MCU用ドライバコード。
    - `definition.bin`: MCUに埋め込むメタデータ。
3. **Build FW:** 生成コードをインクルードしてFWをコンビルド。
4. **Run Master:** PCとMCUを接続し、Masterノードを起動。
    - 設定ファイル不要（Zero-Config）で、DSL通りのトピックが即座に出現。

---

## Appendix: DeviceIDとNodeID

- DeviceIDは各デバイスのハードウェアに依存しており、この開発では具体的な方法は議論しません。
  - 一般にはDIPスイッチなどが考えられ、生成コードのAPIを通じて設定します。
- DeviceID/NodeIDをそのままROSトピック名に含めることも可能ですが、ユーザーが上書きできます。
  - これはYAML等の設定ファイルによって与えられます。
