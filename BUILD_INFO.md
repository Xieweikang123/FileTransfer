# 构建信息

## 当前版本
- **文件大小**: 约 2.5MB
- **链接方式**: 静态链接
- **依赖**: 无额外依赖

## 编译命令
```cmd
g++ -O2 -std=c++11 -static ftool.cpp -lws2_32 -o ftool_static.exe
```

## 编译选项说明
- `-O2`: 优化级别2，平衡性能和编译时间
- `-std=c++11`: 使用C++11标准
- `-static-libgcc`: 静态链接GCC运行时库
- `-static-libstdc++`: 静态链接C++标准库
- `-lws2_32`: 链接Winsock2库

## 优势
✅ **无需依赖**: 包含所有必要的运行时库  
✅ **即插即用**: 可直接在任何Windows系统上运行  
✅ **分发友好**: 单个文件，便于分发和部署  
✅ **兼容性好**: 不依赖系统环境  

## 文件大小构成
- 程序代码: ~50KB
- GCC运行时库: ~1-2MB
- C++标准库: ~1-2MB
- 总计: ~2.5MB

## 使用方式
```cmd
# 编译
compile_mingw_static.bat

# 服务端
ftool_static.exe server 8080 C:\received_files

# 客户端
ftool_static.exe client 127.0.0.1 8080 test.txt
```
