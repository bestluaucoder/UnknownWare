#pragma once

#include <string>
#include <Windows.h>
#include <winhttp.h>
#include <fstream>
#include <sstream>
#include <regex>

#pragma comment(lib, "winhttp.lib")

namespace offset_updater {

    struct UpdateResult {
        bool success;
        std::string message;
        std::string version;
        std::string content;
    };

    // Make HTTPS GET request to rbxoffsets.xyz API
    std::string https_request(const std::wstring& host, const std::wstring& path) {
        HINTERNET hSession = nullptr;
        HINTERNET hConnect = nullptr;
        HINTERNET hRequest = nullptr;
        std::string response;

        try {
            // Initialize WinHTTP
            hSession = WinHttpOpen(L"RBX External Updater/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS, 0);

            if (!hSession) return "";

            // Connect to server
            hConnect = WinHttpConnect(hSession, host.c_str(),
                INTERNET_DEFAULT_HTTPS_PORT, 0);

            if (!hConnect) {
                WinHttpCloseHandle(hSession);
                return "";
            }

            // Create request
            hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                nullptr, WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);

            if (!hRequest) {
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return "";
            }

            // Add required custom header
            std::wstring headers = L"rbxoffsets.xyz: apiv1\r\n";
            WinHttpAddRequestHeaders(hRequest,
                headers.c_str(),
                -1L,
                WINHTTP_ADDREQ_FLAG_ADD);

            // Send request
            if (!WinHttpSendRequest(hRequest,
                WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0,
                0, 0)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return "";
            }

            // Wait for response
            if (!WinHttpReceiveResponse(hRequest, nullptr)) {
                WinHttpCloseHandle(hRequest);
                WinHttpCloseHandle(hConnect);
                WinHttpCloseHandle(hSession);
                return "";
            }

            // Read response
            DWORD bytesAvailable = 0;
            DWORD bytesRead = 0;
            std::string buffer;

            do {
                bytesAvailable = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable))
                    break;

                if (bytesAvailable == 0)
                    break;

                char* tempBuffer = new char[bytesAvailable + 1];
                ZeroMemory(tempBuffer, bytesAvailable + 1);

                if (WinHttpReadData(hRequest, tempBuffer, bytesAvailable, &bytesRead)) {
                    response.append(tempBuffer, bytesRead);
                }

                delete[] tempBuffer;
            } while (bytesAvailable > 0);

            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);

            return response;
        }
        catch (...) {
            if (hRequest) WinHttpCloseHandle(hRequest);
            if (hConnect) WinHttpCloseHandle(hConnect);
            if (hSession) WinHttpCloseHandle(hSession);
            return "";
        }
    }

    // Get latest Roblox version
    std::string get_latest_version() {
        std::string response = https_request(L"rbxoffsets.xyz", L"/api/version/raw");
        
        // Trim whitespace
        response.erase(0, response.find_first_not_of(" \n\r\t"));
        response.erase(response.find_last_not_of(" \n\r\t") + 1);
        
        return response;
    }

    // Get latest offsets content
    std::string get_latest_offsets() {
        return https_request(L"rbxoffsets.xyz", L"/api/latest/raw");
    }

    // Parse offset content and update offset.h file
    bool update_offset_file(const std::string& content, const std::string& version, const std::string& filepath) {
        try {
            std::ofstream file(filepath);
            if (!file.is_open()) {
                return false;
            }

            // Write header
            file << "#pragma once\n\n";
            file << "#include <cstdint>\n";
            file << "#include <string>\n";
            file << "namespace offset {\n";
            file << "    inline std::string ClientVersion = \"" << version << "\";\n\n";

            // Write the offset content (assuming it's already formatted)
            file << content;

            file << "\n}\n";
            file.close();

            return true;
        }
        catch (...) {
            return false;
        }
    }

    // Main update function
    UpdateResult check_and_update(const std::string& current_version, const std::string& offset_file_path) {
        UpdateResult result;
        result.success = false;

        // Get latest version
        std::string latest_version = get_latest_version();
        if (latest_version.empty()) {
            result.message = "Failed to fetch latest version from API";
            return result;
        }

        result.version = latest_version;

        // Check if update is needed
        if (latest_version == current_version) {
            result.success = true;
            result.message = "Offsets are already up to date (" + current_version + ")";
            return result;
        }

        // Get latest offsets
        std::string offsets_content = get_latest_offsets();
        if (offsets_content.empty()) {
            result.message = "Failed to fetch latest offsets from API";
            return result;
        }

        result.content = offsets_content;

        // Update offset file
        if (!update_offset_file(offsets_content, latest_version, offset_file_path)) {
            result.message = "Failed to write offsets to file";
            return result;
        }

        result.success = true;
        result.message = "Successfully updated offsets from " + current_version + " to " + latest_version;
        return result;
    }

    // Silent update check (for background updates)
    bool silent_update_check(const std::string& current_version) {
        std::string latest_version = get_latest_version();
        return !latest_version.empty() && latest_version != current_version;
    }

}
