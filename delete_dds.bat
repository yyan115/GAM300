@echo off
echo Deleting all .dds files under Project\Resources...
for /r "%~dp0Project\Resources" %%f in (*.dds) do (
    del /q "%%f"
    echo Deleted: %%f
)
echo.
echo Done. Launch the editor to recompile textures.
pause
