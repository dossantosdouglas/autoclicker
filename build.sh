#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== 1. Compilando backend nativo Linux (libei) ==="
gcc -O2 ei_clicker.c $(pkg-config --cflags --libs libei-1.0) -o ei_clicker
echo "✓ Linux ei_clicker gerado com sucesso."

echo "=== 2. Compilando recursos Windows (ícone e manifest) ==="
mkdir -p release
cd windows
llvm-rc -I/usr/include/wine/windows resources.rc /fo resources.res
cd "$SCRIPT_DIR"

echo "=== 3. Compilando executável nativo Windows 64-bit (AutoClicker.exe) ==="
clang -O2 \
  -target x86_64-pc-windows-msvc \
  -I/usr/include/wine/msvcrt \
  -I/usr/include/wine/windows \
  -fuse-ld=lld \
  -Wl,-subsystem:windows \
  -Wl,-entry:WinMainCRTStartup \
  -Wl,/DYNAMICBASE \
  -Wl,/NXCOMPAT \
  -Wl,/HIGHENTROPYVA \
  -Wl,/TSAWARE \
  -Wl,-nodefaultlib \
  windows/main.c \
  windows/resources.res \
  /usr/lib/wine/x86_64-windows/libuser32.a \
  /usr/lib/wine/x86_64-windows/libkernel32.a \
  /usr/lib/wine/x86_64-windows/libcomctl32.a \
  /usr/lib/wine/x86_64-windows/libgdi32.a \
  /usr/lib/wine/x86_64-windows/libdwmapi.a \
  -o release/AutoClicker.exe

echo "✓ release/AutoClicker.exe gerado com sucesso! ($(ls -lh release/AutoClicker.exe | awk '{print $5}'))"

echo "=== 4. Empacotando Release ZIP para Windows ==="
python3 -c "import zipfile; z = zipfile.ZipFile('release/AutoClicker-Windows-x64.zip', 'w', zipfile.ZIP_DEFLATED); z.write('release/AutoClicker.exe', 'AutoClicker.exe'); z.write('README.md', 'README.md'); z.close()"
echo "✓ release/AutoClicker-Windows-x64.zip gerado com sucesso!"

