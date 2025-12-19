# M5Stack Core2 SCD41 CO2 Monitor

M5Stack Core2 と Sensirion SCD41 センサーユニットを使用した、CO2（二酸化炭素）濃度、温度、湿度モニターです。
測定値をリアルタイムで表示するだけでなく、過去のデータをグラフで可視化する機能も備えています。

## 特徴

*   **高精度測定**: Sensirion SCD41 センサーを使用し、正確なCO2、温度、湿度を測定します。
*   **リアルタイム表示**: 現在の測定値を大きく見やすく表示します。
*   **トレンドグラフ**: 過去の測定データをグラフ表示し、環境の変化をひと目で確認できます。
*   **自動時刻同期**: NTPサーバーを使用して時刻を自動的に補正します。
*   **バッテリー管理**: バッテリー残量を表示し、USB給電の状態を監視します。省電力のためのLCD制御機能も含みます。

## 必要なハードウェア

*   [M5Stack Core2](https://shop.m5stack.com/products/m5stack-core2-esp32-iot-development-kit)
*   [M5Stack SCD41 Unit (CO2 Sensor)](https://shop.m5stack.com/products/scd41-unit-co2-temperature-humidity-sensor)

## 必要なライブラリ

ビルドには以下のArduinoライブラリが必要です。ライブラリマネージャからインストールしてください。

*   **M5Unified**
*   **M5GFX**
*   **Sensirion I2C SCD4 1.1.0 x**

## セットアップ方法

1.  **リポジトリのクローン**:
    ```bash
    git clone https://github.com/muratetsu/m5core2_scd41.git
    ```

2.  **WiFi設定の変更**:
    `m5core2_scd41.ino` ファイルを開き、以下の行をご自身のWiFi環境に合わせて変更してください。
    ```cpp
    // 180行目付近
    WiFi.begin("YOUR_SSID", "YOUR_PASSWORD");
    ```

3.  **ビルドと書き込み**:
    M5Stack Core2をPCに接続し、Arduino IDE (またはVS Code) でプロジェクトを開いて書き込みを行ってください。

## 使い方

*   起動するとWiFiに接続し、時刻を同期した後、測定を開始します。
*   画面には現在のCO2濃度(ppm)、温度(℃)、湿度(%)が表示されます。
*   画面下部には測定値の推移グラフが表示されます。
*   USB給電時は常時画面が点灯しますが、バッテリー駆動時は省電力設定が適用される場合があります（コード内の実装に依存）。

## ライセンス

このプロジェクトは [MIT License](LICENSE) の下で公開されています。
