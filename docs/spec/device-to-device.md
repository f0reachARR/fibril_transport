# fibril_can_transport Specification (Device-to-Device extension)

## 1. コンセプト: Port-Based Architecture

DSLから具体的な「トピック名」や「リンク名」を排除します。代わりに、デバイスは **Port（データの出入り口）だけを公開します。Masterが交換手（Switchboard）となり、「Device AのPort X」と「Device BのPort Y」を接続する** というルーティング情報を起動時に解決し、CAN IDを動的に割り当てます。

## 2. DSLの変更 (Interface Definition)

DSLは「機能定義」に徹します。

### 記述例

```protobuf
// Device A: コントローラ (GamePad)
node GamePad {
    // ROSへはデフォルトで "~/stick_input" として見える
    // FW内部では単に "stick_input" という変数として扱う
    #[ros("stick_input")] 
    pub Twist2D stick_input; 
}

// Device B: 足回り (MobileBase)
node MobileBase {
    // ROSへはデフォルトで "~/target_vel" として見える
    // 誰からデータが来るかは知る必要がない
    #[ros("target_vel")] 
    sub Twist2D target_vel;
}

```

**ポイント:**
この時点では `GamePad` と `MobileBase` は無関係です。それぞれ独立して開発・コンパイル可能です。

---

## 3. ルーティング設定 (Wiring Configuration)

「誰と誰をつなぐか」は、Masterノードが読み込む **外部設定ファイル（例: `routing.yaml`）** で定義します。これが「Blackboardのメモリ管理」の実体となります。

### routing.yaml のイメージ

```yaml
routes:
  # ルール: ([DeviceID].[SourceNode].[NodeID]|[Alias]).[PubPortName] -> ([DeviceID].[SourceNode].[NodeID]|[Alias]).[SubPortName]
  # [DeviceID].[SourceNode].[NodeID]にエイリアスを付けておくことも可能
  
  # シナリオ1: コントローラから足回りへ直結 (Dev2Dev)
  - from: 1.GamePad.0.stick_input
    to:   0.MobileBase.0.target_vel
    # オプション: データの型チェックはMasterが起動時に自動で行う

  # シナリオ2: あるセンサ値を複数のデバイスで共有 (Multicast)
  - from: 3.IMU.0.gyro_z
    to:
      - 0.MobileBase.0.feedback_yaw
      # 事前に他の箇所で名付けたエイリアスも使える
      - CameraStabilizer.correction_angle

```

---

## 4. 起動シーケンス (Runtime Linking)

Masterは起動時に以下の手順で「動的配線」を行います。

1. **Enumeration & Inspection:**
   * Masterがバス上の全デバイスをスキャンし、各デバイスが持つPortリスト（名前と型）を取得。
2. **Route Resolution:**
   * `routing.yaml` を読み込み、`from` と `to` の型の整合性をチェック。
   * 接続ペアに対して、一意な **Standard CAN ID（例: 0x200）** を発行。
3. **Config Distribution (Config Phase):**
   * **送信側 (GamePad) へ:**
     * 「NodeID `0` の Port `stick_input` のデータは、ID `0x200` で吐け」と指令。
   * **受信側 (MobileBase) へ:**
     * 「ID `0x200` が来たら、NodeID `0` の Port `target_vel` に格納せよ」と指令。
4. **Activation:**
   * 通信開始。
