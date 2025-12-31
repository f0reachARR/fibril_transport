# Fibril IDL (Interface Definition Language) Specification

## 1. 概要

Fibril IDLは、CAN-FDデバイスとROS 2の間のインターフェースを定義するための専用言語です。Protobuf風のシンプルな文法で、デバイス側のコード生成とROS側のインターフェース構築を同時に行います。

## 2. 基本文法

### 2.1. ファイル構造

```protobuf
syntax = "fibril v2";
package robot.mobility;

// インポート（他の.fibrilファイルから型定義を利用）
import "common/types.fibril";
import "sensors/imu.fibril";

// 構造体定義
struct TypeName {
    // フィールド定義
}

// ノード定義
node NodeName {
    // Port定義（pub/sub/service）
}
```

**ファイル構成要素:**

- `syntax`: 必須。ファイルの先頭で宣言。
- `package`: 必須。名前空間を定義。
- `import`: オプション。他のファイルの型定義を利用する場合に記述。
- `struct`: 構造体定義。複数定義可能。
- `node`: ノード定義。1ファイルにつき1つのみ。

### 2.2. コメント

```protobuf
// 単行コメント

/*
 * 複数行コメント
 */
```

### 2.3. インポート（import）

他の `.fibril` ファイルで定義された型を利用する場合、`import` 文で宣言します。

```protobuf
syntax = "fibril v2";
package robot.arm;

import "common/types.fibril";     // 共通型定義をインポート
import "sensors/force.fibril";    // センサ型をインポート

node RobotArm {
    // common/types.fibrilで定義されたTwist2Dを使用
    sub common.Twist2D base_velocity;

    // sensors/force.fibrilで定義されたForceDataを使用
    pub sensors.ForceData gripper_force;
}
```

**インポートの仕様:**

- パスは相対パスまたは絶対パス（検索パスからの解決）
- インポートした型は `パッケージ名.型名` で参照
- 循環インポートは禁止（コンパイルエラー）
- インポート先のファイルは事前にコンパイル済みである必要はない（同時解決）

**型の参照:**

```protobuf
// common/types.fibril
syntax = "fibril v2";
package common;

struct Twist2D {
    float v;
    float w;
}
```

```protobuf
// robot/mobile.fibril
syntax = "fibril v2";
package robot;

import "common/types.fibril";

node MobileBase {
    // 完全修飾名で参照
    sub common.Twist2D target_vel;
}
```

**同一パッケージ内の型:**
同じパッケージ内で定義された型は、パッケージ名なしで参照できます。

```protobuf
syntax = "fibril v2";
package robot.mobility;

struct Twist2D {
    float v;
    float w;
}

node MobileBase {
    sub Twist2D target_vel;  // 同一パッケージなのでパッケージ名不要
}
```

### 2.4. 命名規則

- **パッケージ名**: ドット区切りの小文字 (例: `robot.mobility`)
- **型名/ノード名**: PascalCase (例: `Twist2D`, `MobileBase`)
- **フィールド名/Port名**: snake_case (例: `target_vel`, `battery_voltage`)
- **インポートパス**: スラッシュ区切り、拡張子 `.fibril` を含む (例: `common/types.fibril`)

## 3. 型システム

### 3.1. プリミティブ型

以下のプリミティブ型をサポートします：

| 型名 | サイズ | 説明 |
|------|--------|------|
| `bool` | 1 byte | 真偽値 |
| `int8` | 1 byte | 符号付き8ビット整数 |
| `uint8` | 1 byte | 符号なし8ビット整数 |
| `int16` | 2 bytes | 符号付き16ビット整数 |
| `uint16` | 2 bytes | 符号なし16ビット整数 |
| `int32` | 4 bytes | 符号付き32ビット整数 |
| `uint32` | 4 bytes | 符号なし32ビット整数 |
| `int64` | 8 bytes | 符号付き64ビット整数 |
| `uint64` | 8 bytes | 符号なし64ビット整数 |
| `float` | 4 bytes | 32ビット浮動小数点数 |
| `double` | 8 bytes | 64ビット浮動小数点数 |

**注意:**

- 列挙型（enum）はサポートしません（ROSに準拠）
- 可変長型（string, 可変長配列）はサポートしません（組み込み制約）

### 3.2. 構造体

構造体は複数のフィールドをまとめた複合型です。

```protobuf
struct Twist2D {
    float v;    // 線速度
    float w;    // 角速度
}
```

**ネスト:**
構造体は他の構造体をフィールドとして含むことができます。

```protobuf
struct Vector3 {
    float x;
    float y;
    float z;
}

struct Pose {
    Vector3 position;
    Vector3 orientation;
}
```

### 3.3. 固定長配列

固定長配列は `[サイズ]` 構文で定義します。

```protobuf
struct IMUData {
    float gyro[3];      // 3要素の配列
    float accel[3];
    uint8 buffer[64];   // 64バイトのバッファ
}
```

**制約:**

- 配列サイズは1以上の整数定数
- 可変長配列はサポートしません
- 多次元配列はサポートしません

## 4. Port定義（pub/sub/service）

ノード内で定義するデータの出入り口をPortと呼びます。

### 4.1. sub（購読）

デバイスがMasterから受け取るデータ（Command）。

```protobuf
node MobileBase {
    sub Twist2D target_vel;
}
```

- デバイス視点: 購読（受信）
- ROS視点: PublisherからSubscriberへ
- 通信: Standard ID (Fast Path)

### 4.2. pub（発行）

デバイスがMasterへ送信するデータ（Notify）。

```protobuf
node BatterySensor {
    pub float voltage;
    pub float current;
}
```

- デバイス視点: 発行（送信）
- ROS視点: SubscriberからPublisherへ
- 通信: Standard ID (Fast Path)

**送信周期について:**

- 送信周期はDSLでは指定せず、serviceで設定します
- アノテーション `#[periodic]` で周期的な送信を示唆できます（パッキングの最適化ヒント）

### 4.3. service（サービス）

双方向通信（Request/Response）を定義します。2つのユースケースがあります。

#### 4.3.1. ROS Serviceとして公開

既存のROS `.srv`ファイルを参照して、ROSサービスとして公開します。

```protobuf
// 軽量なRequest/Response型を定義
struct EnableMotorRequest {
    #[ros_map(data)]
    bool enable;
}

struct EnableMotorResponse {
    #[ros_map(success)]
    bool success;
    // std_srvs/srv/SetBoolのmessageフィールドは省略（不要）
}

node MobileBase {
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(EnableMotorRequest) -> EnableMotorResponse;

    #[ros_service(std_srvs/srv/Trigger)]
    service reset_odometry() -> void;
}
```

**特徴:**

- `#[ros_service(...)]` 属性でROS srvファイルを指定
- Request/Response型は軽量に定義し、`#[ros_map]`でマッピング
- 不要なフィールドは省略可能（Master側で補完）、structへの指定がない場合は何も送らないし受け取らない (void相当)
- 通信: Extended ID (Slow Path, 機能=0x03)

#### 4.3.2. ROS Parameterとして公開

シンプルなパラメータ設定用のサービス。ROS 2パラメータとして公開されます。

```protobuf
node MobileBase {
    #[ros_param("~/max_velocity", 1.0)]
    service set_max_velocity(float value) -> void;

    #[ros_param("~/control_frequency", 50)]
    service set_control_freq(uint16 freq) -> void;
}
```

**制約:**

- Request型: プリミティブ型のみ
- Response型: 必ず `void`
- デフォルト値が必須
- ROSサービスは作られず、パラメータサーバ経由でアクセス

**使い分け:**

- 複雑なRequest/Response → `#[ros_service(...)]`
- シンプルな設定値 → `#[ros_param(...)]`

## 5. 属性（Attributes）

属性は `#[key(value1, value2, ...)]` または `#[key]` の形式で、構造体・フィールド・Portに付与します。

### 5.1. 利用可能な属性

#### 5.1.1. ros（ROSトピック/パラメータ/サービス名）

**適用対象:** Port（pub/sub/service）

ROSでの名前を指定します。省略時はPort名がそのまま使われます。

```protobuf
node MobileBase {
    #[ros("/cmd_vel")]
    sub Twist2D target_vel;  // ROS: /cmd_vel

    #[ros("~/voltage")]
    pub float battery_voltage;  // ROS: ~/voltage (相対パス)

    #[ros("~/enable_motor")]
    #[ros_service(std_srvs/srv/SetBool)]
    service enable_motor(EnableMotorRequest) -> EnableMotorResponse;
}
```

- 絶対パス（`/`始まり）: グローバルトピック/サービス
- 相対パス（`~/`始まり）: ノード名前空間内

#### 5.1.2. ros_type（ROSメッセージ型）

**適用対象:** 構造体

対応するROSメッセージ型を指定します。
ここで指定するROSメッセージ型は、後に既存のROSメッセージの型定義を参照し、正しいかどうかをチェックされます。

```protobuf
#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    float v;
    float w;
}
```

#### 5.1.3. ros_map（ROSフィールドマッピング）

**適用対象:** フィールド

ROSメッセージ内のどのフィールドに対応するかを指定します。

```protobuf
#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    #[ros_map(linear.x)]
    float v;

    #[ros_map(angular.z)]
    float w;
}
```

- ドット記法でネストしたフィールドを指定
- 省略時はフィールド名がそのまま使われます

#### 5.1.4. unit（単位）

**適用対象:** フィールド、Port

物理単位を記述します（ドキュメント・可視化用）。

```protobuf
struct Twist2D {
    #[unit("m/s")]
    float v;

    #[unit("rad/s")]
    float w;
}
```

#### 5.1.5. ros_frame_id (ROS Frame ID)

**適用対象:** Port（pub）, 構造体

ROSヘッダの `frame_id` を指定します。デバイスからは送信せず、MasterがROSメッセージを生成する際に自動付与します。

```protobuf
node IMU {
    #[ros("~/imu")]
    #[ros_frame_id("imu_link")]
    pub IMUData imu_data;
}
```

#### 5.1.6. periodic（周期送信ヒント）

**適用対象:** pub Port

周期的に送信されることを示すヒント（パッキング最適化に使用）。

```protobuf
node IMU {
    #[periodic]
    pub IMUData imu_data;  // 周期的に送信される
}
```

**注意:** 実際の送信周期はserviceかparamで設定します。これはパッキング計算のヒントです。

#### 5.1.6. ros_service（ROSサービス型）

**適用対象:** service Port

既存のROS `.srv`ファイルを参照します。

```protobuf
#[ros_service(std_srvs/srv/SetBool)]
service enable_motor(EnableMotorRequest) -> EnableMotorResponse;
```

- EnableMotorRequest/Responseはstructで定義される必要があります。

#### 5.1.7. ros_param（ROSパラメータ公開）

**適用対象:** service Port

ROS 2パラメータとして公開します（サービスは作られません）。

```protobuf
#[ros_param("~/max_velocity", 1.0)]
service set_max_velocity(float value) -> void;
```

**制約:**

- Request型はプリミティブ型のみ
- Response型は必ず `void`
- デフォルト値が第二引数として必須

#### 5.1.8. description（説明）

**適用対象:** すべて

説明文を記述します（ドキュメント生成用）。

```protobuf
#[description("2D速度指令")]
struct Twist2D {
    #[description("線速度"), unit("m/s")]
    float v;

    #[description("角速度"), unit("rad/s")]
    float w;
}
```

### 5.2. 属性の複数指定

複数の属性は連続して記述できます。

```protobuf
#[ros_map(linear.x)]
#[unit("m/s")]
#[description("前進速度")]
float v;
```

## 6. 完全な記述例

### 6.1. 単純なセンサノード

```protobuf
syntax = "fibril v2";
package sensors;

node BatterySensor {
    // 定期的にバッテリー電圧を送信
    #[ros("~/voltage")]
    #[unit("V")]
    #[periodic]
    pub float battery_voltage;

    // 電流値も送信
    #[ros("~/current")]
    #[unit("A")]
    #[periodic]
    pub float battery_current;

    // 送信周期の設定（パラメータ）
    #[ros_param("~/publish_rate", 10)]
    #[unit("Hz")]
    service set_publish_rate(uint16 rate) -> void;
}
```

### 6.2. モバイルロボット制御

```protobuf
syntax = "fibril v2";
package robot.mobility;

// ROS標準メッセージとのマッピング
#[ros_type(geometry_msgs/msg/Twist)]
#[description("2次元速度指令")]
struct Twist2D {
    #[ros_map(linear.x), unit("m/s")]
    float v;

    #[ros_map(angular.z), unit("rad/s")]
    float w;
}

// オドメトリデータ
#[ros_type(nav_msgs/msg/Odometry)]
struct Odometry2D {
    #[ros_map(pose.pose.position.x), unit("m")]
    float x;

    #[ros_map(pose.pose.position.y), unit("m")]
    float y;

    #[ros_map(pose.pose.orientation.z), unit("rad")]
    float theta;

    #[ros_map(twist.twist.linear.x), unit("m/s")]
    float vx;

    #[ros_map(twist.twist.angular.z), unit("rad/s")]
    float wz;
}

node MobileBase {
    // --- Command (Sub) ---
    #[ros("/cmd_vel")]
    #[description("速度指令")]
    sub Twist2D target_vel;

    // --- Notify (Pub) ---
    #[ros("~/odom")]
    #[description("オドメトリ情報")]
    #[periodic]
    pub Odometry2D odometry;

    #[ros("~/voltage")]
    #[unit("V")]
    #[periodic]
    pub float battery_voltage;

    // --- Parameters ---
    #[ros_param("~/max_velocity", 1.0)]
    #[unit("m/s")]
    #[description("最大速度制限")]
    service set_max_velocity(float value) -> void;

    #[ros_param("~/max_angular_velocity", 2.0)]
    #[unit("rad/s")]
    service set_max_angular_velocity(float value) -> void;

    #[ros_param("~/odom_publish_rate", 50)]
    #[unit("Hz")]
    service set_odom_publish_rate(uint16 rate) -> void;

    #[ros_param("~/battery_publish_rate", 1)]
    #[unit("Hz")]
    service set_battery_publish_rate(uint16 rate) -> void;
}
```

### 6.3. IMUセンサ

```protobuf
syntax = "fibril v2";
package sensors;

#[ros_type(sensor_msgs/msg/Imu)]
struct IMUData {
    #[ros_map(angular_velocity.x), unit("rad/s")]
    float gyro_x;

    #[ros_map(angular_velocity.y), unit("rad/s")]
    float gyro_y;

    #[ros_map(angular_velocity.z), unit("rad/s")]
    float gyro_z;

    #[ros_map(linear_acceleration.x), unit("m/s^2")]
    float accel_x;

    #[ros_map(linear_acceleration.y), unit("m/s^2")]
    float accel_y;

    #[ros_map(linear_acceleration.z), unit("m/s^2")]
    float accel_z;
}

node IMU {
    #[ros("~/imu")]
    #[periodic]
    pub IMUData imu_data;

    #[ros_param("~/publish_rate", 100)]
    #[unit("Hz")]
    service set_publish_rate(uint16 rate) -> void;
}
```

### 6.4. 配列を使った例

```protobuf
syntax = "fibril v2";
package sensors;

struct JointState {
    float position[6];   // 6軸の関節位置
    float velocity[6];   // 6軸の関節速度
    float effort[6];     // 6軸のトルク
}

node RobotArm {
    #[ros("/joint_command")]
    sub float target_positions[6];

    #[ros("~/joint_states")]
    #[periodic]
    pub JointState joint_state;

    #[ros_param("~/publish_rate", 50)]
    #[unit("Hz")]
    service set_publish_rate(uint16 rate) -> void;
}
```

## 7. 制約と注意事項

### 7.1. サイズ制限

- 1つの構造体の合計サイズは64byte以下を推奨（CANフレームサイズ）
- 大きな構造体は自動的に複数フレームに分割されますが、パフォーマンスに影響

### 7.2. 型の互換性

- Device-to-Device接続では、送信側と受信側の型が構造的に一致する必要があります
- フィールド名、型、順序がすべて一致していること
- 構造体名は異なっていてもOK

### 7.3. パラメータのデフォルト値

- `ros_param`には必ずデフォルト値を指定します
- デフォルト値は定数リテラルのみ（式は不可）

### 7.4. ROS型マッピング

- `ros_type`を指定する場合、すべてのフィールドに`ros_map`の指定を推奨
- 省略した場合はフィールド名がそのまま使われます

## 8. 生成されるコード

### 8.1. デバイス側（C++）

```cpp
// GamePad.hpp (生成コード)
class GamePad {
public:
    // Port変数（直接アクセス可能）
    Twist2D stick_input;  // pub

    // 送信トリガー
    void publish_stick_input();

    // パラメータアクセス（初回にMasterからデフォルト値が設定される）
    void set_publish_rate(float value);
};
```

### 8.2. Master側（ROS 2）

- トピックの自動生成（型マッピング込み）
- パラメータサーバへの自動登録
- Definition Binary (*.bin) の埋め込み

## 9. ベストプラクティス

### 9.1. 型の再利用

よく使う型は別ファイルに定義し、複数のノードで再利用します。

```protobuf
// common/types.fibril
syntax = "fibril v2";
package common;

#[ros_type(geometry_msgs/msg/Twist)]
struct Twist2D {
    #[ros_map(linear.x), unit("m/s")]
    float v;
    #[ros_map(angular.z), unit("rad/s")]
    float w;
}

#[ros_type(geometry_msgs/msg/Vector3)]
struct Vector3 {
    float x;
    float y;
    float z;
}
```

```protobuf
// robot/mobile.fibril
syntax = "fibril v2";
package robot;

import "common/types.fibril";

node MobileBase {
    sub common.Twist2D target_vel;
    pub common.Vector3 position;
}
```

### 9.2. 単位の明記

物理量には必ず単位を記述します。

```protobuf
#[unit("m/s")]
float velocity;  // Good

float velocity;  // Bad - 単位が不明
```

### 9.3. 説明の追加

複雑な構造には説明を追加します。

```protobuf
#[description("PID制御パラメータ")]
struct PIDParams {
    #[description("比例ゲイン")]
    float kp;

    #[description("積分ゲイン")]
    float ki;

    #[description("微分ゲイン")]
    float kd;
}
```

---

## Appendix A: インポート解決の詳細

### A.1. インポートパスの解決

`fibril_gen` ツールは以下の順序でインポートファイルを検索します：

1. **相対パス**: 現在の `.fibril` ファイルからの相対パス
2. **インクルードパス**: `-I` オプションで指定されたディレクトリ
3. **標準ライブラリパス**: システムの標準ライブラリディレクトリ（将来的に追加予定）

**例:**

```bash
# 相対パスでインポート
fibril_gen robot/mobile.fibril

# インクルードパスを指定
fibril_gen -I ./common -I ./sensors robot/mobile.fibril
```

### A.2. 型の完全修飾名

インポートした型は、パッケージ名で修飾して使用します。

```protobuf
import "common/types.fibril";  // package common

node Example {
    sub common.Twist2D cmd;    // 完全修飾名
}
```

**エイリアス（将来的に追加予定）:**

```protobuf
import "common/types.fibril" as types;

node Example {
    sub types.Twist2D cmd;  // エイリアスを使用
}
```

### A.3. 循環インポートの検出

循環インポートはコンパイルエラーとなります。

```protobuf
// a.fibril
import "b.fibril";  // b.fibrilをインポート

// b.fibril
import "a.fibril";  // a.fibrilをインポート → エラー
```

**エラーメッセージ例:**

```text
Error: Circular import detected: a.fibril -> b.fibril -> a.fibril
```

---

## Appendix B: 予約語一覧

以下のキーワードは予約語として使用できません：

- `syntax`
- `package`
- `import`
- `struct`
- `node`
- `pub`
- `sub`
- `service`
- すべてのプリミティブ型名（`bool`, `int8`, `uint8`, ...）

---

## Appendix C: ファイル構成の推奨例

プロジェクトのディレクトリ構成例：

```text
project/
├── common/
│   └── types.fibril          # 共通型定義
├── sensors/
│   ├── imu.fibril            # IMUノード定義
│   └── battery.fibril        # バッテリーセンサノード定義
├── actuators/
│   └── motor.fibril          # モーターノード定義
└── robots/
    ├── mobile_base.fibril    # モバイルベースノード定義
    └── arm.fibril            # アームノード定義
```

各ファイルは独立してコンパイル可能で、必要に応じてインポートします。
