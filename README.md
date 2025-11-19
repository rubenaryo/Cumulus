# Cumulus
A DirectX 12 project for real-time enabled cloud simulation and interactivity.

Made by [Ruben Young]() [Jacky Park]() [Eli Asimow](https://easimow.com/) and [Avi Serebrenik](https://aviserebrenik.wixsite.com/cvsite), Fall 2025

[Project Document](https://docs.google.com/document/d/1CNzmIo68LndPGS8ccK94FSIFHYH-5GL8HYOYqQdy90E/edit?usp=sharing)
[Milestone 1 Presentation](https://docs.google.com/presentation/d/1gGSEbZ7L8bbZHOn7OLQdZIwVBOCtymcEXTAHn48AE7w/edit?usp=sharing)

<p align="center">
  <img width="80%" alt="image" src="images/fly_in.gif" />
  <br>
  <em>Camera flying in to a cloud</em>
</p>
<p align="center">
  <img width="80%" alt="image" src="images/beauty.png" />
  <br>
  <em>A nice cloud in our engine</em>
</p>

## Overview
Description of general project here

## Features
### Engine
Ruben's stuff goes here
#### Core
#### Extra Features
### Clouds
#### Rendering
Jacky's stuff goes here
#### Generation
### Collision
Eli's stuff goes here
### Atmosphere
Avi's stuff goes here

## Building
This project uses the Premake 5 build system, which is bundled with the application and the executable can be found under ./external/

To generate a Visual Studio solution, simply run generate_vs2022.bat on Windows. This will: 
- Generate a "Cumulus.sln" solution file
- Generate and configure the VS projects specified under ./premake5.lua
- Any Source/Header Files in the specified folder will be automatically added to the corresponding project. It is not necessary to modify the lua build script if adding a new file. 

## Details
This project is built using MSVC with the Visual Studio 2022 toolset (v143) for the C++17 standard.

## Dependencies
* [DirectX Tex](https://github.com/microsoft/DirectXTex/)
  * Reading image files for texture generation
* [Assimp 3.0.0](http://www.assimp.org/)
  * Loading 3D Models

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
