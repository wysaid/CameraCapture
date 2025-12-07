from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os

class CcapConan(ConanFile):
    name = "ccap"
    version = "1.3.2"
    license = "MIT"
    author = "wysaid (this@wysaid.org)"
    url = "https://github.com/wysaid/CameraCapture"
    description = "A C/C++ library for camera capture"
    topics = ("camera", "capture", "video", "cpp")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "no_log": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "no_log": False,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CCAP_BUILD_SHARED"] = self.options.shared
        tc.variables["CCAP_NO_LOG"] = self.options.no_log
        tc.variables["CCAP_BUILD_EXAMPLES"] = False
        tc.variables["CCAP_BUILD_TESTS"] = False
        tc.variables["CCAP_INSTALL"] = True
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        self.cpp_info.libs = ["ccap"]
        
        if self.settings.os == "Macos":
            self.cpp_info.frameworks.extend(["Foundation", "AVFoundation", "CoreVideo", "CoreMedia", "Accelerate"])
        elif self.settings.os == "Linux":
            self.cpp_info.system_libs.append("pthread")
