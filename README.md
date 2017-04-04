# AMD DepthOfFieldFX

![AMD DepthOfFieldFX](http://gpuopen-effects.github.io/media/effects/depthoffieldfx_thumbnail.png)

The DepthOfFieldFX library provides a GCN-optimized Compute Shader implementation of Depth of Field using the Fast Filter Spreading approach put forth by Todd Jerome Kosloff, Justin Hensley, and Brian A. Barsky in their 2009 paper – *Fast Filter Spreading and its Applications*.

<div>
  <a href="https://github.com/GPUOpen-Effects/DepthOfFieldFX/releases/latest/"><img src="http://gpuopen-effects.github.io/media/latest-release-button.svg" alt="Latest release" title="Latest release"></a>
</div>

The library implements depth of field using a Bartlett and Box filter. There is an option to run the Bartlett at quarter resolution for improved performance in some cases. The advantage of this approach is a fixed performance cost that is not dependent on kernel size. The library takes the scene Color Buffer and Circle of Confusion buffer as input and an Unordered Access View for the final results.

### Prerequisites
* AMD Radeon&trade; GCN-based GPU (HD 7000 series or newer)
  * Or other DirectX&reg; 11 compatible discrete GPU with Shader Model 5 support
* 64-bit Windows&reg; 7 (SP1 with the [Platform Update](https://msdn.microsoft.com/en-us/library/windows/desktop/jj863687.aspx)), Windows&reg; 8.1, or Windows&reg; 10
* Visual Studio&reg; 2013 or Visual Studio&reg; 2015

### Getting started
* Visual Studio solutions for VS2013 and VS2015 can be found in the `amd_depthoffieldfx_sample\build` directory.
* There are also solutions for just the core library in the `amd_depthoffieldfx\build` directory.
* Additional documentation is available in the `amd_depthoffieldfx\doc` directory.

### Premake
The Visual Studio solutions and projects in this repo were generated with Premake. If you need to regenerate the Visual Studio files, double-click on `gpuopen_depthoffieldfx_update_vs_files.bat` in the `premake` directory.

This version of Premake has been modified from the stock version to use the property sheet technique for the Windows SDK from this [Visual C++ Team blog post](http://blogs.msdn.com/b/vcblog/archive/2012/11/23/using-the-windows-8-sdk-with-visual-studio-2010-configuring-multiple-projects.aspx). The technique was originally described for using the Windows 8.0 SDK with Visual Studio 2010, but it applies more generally to using newer versions of the Windows SDK with older versions of Visual Studio.

By default, Visual Studio 2013 projects will compile against the Windows 8.1 SDK. However, the VS2013 projects generated with this version of Premake will use the next higher SDK (i.e. the Windows 10 SDK), if the newer SDK exists on the user's machine.

For Visual Studio 2015, the `systemversion` Premake function is used to add the `WindowsTargetPlatformVersion` element to the project file, to specify which version of the Windows SDK will be used. To change `WindowsTargetPlatformVersion` for Visual Studio 2015, change the value for `_AMD_WIN_SDK_VERSION` in `premake\amd_premake_util.lua` and regenerate the Visual Studio files.

### Third-Party Software
* DXUT is distributed under the terms of the MIT License. See `framework\d3d11\dxut\MIT.txt`.
* Premake is distributed under the terms of the BSD License. See `premake\LICENSE.txt`.

DXUT is only used by the sample, not the core library. Only first-party software (specifically `amd_depthoffieldfx` and `amd_lib`) is needed to build the DepthOfFieldFX library.

### Attribution
* AMD, the AMD Arrow logo, Radeon, and combinations thereof are either registered trademarks or trademarks of Advanced Micro Devices, Inc. in the United States and/or other countries.
* Microsoft, DirectX, Visual Studio, and Windows are either registered trademarks or trademarks of Microsoft Corporation in the United States and/or other countries.
