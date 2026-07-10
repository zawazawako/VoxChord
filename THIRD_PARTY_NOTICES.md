# Third-Party Notices

VoxChord itself is licensed under the GNU Affero General Public License v3.0 (see [LICENSE](LICENSE)).

VoxChord is built using, links against, or is designed to work with the third-party frameworks, SDKs and technologies listed below. This repository does not include their source code; they are obtained separately (see the Dependencies section of [README.md](README.md)). Their code is, however, linked into the compiled VST3 / Standalone binaries, so these notices apply to any binary distribution of VoxChord.

---

## JUCE

VoxChord is built with JUCE, a C++ framework for audio applications and plug-ins.

- Project: https://github.com/juce-framework/JUCE
- Website: https://juce.com/
- Licence: the JUCE Framework modules are dual-licensed under the GNU Affero General Public License version 3 (AGPLv3) and the commercial JUCE licence. VoxChord is distributed as open source under the AGPLv3, i.e. under the AGPLv3 option.
- Copyright: JUCE is copyright Raw Material Software Limited and/or its contributors.

---

## melatonin_blur

Cached drop and inner shadows used by the plugin editor. Linked into both Debug and Release builds.

- Project: https://github.com/sudara/melatonin_blur
- Licence: MIT

```
MIT License

Copyright (c) 2023 Sudara Williams

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## melatonin_inspector

A component inspector used as a development tool. **Linked into Debug builds only**; it is not present in released Release binaries.

- Project: https://github.com/sudara/melatonin_inspector
- Licence: MIT

```
MIT License

Copyright (c) 2021 Sudara Williams

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## awesome-juce (reference only)

A curated list of JUCE resources.

- Project: https://github.com/sudara/awesome-juce
- Licence: MIT

No code from this repository is compiled into or distributed with VoxChord, so its licence imposes no obligation here. It is recorded only because it was consulted during development.

---

## VST 3 SDK / VST 3 Technology

VoxChord can be built as a VST3 audio plug-in.

- Project: https://github.com/steinbergmedia/vst3sdk
- Developer portal: https://steinbergmedia.github.io/vst3_dev_portal/
- Licence: the VST 3 SDK is available under the MIT License.
- Notice: "VST" is a trademark of Steinberg Media Technologies GmbH.

---

## ASIO Technology

The VoxChord standalone application can use ASIO devices through JUCE audio device support when ASIO support is enabled.

- Website: https://www.steinberg.net/developers/
- Licence: Steinberg ASIO technology is available in open-source form under the GNU General Public License version 3.
- Notice: ASIO is a trademark and software technology of Steinberg Media Technologies GmbH.

---

## Microsoft Visual C++ Runtime

VoxChord Windows binaries may depend on the Microsoft Visual C++ Runtime depending on the build configuration.

- Website: https://learn.microsoft.com/cpp/windows/latest-supported-vc-redist
- Licence: Microsoft Visual C++ Redistributable components are provided by Microsoft under Microsoft's own license terms.
- Notice: Microsoft, Windows, and Visual Studio are trademarks or registered trademarks of Microsoft Corporation.

---

## Other dependencies

No additional third-party DSP libraries are bundled with VoxChord.

If additional libraries, assets, fonts, icons, or sample audio files are added in the future, their licence notices should be added to this file.
