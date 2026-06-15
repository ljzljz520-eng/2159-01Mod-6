# 🎆 C语言烟花秀 (Fireworks in C & WebAssembly)

这是一个基于 **C语言** 和 **WebAssembly** 技术构建的高性能烟花动画项目。核心渲染逻辑完全由 C 语言编写，通过 Emscripten 编译运行在浏览器 Canvas 上，展示了 Native 级代码在 Web 前端的强大性能。

## ✨ 项目亮点 (Highlights)
- **纯 C 语言内核**: 摒弃传统 JS 动画库，使用 C 语言 + SDL2 图形库直接控制像素，体验硬核编程之美。
- **WebAssembly 驱动**: 通过 WASM 技术将 C 代码跑在浏览器中，兼顾高性能与跨平台特性。
- **极致视觉体验**: 
  - 🌈 **全彩光谱**: 使用 HSL 色彩空间生成高饱和度、绚丽多彩的烟花颜色。
  - ⚛️ **物理引擎**: 模拟重力、空气阻力、爆炸动量，粒子运动轨迹自然逼真。
  - 🚀 **持续绽放**: 智能发射算法，确保屏幕上时刻有烟花升空绽放，拒绝冷场。
  - 🎞️ **拖尾特效**: 经典的视觉暂留拖尾效果，营造梦幻氛围。
- **Docker 一键交付**: 完整的容器化开发环境，无需本地配置复杂的 C/Emscripten 编译链。

## 🛠 技术栈 (Tech Stack)
- **Core Logic**: C Language (Standard C99)
- **Graphics Library**: SDL2 (Simple DirectMedia Layer)
- **Compiler**: Emscripten (C -> WebAssembly)
- **Frontend Host**: HTML5 Canvas
- **Deployment**: Docker + Nginx (Multi-stage Build)

## 🚀 启动指南 (How to Run)
无需安装任何 C 语言编译器或 Node.js 环境，只需 Docker：

1. 确保 Docker Desktop 已启动。
2. 在项目根目录执行：
   ```bash
   docker-compose up -d --build
   ```
3. 等待镜像拉取和编译完成（首次构建需要下载 Emscripten 镜像，约需几分钟）。
4. 浏览器访问：[http://localhost:3000](http://localhost:3000)

## � 项目结构
```text
/
├── frontend/
│   ├── fireworks.c    # 核心 C 语言源码 (粒子系统、渲染循环)
│   └── index.html     # Web 入口页面
├── Dockerfile         # 多阶段构建文件 (Build C -> Serve Nginx)
├── nginx.conf         # Web 服务器配置
└── docker-compose.yml # 容器编排
```

## 📝 开发说明
本项目演示了如何将传统 C/C++ 图形程序移植到现代 Web 环境。核心逻辑位于 `frontend/fireworks.c`，你可以像写传统桌面程序一样编写 Web 前端特效。
