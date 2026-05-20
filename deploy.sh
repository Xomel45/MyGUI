#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════════════
# deploy.sh — упаковка и развёртывание артефактов сборки MyGUI
# ══════════════════════════════════════════════════════════════════════════════
#
# Использование:
#   ./deploy.sh beta                  — Release ELF + assets → builds/beta/
#   ./deploy.sh release               — все Linux-форматы → builds/releases/VERSION-linux/
#   ./deploy.sh release linux         — AppImage → builds/releases/VERSION-linux/
#   ./deploy.sh release pkg           — .pkg.tar.zst (Arch) → builds/releases/VERSION-linux/
#   ./deploy.sh release deb           — .deb → builds/releases/VERSION-linux/
#   ./deploy.sh release rpm           — .rpm → builds/releases/VERSION-linux/
#   ./deploy.sh release my            — авто: пакет для текущего дистрибутива
#   ./deploy.sh release linux-all     — все Linux-форматы разом
#
# Опции:
#   --build     Пересобрать проект перед деплоем (Release, через CMake)
#   --clean     Удалить целевую директорию перед копированием
#
# Примеры:
#   ./deploy.sh beta --build              Собрать и задеплоить beta
#   ./deploy.sh release linux --clean     AppImage поверх очищенной директории
#   ./deploy.sh release --build --clean   Полный цикл: сборка + все форматы
# ══════════════════════════════════════════════════════════════════════════════

set -euo pipefail

# ── Цвета и вывод ─────────────────────────────────────────────────────────────
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
BLUE='\033[0;34m'; CYAN='\033[0;36m'; BOLD='\033[1m'; NC='\033[0m'

log()    { echo -e "${BLUE}[DEPLOY]${NC} $*"; }
ok()     { echo -e "${GREEN}[  OK  ]${NC} $*"; }
warn()   { echo -e "${YELLOW}[ WARN ]${NC} $*"; }
fail()   { echo -e "${RED}[ERROR ]${NC} $*" >&2; exit 1; }
header() { echo -e "\n${BOLD}${CYAN}══ $* ══${NC}"; }
rule()   { printf "${CYAN}%.0s─${NC}" {1..60}; echo; }

# ── Всегда запускаемся из корня проекта ───────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── Константы путей ───────────────────────────────────────────────────────────
APP_NAME="MyGUI"
BUILD_LINUX="build-release"
BUILDS_DIR="builds"

# ── Чтение версии из CMakeLists.txt ──────────────────────────────────────────
get_version() {
    local v
    v=$(sed -n 's/^project([^ ]* VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | head -1)
    echo "${v:-unknown}"
}
VERSION=$(get_version)

# ── Парсинг аргументов ────────────────────────────────────────────────────────
MODE="${1:-help}"
PLATFORM="${2:-both}"
DO_BUILD=false
DO_CLEAN=false

for arg in "$@"; do
    [[ "$arg" == "--build" ]] && DO_BUILD=true
    [[ "$arg" == "--clean" ]] && DO_CLEAN=true
done

# ── Вспомогательные функции ───────────────────────────────────────────────────

file_size() { du -h "$1" 2>/dev/null | cut -f1 || echo "?"; }

git_hash() { git rev-parse --short HEAD 2>/dev/null || echo "unknown"; }

write_build_info() {
    local dir="$1"
    printf "version=%s\nbuilt=%s\ncommit=%s\nplatform=%s\n" \
        "$VERSION" \
        "$(date '+%Y-%m-%d %H:%M:%S')" \
        "$(git_hash)" \
        "${2:-linux}" \
        > "$dir/build-info.txt"
}

detect_pkg_format() {
    local id="" id_like=""
    if [[ -f /etc/os-release ]]; then
        id=$(. /etc/os-release && echo "${ID:-}")
        id_like=$(. /etc/os-release && echo "${ID_LIKE:-}")
    fi
    local combined="${id} ${id_like}"

    local arch_re='arch|manjaro|endeavouros|garuda|artix|parabola|cachyos|blackarch'
    arch_re+='|archlabs|archcraft|arcolinux|archman|archstrike|bluestar|crystal'
    arch_re+='|ctlos|hyperbola|kaos|librewish|obarun|rebornos|anarchy|axyl|snal'
    arch_re+='|steamos'

    local deb_re='debian|ubuntu|linuxmint|raspbian|raspios|kali|elementary|zorin'
    deb_re+='|popos|pop_os|backbox|parrot|tails|deepin|mx|antix|devuan|pureos'
    deb_re+='|sparky|lmde|bunsenlabs|crunchbang|bodhi|mobian|armbian|siduction'
    deb_re+='|solydxk|trisquel|netrunner|nitrux|regolith|q4os|peppermint|lite'
    deb_re+='|neon|kubuntu|lubuntu|xubuntu|endless|astra|knoppix|wubuntu'

    local rpm_re='fedora|rhel|centos|rocky|almalinux|opensuse|suse|oracle|scientific'
    rpm_re+='|springdale|eurolinux|clearos|cloudlinux|mageia|pclinuxos|openmandriva'
    rpm_re+='|rosa|nobara|ultramarine|bazzite|aurora|bluefin|coreos|qubes|asahi'
    rpm_re+='|turbolinux|vine|alt|mandriva|centos-stream|circle'

    if   echo "$combined" | grep -qiE "$arch_re"; then echo "pkg"
    elif echo "$combined" | grep -qiE "$deb_re";  then echo "deb"
    elif echo "$combined" | grep -qiE "$rpm_re";  then echo "rpm"
    else echo "unknown"
    fi
}

make_zip() {
    local src="$1" zip_out="$2"
    local abs_zip
    abs_zip="$(realpath -m "$zip_out")"
    log "ZIP: $(basename "$abs_zip")..."
    rm -f "$abs_zip"
    if [[ -d "$src" ]]; then
        (cd "$(dirname "$src")" && zip -9 -r "$abs_zip" "$(basename "$src")")
    else
        (cd "$(dirname "$src")" && zip -9 "$abs_zip" "$(basename "$src")")
    fi
    ok "  + $(basename "$abs_zip")  ($(file_size "$abs_zip"))"
}

copy_file() {
    local src="$1" dst="$2" label="$3"
    if [[ -f "$src" ]]; then
        cp "$src" "$dst"
        ok "  + ${label:-$(basename "$src")}"
        return 0
    else
        warn "  ? Не найден: ${label:-$(basename "$src")}"
        return 1
    fi
}

ensure_builds_tree() {
    mkdir -p "$BUILDS_DIR/beta"
    mkdir -p "$BUILDS_DIR/releases"
}

safe_clean() {
    local target="$1"
    local abs_builds abs_target
    abs_builds="$(realpath -m "$BUILDS_DIR")"
    abs_target="$(realpath -m "$target")"

    if [[ "$abs_target" != "$abs_builds/"* && "$abs_target" != "$abs_builds" ]]; then
        fail "safe_clean: цель '${target}' (→ ${abs_target}) находится вне ${abs_builds} — отказ в удалении!"
    fi

    log "Очистка $target ..."
    rm -rf "${target:?}"
}

# Копирует папку assets/ в директорию назначения
copy_assets() {
    local dest="$1"
    if [[ -d "assets" ]]; then
        cp -r assets "$dest/"
        ok "  + assets/"
    else
        warn "  ? assets/ не найдена"
    fi
}

# ── Сборка (опциональная) ─────────────────────────────────────────────────────

build_release() {
    header "Сборка Release"
    cmake -B "$BUILD_LINUX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DFETCHCONTENT_UPDATES_DISCONNECTED=ON
    cmake --build "$BUILD_LINUX" --parallel "$(( $(nproc) - 1 ))"
    ok "Бинарник собран: $BUILD_LINUX/$APP_NAME"
}

# ── РЕЖИМ: Beta ───────────────────────────────────────────────────────────────
# Назначение: быстрая проверка, итерации.
# Артефакт:   Release ELF + assets (требует GLFW / OpenGL в системе).
# Путь:       builds/beta/
deploy_beta() {
    header "Beta Deploy → ${BUILDS_DIR}/beta/"
    ensure_builds_tree

    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_release
    fi

    local dest="${BUILDS_DIR}/beta"

    $DO_CLEAN && { safe_clean "$dest"; mkdir -p "$dest"; }

    cp "$BUILD_LINUX/$APP_NAME" "$dest/$APP_NAME"
    chmod +x "$dest/$APP_NAME"
    ok "  + $APP_NAME"

    copy_assets "$dest"
    write_build_info "$dest" "linux-beta"

    rule
    ok "Beta готов!"
    echo "  Путь:    ${BOLD}$dest/$APP_NAME${NC}"
    echo "  Размер:  $(file_size "$dest/$APP_NAME")"
    echo "  Версия:  $VERSION  |  commit: $(git_hash)"
    echo ""
    echo -e "  ${YELLOW}Запуск (требует GLFW и OpenGL в системе):${NC}"
    echo "    ./$dest/$APP_NAME"
    echo ""
}

# ── РЕЖИМ: Release Linux (AppImage) ──────────────────────────────────────────
# Назначение: финальный самодостаточный дистрибутив.
# Артефакт:   MyGUI-VERSION-x86_64.AppImage
# Путь:       builds/releases/VERSION-linux/
deploy_release_linux() {
    header "Release AppImage → ${BUILDS_DIR}/releases/${VERSION}-linux/"
    ensure_builds_tree

    if ! command -v linuxdeploy &>/dev/null; then
        fail "linuxdeploy не найден. Скачай: https://github.com/linuxdeploy/linuxdeploy/releases"
    fi

    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_release
    fi

    local dest="${BUILDS_DIR}/releases/${VERSION}-linux"
    local appimage_name="${APP_NAME}-${VERSION}-x86_64.AppImage"

    $DO_CLEAN && { safe_clean "$dest"; }
    mkdir -p "$dest"

    # Создаём .desktop если нет
    if [[ ! -f "${APP_NAME}.desktop" ]]; then
        cat > "${APP_NAME}.desktop" << DESK_EOF
[Desktop Entry]
Name=MyGUI
Comment=Naleystogramm Theme Editor
Exec=MyGUI
Icon=MyGUI
Type=Application
Categories=Development;
DESK_EOF
        ok "  + ${APP_NAME}.desktop (сгенерирован)"
    fi

    # Распаковываем AppImage в BUILD_LINUX, затем копируем артефакт
    # Иконка — берём из assets/icons/ если есть
    local icon_arg=""
    for ext in png svg; do
        if [[ -f "$SCRIPT_DIR/assets/icons/${APP_NAME}.${ext}" ]]; then
            icon_arg="--icon-file=$SCRIPT_DIR/assets/icons/${APP_NAME}.${ext}"
            break
        fi
    done

    log "Запуск linuxdeploy..."
    (
        cd "$BUILD_LINUX"
        OUTPUT="${appimage_name}" \
        DISABLE_COPYRIGHT_FILES_DEPLOYMENT=1 \
        LINUXDEPLOY_PLUGIN_STRIP=0 \
        NO_STRIP=1 \
        linuxdeploy \
            --appdir AppDir \
            --executable "$APP_NAME" \
            --desktop-file "$SCRIPT_DIR/${APP_NAME}.desktop" \
            ${icon_arg} \
            --output appimage
    )

    local created
    created=$(ls "$BUILD_LINUX/${APP_NAME}"-*.AppImage 2>/dev/null | sort -V | tail -1 || true)
    if [[ -z "$created" || ! -f "$created" ]]; then
        fail "AppImage не найден в $BUILD_LINUX/ после сборки!"
    fi

    cp "$created" "$dest/$appimage_name"
    chmod +x "$dest/$appimage_name"
    write_build_info "$dest" "linux"

    rule
    ok "AppImage готов!"
    echo "  Директория:  ${BOLD}$dest/${NC}"
    echo "  AppImage:    $appimage_name  ($(file_size "$dest/$appimage_name"))"
    echo "  Версия:      $VERSION  |  commit: $(git_hash)"
    echo ""
    echo "  Запуск (GLFW/OpenGL не нужны в системе — всё внутри):"
    echo "    chmod +x $dest/$appimage_name && ./$dest/$appimage_name"
    echo ""
}

# ── РЕЖИМ: Release .pkg.tar.zst (Arch Linux) ─────────────────────────────────
deploy_release_pkg() {
    header "Release .pkg.tar.zst → ${BUILDS_DIR}/releases/${VERSION}-linux/"
    ensure_builds_tree

    if ! command -v zstd &>/dev/null; then
        fail "zstd не найден. Установите: sudo pacman -S zstd"
    fi

    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_release
    fi

    local dest="${BUILDS_DIR}/releases/${VERSION}-linux"
    local pkgver="${VERSION}-1"
    local pkg_name="mygui-${pkgver}-x86_64.pkg.tar.zst"
    local staging="${BUILD_LINUX}/pkg-staging"

    $DO_CLEAN && { safe_clean "$dest"; }
    mkdir -p "$dest"

    rm -rf "$staging"
    mkdir -p "$staging/usr/bin"
    mkdir -p "$staging/usr/share/mygui"
    mkdir -p "$staging/usr/share/applications"
    mkdir -p "$staging/usr/share/icons/hicolor/256x256/apps"

    cp "$BUILD_LINUX/$APP_NAME" "$staging/usr/bin/mygui"
    chmod 755 "$staging/usr/bin/mygui"
    ok "  + usr/bin/mygui"

    # Шрифты и прочие ассеты — кладём рядом с бинарником
    if [[ -d "assets" ]]; then
        cp -r assets "$staging/usr/share/mygui/"
        ok "  + usr/share/mygui/assets/"
    fi

    if [[ -f "${APP_NAME}.desktop" ]]; then
        cp "${APP_NAME}.desktop" "$staging/usr/share/applications/mygui.desktop"
        ok "  + usr/share/applications/mygui.desktop"
    fi

    local installed_size
    installed_size=$(du -sb "$staging" | cut -f1)

    cat > "$staging/.PKGINFO" << PKGINFO_EOF
pkgname = mygui
pkgver = ${pkgver}
pkgdesc = Редактор тем для Naleystogramm
url = https://github.com/xomel45/MyGUI
builddate = $(date +%s)
packager = xomel45 <xom.xom.zip@gmail.com>
size = ${installed_size}
arch = x86_64
depend = glfw
depend = libgl
PKGINFO_EOF
    ok "  + .PKGINFO  (depends: glfw, libgl)"

    tar --zstd -cf "$dest/$pkg_name" -C "$staging" .PKGINFO usr
    rm -rf "$staging"

    rule
    ok "Release .pkg.tar.zst готов!"
    echo "  Директория:  ${BOLD}$dest/${NC}"
    echo "  Пакет:       $pkg_name  ($(file_size "$dest/$pkg_name"))"
    echo "  Версия:      $VERSION  |  commit: $(git_hash)"
    echo ""
    echo "  Установка:"
    echo "    sudo pacman -U $dest/$pkg_name"
    echo ""
}

# ── РЕЖИМ: Release .deb (Debian / Ubuntu) ────────────────────────────────────
deploy_release_deb() {
    header "Release .deb → ${BUILDS_DIR}/releases/${VERSION}-linux/"
    ensure_builds_tree

    if ! command -v dpkg-deb &>/dev/null; then
        fail "dpkg-deb не найден. Установите: sudo pacman -S dpkg  /  sudo apt install dpkg-dev"
    fi

    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_release
    fi

    local dest="${BUILDS_DIR}/releases/${VERSION}-linux"
    local deb_name="mygui_${VERSION}_amd64.deb"
    local pkg_dir="${BUILD_LINUX}/deb-staging"

    $DO_CLEAN && { safe_clean "$dest"; }
    mkdir -p "$dest"

    rm -rf "$pkg_dir"
    mkdir -p "$pkg_dir/DEBIAN"
    mkdir -p "$pkg_dir/usr/bin"
    mkdir -p "$pkg_dir/usr/share/mygui"
    mkdir -p "$pkg_dir/usr/share/applications"
    mkdir -p "$pkg_dir/usr/share/icons/hicolor/256x256/apps"

    local installed_kb
    installed_kb=$(du -sk "$BUILD_LINUX/$APP_NAME" | cut -f1)

    cat > "$pkg_dir/DEBIAN/control" << CTRL_EOF
Package: mygui
Version: ${VERSION}
Section: devel
Priority: optional
Architecture: amd64
Installed-Size: ${installed_kb}
Depends: libglfw3, libgl1
Maintainer: xomel45 <xom.xom.zip@gmail.com>
Homepage: https://github.com/xomel45/MyGUI
Description: Редактор тем для Naleystogramm
 MyGUI — визуальный редактор кастомных тем для мессенджера Naleystogramm.
 Позволяет редактировать 23 цвета палитры и CSS-переопределение в реальном
 времени с live-превью интерфейса Naleystogramm.
CTRL_EOF

    cp "$BUILD_LINUX/$APP_NAME" "$pkg_dir/usr/bin/mygui"
    chmod 755 "$pkg_dir/usr/bin/mygui"
    ok "  + usr/bin/mygui"

    if [[ -d "assets" ]]; then
        cp -r assets "$pkg_dir/usr/share/mygui/"
        ok "  + usr/share/mygui/assets/"
    fi

    if [[ -f "${APP_NAME}.desktop" ]]; then
        cp "${APP_NAME}.desktop" "$pkg_dir/usr/share/applications/mygui.desktop"
        ok "  + usr/share/applications/mygui.desktop"
    fi

    find "$pkg_dir/usr/share" -type f -exec chmod 644 {} \;
    find "$pkg_dir" -type d -exec chmod 755 {} \;

    dpkg-deb --build --root-owner-group "$pkg_dir" "$dest/$deb_name"
    rm -rf "$pkg_dir"

    rule
    ok "Release .deb готов!"
    echo "  Директория:  ${BOLD}$dest/${NC}"
    echo "  Пакет:       $deb_name  ($(file_size "$dest/$deb_name"))"
    echo "  Версия:      $VERSION  |  commit: $(git_hash)"
    echo ""
    echo "  Установка:"
    echo "    sudo dpkg -i $dest/$deb_name"
    echo "    sudo apt-get install -f   # если нужно подтянуть зависимости"
    echo ""
}

# ── РЕЖИМ: Release .rpm (Fedora / RHEL / openSUSE) ───────────────────────────
deploy_release_rpm() {
    header "Release .rpm → ${BUILDS_DIR}/releases/${VERSION}-linux/"
    ensure_builds_tree

    if ! command -v rpmbuild &>/dev/null; then
        fail "rpmbuild не найден. Установите: sudo pacman -S rpm-tools  /  sudo dnf install rpm-build"
    fi

    if $DO_BUILD || [[ ! -f "$BUILD_LINUX/$APP_NAME" ]]; then
        build_release
    fi

    local dest="${BUILDS_DIR}/releases/${VERSION}-linux"
    local rpm_version="${VERSION//-/_}"
    local rpm_name="mygui-${rpm_version}-1.x86_64.rpm"
    local rpm_topdir="${BUILD_LINUX}/rpm-staging"
    local buildroot="${rpm_topdir}/BUILDROOT/mygui-${rpm_version}-1.x86_64"

    $DO_CLEAN && { safe_clean "$dest"; }
    mkdir -p "$dest"

    rm -rf "$rpm_topdir"
    mkdir -p "${rpm_topdir}"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
    mkdir -p "$buildroot/usr/bin"
    mkdir -p "$buildroot/usr/share/mygui"
    mkdir -p "$buildroot/usr/share/applications"
    mkdir -p "$buildroot/usr/share/icons/hicolor/256x256/apps"

    cp "$BUILD_LINUX/$APP_NAME" "$buildroot/usr/bin/mygui"
    chmod 755 "$buildroot/usr/bin/mygui"
    ok "  + usr/bin/mygui"

    if [[ -d "assets" ]]; then
        cp -r assets "$buildroot/usr/share/mygui/"
        ok "  + usr/share/mygui/assets/"
    fi

    if [[ -f "${APP_NAME}.desktop" ]]; then
        cp "${APP_NAME}.desktop" "$buildroot/usr/share/applications/mygui.desktop"
        ok "  + usr/share/applications/mygui.desktop"
    fi

    local files_list
    files_list=$(find "$buildroot" \( -type f -o -type l \) \
        | sed "s|${buildroot}||" | sort)

    local changelog_date
    changelog_date=$(LC_ALL=C date '+%a %b %d %Y')

    cat > "${rpm_topdir}/SPECS/mygui.spec" << SPEC_EOF
Name:           mygui
Version:        ${rpm_version}
Release:        1
Summary:        Редактор тем для Naleystogramm
License:        Proprietary
URL:            https://github.com/xomel45/MyGUI
BuildArch:      x86_64
Requires:       glfw, mesa-libGL
%define __spec_install_pre %{nil}
%define _unpackaged_files_terminate_build 0

%description
MyGUI — визуальный редактор кастомных тем для мессенджера Naleystogramm.
Позволяет редактировать 23 цвета палитры и CSS-переопределение в реальном
времени с live-превью интерфейса Naleystogramm.

%build
# pre-built binary

%install
# buildroot already populated externally

%files
$(echo "$files_list")

%changelog
* ${changelog_date} xomel45 <xom.xom.zip@gmail.com> - ${rpm_version}-1
- Release ${VERSION}
SPEC_EOF

    rpmbuild -bb \
        --nodeps \
        --define "_topdir $(realpath "$rpm_topdir")" \
        --buildroot "$(realpath "$buildroot")" \
        "${rpm_topdir}/SPECS/mygui.spec"

    local created_rpm
    created_rpm=$(find "${rpm_topdir}/RPMS" -name "*.rpm" | head -1 || true)

    if [[ -z "$created_rpm" || ! -f "$created_rpm" ]]; then
        fail ".rpm не создан! Проверь вывод rpmbuild выше."
    fi

    cp "$created_rpm" "$dest/$rpm_name"
    rm -rf "$rpm_topdir"

    rule
    ok "Release .rpm готов!"
    echo "  Директория:  ${BOLD}$dest/${NC}"
    echo "  Пакет:       $rpm_name  ($(file_size "$dest/$rpm_name"))"
    echo "  Версия:      $VERSION  |  commit: $(git_hash)"
    echo ""
    echo "  Установка:"
    echo "    sudo rpm -i $rpm_name                 # RPM-based"
    echo "    sudo dnf install ./$rpm_name          # Fedora / RHEL"
    echo ""
}

# ── Вывод помощи ─────────────────────────────────────────────────────────────
show_help() {
    echo ""
    echo -e "${BOLD}${CYAN}deploy.sh${NC} — упаковщик артефактов MyGUI v${BOLD}${VERSION}${NC}"
    rule
    echo ""
    echo -e "  ${BOLD}Использование:${NC}"
    echo "    ./deploy.sh beta                  ELF + assets → builds/beta/"
    echo "    ./deploy.sh release               Все Linux-форматы → builds/releases/${VERSION}-linux/"
    echo "    ./deploy.sh release linux         AppImage → builds/releases/${VERSION}-linux/"
    echo "    ./deploy.sh release pkg/arch      .pkg.tar.zst (Arch/pacman)"
    echo "    ./deploy.sh release deb/debian    .deb (Debian/Ubuntu)"
    echo "    ./deploy.sh release rpm/rh        .rpm (Fedora/RHEL/openSUSE)"
    echo "    ./deploy.sh release my            Авто: пакет для текущего дистрибутива"
    echo "    ./deploy.sh release linux-all     Все Linux-форматы разом"
    echo ""
    echo -e "  ${BOLD}Опции:${NC}"
    echo "    --build     Пересобрать (Release, отдельно от cmake-build-debug)"
    echo "    --clean     Удалить целевые директории перед деплоем"
    echo ""
    echo -e "  ${BOLD}Примеры:${NC}"
    echo "    ./deploy.sh beta --build                   # собрать + beta"
    echo "    ./deploy.sh release linux --clean          # AppImage поверх чистой папки"
    echo "    ./deploy.sh release linux-all --build      # все Linux-пакеты"
    echo "    ./deploy.sh release all --build --clean    # полный цикл"
    echo ""
    echo -e "  ${BOLD}Требования:${NC}"
    echo "    AppImage      — linuxdeploy (https://github.com/linuxdeploy/linuxdeploy/releases)"
    echo "    .pkg.tar.zst  — zstd  (sudo pacman -S zstd)"
    echo "    .deb          — dpkg-deb  (sudo pacman -S dpkg  /  sudo apt install dpkg-dev)"
    echo "    .rpm          — rpmbuild  (sudo pacman -S rpm-tools  /  sudo dnf install rpm-build)"
    echo ""
    echo -e "  ${BOLD}Структура вывода:${NC}"
    echo "    builds/"
    echo "    ├── beta/"
    echo "    │   ├── MyGUI"
    echo "    │   ├── assets/"
    echo "    │   └── build-info.txt"
    echo "    └── releases/"
    echo "        └── ${VERSION}-linux/"
    echo "            ├── MyGUI-${VERSION}-x86_64.AppImage"
    echo "            ├── mygui-${VERSION}-1-x86_64.pkg.tar.zst"
    echo "            ├── mygui_${VERSION}_amd64.deb"
    echo "            ├── mygui-${VERSION}-1.x86_64.rpm"
    echo "            └── build-info.txt"
    echo ""
}

# ── Точка входа ───────────────────────────────────────────────────────────────
case "$MODE" in
    beta)
        deploy_beta
        ;;
    release)
        case "$PLATFORM" in
            linux)
                deploy_release_linux
                ;;
            pkg|arch|Arch)
                deploy_release_pkg
                ;;
            deb|debian|Debian)
                deploy_release_deb
                ;;
            rpm|rh|RH|red-hat|Red-Hat|red_hat|Red_Hat)
                deploy_release_rpm
                ;;
            my)
                fmt=$(detect_pkg_format)
                case "$fmt" in
                    pkg) log "Определён дистрибутив: Arch-семейство → .pkg.tar.zst"; deploy_release_pkg ;;
                    deb) log "Определён дистрибутив: Debian-семейство → .deb";        deploy_release_deb ;;
                    rpm) log "Определён дистрибутив: RPM-семейство → .rpm";           deploy_release_rpm ;;
                    *)   fail "Не удалось определить семейство дистрибутива. Укажи формат вручную: pkg / deb / rpm" ;;
                esac
                ;;
            linux-all|all)
                if $DO_CLEAN; then
                    ensure_builds_tree
                    safe_clean "${BUILDS_DIR}/releases/${VERSION}-linux"
                    DO_CLEAN=false
                fi
                deploy_release_linux
                deploy_release_pkg
                deploy_release_deb
                deploy_release_rpm
                ;;
            both|--build|--clean|*)
                if $DO_CLEAN; then
                    ensure_builds_tree
                    safe_clean "${BUILDS_DIR}/releases/${VERSION}-linux"
                    DO_CLEAN=false
                fi
                deploy_release_linux
                deploy_release_pkg
                deploy_release_deb
                deploy_release_rpm
                ;;
        esac
        ;;
    help|--help|-h)
        show_help
        ;;
    *)
        show_help
        fail "Неизвестная команда: '$MODE'"
        ;;
esac
