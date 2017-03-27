@echo off 
SET FXC_COMPILE_CS=fxc.exe /nologo /T cs_5_0 /O3

echo Compiling DepthOfFieldFX_FastFilterDOF.hlsl
%FXC_COMPILE_CS% /E FastFilterSetup           /Vn g_csFastFilterSetup           ..\DepthOfFieldFX_FastFilterDOF.hlsl /Fh ..\inc\CS_FAST_FILTER_SETUP.inc
%FXC_COMPILE_CS% /E QuarterResFastFilterSetup /Vn g_csFastFilterSetupQuarterRes ..\DepthOfFieldFX_FastFilterDOF.hlsl /Fh ..\inc\CS_FAST_FILTER_SETUP_QUARTER_RES.inc
%FXC_COMPILE_CS% /E BoxFastFilterSetup        /Vn g_csBoxFastFilterSetup        ..\DepthOfFieldFX_FastFilterDOF.hlsl /Fh ..\inc\CS_BOX_FAST_FILTER_SETUP.inc
%FXC_COMPILE_CS% /E VerticalIntegrate         /Vn g_csDoubleVerticalIntegrate   ..\DepthOfFieldFX_FastFilterDOF.hlsl /Fh ..\inc\CS_DOUBLE_VERTICAL_INTEGRATE.inc
%FXC_COMPILE_CS% /E VerticalIntegrate         /Vn g_csVerticalIntegrate         ..\DepthOfFieldFX_FastFilterDOF.hlsl /Fh ..\inc\CS_VERTICAL_INTEGRATE.inc /DDOUBLE_INTEGRATE=0
%FXC_COMPILE_CS% /E ReadFinalResult           /Vn g_csReadFinalResult           ..\DepthOfFieldFX_FastFilterDOF.hlsl /Fh ..\inc\CS_READ_FINAL_RESULT.inc




