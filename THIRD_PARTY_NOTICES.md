# Third-party notices

Pyros3D's own licence is in [LICENSE.md](LICENSE.md). Code adapted from the projects below keeps their notices here, as their licences require.

## Ultra Water (Humpalumps)

- Source: https://github.com/Humpalumps/Ultra-Water
- Used in: `src/Pyros3D/Rendering/Water/OceanSim.cpp` and `include/Pyros3D/Rendering/Water/OceanSim.h` (the FFT ocean: cascade layout, JONSWAP spectrum with cos-2s spreading, gravity-capillary dispersion, looped time, field packing, slope moments for roughness, Jacobian foam), the sea shader of the NatureLab demo (variance roughness, Cox-Munk remainder, geometric specular AA, roughness-aware Fresnel, Poisson sun glitter, the shore breakers on a travel-time field, the whitewater and surf lace), and NatureLab's tools build_seagrid.py (the polar grid), build_shore.py / shore_bake.cpp (the shore map: EDT, fast-sweeping eikonal, ray-tube shoaling) and the baked foam texture.
- Copyright (c) 2026 Humpalumps
- MIT License

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
