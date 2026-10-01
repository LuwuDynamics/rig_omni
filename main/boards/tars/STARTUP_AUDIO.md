# TARS 开机声音设计

目标是做一段原创的、克制的深空设备启动声，而不是电影原声、台词或配乐的复制。它需与 `launch` 的启动状态同时播放。

## 素材获取

- [Freesound](https://freesound.org/) 搜索 `relay click`、`spacecraft hum`、`synth blip`，筛选 **CC0**。
- [Pixabay Music 和音效](https://pixabay.com/music/) 搜索 `sci fi interface`、`deep drone`、`computer startup`。保留下载页与许可摘要。
- [ZapSplat](https://www.zapsplat.com/) 搜索 `sci fi computer` 或 `mechanical relay`。免费帐号使用时需按当前许可条款归因。

不要从《星际穿越》的电影、原声带或视频中截取后随固件分发。如需使用原作素材，必须另行取得权利方的书面授权。

## 生成提示词

> 4.5-second original spacecraft computer boot sound effect, restrained and utilitarian; one soft mechanical relay click, a low warm power hum, two short clean telemetry tones, and a subtle filtered noise rise resolving to silence. No melody, no vocals, no dialogue, no orchestral score, no recognizable film theme, no dramatic impact. Wide but mono-compatible, precise, quiet, premium industrial design.

时间线：0.00s 继电器点亮；0.25–1.40s 低频电源嗡鸣；1.40–2.10s 两声短遥测提示音；2.10–4.50s 微弱上扬后稳定。

## 导出给固件

把最终文件覆盖为 `main/boards/tars/tars_startup.ogg`。现有固件使用 Opus、单声道、16 kHz；使用 FFmpeg 可转码：

```sh
ffmpeg -i source.wav -ss 0 -t 4.5 -ac 1 -ar 16000 -c:a libopus -b:a 24k main/boards/tars/tars_startup.ogg
```

如需配音，推荐在音效结束后 100 ms 约使用中性合成音播报：“系统上线。任务参数已载入。”避免模仿任何演员的声线。
