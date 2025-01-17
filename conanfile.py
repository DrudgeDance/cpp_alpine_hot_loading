from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain

class YourAppConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "*:shared": False,  # Force all dependencies to be static
    }

    def requirements(self):
        self.requires("boost/1.83.0")

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        # Define a flat build structure
        self.folders.source = "."
        self.folders.build = "."  # Set to root to prevent nesting
        self.folders.generators = "generators"
        
        # Configure cpp layout
        self.cpp.source.includedirs = ["include"]
        self.cpp.build.libdirs = ["lib"]
        self.cpp.build.bindirs = ["bin"]
        self.cpp.build.objects = ["obj"]

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC", True)
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.generate()
        
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build() 