from __future__ import annotations

import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import collect_libs, copy, get

required_conan_version = ">=2.0"


class CcapConan(ConanFile):
    name = "ccap"
    package_type = "library"

    license = "MIT"
    url = "https://github.com/conan-io/conan-center-index"
    homepage = "https://ccap.work"
    description = "High-performance cross-platform camera capture library with fast pixel format conversion"
    topics = ("camera", "capture", "video", "v4l2", "directshow", "avfoundation")

    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "no_log": [True, False],
        "enable_file_playback": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "no_log": False,
        "enable_file_playback": True,
    }

    def config_options(self) -> None:
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self) -> None:
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")

    def validate(self) -> None:
        check_min_cppstd(self, 17)

        # iOS build is possible in the upstream project, but ConanCenter recipes
        # typically focus on desktop targets.
        if str(self.settings.os) in {"iOS", "watchOS", "tvOS"}:
            raise ConanInvalidConfiguration("ccap ConanCenter recipe currently targets desktop OSes")

    def layout(self) -> None:
        cmake_layout(self)

    def source(self) -> None:
        get(self, **self.conan_data["sources"][self.version], strip_root=True)

    def generate(self) -> None:
        deps = CMakeDeps(self)
        deps.generate()

        tc = CMakeToolchain(self)
        tc.cache_variables["CCAP_INSTALL"] = True
        tc.cache_variables["CCAP_BUILD_EXAMPLES"] = False
        tc.cache_variables["CCAP_BUILD_TESTS"] = False
        tc.cache_variables["CCAP_BUILD_CLI"] = False
        tc.cache_variables["CCAP_BUILD_CLI_STANDALONE"] = False

        tc.cache_variables["CCAP_BUILD_SHARED"] = bool(self.options.shared)
        tc.cache_variables["CCAP_NO_LOG"] = bool(self.options.no_log)
        tc.cache_variables["CCAP_ENABLE_FILE_PLAYBACK"] = bool(self.options.enable_file_playback)

        if self.options.get_safe("fPIC") is not None:
            tc.cache_variables["CMAKE_POSITION_INDEPENDENT_CODE"] = bool(self.options.fPIC)

        tc.generate()

    def build(self) -> None:
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self) -> None:
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()

    def package_info(self) -> None:
        self.cpp_info.libs = collect_libs(self)

        # Upstream installs a CMake config package named `ccap` and exports `ccap::ccap`.
        self.cpp_info.set_property("cmake_file_name", "ccap")
        self.cpp_info.set_property("cmake_target_name", "ccap::ccap")
        self.cpp_info.set_property("pkg_config_name", "ccap")
