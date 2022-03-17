# flv-parser
## 1. 功能
1. 解析flv格式数据，分离Flv header和Flv Tag。
2. 分离Flv Tag的Video、Audio、ScriptData。
3. 将分离的Tag重组输出H264、AAC文件。
4. 将分离的Tag重组Flv格式文件。
## 2. 使用
1. 指定输入源和输出flv文件名
```asm
./FlvParser in.flv out.flv
```
