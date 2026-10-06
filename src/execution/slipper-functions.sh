#!/bin/bash
# Slipper Native EAPI Execution Wrapper

if [ -z "${1}" ] || [ -z "${2}" ]; then
    echo "Usage: slipper-functions.sh  "
    exit 1
fi

EBUILD_PATH="${1}"
PHASE="${2}"

export EBUILD_PHASE="${PHASE}"

die() { 
    echo -e "\033[1;31m[!] FATAL:\033[0m ${*}" >&2
    exit 1
}

einfo() { 
    echo -e "\033[1;32m * \033[0m${*}"
}

ewarn() {
    echo -e "\033[1;33m * WARNING:\033[0m ${*}"
}

# --- Bulletproof Bash Environment Engine ---

save_env() (
    unset BASH_ENV
    while IFS=' ' read -r _ _ func_name; do
        if [[ "${func_name}" == ___* ]]; then
            unset -f "${func_name}"
        fi
    done < <(declare -F)
    
    # Save raw files for the Slipper C++ daemon to parse
    declare -fp > "${T}/environment-funcs.raw" 2>/dev/null || true
    
    # Aggressively strip internal read-only bash variables so the source command doesn't abort
    declare -p 2>/dev/null | grep -E -v ' (BASH_[a-zA-Z_]+|BASHOPTS|FUNCNAME|GROUPS|EUID|PPID|UID|SHELLOPTS|DIRSTACK)=' > "${T}/environment-vars.raw" || true

    # Save a native, uncorrupted bash state for phase-to-phase continuity
    declare -fp > "${T}/bash-env.sh" 2>/dev/null || true
    declare -p 2>/dev/null | grep -E -v ' (BASH_[a-zA-Z_]+|BASHOPTS|FUNCNAME|GROUPS|EUID|PPID|UID|SHELLOPTS|DIRSTACK)=' >> "${T}/bash-env.sh" || true
)
# ---------------------------------------------

# --- Portage Version Math ---
__ver_parse_range() {
    local range="${1}"
    local max="${2}"
    start="${range%-*}"
    [[ "${range}" == *-* ]] && end="${range#*-}" || end="${start}"
    if [[ "${end}" ]]; then
        [[ "${end}" -le "${max}" ]] || end="${max}"
    else
        end="${max}"
    fi
}

__ver_split() {
    local v="${1}" LC_ALL=C
    comp=()
    local s c
    while [[ "${v}" ]]; do
        s="${v%%[a-zA-Z0-9]*}"
        v="${v:${#s}}"
        [[ "${v}" == [0-9]* ]] && c="${v%%[^0-9]*}" || c="${v%%[^a-zA-Z]*}"
        v="${v:${#c}}"
        comp+=( "${s}" "${c}" )
    done
}

__ver_compare_int() {
    local a="${1}" b="${2}" d="$(( ${#1} - ${#2} ))"
    if [[ "${d}" -gt 0 ]]; then
        printf -v b "%0${d}d%s" 0 "${b}"
    elif [[ "${d}" -lt 0 ]]; then
        printf -v a "%0$(( -d ))d%s" 0 "${a}"
    fi
    [[ "${a}" > "${b}" ]] && return 3
    [[ "${a}" == "${b}" ]]
}

__ver_compare() {
    local va="${1}" vb="${2}" a an al as ar b bn bl bs br re LC_ALL=C
    re="^([0-9]+(\.[0-9]+)*)([a-z]?)((_(alpha|beta|pre|rc|p)[0-9]*)*)(-r[0-9]+)?$"
    [[ "${va}" =~ ${re} ]] || die "invalid version: ${va}"
    an="${BASH_REMATCH[1]}"
    al="${BASH_REMATCH[3]}"
    as="${BASH_REMATCH[4]}"
    ar="${BASH_REMATCH[7]}"
    [[ "${vb}" =~ ${re} ]] || die "invalid version: ${vb}"
    bn="${BASH_REMATCH[1]}"
    bl="${BASH_REMATCH[3]}"
    bs="${BASH_REMATCH[4]}"
    br="${BASH_REMATCH[7]}"
    __ver_compare_int "${an%%.*}" "${bn%%.*}" || return
    while [[ "${an}" == *.* && "${bn}" == *.* ]]; do
        an="${an#*.}"
        bn="${bn#*.}"
        a="${an%%.*}"
        b="${bn%%.*}"
        if [[ "${a}" == 0* || "${b}" == 0* ]]; then
            [[ "${a}" =~ 0+$ ]] && a="${a%${BASH_REMATCH[0]}}"
            [[ "${b}" =~ 0+$ ]] && b="${b%${BASH_REMATCH[0]}}"
            [[ "${a}" > "${b}" ]] && return 3
            [[ "${a}" < "${b}" ]] && return 1
        else
            __ver_compare_int "${a}" "${b}" || return
        fi
    done
    [[ "${an}" == *.* ]] && return 3
    [[ "${bn}" == *.* ]] && return 1
    [[ "${al}" > "${bl}" ]] && return 3
    [[ "${al}" < "${bl}" ]] && return 1
    as="${as#_}${as:+_}"
    bs="${bs#_}${bs:+_}"
    while [[ -n "${as}" && -n "${bs}" ]]; do
        a="${as%%_*}"
        b="${bs%%_*}"
        if [[ "${a%%[0-9]*}" == "${b%%[0-9]*}" ]]; then
            __ver_compare_int "${a##*[a-z]}" "${b##*[a-z]}" || return
        else
            [[ "${a%%[0-9]*}" == p ]] && return 3
            [[ "${b%%[0-9]*}" == p ]] && return 1
            [[ "${a}" > "${b}" ]] && return 3 || return 1
        fi
        as="${as#*_}"
        bs="${bs#*_}"
    done
    if [[ -n "${as}" ]]; then
        [[ "${as}" == p[_0-9]* ]] && return 3 || return 1
    elif [[ -n "${bs}" ]]; then
        [[ "${bs}" == p[_0-9]* ]] && return 1 || return 3
    fi
    __ver_compare_int "${ar#-r}" "${br#-r}" || return
    return 2
}

ver_test() {
    local va op vb
    if [[ $# -eq 3 ]]; then
        va="${1}"
        shift
    else
        va="${PVR}"
    fi
    op="${1}"
    vb="${2}"
    __ver_compare "${va}" "${vb}"
    test $? "${op}" 2
}
# -------------------------------------------------------------

# --- Portage Core Baseline EAPI Helpers ---

# EAPI 8: Banned commands
useq() { die "useq is banned in EAPI 8. Use 'use' instead."; }
hasv() { die "hasv is banned in EAPI 8. Use 'has' instead."; }
hasq() { die "hasq is banned in EAPI 8. Use 'has' instead."; }

has() {
    local needle="${1}"
    shift
    local item
    for item in "${@}"; do
        [ "${item}" = "${needle}" ] && return 0
    done
    return 1
}

has_version() { return 0; }

EXPORT_FUNCTIONS() {
    local phase
    for phase in "${@}"; do
        eval "${phase}() { ${ECLASS}_${phase} \\"$@\\"; }"
    done
}

elog() { einfo "${*}"; }
eerror() { echo -e "\033[1;31m[ERROR]\033[0m ${*}" >&2; }
eqawarn() { ewarn "${*}"; }
debug-print-function() { :; }
debug-print() { :; }

use() {
    local flag="${1}"
    for u in ${USE}; do
        if [ "${u}" = "${flag}" ]; then
            return 0
        fi
    done
    return 1
}

# EAPI 8: usev takes an optional second argument. If true, prints $2, else $1.
usev() {
    if use "${1}"; then
        echo "${2:-${1}}"
        return 0
    fi
    return 1
}

usex() {
    if use "${1}"; then
        echo "${2-yes}${4}"
    else
        echo "${3-no}${5}"
    fi
}

use_with() {
    if use "${1}"; then
        echo "--with-${1}${2+=${2}}"
    else
        echo "--without-${1}"
    fi
}

use_enable() {
    if use "${1}"; then
        echo "--enable-${1}${2+=${2}}"
    else
        echo "--disable-${1}"
    fi
}

eapply() {
    for patch in "${@}"; do
        einfo "Applying ${patch}..."
        patch -p1 < "${patch}" || die "eapply failed on ${patch}"
    done
}
eapply_user() { return 0; }

default() {
    local phase_func="default_src_${PHASE}"
    if [ "$(type -t "${phase_func}")" = "function" ]; then
        "${phase_func}"
    elif [ "$(type -t "default_pkg_${PHASE}")" = "function" ]; then
        "default_pkg_${PHASE}"
    else
        ewarn "No default implementation for phase: ${PHASE}"
    fi
}
# -----------------------------------------------

# --- Slipper Native Phase Helpers ---
get_libdir() {
    local libdir_var="LIBDIR_${ABI}"
    local libdir="lib"

    if [[ -n ${ABI} && -n ${!libdir_var} ]]; then
        libdir=${!libdir_var}
    fi

    echo "${libdir}"
}
# ----------------------------------

inherit() { 
    local eclass
    for eclass in "${@}"; do
        local eclass_path="/var/db/repos/gentoo/eclass/${eclass}.eclass"
        if [ -f "${eclass_path}" ]; then
            local ECLASS="${eclass}"
            source "${eclass_path}" || die "Failed to source eclass: ${eclass}"
        else
            ewarn "Eclass not found: ${eclass} (Stubbing execution...)"
        fi
    done
}

tc-export() {
    for var in "${@}"; do
        case "${var}" in
            CC) export CC="${CC:-gcc}" ;;
            CXX) export CXX="${CXX:-g++}" ;;
            AR) export AR="${AR:-ar}" ;;
        esac
    done
}

dobin() {
    local dest="${ED}/usr/bin"
    install -d "${dest}"
    install -m0755 "${@}" "${dest}/" || die "dobin failed"
}

doman() {
    local dest="${ED}/usr/share/man"
    for f in "${@}"; do
        local ext="${f##*.}"
        install -d "${dest}/man${ext}"
        install -m0644 "${f}" "${dest}/man${ext}/" || die "doman failed on ${f}"
    done
}

einstalldocs() {
    local dest="${ED}/usr/share/doc/${PF}"
    install -d "${dest}"
    for doc in README* ChangeLog AUTHORS NEWS TODO; do
        if [ -f "${doc}" ]; then
            install -m0644 "${doc}" "${dest}/"
        fi
    done
}

econf() { 
    einfo "Configuring..."
    
    local conf_args=( --prefix=/usr --sysconfdir=/etc --localstatedir=/var )
    
    if [ -f "./configure" ]; then
        local help_text
        help_text="$(./configure --help 2>/dev/null)"
        
        # EAPI 8 specific support flags
        if echo "${help_text}" | grep -q -- "--datarootdir"; then
            conf_args+=( "--datarootdir=${EPREFIX}/usr/share" )
        fi
        if echo "${help_text}" | grep -q -- "--disable-static"; then
            conf_args+=( "--disable-static" )
        fi
        if echo "${help_text}" | grep -q -- "--disable-dependency-tracking"; then
            conf_args+=( "--disable-dependency-tracking" )
        fi
        if echo "${help_text}" | grep -q -- "--disable-silent-rules"; then
            conf_args+=( "--disable-silent-rules" )
        fi
    fi
    
    ./configure "${conf_args[@]}" "${@}" || die "econf failed"
}

emake() { 
    einfo "Compiling via make..."
    make "${@}" || die "emake failed"
}

default_pkg_setup() { return 0; }

# --- Slipper Native EAPI 8 Unpack Engine ---

unpack() {
    # SECURITY: Prevent host environment pollution from poisoning the tar execution
    unset TAR_OPTIONS

    local f
    for f in "${@}"; do
        local srcfile=""
        
        # EAPI 8 allows absolute paths and paths relative to the working directory[cite: 128]
        if [[ "${f}" == /* ]] || [[ "${f}" == ./* ]]; then
            srcfile="${f}"
        else
            srcfile="/var/cache/distfiles/${f}"
        fi

        if [ ! -s "${srcfile}" ]; then
            die "unpack: file does not exist or is empty: ${srcfile}"
        fi

        einfo "Unpacking ${f}..."
        
        # EAPI 8: Case-insensitive matching for extensions[cite: 128]
        local lower_f="${f,,}"

        case "${lower_f}" in
            *.tar)
                command tar -xf "${srcfile}" || die "Unpacking ${f} failed"
                ;;
            *.tar.gz|*.tgz|*.tar.z)
                command tar -xzf "${srcfile}" || die "Unpacking ${f} failed"
                ;;
            *.tar.bz2|*.tbz2|*.tar.bz|*.tbz)
                command tar -xjf "${srcfile}" || die "Unpacking ${f} failed"
                ;;
            *.tar.xz|*.txz)
                command tar -xJf "${srcfile}" || die "Unpacking ${f} failed"
                ;;
            *.gz|*.z)
                command gzip -dc "${srcfile}" > "${f%.*}" || die "Unpacking ${f} failed"
                ;;
            *.bz2|*.bz)
                command bzip2 -dc "${srcfile}" > "${f%.*}" || die "Unpacking ${f} failed"
                ;;
            *.xz)
                command xz -dc "${srcfile}" > "${f%.*}" || die "Unpacking ${f} failed"
                ;;
            *.zip|*.jar)
                command unzip -qo "${srcfile}" || die "Unpacking ${f} failed"
                ;;
            *)
                ewarn "unpack: unrecognised file format: ${f}"
                ;;
        esac
    done

    # EAPI 8 Mandatory Permissions Fix:
    # All objects get a+r, u+w, go-w. All directories get a+x.
    # Excludes the current working directory itself.[cite: 126]
    find . -mindepth 1 -exec chmod a+r,u+w,go-w {} + || die "Failed to adjust unpacked file permissions"
    find . -mindepth 1 -type d -exec chmod a+x {} + || die "Failed to adjust unpacked directory permissions"
}

default_src_unpack() {
    if [ -n "${A}" ]; then
        unpack ${A}
    fi
}
default_src_prepare() { return 0; }
default_src_configure() { if [ -x "./configure" ]; then econf; fi; }
default_src_compile() { 
    if [ -f "Makefile" ] || [ -f "GNUmakefile" ]; then 
        emake
    else
        ewarn "No Makefile found in $(pwd)! Skipping compile."
    fi 
}
default_src_install() { if [ -f "Makefile" ]; then emake DESTDIR="${D}" install; fi; }

pkg_setup() { default_pkg_setup; }
src_unpack() { default_src_unpack; }
src_prepare() { default_src_prepare; }
src_configure() { default_src_configure; }
src_compile() { default_src_compile; }
src_install() { default_src_install; }

if [ ! -f "${EBUILD_PATH}" ]; then
    die "Ebuild not found: ${EBUILD_PATH}"
fi

source "${EBUILD_PATH}" || die "Failed to parse${EBUILD_PATH}"

# Inline the environment load at the global scope so 'declare' creates global variables
# Prefer our native bash-env.sh to bypass C++ string mangling quirks
if [ -f "${T}/bash-env.sh" ]; then
    source "${T}/bash-env.sh" || ewarn "Syntax error encountered while loading bash-env.sh!"
elif [ -f "${T}/environment.sh" ]; then
    source "${T}/environment.sh" || ewarn "Syntax error encountered while loading environment.sh!"
fi

# RESTORE THE TARGET PHASE (environment.sh overwrites it with the previous phase's state)
PHASE="${2}"
export EBUILD_PHASE="${PHASE}"

einfo "Executing phase: ${PHASE}"

# EAPI 8 Requirement: pkg_* phases MUST execute in a completely empty directory
if [[ "${PHASE}" == pkg_* ]]; then
    if [ -d "${PORTAGE_EMPTY_DIR}" ]; then
        cd "${PORTAGE_EMPTY_DIR}" || die "Failed to enter EAPI 8 empty directory for ${PHASE}"
    else
        die "EAPI 8 empty directory missing: ${PORTAGE_EMPTY_DIR}"
    fi
elif [ "${PHASE}" != "setup" ] && [ "${PHASE}" != "unpack" ]; then
    if [ -n "${S}" ]; then
        if [ -d "${S}" ]; then
            cd "${S}" || die "Failed to enter source directory: ${S}"
        else
            ewarn "Expected source directory missing: ${S}"
        fi
    fi
fi

case "${PHASE}" in
    setup)     pkg_setup ;;
    unpack)    src_unpack ;;
    prepare)   src_prepare ;;
    configure) src_configure ;;
    compile)   src_compile ;;
    install)   src_install ;;
    preinst)   pkg_preinst ;;
    postinst)  pkg_postinst ;;
    prerm)     pkg_prerm ;;
    postrm)    pkg_postrm ;;
    *)         die "Unknown phase requested by Slipper daemon: ${PHASE}" ;;
esac

save_env
exit 0
