# FrameColor

[English](README.md)

## 概要

Steam Frameの表示色を，ヘッドセット内のSteamVRダッシュボードから調整するツールです．パネル輝度に加え，明るさ・彩度・コントラスト・ガンマを全体または赤・緑・青のチャンネルごとに調整できます．標準・鮮やか・夜間のプリセットと，保存できる3つのプリセット枠があります．

Steam Frame専用です．色補正はSteamVRの `vrcompositor` に読み込ませるVulkanレイヤーで行います．ローカル実行とストリーミングの映像に加え，ダッシュボードなどの表示にも影響します．

## 必要要件

- Steam Frame（SteamOS，ARM64）と，標準の `/opt/steamvr` にあるSteamVR
- 開発者モードとSSH接続．アーカイブをSteam Frameに転送してコマンドを実行できること
- Steam Frame側のPython 3.10以降（インストール時のLUT生成に使用）

配布アーカイブにはSteam Frame用のビルド済みバイナリが含まれます．ソースからビルドする場合に必要なツールは「開発」を参照してください．

## インストール

GitHub Releasesから `framecolor-VERSION-steamframe-aarch64.tar.gz` を入手してください．チェックサムを確認する場合は，同名の `.sha256` ファイルも入手します．以下の `VERSION` と `FRAME_IP` は，ダウンロードした版とSteam FrameのIPアドレスに置き換えます．

PCからSteam Frameのホームディレクトリに転送します．

```sh
scp framecolor-VERSION-steamframe-aarch64.tar.gz steamos@FRAME_IP:
```

チェックサムを確認する場合のみ，`.sha256` ファイルも転送します．

```sh
scp framecolor-VERSION-steamframe-aarch64.tar.gz.sha256 steamos@FRAME_IP:
```

SSHで接続します．

```sh
ssh steamos@FRAME_IP
```

以降はSteam Frame上で実行します．`.sha256` ファイルを転送した場合は，展開前に次の確認を行います．

```sh
sha256sum -c framecolor-VERSION-steamframe-aarch64.tar.gz.sha256
```

その後，展開してインストールします．

```sh
tar -xzf framecolor-VERSION-steamframe-aarch64.tar.gz
cd framecolor
sh contrib/install.sh
~/.local/bin/framecolorctl enable
```

インストールは通常ユーザーで実行し，`sudo` は付けないでください．新規インストール直後はVulkan補正が無効です．最後の `enable` で補正とダッシュボードを有効にし，SteamVRを再起動します．インストール先はユーザー領域の `~/.local` と `~/.config` のみです．

更新するときは，新しいアーカイブを取得して同じ手順で `sh contrib/install.sh` を実行します．保存済みの調整値とプリセットは保持されます．補正がすでに有効なら，インストーラーがSteamVRを再起動します．

完全に削除するには，展開済みのアーカイブのディレクトリから次を実行します．

```sh
sh contrib/uninstall.sh
```

アンインストールでは保存した調整値・プリセットも削除します．SteamVR側に保存されたパネル輝度は残る場合があります．必要なら，削除前にダッシュボードの「リセット」で標準値に戻してください．

## 設定

SteamVRダッシュボードで「Display Color Adjustment」（ディスプレイ色調整）を開いて操作します．
パネル右上の言語ドロップダウンメニューから英語と日本語を切り替えることができます．

| 項目 | 範囲 | 適用先 |
| --- | --- | --- |
| パネル輝度 | 40～125% | SteamVRのパネル輝度設定．チャンネル共通 |
| 明るさ | 50～150% | Vulkan補正．全体またはRGB別 |
| 彩度 | 0～200% | Vulkan補正．全体またはRGB別．100%が標準 |
| コントラスト | 0～100% | Vulkan補正．全体またはRGB別．0%が標準 |
| ガンマ | 0.50～2.50 | Vulkan補正．全体またはRGB別．1.00が標準 |

「調整チャンネル」で全体・赤・緑・青を選びます．全体を動かすと，各チャンネルの現在値からまとめて変化します．パネル輝度はチャンネル選択に関係しません．「保存」を押してからP1～P3を選ぶと現在の設定を保存できます．「リセット」は標準プリセットを適用します．

SSHからは次のコマンドを使えます．

```sh
~/.local/bin/framecolorctl status
~/.local/bin/framecolorctl doctor
~/.local/bin/framecolorctl disable
~/.local/bin/framecolorctl enable
~/.local/bin/framecolorctl language
```

`disable` はVulkan補正とダッシュボードを無効にし，`enable` は両方を有効にします．どちらもSteamVRを再起動します．`disable` では保存済みの調整値とプリセットは削除されません．画面が見づらくなった場合は，SSHから `disable` を実行してください．

SteamVRが起動開始から10秒未満で異常終了した場合，起動ガードは標準で1回の失敗後にVulkanレイヤーを無効化します．原因がFrameColorとは限りません．3回連続の失敗まで待つ設定にもできます．

```sh
~/.local/bin/framecolorctl guard 3
~/.local/bin/framecolorctl guard 1
```

ガードはすべての障害からの自動復旧を保証するものではありません．SSHからの手動復旧手段も確保してください．

## 免責

FrameColorは非公式ツールです．また，ベータ版です．SteamVRやSteamOSの更新後に動作しなくなる可能性があります．

色補正は最終表示にかかるため，ダッシュボードや安全境界の色も変わる可能性があります．強い補正で視認性を損なう設定は避け，周囲の安全を確認して使用してください．このツールは安全境界を無効化・変更するものではありません．

現在の構成は標準の `/opt/steamvr` のみを対象とします．問題が起きた場合は，まずSSHから `~/.local/bin/framecolorctl disable` を実行してください．パネル輝度などSteamVR自身が保持する設定は，補正を無効化しても残る場合があります．ソフトウェアの無保証条項は [LICENSE](LICENSE) を参照してください．

このツールはCodexを利用して作成しました．動作確認は行っていますが，自己責任でお使いください．

## 開発

ソースからのビルドはSteam Frame上で行います．CMake，Meson，Ninja，pkg-config，`glslangValidator`，Cairo・FreeType・Vulkanの開発用ファイル，およびSteamVRに付属する `libopenvr_api.so` が必要です．配布アーカイブの作成には `strip`，`tar`，`sha256sum` も使います．

```sh
sh contrib/install.sh                 # ソースからビルドしてユーザー領域へインストール
sh tests/test_guard.sh
sh tests/test_framecolorctl.sh
python3 -m unittest discover -s tests -v
sh contrib/package.sh VERSION        # dist/ にアーカイブと.sha256を生成
```

`contrib/install.sh` はビルド済みバイナリが同梱されていなければソースからビルドします．`contrib/package.sh` はSteam Frame（ARM64）上で実行してください．生成した `build/` と `dist/` はGit管理の対象外です．

## 謝辞

ダッシュボード関連のコードとデザインは [frameeyeosc](https://github.com/sasaken1102r/frameeyeosc) を参考・改変しています．オーバーレイには [OpenVR](https://github.com/ValveSoftware/openvr) のヘッダーを使用し，表示色の補正には [vkBasalt](https://github.com/DadSchoorse/vkBasalt) の改変版を使用しています．作者と貢献者の皆さまに感謝します．

FrameColor独自部分は [MITライセンス](LICENSE) です．第三者由来コードの詳細とライセンス原文は [サードパーティーに関する表示](THIRD_PARTY_LICENSES.ja.md) を参照してください．
