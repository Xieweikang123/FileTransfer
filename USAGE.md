# 使用说明

## 编译
```cmd
compile_mingw_static.bat
```

**生成文件：** `ftool_static.exe` (静态链接，无需额外运行时库)

## 服务端
```cmd
ftool_static.exe server <端口> <输出目录>
```

**示例：**
```cmd
ftool_static.exe server 8080 C:\received_files
```

## 客户端
```cmd
ftool_static.exe client <服务器IP> <端口> <文件路径>
```

**示例：**
```cmd
ftool_static.exe client 127.0.0.1 8080 test.txt
```

## 测试步骤

1. **启动服务端**
   ```cmd
   ftool_static.exe server 8080 .
   ```

2. **启动客户端**（在另一个命令行窗口）
   ```cmd
   ftool_static.exe client 127.0.0.1 8080 test.txt
   ```

3. **查看结果**
   - 服务端会显示接收进度
   - 客户端会显示发送进度
   - 文件会保存到服务端指定的目录

## 注意事项

- 确保 MinGW-w64 已安装并添加到 PATH
- 确保防火墙允许指定端口
- 确保有足够的文件读写权限
