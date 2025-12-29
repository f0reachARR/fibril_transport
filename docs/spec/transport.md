# Fibril Transport Protocol (FTP) Specification

## 1. 概要

本ドキュメントでは、Fibrilにおけるデータ転送レイヤ（Transport Layer）の詳細を定義します。
CAN-FDの最大64byteという制約の中で、大きく分けて2つの転送モードを提供します。

1.  **Fast Path (Standard ID)**: 制御周期に関わるリアルタイムデータ用。**分割不可**。
2.  **Slow Path (Extended ID)**: ディスカバリ、定義交換、パラメータ設定、サービスコール用。**分割転送（Fragmentation）**をサポート。

---

## 2. Fast Path (Standard ID)

### 2.1. 転送ルール

-   **IDフォーマット**: 11-bit Standard ID
-   **最大ペイロード**: 64 bytes (CAN-FD)
-   **分割**: **禁止**。必ず単一フレームに収まるように設計・運用すること。

### 2.2. アクション

64byteを超える構造体を `topic` (pub/sub) として定義した場合、Master（CodeGen）は以下の戦略をとります：

1.  **Sub-struct Packing**:
    構造体を意味のある単位（サブ構造体ごと、あるいはフィールド単位）で分割し、それぞれに個別のStandard IDを割り当てます。
2.  **Explicit Split**:
    ユーザーに対して、モデル定義の分割を促します（警告またはエラー）。

これにより、リアルタイム制御における再構築オーバーヘッドとパケットロスによる遅延リスクを排除します。

---

## 3. Slow Path (Extended ID)

Slow Pathでは、ISO-TP (ISO 15765-2) を簡略化した独自の分割転送プロトコルを採用します。
Extended IDの29-bit空間のうち、`Sub/Sequence` フィールド（14-bit）を活用してフロー制御を行います。

### 3.1. ID構造の再確認

```text
[28:24] Feature (5-bit)
[23:19] DeviceID (5-bit)
[18:14] NodeID   (5-bit)
[13:0]  Sequence/Sub (14-bit)
```

この `[13:0]` 領域を、転送コンテキストに応じて以下のように使い分けます。

### 3.2. FTP Header (First byte of Payload)

Slow Pathの全フレームは、ペイロードの先頭1byteを **FTP Header** とします。

| Bit | Name | Description |
| --- | --- | --- |
| 7:6 | **Frame Type** | `00`: Single, `01`: First, `10`: Consecutive, `11`: FlowControl |
| 5:0 | **Info** | Frame Typeに依存する追加情報 |

### 3.3. Frame Types

#### 3.3.1. Single Frame (SF) - Type `00`

63byte以下のデータを1フレームで送る場合。

-   **Header**: `0x00` | `DL (Data Length)` (6-bit)
-   **Payload**: `[Header] [Data...]` (Max 63 bytes)

#### 3.3.2. First Frame (FF) - Type `01`

マルチフレーム送信の開始。全体のデータ長を通知します。

-   **Header**: `0x40` | `Data Length Upper 6-bits`
-   **Byte 1**: `Data Length Lower 8-bits` (Total 14-bit length, Max 16KB)
-   **Payload**: `[Header] [LenLow] [Data...]` (Max 62 bytes)

> Note: Sequence ID (CAN ID内) は、このフレームから一連のシーケンスとして扱われます。

#### 3.3.3. Consecutive Frame (CF) - Type `10`

後続のデータフレーム。

-   **Header**: `0x80` | `Sequence Number` (6-bit, 0-63 loop)
-   **Payload**: `[Header] [Data...]` (Max 63 bytes)

#### 3.3.4. Flow Control (FC) - Type `11`

受信側からのフロー制御（基本的には使用せず、Master主導のRequest/Responseで制御することを推奨するが、将来拡張のために定義）。

### 3.4. Transaction ID

Extended ID内の `[13:0]` 部分は、一連の送受信トランザクションを識別するために使用します。

-   **Request (Client -> Server)**: `Sequence` フィールドに任意の **Transaction ID** をセット。
-   **Response (Server -> Client)**: Requestと同じ `Sequence` フィールドを使用して返信。

これにより、多重リクエスト時の対応付けを行います。

---

## 4. Service Protocol (Feature ID 0x03)

機能ID `0x03` は、汎用的な Request/Response 通信に使用されます。

### 4.1. Payload Format

データ本体（Reassembly後）は以下のフォーマットを持ちます。

**Request:**
```binary
[Service ID (1 byte)] [Arg Data...]
```

**Response:**
```binary
[Result Code (1 byte)] [Return Data...]
```

### 4.2. Result Codes

| Code | Name | Description |
| --- | --- | --- |
| `0x00` | **OK** | 成功。以降にReturn Dataが続く。 |
| `0x01` | **Unknown Service** | 指定されたIDのサービスが見つからない。 |
| `0x02` | **Invalid Argument** | 引数データのデシリアライズ失敗など。 |
| `0x03` | **Internal Error** | サービス実行中のエラー。 |
| `0xFF` | **Busy/Retry** | 処理中につきリトライ推奨。 |

---
