param([string]$BuildRoot="build", [string]$AndroidPlatform="android-28", [string]$NdkHome="", [string]$Generator="Ninja")
$ErrorActionPreference="Stop"
$scriptDir=Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot=Resolve-Path (Join-Path $scriptDir "..")
function Resolve-NdkHome {
  if($NdkHome -and (Test-Path $NdkHome)){return (Resolve-Path $NdkHome).Path}
  if($env:ANDROID_HOME){$p=Join-Path $env:ANDROID_HOME "ndk/28.2.13676358"; if(Test-Path $p){return (Resolve-Path $p).Path}}
  if($env:ANDROID_NDK_HOME -and (Test-Path $env:ANDROID_NDK_HOME)){return (Resolve-Path $env:ANDROID_NDK_HOME).Path}
  throw "Android NDK 28.2.13676358 not found"
}
$configBuild=Join-Path $repoRoot "$BuildRoot-config"
cmake -S $repoRoot -B $configBuild -G $Generator
cmake --build $configBuild --target levi_generate_config
$generated=((Resolve-Path (Join-Path $configBuild "generated-config")).Path -replace "\\","/")
$ndk=Resolve-NdkHome
$toolchain=Join-Path $ndk "build/cmake/android.toolchain.cmake"
$build=Join-Path $repoRoot "$BuildRoot-arm64-v8a"
cmake -S $repoRoot -B $build -G $Generator `
  -DCMAKE_TOOLCHAIN_FILE="$toolchain" -DANDROID_ABI="arm64-v8a" `
  -DANDROID_PLATFORM="$AndroidPlatform" -DANDROID_STL="c++_shared" `
  -DLEVI_PACKAGE_CONFIG_DIR="$generated"
cmake --build $build --target levi_package
