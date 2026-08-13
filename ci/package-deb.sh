#!/bin/bash
# package-deb.sh <arch> [version] — 在已构建好的源码树内打包 deb
# 由 GitHub Actions 在 chroot 内调用（源码在 /src，构建产物在 /src/build）
set -e

ARCH="${1:?usage: package-deb.sh <arch> [version]}"
VER="${2:-0.7.2}"
PKG="deepin-pdf-printer"
SRC="${SRC_DIR:-/src}"
ROOT="$SRC/debian/$PKG"
MULTIARCH=$(dpkg-architecture -qDEB_HOST_MULTIARCH)

echo "=== 打包 deepin-pdf-printer ${VER} (${ARCH}) ==="
rm -rf "$ROOT"
mkdir -p "$ROOT/usr/lib/cups/backend"
mkdir -p "$ROOT/usr/lib/$MULTIARCH/dde-control-center/plugins_v1.1/pdfprinter"
mkdir -p "$ROOT/usr/share/dde-control-center/translations/v1.1"
mkdir -p "$ROOT/usr/share/dsg/icons"
mkdir -p "$ROOT/DEBIAN"

# CUPS backend（Python，架构无关）
install -m 700 "$SRC/backend/deepinpdf" "$ROOT/usr/lib/cups/backend/deepinpdf"

# 插件 .so + QML 资源
PLUGIN_SRC="$SRC/build/lib/plugins_v1.1/pdfprinter"
[ -d "$PLUGIN_SRC" ] || PLUGIN_SRC="$SRC/build/plugins_v1.1/pdfprinter"
if [ ! -d "$PLUGIN_SRC" ]; then
    echo "错误：找不到构建产物 $PLUGIN_SRC" >&2
    find "$SRC/build" -name '*.so' 2>/dev/null | head -5 || true
    exit 1
fi
cp "$PLUGIN_SRC"/pdfprinter.so "$PLUGIN_SRC"/libpdfprinter_qml.so \
    "$ROOT/usr/lib/$MULTIARCH/dde-control-center/plugins_v1.1/pdfprinter/"
cp "$SRC"/src/plugin/qml/*.qml "$ROOT/usr/lib/$MULTIARCH/dde-control-center/plugins_v1.1/pdfprinter/"
cp "$PLUGIN_SRC"/qmldir "$PLUGIN_SRC"/*.qmltypes \
    "$ROOT/usr/lib/$MULTIARCH/dde-control-center/plugins_v1.1/pdfprinter/" 2>/dev/null || true

# 翻译 + 图标
cp "$SRC"/src/plugin/translations/pdfprinter_zh_CN.qm \
    "$ROOT/usr/share/dde-control-center/translations/v1.1/"
cp "$SRC"/assets/icons/dcc_pdfprinter.dci "$ROOT/usr/share/dsg/icons/"

# DEBIAN 脚本
cp "$SRC"/debian/deepin-pdf-printer.postinst "$ROOT/DEBIAN/postinst"
cp "$SRC"/debian/deepin-pdf-printer.prerm "$ROOT/DEBIAN/prerm"
cp "$SRC"/debian/deepin-pdf-printer.postrm "$ROOT/DEBIAN/postrm"
chmod 755 "$ROOT/DEBIAN/postinst" "$ROOT/DEBIAN/prerm" "$ROOT/DEBIAN/postrm"

# control
cat > "$ROOT/DEBIAN/control" <<EOF
Package: deepin-pdf-printer
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

dpkg-deb --root-owner-group --build "$ROOT" "$SRC/deepin-pdf-printer_${VER}_${ARCH}.deb"
echo "=== 产物: $SRC/deepin-pdf-printer_${VER}_${ARCH}.deb ==="
ls -la "$SRC/deepin-pdf-printer_${VER}_${ARCH}.deb"
