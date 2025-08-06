from conan import ConanFile 
from conan.tools.meson import MesonToolchain
from conan.tools.gnu import PkgConfigDeps


class XpumConan(ConanFile):
    name = "xpum"
    version = "1.0.0"
    
    # Package metadata
    license = "MIT"
    description = "Intel(R) XPU Manager and XPU System Management Interface"
    url = "https://github.com/intel/xpum"
    
    # Settings and options
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
        "with_igsc": [True, False],
        "with_tests": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
        "with_igsc": True,
        "with_tests": False,
    }
    
    def requirements(self):
        self.requires("level-zero/1.23.1")
        self.requires("igsc/0.9.6")
        self.requires("nlohmann_json/3.11.3")
        
        # Platform-specific dependencies
        if self.settings.os == "Linux":
            pass
        

    def build_requirements(self):
        self.tool_requires("meson/1.3.2")
        self.tool_requires("ninja/1.13.1")  # Use the version compatible with meson
        
        # Development tools
        if self.options.with_tests:
            self.tool_requires("cmake/3.25.3")  # For some test dependencies

    def configure(self):
        # Configure fPIC based on shared option
        if self.options.shared:
            self.options.rm_safe("fPIC")
        
        # Platform-specific configuration
        if self.settings.os == "Windows":
            # Windows doesn't need fPIC
            self.options.rm_safe("fPIC")

    def generate(self):
        pc = PkgConfigDeps(self)
        pc.generate()
        # Generate Meson toolchain
        tc = MesonToolchain(self)
        
        # Add custom meson options based on conan options
        if not self.options.with_igsc:
            tc.project_options["use_system_igsc"] = False
            
        tc.generate()
        