# MikanXR
A simple tool for mixed reality camera calibration and video compositing

<!-- AI_USAGE_BADGES:BEGIN -->
![AI tokens](https://img.shields.io/badge/AI_tokens-14M_out_%2F_5.5B_read-blueviolet) ![est. energy](https://img.shields.io/badge/est._energy-~303_kWh-yellow) ![est. water](https://img.shields.io/badge/est._water-~909_L-blue)
(estimates, see [TOKEN_STATS.md](TOKEN_STATS.md))
<!-- AI_USAGE_BADGES:END -->

[![Watch the video(https://user-images.githubusercontent.com/3753542/186327800-949914af-447a-473b-88ae-83e3d18be67d.png)]](https://user-images.githubusercontent.com/3753542/186327162-e775ed7c-5caa-484b-afee-804d27fa2b6a.mp4)

# IMPORTANT NOTE
This project is still heavily under development and is not really in a usuable state yet. Look at this issue for last status of things

# Prebuilt Releases
You can download prebuilt releases (Windows only at the moment) from the [Releases](https://github.com/brendanwalker/MikanXR/releases) page. Then follow the initial setup instructions found in the [wiki](https://github.com/brendanwalker/MikanXR/wiki#initial-setup). 

# Building from source
If you want to make modifications to the service or want to debug it, you can build the project from source by following the  [Building-from-source](https://github.com/brendanwalker/MikanXR/wiki/Building-from-source) instructions. Currently the supported build platform is Win10 with Linux support hopefully coming in the near future.

# Reporting a crash
When MikanXR crashes it writes a report to `Documents\MikanXR\CrashReports` and offers to open that folder the next time it starts. Please attach the report's `.dmp`, `.txt` and `.log` files to a [GitHub issue](https://github.com/brendanwalker/MikanXR/issues) along with what you were doing. Nothing is uploaded automatically.

# Contributing
Code under `src/` is formatted with clang-format and enforced by CI. See [CONTRIBUTING.md](CONTRIBUTING.md) for how to format your changes (`cmake --build build --target FormatFix`).

The app's UI text is currently machine translated. Fixing it needs no build and no C++, just [POEdit](https://poedit.net/) and a pull request: see [docs/TRANSLATING.md](docs/TRANSLATING.md).

# Documentation
* General setup guides, troubleshooting and design docs can be found on the [wiki](https://github.com/brendanwalker/MikanXR/wiki)

# Usage in other projects
Feel free to use this code as-is, modified or adapted in other projects (commercial or otherwise). It's currently intended to be a reference that I hope other can make use of in any way they see fit.
