from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain

class AlpineAppConan(ConanFile):
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
        self.requires("boost/1.84.0", options={
            "without_log": False,  # Enable log (includes log_setup)
            "without_thread": False,  # Required for logging
            "without_filesystem": False,  # Required for logging
            "without_date_time": False,  # Required for logging
            "without_regex": False,  # Required for logging
            "without_chrono": False,  # Required for logging
            "without_atomic": False,  # Required for logging
            "shared": False,  # Force static linking
            "header_only": False,  # Ensure we build the libraries
            "error_code_header_only": False,
            "system_no_deprecated": False,
            "asio_no_deprecated": False,
            "filesystem_no_deprecated": False,
            "visibility": "hidden"
        })
        self.requires("openssl/3.2.0", options={
            "shared": False,  # Force static linking
        })

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
        self.cpp.build.libdirs = ["build/lib"]
        self.cpp.build.bindirs = ["bin"]
        self.cpp.build.objects = ["build/obj"]

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_POSITION_INDEPENDENT_CODE"] = self.options.get_safe("fPIC", True)
        tc.variables["BUILD_SHARED_LIBS"] = False
        tc.generate()
        
    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package_info(self):
        # Declare boost_log_setup as a system library
        self.cpp_info.system_libs.append("boost_log_setup") 