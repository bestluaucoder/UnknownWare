# RBX External - Automatic Offset Updater

Automatically fetches the latest Roblox offsets from rbxoffsets.xyz API and updates your project.

## Quick Start

### Option 1: Python Script (Recommended)

**Requirements:**
- Python 3.x
- `requests` library

**Usage:**
```bash
# Double-click the batch file
update_offsets.bat

# Or run directly
python update_offsets.py
```

The script will:
1. Check your current offset version
2. Fetch the latest version from rbxoffsets.xyz
3. Download and apply the latest offsets if needed
4. Notify you to rebuild the project

### Option 2: C++ Integration

The C++ updater can be integrated into your main executable for automatic updates.

**Files:**
- `src/updater/offset_updater.h` - Core updater functionality
- `src/updater/main.cpp` - Standalone executable or integration code

**Integration Example:**
```cpp
#include "updater/offset_updater.h"

// On application startup
auto result = offset_updater::check_and_update(
    offset::ClientVersion,
    "src/engine/offset.h"
);

if (result.success && result.version != offset::ClientVersion) {
    // Notify user to restart application
    MessageBoxA(NULL, result.message.c_str(), "Update Available", MB_ICONINFORMATION);
}
```

## API Information

**Base URL:** https://rbxoffsets.xyz

**Endpoints Used:**
- `GET /api/version/raw` - Get latest Roblox version
- `GET /api/latest/raw` - Get latest offset content

**Required Header:**
```
rbxoffsets.xyz: apiv1
```

## Manual Update Process

If you prefer to update manually:

1. Visit https://rbxoffsets.xyz
2. Copy the latest offsets
3. Update `src/engine/offset.h`
4. Rebuild: `msbuild rbx-external.sln /p:Configuration=Release /p:Platform=x64`

## Troubleshooting

**"Python not found"**
- Install Python 3 from https://www.python.org/downloads/
- Make sure "Add Python to PATH" is checked during installation

**"Module 'requests' not found"**
- Run: `pip install requests`

**"Failed to fetch from API"**
- Check your internet connection
- Verify rbxoffsets.xyz is accessible
- Check if you're behind a firewall/proxy

**"Failed to write offset file"**
- Run as Administrator
- Check file permissions
- Ensure `src/engine/offset.h` exists

## After Updating

Always rebuild the project after updating offsets:

```cmd
cd c:\Users\grabi\OneDrive\Documents\rbloxext
msbuild rbx-external.sln /p:Configuration=Release /p:Platform=x64
```

## Automation

You can automate offset updates by:

1. **Scheduled Task** - Run `update_offsets.bat` daily
2. **Pre-build Event** - Add to Visual Studio build events
3. **Startup Check** - Integrate C++ updater into your main executable

## Notes

- Offsets are version-specific to Roblox client builds
- Always test after updating offsets
- Keep a backup of working offsets
- The API updates shortly after Roblox releases new versions

## API Rate Limits

The rbxoffsets.xyz API may have rate limiting. The updater:
- Makes minimal API calls (2 per update check)
- Uses proper headers to avoid blocks
- Handles errors gracefully

## Support

For API issues, visit: https://rbxoffsets.xyz
For project issues, check your implementation
