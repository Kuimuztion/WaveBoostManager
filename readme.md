# 安装环境依赖
```powershell
winget install Microsoft.DotNet.SDK.9
```

# 编译并测试运行
```powershell
dotnet clean
dotnet build
dotnet run
```

# 打包exe程序
```powershell
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -o .\publish
```