# 开发指南

## 版本控制

### Git 设置
项目已配置 `.gitignore`，以下文件会被自动忽略：

**编译产物**
- `*.exe` - 可执行文件
- `*.obj`, `*.o` - 目标文件
- `*.lib`, `*.a` - 库文件

**临时文件**
- `*.tmp`, `*.temp` - 临时文件
- `*.log` - 日志文件
- `*.bak` - 备份文件

**IDE 文件**
- `.vscode/` - VS Code 配置
- `.idea/` - JetBrains IDE 配置
- `*.swp`, `*.swo` - Vim 临时文件

**测试目录**
- `received_files/` - 服务端接收文件目录
- `output/` - 输出目录
- `temp/` - 临时目录

### 开发流程

1. **克隆项目**
   ```cmd
   git clone <repository-url>
   cd FileTransfer
   ```

2. **编译测试**
   ```cmd
   compile_mingw.bat
   ```

3. **功能测试**
   ```cmd
   # 服务端
   ftool.exe server 8080 received_files
   
   # 客户端
   ftool.exe client 127.0.0.1 8080 test.txt
   ```

4. **提交代码**
   ```cmd
   git add .
   git commit -m "功能描述"
   git push
   ```

### 注意事项

- 编译后的 `ftool.exe` 不会被提交（除了初始版本）
- 测试时创建的接收目录会被忽略
- 确保代码修改后能正常编译运行

### 文件结构

```
FileTransfer/
├── ftool.cpp              # 主源码文件
├── compile_mingw.bat      # 编译脚本
├── test.txt               # 测试文件
├── README.md              # 主要文档
├── QUICKSTART.md          # 快速开始
├── USAGE.md               # 使用说明
├── DEVELOPMENT.md         # 开发指南
├── BUILD_INFO.md          # 构建信息
├── .gitignore             # Git 忽略文件
└── ftool.exe              # 预编译版本
```

### 发布流程

1. 测试功能完整性
2. 更新版本文档
3. 编译发布版本
4. 创建 Git tag
5. 发布 Release
