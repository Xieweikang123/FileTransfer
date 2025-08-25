# 快速开始指南

## 立即编译和测试

### 步骤 1：编译
运行以下命令：

```cmd
compile_mingw_static.bat
```

**说明：** 生成的可执行文件约 2.5MB，包含所有必要的运行时库，无需额外依赖。

### 步骤 2：测试文件传输

#### 启动服务端
```cmd
ftool_static.exe server 8080 C:\received_files
```

#### 在另一个命令行窗口启动客户端
```cmd
ftool_static.exe client 127.0.0.1 8080 test.txt
```

## 常见问题解决

### 问题 1：编译失败
**错误**：`'g++' 不是内部或外部命令`

**解决方案**：
1. 安装 MinGW-w64
2. 将 MinGW-w64 的 bin 目录添加到 PATH 环境变量
3. 验证安装：运行 `g++ --version`

### 问题 2：连接失败
**错误**：`Connect failed`

**解决方案**：
1. 确保服务端正在运行
2. 检查端口号是否正确
3. 检查防火墙设置

### 问题 3：文件传输失败
**错误**：`Failed to open file`

**解决方案**：
1. 检查文件路径是否正确
2. 确保有文件读取权限
3. 确保目标目录有写入权限

## 快速测试

1. **编译**：`compile_mingw_static.bat`
2. **服务端**：`ftool_static.exe server 8080 .`
3. **客户端**：`ftool_static.exe client 127.0.0.1 8080 test.txt`

如果一切正常，您应该看到：
- 服务端显示接收进度
- 客户端显示发送进度
- 文件成功传输到当前目录
