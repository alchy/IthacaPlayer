## Cesty na build soubory

[build]   juce_vst3_helper.vcxproj .\build\Debug\juce_vst3_helper.exe
[build]   IthacaPlayer.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\IthacaPlayer_SharedCode.lib
[build]   IthacaPlayer_VST3.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\VST3\IthacaPlayer.vst3\Contents\x86_64-win\IthacaPlayer.vst3
[build]   IthacaPlayer_Standalone.vcxproj -> .\build\IthacaPlayer_artefacts\Debug\Standalone\IthacaPlayer.exe


## Nastavení vývojového prostředí

Pro kompilaci C++ projektu s CMakeLists.txt ve Visual Studio Code (VS Code) proveďte následující kroky. Předpokládá se instalace Visual Studio Build Tools (s MSVC kompilátorem: https://visualstudio.microsoft.com/cs/visual-cpp-build-tools/) a CMake.

### Požadavky
- Visual Studio Build Tools (MSVC kompilátor).
- CMake nainstalovaný a přidán do PATH (např. C:\Program Files\CMake\bin).
- VS Code.

### Kroky nastavení
1. **Instalace rozšíření ve VS Code**:
   - Otevřete Extensions (Ctrl+Shift+X).
   - Nainstalujte: C/C++ (od Microsoftu pro podporu C++ syntaxe, IntelliSense a ladění) a CMake Tools (od Microsoftu pro integraci CMake).
   - Restartujte VS Code.
2. **Otevření projektu**:
   - Přejděte na File > Open Folder a vyberte složku s CMakeLists.txt.
3. **Výběr kompilátoru (kit)**:
   - V Command Palette (Ctrl+Shift+P) napište "CMake: Select a Kit".
   - Vyberte "amd64" (64-bit) nebo ekvivalent podle potřeby (např. x64 pro moderní systémy).
4. **Konfigurace projektu**:
   - V Command Palette napište "CMake: Configure". To vygeneruje build soubory (obvykle ve složce "build").
5. **Build projektu**:
   - V Command Palette napište "CMake: Build" nebo použijte Shift+Ctrl+B (nyní nabídne CMake úlohy).
6. **Debugování (volitelně)**:
   - Nastavte breakpointy a spusťte "CMake: Debug" v Command Palette.
