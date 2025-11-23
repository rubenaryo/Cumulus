@echo off
setlocal enabledelayedexpansion

REM Highest existing index (1..MAX)
set "MAX=128"

for /L %%N in (%MAX%,-1,1) do (
    REM Zero-pad old number (1 -> 001)
    set "OLDNUM=00%%N"
    set "OLDNUM=!OLDNUM:~-3!"

    REM New number is old - 1 (1 -> 0, 128 -> 127)
    set /a NEWNUM=%%N-1
    set "NEWSTR=00!NEWNUM!"
    set "NEWSTR=!NEWSTR:~-3!"

    REM Build old and new filenames
    set "OLDNAME=NubisVoxelCloudNoise.!OLDNUM!.tga"
    set "NEWNAME=nubis_voxel_cloud_noise_!NEWSTR!.tga"

    if exist "!OLDNAME!" (
        echo ren "!OLDNAME!" "!NEWNAME!"
        ren "!OLDNAME!" "!NEWNAME!"
    )
)

endlocal
echo Done.
