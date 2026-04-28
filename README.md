# Qt6 Client

**基于 Qt 6.9 & C++20 的高性能现代化即时通讯客户端**

 ## 项目目标
 构建一个跨平台（Windows/macOS/Linux）、高性能且视觉效果现代化的 IM 客户端。采用 **SSOT（Single Source of
 Truth，单一数据源）** 架构，确保 UI 状态与底层数据模型的高度一致性。

 ## 技术栈
 - **UI 框架**: Qt 6.9 (QML + C++ 混合编程)
- **核心语言**: C++ 20
- **本地存储**: SQLite 3 (采用预处理语句与异步 Worker 线程模式)
- **网络层**: 原生 TCP Socket + Protobuf (自定义协议帧：2字节ID + 2字节长度 + Payload)
- **窗口管理**: [QWindowKit](https://github.com/microsoft/QWindowKit) (实现无边框阴影与跨平台原生交互)

## 核心特性
- **响应式数据流**: `UserMgr` 作为唯一事实来源，任何数据变更仅触发一个信号，由 UI Model 自主拉取更新。
- **增量同步协议**: 登录时基于 `version` 机制拉取增量好友、申请列表及会话，极大减少首屏等待时间。
- **消息空洞自愈**: 实现了三级防线（快速重传、定时器兜底、服务端仲裁）以应对弱网环境下的消息丢失。
- **极速视觉体验**: 深度定制 QML 控件，支持毛玻璃效果、胶囊形输入框及丝滑的列表滚动动效。

## 目录结构
- `ChatMsg_Layer/`: 聊天会话与发送限流管理
- `Model_Layer/`: 抽象数据模型（好友列表、消息模型等）
- `Net_Layer/`: TCP 工作线程与底层 Socket 处理
- `Sqlite_Layer/`: 数据库异步读写逻辑
- `Src/qml/`: QML 界面源码