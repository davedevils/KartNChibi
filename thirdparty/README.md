# Third-Party Libraries

This directory contains third-party libraries used by the KartNChibi Engine.
All libraries are MIT/BSD licensed and cross-platform compatible.

## Libraries

### tinyddsloader
- **Purpose**: DDS texture loading (DXT1-5, BC1-7 compressed formats)
- **License**: MIT
- **Source**: https://github.com/benikabocha/tinyddsloader
- **Type**: Header-only

Used as fallback when built-in DDS support doesn't handle
GameBryo-specific DDS formats.

### libjpeg 9f (IJG)
- **Purpose**: Fallback JPEG decoder for exotic formats
- **License**: IJG License (BSD-like)
- **Source**: http://www.ijg.org/files/jpegsr9f.zip
- **Type**: Source files (.c/.h)
- **Version**: 9f (2024-01-14)

**This software is based in part on the work of the Independent JPEG Group.**

Included files are the decompression subset only (no compression).
Most JPEG files work with Raylib's stb_image. Only needed for CMYK 
color space or unusual encodings.

To enable: Define `KNC_USE_LIBJPEG_FALLBACK` and compile the .c files.

## Texture Loading Chain

The engine uses this fallback chain:

1. **Raylib/stb_image** (built into Raylib)
   - PNG, JPEG, BMP, TGA, HDR, GIF
   - Common DDS formats
   - Fast, well-tested

2. **tinyddsloader**
   - DDS files that stb_image can't handle
   - BC1-BC7 compressed textures
   - Legacy DXT formats from GameBryo

3. **libjpeg** (if enabled)
   - Exotic JPEG encodings
   - CMYK color space
   - Unusual subsampling

## License Notices

### tinyddsloader
```
Copyright (c) 2019 benikabocha
MIT License - see tinyddsloader/LICENSE
```

### libjpeg (if used)
```
This software is based in part on the work of the Independent JPEG Group.
See libjpeg/README.md for license details.
```

### stb_image 
```
Public Domain - https://github.com/nothings/stb
```

