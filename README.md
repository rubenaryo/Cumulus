<div align="center">
  <h1 align="center">Cumulus</h1>

  <a href="https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B">
    <img src="https://img.shields.io/badge/C%2B%2B-17-blue.svg?style=flat-square&logo=c%2B%2B" alt="C++17" />
  </a>
  <a href="https://img.shields.io/badge/DirectX-12-green.svg?style=flat-square&logo=windows">
    <img src="https://img.shields.io/badge/DirectX-12-green.svg?style=flat-square&logo=windows" alt="DirectX 12" />
  </a>
  <a href="https://img.shields.io/badge/Platform-Windows-lightgrey.svg?style=flat-square&logo=windows">
    <img src="https://img.shields.io/badge/Platform-Windows-lightgrey.svg?style=flat-square&logo=windows" alt="Windows" />
  </a>
  <a href="https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square">
    <img src="https://img.shields.io/badge/License-MIT-yellow.svg?style=flat-square" alt="License" />
  </a>

  <br>

  **A DirectX 12 implementation of real-time interactive volumetric cloud.**

  <br>

  <img width="100%" alt="Real-time Flythrough" src="images/fly_in.gif" />
  <br>
  <em>Real-time volumetric fly-through</em>

</div>
<br>

# Overview
**Cumulus** is a real-time, volumetric cloud rendering engine built from scratch in DirectX 12. 

It extends the architectural principles of Guerrilla Games' **[Nubis 3](https://www.guerrilla-games.com/read/nubis-cubed)** technology by introducing a fully procedural, **GPU-driven generation pipeline**. Unlike static implementations, Cumulus supports dynamic object interaction (collisions), real-time weather transitions, and variable density modeling without offline pre-computation.

# Table of Contents
- [Overview](#overview)
- [Features](#features)
  - [Volumetric Cloud Rendering](#volumetric-cloud-rendering)
  - [Procedural Cloud Generation](#procedural-cloud-generation)
  - [DirectX 12 Engine](#muon-a-directx-12-engine)
  - [Physically-Based Atmosphere](#physically-based-atmosphere)
- [Setup & Development](#setup--development)
  - [Building](#building)
  - [Technical Details](#technical-details)
  - [Dependencies](#dependencies)
- [Appendices](#appendices)
  - [External Credits](#external-credits)
  - [Related Presentations](#related-presentations)
  - [Bloopers](#bloopers)

# Features
## Volumetric Cloud Rendering
<p align="center">
  <img width="80%" alt="image" src="images/beauty.png" />
  <br>
  <em>A nice cloud in our engine</em>
</p>

### Rendering

 - Ray-marched rendering — integrates density with Beer–Lambert absorption/compositing
 - SDF-guided stepping — signed-distance field cached in 3D textures to skip empty space
 - Noise-based Details — Additional details at 0.5m scale using Alligator and "Curly-Alligator" noise
 - Cloud Lighting - interactive lighting using atmosphere, as well as multiple scattering and ambient light
## Procedural Cloud Generation
<p align="center">
  <img width="80%" alt="image" src="images/CloudControl.gif" />
  <br>
  <em>Clouds created in real-time in a compute shader</em>
</p>

 - We support procedurally creating cloud NVDF data in a compute shader pass, which can be controlled in an ImGUI tab specifying cloud number and their average scale multiplier.
 - Creation starts by initializing cloud "seeds" on the CPU side, which become world-space coordinates where clouds get initialized. These positions can be updated to show cloud movement and formation.
 - For each seed, we create an SDF (given by [Inigo Quilez's blog](https://iquilezles.org/articles/distfunctions/)) that is a simple round cone with "Vesica Segments" aka football shapes around them. The number of these shapes, their size, and the orientation of all of these is given by noise, scaled by the input scale value.
 - Next, based on this SDF, we use a special "billow noise," which is a modified Perlin noise that gets its cells rotated to give a billowy look, as shown by the noise's authors [here](https://www.shadertoy.com/view/fdfcWs). Importantly, this noise gets the fractal sum treatment to ease out values for a less voxely look. It also gets eased out as we move away from the sdf boundaries for the same reason.
 - Finally, detail type and density scale get calculated, which give the clouds their upscaled features. Detail type comes from the same billowy noise at a different scale and gets more intense the farther up we go, to mimic how clouds are more whispy lower down, and density scale is quite uniform for us within the cloud's profile.

## Muon: A DirectX 12 Engine
<table align="center">
  <tr>
    <td align="center">
      <img src="images/Muon_Diagram.png"  width = "100%"/>
      <br>
      <em>Engine overview diagram</em>
    </td>
    <td align="center">
      <img src="images/Render Pipeline.png"  width = "110%"/>
      <br>
      <em>Full render pipeline</em>
    </td>
  </tr>
</table>

### Core
 - Atmosphere rendering pass
 - Compute Shader pipeline for ray-marching
 - Compute Shader for collisions and cloud data generation
 - Post-processing pipeline
 - Automated loading of models/textures from files
 - Construction of 3D NVDF data fields for the core Nubis method, 3D textures for atmosphere
### Extra Features
 - String-based, shader-driven resource binding for easy user experience
    - Driven by ID3D12ShaderReflection
 - Rendering is abstracted into a “Pass” framework, 
    - Automatically generates Root Signatures and Pipeline States (shader-driven)
 - Upload Buffer system for staging CPU data temporarily before copying to the default heap
 - Automatic lifetime reporting for catching memory leaks in Debug mode
 - Diligent error detection and logging
 - ImGUI integration

<!-- 
Add this back later once more complete  

### Wind

![output](https://github.com/user-attachments/assets/de29dfd9-aaee-43ab-8449-aa158530d611)
 -->


## Physically-Based Atmosphere
<p align="center">
  <img width="80%" alt="image" src="images/atmosphere.png" />
  <br>
  <em>Sunrise, daytime, sunset, night time, with the ImGUI controls</em>
</p>

 - Based on [Eric Bruneton's Precomputed Atmospheric Scattering](https://ebruneton.github.io/precomputed_atmospheric_scattering/)
  - Skips precompute to read from offline Irradiance, Scattering, and Transmission textures
  - Blends Polar and Cartesian camera models
  - Day and night cycle with selectable time of day
  - Fully calculated in a pre-pass with raycasting
  - Added moon and night time sky.
  - Daytime can be modified in the UI to set sun position.
# Setup & Development 
## Building
This project uses the Premake 5 build system, which is bundled with the application and the executable can be found under ./external/

To generate a Visual Studio solution, simply run generate_vs2022.bat on Windows. This will: 
- Generate a "Cumulus.sln" solution file
- Generate and configure the VS projects specified under ./premake5.lua
- Any Source/Header Files in the specified folder will be automatically added to the corresponding project. It is not necessary to modify the lua build script if adding a new file. 

## Technical Details
This project is built using MSVC with the Visual Studio 2022 toolset (v143) for the C++17 standard.

## Dependencies
* [DirectX Tex](https://github.com/microsoft/DirectXTex/)
  * Reading image files for texture generation
* [Assimp 3.0.0](http://www.assimp.org/)
  * Loading 3D Models

# Appendices
## External Credits
 - [Nubis 3](https://www.guerrilla-games.com/read/nubis-cubed), the presentation behind this whole project
 - Stefan Gustavson and Ian MacEwan for making [billowy noise](https://github.com/stegu/psrdnoise/), and Stefan and Ashima Arts for [fast perlin noise](https://github.com/ashima/webgl-noise/tree/master) as well
 - Domenic Portera for the [HLSL port](https://github.com/domportera/hlsl-noise/tree/main) of the billowy noise.
 - [Eric Bruneton's Precomputed Atmospheric Scattering](https://ebruneton.github.io/precomputed_atmospheric_scattering/)
 - [Inigo Quilez's blog on SDFs](https://iquilezles.org/articles/distfunctions/)

## Related Presentations 
- [Milestone 1 Presentation](https://docs.google.com/presentation/d/1gGSEbZ7L8bbZHOn7OLQdZIwVBOCtymcEXTAHn48AE7w/edit?usp=sharing)
- [Milestone 2 Presentation](https://docs.google.com/presentation/d/1K_11dz4fgYK21hM76VZPrON-3IzXVvsdjsIIQxzjrf8/edit?usp=sharing)

## Bloopers
<p align="center">
  <img width="80%" alt="image" src="images/bloopers/atmosphere_disco.gif" />
  <br>
  <em>Broken Atmospheric Scattering</em>
</p>
<p align="center">
  <img width="80%" alt="image" src="images/bloopers/botched_camera.png" />
  <br>
  <em>Broken Camera Matrix while working on Atmospheric Scattering</em>
</p>
