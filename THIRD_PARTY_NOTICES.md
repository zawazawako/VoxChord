# Third-Party Notices

VoxChord itself is licensed under the GNU Affero General Public License v3.0 (see [LICENSE](LICENSE)).

This document lists third-party components that VoxChord builds against, together with their licences and the notices those licences require. The components are not redistributed in this repository; they are cloned separately (see the Dependencies section of [README.md](README.md)). Their code is, however, linked into the compiled VST3 / Standalone binaries, so these notices apply to any binary distribution of VoxChord.

---

## JUCE Framework

- Source: https://github.com/juce-framework/JUCE
- Licence: dual-licensed under the AGPLv3 and the commercial JUCE licence.

VoxChord is distributed under the AGPLv3, which is compatible with the AGPLv3 option of the JUCE Framework modules. See JUCE's own `LICENSE.md` for the full terms.

---

## melatonin_blur

- Source: https://github.com/sudara/melatonin_blur
- Licence: MIT
- Used for: cached drop and inner shadows in the plugin editor. Linked into both Debug and Release builds.

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

- Source: https://github.com/sudara/melatonin_inspector
- Licence: MIT
- Used for: the component inspector development tool. **Linked into Debug builds only**; it is not present in Release binaries.

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

- Source: https://github.com/sudara/awesome-juce
- Licence: MIT

A curated list of JUCE resources. No code from this repository is compiled into or distributed with VoxChord, so its licence imposes no obligation here. It is recorded only because it was consulted during development.
