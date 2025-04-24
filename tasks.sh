#!/usr/bin/env bash

cd "$(dirname "$0")"
THIS_DIR=$(pwd)
BUILD_DIR="$THIS_DIR/build"

MY_CMAKE_BUILD_DEFINE="-DCMAKE_BUILD_TYPE=Debug"

function isWsl() {
    [[ -d "/mnt/c" ]] || command -v wslpath &>/dev/null
}

if isWsl && [[ "$0" =~ ^/mnt/ ]]; then
    # 在 wsl 下使用, 但是项目在 Windows 中, 尝试切换至 Git Bash
    echo "You're using WSL, but WSL linux is not supported! Tring to run with Git Bash!" >&2
    GIT_BASH_PATH_WIN=$(/mnt/c/Windows/system32/cmd.exe /C "where bash.exe" | grep -i Git | head -n 1 | tr -d '\n\r')
    GIT_BASH_PATH_WSL=$(wslpath -u "$GIT_BASH_PATH_WIN")
    echo "== GIT_BASH_PATH_WIN=$GIT_BASH_PATH_WIN"
    echo "== GIT_BASH_PATH_WSL=$GIT_BASH_PATH_WSL"
    if [[ -f "$GIT_BASH_PATH_WSL" ]]; then
        THIS_BASE_NAME=$(basename "$0")
        "$GIT_BASH_PATH_WSL" "$THIS_BASE_NAME" $@
        exit $?
    else
        echo "Git Bash not found, please install Git Bash!" >&2
        exit 1
    fi
fi

if [[ -f ~/.bash_profile ]]; then
    source ~/.bash_profile
fi

export RED_BOLD="\033[1;31m"
export RED_BOLD_RESET="\033[0m"

function isWindows() {
    # check mingw and cygwin
    if isWsl; then
        # 如果 $0 路径不以 /mnt 开头, 那么在 WSL 下按照正经 Linux 处理(isWindows 返回 1)
        [[ "$THIS_DIR" =~ ^/mnt/ ]] && return 0 || return 1
    fi

    [[ -d "/c" ]] || [[ -d "/cygdrive/c" ]]
}

function isMacOS() {
    [[ $(uname -s) == "Darwin" ]]
}

if ! command -v grealpath >/dev/null && command -v realpath >/dev/null; then
    function grealpath() {
        realpath $@
    }
fi

THIS_DIR=$(pwd)

# 如果是在 vscode 里面调用, 那么不给错误码, 否则会导致 vscode tasks 无法正常执行任务并显示错误.

if [[ "$TERM_PROGRAM" =~ vscode ]]; then
    export EXIT_WHEN_FAILED=false
else
    export EXIT_WHEN_FAILED=true
fi

function cmakeLoad() {
    cd "$THIS_DIR"
    mkdir -p build && cd build || exit 1
    CONBINED_CMAKE_BUILD_DEFINE="${LIVE_MATE_CMAKE_ARG[@]}"

    if isWindows; then
        echo cmd "/C cmake.exe $CI_BUILD_ARGS $MY_CMAKE_BUILD_DEFINE $CONBINED_CMAKE_BUILD_DEFINE ../platform/desktop"
        cmd.exe "/C cmake.exe $CI_BUILD_ARGS $MY_CMAKE_BUILD_DEFINE $CONBINED_CMAKE_BUILD_DEFINE ../platform/desktop"
    else
        echo cmake $CI_BUILD_ARGS $MY_CMAKE_BUILD_DEFINE $CONBINED_CMAKE_BUILD_DEFINE ../platform/desktop
        cmake $CI_BUILD_ARGS $MY_CMAKE_BUILD_DEFINE $CONBINED_CMAKE_BUILD_DEFINE ../platform/desktop
    fi
}

function cleanProject() {
    if [[ -d "$BUILD_DIR" ]]; then
        git clean -ffdx build* || echo "git clean skipped"
    fi
}

function cmakeReload() {
    cleanProject
    cmakeLoad
}

function cmakeBuiild() {
    # 如果  MY_CMAKE_BUILD_DEFINE 里面包含 =Release, 那么就是 release 模式, 否则就是 debug 模式
    if [[ $MY_CMAKE_BUILD_DEFINE == *"Release"* ]]; then
        USER_CMAKE_BUILD_DEFINE="--config Release $USER_CMAKE_BUILD_DEFINE"
    else
        USER_CMAKE_BUILD_DEFINE="--config Debug $USER_CMAKE_BUILD_DEFINE"
    fi

    cd "$BUILD_DIR"

    if isWindows; then
        echo start: cmake.exe --build . $USER_CMAKE_BUILD_DEFINE -- /m
        # ref: https://stackoverflow.com/questions/11865085/out-of-a-git-console-how-do-i-execute-a-batch-file-and-then-return-to-git-conso
        if ! cmd "/C cmake.exe --build . $USER_CMAKE_BUILD_DEFINE -- /m" && $EXIT_WHEN_FAILED; then
            exit 1
        fi
        echo end: cmake.exe --build . $USER_CMAKE_BUILD_DEFINE -- /m
    else
        if ! cmake --build . $USER_CMAKE_BUILD_DEFINE -- -j $(getconf _NPROCESSORS_ONLN) && $EXIT_WHEN_FAILED; then
            exit 1
        fi
    fi
}

while [[ $# -gt 0 ]]; do
    key="$1"
    case $key in
    --load)
        cmakeLoad
        shift
        ;;
    --reload)
        cmakeReload
        shift
        ;;
    --build)
        echo "Building..."
        if [[ ! -d "$BUILD_DIR/CMakeFiles" ]]; then
            cmakeLoad
        fi
        cmakeBuiild
        shift
        ;;
    -t | --target)
        export USER_CMAKE_BUILD_DEFINE="$USER_CMAKE_BUILD_DEFINE --target $2"
        shift
        shift
        ;;
    --release)
        echo "enable release mode"
        # 先从 MY_CMAKE_BUILD_DEFINE 里面去掉 -DCMAKE_BUILD_TYPE=Debug
        MY_CMAKE_BUILD_DEFINE=$(echo $MY_CMAKE_BUILD_DEFINE | sed -e 's/-DCMAKE_BUILD_TYPE=Debug//g')
        # 再加上 -DCMAKE_BUILD_TYPE=Release
        MY_CMAKE_BUILD_DEFINE="$MY_CMAKE_BUILD_DEFINE -DCMAKE_BUILD_TYPE=Release"
        export RELEASE_ENABLED=true
        shift # past argument
        ;;
    --clean)
        echo "Cleaning..."
        cleanProject
        shift
        ;;
    --run)
        echo "Running demo..."
        cd "$BUILD_DIR" || exit 1
        if isWindows; then
            if [[ -f Debug/CameraDemo.exe ]]; then
                cd Debug
                ./CameraDemo.exe
            elif [[ -f Release/CameraDemo.exe ]]; then
                cd Release
                ./CameraDemo.exe
            else
                echo "CameraDemo.exe not found"
                exit 1
            fi
        elif isMacOS; then
            if [[ -f CameraDemo.app/Contents/MacOS/CameraDemo ]]; then
                cd CameraDemo.app/Contents/MacOS
                ./CameraDemo
            else
                echo "CameraDemo.app not found"
                exit 1
            fi
        else
            echo "Unsupported platform"
            exit 1
        fi
        shift
        ;;
    *)
        echo "Unknown option: $1"
        exit 1
        ;;
    esac
done
