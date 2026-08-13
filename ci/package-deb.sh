#!/bin/bash
# package-deb.sh <arch> [version] — 从 make install DESTDIR 产物打包 deb
# 由 GitHub Actions 在 chroot 内调用（源码 /src，构建产物 /src/build，
# make install 已装到 /tmp/inst）
set -e

ARCH="${1:?usage: package-deb.sh <arch> [version]}"
VER="${2:-0.7.2}"
PKG="dde-pdf-printer"
SRC="${SRC_DIR:-/src}"
INST="${INST_DIR:-/tmp/inst}"
ROOT="$SRC/debian/$PKG"
MULTIARCH=$(gcc -print-multiarch 2>/dev/null || dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || echo "$ARCH")

echo "=== 打包 dde-pdf-printer ${VER} (${ARCH}, multiarch=$MULTIARCH) ==="
rm -rf "$ROOT"
mkdir -p "$ROOT/DEBIAN"

# 1. make install 产物（插件 .so/QML/翻译由 CMake 安装规则决定路径）
if [ -d "$INST/usr" ]; then
    cp -a "$INST/usr/." "$ROOT/usr/"
else
    echo "错误：DESTDIR 安装产物不存在 $INST/usr" >&2
    exit 1
fi

# 2. CUPS backend（Python，架构无关，root:root 700）
mkdir -p "$ROOT/usr/lib/cups/backend"
install -m 700 "$SRC/backend/ddepdf" "$ROOT/usr/lib/cups/backend/ddepdf"

# 3. 控制中心图标（DCI，make install 未覆盖则手动放）
if ! find "$ROOT/usr" -name 'dcc_pdfprinter.dci' 2>/dev/null | grep -q .; then
    mkdir -p "$ROOT/usr/share/dsg/icons"
    cp "$SRC/assets/icons/dcc_pdfprinter.dci" "$ROOT/usr/share/dsg/icons/"
fi

# 3.5 桌面入口（.desktop，deepin 打包规范：应用中心据此提供系统卸载能力；644 权限）
mkdir -p "$ROOT/usr/share/applications"
install -m 644 "$SRC/assets/applications/dde-pdf-printer.desktop" "$ROOT/usr/share/applications/"

# 4. DEBIAN 脚本
cp "$SRC"/debian/dde-pdf-printer.postinst "$ROOT/DEBIAN/postinst"
cp "$SRC"/debian/dde-pdf-printer.prerm "$ROOT/DEBIAN/prerm"
cp "$SRC"/debian/dde-pdf-printer.postrm "$ROOT/DEBIAN/postrm"
chmod 755 "$ROOT/DEBIAN/postinst" "$ROOT/DEBIAN/prerm" "$ROOT/DEBIAN/postrm"

# 5. control
cat > "$ROOT/DEBIAN/control" <<EOF
Package: dde-pdf-printer
Version: $VER
Section: utils
Priority: optional
Architecture: $ARCH
Maintainer: Deepin PDF Printer Developers <dev@example.com>
Depends: cups, cups-filters, ghostscript, dde-control-center
Description: Deepin virtual PDF printer (CUPS backend + control center plugin)
 Provides a virtual PDF printer for Deepin desktop, similar to
 "Microsoft Print to PDF" on Windows.
EOF

dpkg-deb --root-owner-group --build "$ROOT" "$SRC/dde-pdf-printer_${VER}_${ARCH}.deb"
echo "=== 产物: $SRC/dde-pdf-printer_${VER}_${ARCH}.deb ==="
ls -la "$SRC/dde-pdf-printer_${VER}_${ARCH}.deb"
