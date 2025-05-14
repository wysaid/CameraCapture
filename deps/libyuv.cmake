cmake_minimum_required(VERSION 3.10)

include(FetchContent)

if(NOT DEFINED LIBYUV_REPO_PRIMARY)
    set(LIBYUV_REPO_PRIMARY "https://github.com/lemenkov/libyuv.git")
endif()

if(NOT DEFINED LIBYUV_REPO_SECONDARY)
    set(LIBYUV_REPO_SECONDARY "git@github.com:lemenkov/libyuv.git")
endif()

if(NOT DEFINED LIBYUV_VERSION)
    # This is the version as of 2025/5/15. You can use a newer version if needed. We do not use a branch here to avoid untested changes.
    set(LIBYUV_VERSION "0853c93")
endif()

# 尝试主仓库
FetchContent_Declare(
    libyuv
    GIT_REPOSITORY ${LIBYUV_REPO_PRIMARY}

    GIT_TAG ${LIBYUV_VERSION}
)
FetchContent_GetProperties(libyuv)

if(NOT libyuv_POPULATED)
    message(STATUS "Trying to fetch libyuv from primary repo: ${LIBYUV_REPO_PRIMARY}")
    FetchContent_Populate(libyuv)

    if(NOT EXISTS "${libyuv_SOURCE_DIR}/README.md")
        message(WARNING "Failed to clone ${LIBYUV_REPO_PRIMARY}, trying secondary repo: ${LIBYUV_REPO_SECONDARY}")

        # 重新声明并尝试备用仓库
        FetchContent_Declare(
            libyuv
            GIT_REPOSITORY ${LIBYUV_REPO_SECONDARY}
            GIT_TAG ${LIBYUV_VERSION}
        )
        FetchContent_Populate(libyuv)
    endif()
endif()

if(NOT libyuv_POPULATED)
    message(WARNING "Failed to fetch libyuv, now disable libyuv")
    set(ENABLE_LIBYUV OFF CACHE BOOL "Disable libyuv" FORCE)
else()
    set(LIBYUV_DIR ${libyuv_SOURCE_DIR} CACHE PATH "libyuv source directory" FORCE)
endif()