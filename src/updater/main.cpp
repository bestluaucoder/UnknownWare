#include "offset_updater.h"
#include <iostream>
#include <Windows.h>

#ifdef _CONSOLE
int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "   RBX External - Offset Updater\n";
    std::cout << "========================================\n\n";

    // Current version (this would normally be read from offset.h)
    std::string current_version = "version-5cf2272675e145f5";
    std::string offset_file_path = "src/engine/offset.h";

    // Check if custom path provided
    if (argc > 1) {
        offset_file_path = argv[1];
    }

    std::cout << "[*] Current version: " << current_version << "\n";
    std::cout << "[*] Checking for updates...\n\n";

    // Check and update
    auto result = offset_updater::check_and_update(current_version, offset_file_path);

    if (result.success) {
        std::cout << "[+] " << result.message << "\n";
        if (result.version != current_version) {
            std::cout << "[!] Please rebuild the project to apply changes.\n";
        }
        return 0;
    }
    else {
        std::cout << "[-] " << result.message << "\n";
        return 1;
    }
}
#else
// If built as DLL or integrated into main executable
namespace updater_integration {
    
    // Auto-update on startup (silent check)
    bool check_on_startup(const std::string& current_version) {
        return offset_updater::silent_update_check(current_version);
    }

    // Manual update with UI notification
    offset_updater::UpdateResult manual_update(const std::string& current_version, const std::string& offset_path) {
        return offset_updater::check_and_update(current_version, offset_path);
    }
}
#endif
